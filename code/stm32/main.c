/***************************************************************************
  Copyright (C) 2020 Brett Walach <technobly at gmail.com>
  --------------------------------------------------------------------------
  VEXTREME main.c

  The magic starts here!

  Original header follows:
  --------------------------------------------------------------------------
  Copyright (C) 2015 Jeroen Domburg <jeroen at spritesmods.com>

  This library is free software: you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with this library.  If not, see <http://www.gnu.org/licenses/>.
  *************************************************************************/

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/syscfg.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/usart.h>
#include <libopencm3/stm32/exti.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/pwr.h>
#include <libopencm3/stm32/flash.h>
#include <libopencm3/cm3/dwt.h>
#include <stdlib.h>
#include <string.h>

#include "sys.h"
#include "led.h"
#include "delay.h"
#include "main.h"
#include "menu.h"
#include "msc.h"
#include "highscore.h"
#include "settings.h"
// #include "flash.h"

//#include "rom.h"
#include "xprintf.h"
#include "fatfs/ff.h"

// Externs
extern GameFileRecord* pActiveGameData;
extern char menu_start;
extern char menu_end;

#define MENU_DATA_ADDR 		0x4000							     // fixed location in menu.bin
#define MENU_DATA_SIZE 		0x200							       // size allocated for menu data
#define MENU_SYS_ADDR			(MENU_DATA_ADDR - 0x20)  // 32 bytes reserved for system data
#define MENU_MEM_ADDR			(MENU_DATA_ADDR - 0x140) // 288 bytes reserved for settings data (0x3FC0)
#define PARM_RAM_ADDR			0x7f00
#define PARM_RAM_SIZE			0xfe

//Memory for the menu ROM and the running cartridge.
//We keep both in memory so we can quickly exchange them when a reset has been detected.
char menuData[20*1024];				   // 16KB for menu ROM, 4KB for menu data
char devmodeData[2*1024];
char* romData = menuData;
unsigned char parmRam[256];

char menuDir[_MAX_LFN + 1];
static bool isItemAFile = false; // Default to being a directory and not a file

static SettingsRecord settings 			= {0};
static bool 					settingsReady = false;

union cart_and_listing {
	dir_listing listing;
	char cartData[64*1024];
};

union cart_and_listing c_and_l;
char* cartData = c_and_l.cartData;

system_options sys_opt;
char* sysData = (char*)&sys_opt;
uint8_t checkDevMode = 0;

FILINFO cart_file_info;

/*
//Pinning:
A0-A14,SWCTL - PC0-PC15
D0-D7 - PA0-PA7
nWR - PB1
nCART - PB15
led - PB0
USBPWR - PA9
*/

#define SYSCFG_MEMRMP			MMIO32(SYSCFG_BASE + 0x00)

// Local functions
static void doStartRom(void);

void uart_output_func(unsigned char c){
	uint32_t reg;
	do {
		reg = USART_SR(USART1);
	} while ((reg & USART_SR_TXE) == 0);
	USART_DR(USART1) = (uint16_t) c & 0xff;
}

void applyLedSettings(bool initial) {
	SettingsRecord *s = &settings;
	uint8_t i = ledsNumPixels();
	uint8_t l = settings.led_luma * 8;
	uint32_t c =    ((s->led_red   * 8 + 7) << 16)
								| ((s->led_green * 8 + 7) << 8)
								| ((s->led_blue  * 8 + 7) << 0);

	switch (settings.led_mode)
	{
	case 0:  // off
		ledsClear();
		ledsUpdate();
		break;

	case 1:  // rainbow
		ledsSetBrightness(l);
		if (initial) {
			rainbowStep(4);
		}
		break;

	case 2:  // solid color
		ledsSetBrightness(l);
		while (i > 0) {
			ledsSetPixelColor(--i, c);
		}
		ledsUpdate();
		break;
	
	default:
		break;
	}
}

// Asm function
extern void romemu(void);

// Load a ROM into cartridge memory
void loadMenu() {
	bool use_embedded_menu = (f_stat("/menu.bin", &cart_file_info) != FR_OK);

	loadRomWithHighScore("/menu.bin", false, use_embedded_menu);
}

void loadRom(char *fn) {
	loadRomWithHighScore(fn, false, false); // by default, do not load high score mode
}

void loadRomWithHighScore(char *fn, bool load_hs_mode, bool use_embedded_menu) {
	const int SCORE_ADDR = MENU_SYS_ADDR + 0x10;
	FIL f;
	FRESULT fr = FR_NO_FILE; // assume no file, so we can test if we ever opened the file later
	UINT r = 0;
	int n;
	UINT x;
	if (romData == c_and_l.cartData) {
		// loaded cart data
		n = sizeof(c_and_l.cartData);
	} else {
		// loaded menu data
		n = sizeof(menuData);
	}
	if (romData == menuData && use_embedded_menu == true) {
		xprintf("Copying %lu bytes of menu data... ", &menu_end - &menu_start);
		memcpy(menuData, &menu_start, &menu_end - &menu_start);
		xprintf("OK!\n");
	} else {
		fr = f_open(&f, fn, FA_READ);
		if (fr) {
			xprintf("Error opening file: %d\n", fr);
		} else {
			xprintf("Opened file: %s\n", fn);
		}
		f_read(&f, romData, 64*1024, &r);
		xprintf("Read %d bytes of rom data.\n", r);
	}
	// It's a game and it's <= 32KB
	if (n > 32*1024 && r <= 32*1024) {
		// pad with 0x01 for Mine Storm II and Polar Rescue (and any
		// other buggy game that reads outside its program space)
		for (x = r; x < 32*1024; x++) {
			romData[x] = 0x01;
		}
		xprintf("Padded remaining %d bytes of rom data with 0x01\n", x - r);
		//Duplicate bank to upper bank
		for (n = 0; n < 32*1024; n++) {
			romData[n+32*1024] = romData[n];
		}

		if (load_hs_mode) {
			// Search for game in highscore file and if it exists read it into
			// the active game high score record.  If the game doesn't exist,
			// then setup record to default values for high score and add the name.
			// A NULL as the second param will use the default active pointer.
			HighScoreRetVal hs_ret_val = highScoreGet((const unsigned char *)&romData[0x11], NULL);
			if (hs_ret_val != HIGH_SCORE_SUCCESS) {
				xprintf("highScoreGet failure: %d, creating default high score for: %s\n", hs_ret_val, fn);
				// Set the active game record to defaults and change name to the current
				// game we are loading. This will prepare it to be stored once we
				// acquire a new high score from the game ROM running.
				hs_ret_val = highScoreSetGameRecordToDefaults((const unsigned char *)&romData[0x11], NULL);
				if (hs_ret_val != HIGH_SCORE_SUCCESS) {
					xprintf("highScoreSetGameRecordToDefaults failure: %d\n", hs_ret_val);
				}
			} else {
				xprintf("highScoreGet success\n");
			}

			// Store high score towards the end of the menu data
			menuData[SCORE_ADDR + 0x00] = pActiveGameData->maxScore[0];
			menuData[SCORE_ADDR + 0x01] = pActiveGameData->maxScore[1];
			menuData[SCORE_ADDR + 0x02] = pActiveGameData->maxScore[2];
			menuData[SCORE_ADDR + 0x03] = pActiveGameData->maxScore[3];
			menuData[SCORE_ADDR + 0x04] = pActiveGameData->maxScore[4];
			menuData[SCORE_ADDR + 0x05] = pActiveGameData->maxScore[5];
			menuData[SCORE_ADDR + 0x06] = 0x80;
			menuData[MENU_SYS_ADDR] = 0x77; // High Score flag (IDLE:0x66, LOAD2VEC:0x77, SAVE2STM:0x88)
			// parmRam[0xe0] = 0x77; // High Score flag (IDLE:0x66, LOAD2VEC:0x77, SAVE2STM:0x88)

			// Don't start game yet, stay in menu so that the VEXTREME menu
			// can read and store the high score
			romData = menuData;
		} // end if (load_hs_mode)
	}
	// It's the menu, patch in the HW/SW versions
	else if (romData == menuData) {
		char* ptr1 = strstr(menuData, "11");
		char* ptr2 = strstr(menuData, "22");
		char* ptr3 = strstr(menuData, "33");
		char* ptr4 = strstr(menuData, "44");
		if (ptr1 && ptr2 && ptr3 && ptr4) {
			*ptr1++ = 'V';
			*ptr1   = '0' + (sys_opt.hw_ver >> 8) % 10;
			*ptr2++ = '0' + (sys_opt.hw_ver & 0xFF) / 10;
			*ptr2   = '0' + (sys_opt.hw_ver & 0xFF) % 10;
			*ptr3++ = 'V';
			*ptr3   = '0' + (sys_opt.sw_ver >> 8) % 10;
			*ptr4++ = '0' + (sys_opt.sw_ver & 0xFF) / 10;
			*ptr4   = '0' + (sys_opt.sw_ver & 0xFF) % 10;
		}
		// if (load_hs_mode) {
		// 	// parmRam[0xe0] = 0x66; // High Score flag (IDLE:0x66, LOAD2VEC:0x77, SAVE2STM:0x88)
		// 	menuData[MENU_SYS_ADDR] = 0x66;
		// }

		if (settingsReady) {
			xprintf("Copying %lu bytes of settings data... ", sizeof(SettingsRecord));
			// copy setting to ROM
			memcpy(&menuData[MENU_MEM_ADDR], &settings, sizeof(SettingsRecord));
			xprintf("OK!\n");
		}
	}
	if (fr != FR_NO_FILE) {
		f_close(&f);
	}
}

//Stream data, for the Bad Apple demo
//Name of the file sucks and I'd like to make a rpc function that's a bit
//more universal... you should eg be able to pass through the name of the
//file you'd like to stream and the address and chunk size... but this is
//a start.
FIL streamFile;
int streamLoaded=0;

void loadStreamData(int addr, int len) {
	UINT r=0;
	if (!streamLoaded) {
		f_open(&streamFile, "vec.bin", FA_READ);
		streamLoaded=1;
	}
	f_read(&streamFile, &romData[addr], len, &r);
}

void doUpDir() {
	if (strcmp(menuDir, "/roms") != 0) {
		doChangeDir("..");
	}
}

void doChangeDir(char* dirname) {
	xprintf("Found directory: %s\n", dirname);
	if (strcmp(dirname,"..") == 0) {
		char* ptr = strrchr(menuDir,'/');
		if (ptr != NULL) {
			*ptr = '\0';
		}
	} else {
		xsprintf(menuDir, "%s/%s", menuDir, dirname);
	}

	romData=menuData;
	loadListing(menuDir, &c_and_l.listing, MENU_DATA_ADDR, 
	            MENU_DATA_ADDR + MENU_DATA_SIZE, romData);

	xprintf("Done listing for : %s\n", menuDir);

	// save current directory and reset cursor
	settings.last_cursor = 0;
	strncpy(settings.directory, menuDir, sizeof(menuDir));
	syncSettings();
}

void syncSettings() {
	memcpy(&menuData[MENU_MEM_ADDR], &settings, sizeof(SettingsRecord));
	settingsSave(&menuData[MENU_MEM_ADDR]);
}

/**
 * User has made a selection in the cart menu (chose the i'th item)
 * so now we have to load the ROM or Directory
 */
void doChangeRom(char* basedir, int i) {
	char buff[300];

	xprintf("Changing to rom no %d in %s\n", i, basedir);
	sortDirectory(basedir, &c_and_l.listing); // recreate file listing, as loading a cart overwrote the union
	file_entry f = c_and_l.listing.f_entry[i];

	if (f.is_dir) {
		isItemAFile = false;
		doChangeDir(f.fname);
	} else {
		isItemAFile = true;
		xprintf("Adding filename [%s] to path\n", f.fname);
		xsprintf(buff, "%s/%s", basedir, f.fname);
		
		// save current cursor position selection
		settings.last_cursor = i;
		syncSettings();

		romData=c_and_l.cartData;
		xprintf("Going to read rom image %s\n", buff);
		loadRomWithHighScore(buff, true, false);
	}


}

/**
 * Point romData to the cartData or menuData based on what was last loaded
 * in doChangeRom() and return.
 */
static void doStartRom(void) {
	if (isItemAFile == true) {
		romData = c_and_l.cartData;
	} else {
		romData = menuData;
	}
}

void updateAll() {
	uint16_t i = ledsNumPixels();
	while (i > 0) {                     // color index
		ledsSetPixelColor(--i, colors[(int)parmRam[254]]);
	}
	ledsUpdate();
}

void updateOne() {
	//                led index        , color index
	ledsSetPixelColor((int)parmRam[253], colors[(int)parmRam[254]]);
	ledsUpdate();
}

void updateMulti() {
	// uint16_t i = ledsNumPixels();
	for (uint16_t i = 0; i < ledsNumPixels(); i++) {
		// xprintf("LED%d = %d\n", i, (int)parmRam[0xf0 + i]);
		ledsSetPixelColor(i, colors[(int)parmRam[0xf0 + i]]); // 0xf0 = LED0, 0xf9 = LED9
	}
	// xprintf("\n");
	ledsUpdate();
}

void doLedOn(int on) {
	if (on) {
		gpio_set(GPIOB, GPIO0);
	} else {
		gpio_clear(GPIOB, GPIO0);
	}
}

// This function to be used by games (cartData)
// Load HW/SW versions so that the menu can access and display them
// Warning: This permanently alters the last 4 bytes of 32K ROM game data!
void loadVersions() {
	c_and_l.cartData[0x7ffc] = sys_opt.hw_ver >> 8;
	c_and_l.cartData[0x7ffd] = sys_opt.hw_ver & 0xFF;
	c_and_l.cartData[0x7ffe] = sys_opt.sw_ver >> 8;
	c_and_l.cartData[0x7fff] = sys_opt.sw_ver & 0xFF;
}

// This function to be used by the Menu (multcart.asm)
// Load sys_opt data starting at address specified, up to 15 bytes specified by size.
// addr = $7ffd
// size = $7ffe
// data returned in $3ffa ~ $3ffa+size
void loadSysOpt() {
	int addr = (int)parmRam[0xfd];
	int size = (int)parmRam[0xfe];
	if (size > 15) size = 15; // limited to 15 for now
	for (int i = 0; i < size; i++) {
		menuData[MENU_SYS_ADDR + 0x1a + i] = sysData[addr + i];
		// xprintf("sysData[%x]=%u,checkDevMode=%d\n", addr + i, sysData[addr + i], checkDevMode);
	}
}

// This function to be used by the Menu (multcart.asm)
// Load parmRam data starting at address specified, up to 15 bytes specified by size.
// addr = $7ffd
// size = $7ffe
// data returned in $3fe0 ~ $3fe0+size
void loadParmRam() {
	int addr = (int)parmRam[0xfd];
	int size = (int)parmRam[0xfe];
	if (size > 16) size = 16; // limited to 15 for now
	for (int i = 0; i < size; i++) {
		menuData[MENU_SYS_ADDR + i] = parmRam[addr + i];
		// xprintf("parmRam[%x]=%u\n", addr + i, sysData[addr + i]);
	}
}

// This function to be used by games (cartData)
// Dump the data in hex bytes from starting address to ending address specified
// $7ff0 - Start Address High Byte
// $7ff1 - Start Address Low  Byte
// $7ff2 -   End Address High Byte
// $7ff3 -   End Address Low  Byte
// data output on TX pin in table format, use with RAM WRITE app for debugging
void dumpMemory() {
	// int start_addr = 0x1fe0; // hard code if desired
	// int end_addr = 0x281f;   // hard code if desired
	int start_addr = ((int)parmRam[0xf0] << 8) + (int)parmRam[0xf1];
	int end_addr = ((int)parmRam[0xf2] << 8) + (int)parmRam[0xf3];
	int current_addr = start_addr;
	xprintf("ADDR | 0001 0203 0405 0607 0809 0A0B 0C0D 0E0F 1011 1213 1415 1617 1819 1A1B 1C1D 1E1F\n");
	xprintf("==== | ===============================================================================\n");
	while ( current_addr <= end_addr ) {
		xprintf("%04x | ", current_addr);
		for (int byte = 0; byte < 32; byte++) {
			if (current_addr + byte > end_addr) break;
			xprintf("%02x", cartData[current_addr + byte]);
			if (((byte+1) % 2) == 0 && byte != 31) {
				xprintf(" ");
			}
		}
		xprintf("\n");
		current_addr += 32;
	}
}

// This function to be used by the Menu (menu.asm)
// Store 1 byte at specified address in ROM address space
// $7ff0 - Start Address High Byte
// $7ff1 - Start Address Low  Byte
// $7ff2 - 1 byte to store
void storeToRom() {
	unsigned int addr = ((int)parmRam[0xf0] << 8) + (int)parmRam[0xf1];
	uint8_t value = parmRam[0xf2];
	menuData[addr] = value;

	// settings region was updated, apply
	if (addr >= MENU_MEM_ADDR && addr <= MENU_MEM_ADDR + sizeof(SettingsRecord)) {
		memcpy(&settings, &menuData[MENU_MEM_ADDR], sizeof(SettingsRecord));

		applyLedSettings(false);
	}
}

void loadApp() {
	switch((int)parmRam[0xfe]) {
		case 0:
			romData = c_and_l.cartData;
			xprintf("Launching /devmode.bin\n");
			loadRom("/devmode.bin");
			break;
		default:
			break;
	}
}

void ledsCyan() {
	uint16_t i = ledsNumPixels();
	while (i > 0) {
		ledsSetPixelColor(--i, colors[5]);
	}
	ledsUpdate();
}

void ledsMagenta() {
	uint16_t i = ledsNumPixels();
	while (i > 0) {
		ledsSetPixelColor(--i, colors[8]);
	}
	ledsUpdate();
}

void ledsOff() {
	ledsClear();
	ledsUpdate();
}

static FATFS FatFs;
FIL cart_file;
void doRamDisk() {
	/**
	 * Normally address 0,1 is 'g',' ' which is the game 'copyright' and 'space'.
	 * We want to let the Vectrex know it's time to make one RPCFN call when 0,1 == 'v','x'
	 * and make sure we put it back to 'g',' ' in case the Vectrex gets reset.
	 * FIXME: change the place where this is done
	 * The vectrex should also make sure it only makes one call to RPCFN 10 for each
	 * operation required (START DEV MODE, EXIT DEV MODE, RUN CART.BIN).
	 */
	menuData[MENU_SYS_ADDR + 0x1c] = 0x01; // make sure we are blocking the RPCFN yield bytes again
	menuData[MENU_SYS_ADDR + 0x1d] = 0x01; // This will get overwritten when the cart.bin or menu loads
	menuData[0x0] = 0x01;   // |
	menuData[0x1] = 0x01;   // |

	switch ((int)parmRam[254]) {
		case 0: /*xprintf("WAIT DEV\n");*/ sys_opt.usb_dev = USB_DEV_WAIT; break;
		case 1: xprintf("EXIT DEV\n"); sys_opt.usb_dev = USB_DEV_EXIT; break;
		case 4: xprintf("RUN DEV\n"); sys_opt.usb_dev = USB_DEV_RUN; break;
		case 5:
			if (gpio_get(GPIOA, GPIO9)) {
				menuData[MENU_SYS_ADDR + 0x1b] = 0x99; // HIGH:0x99
			} else {
				menuData[MENU_SYS_ADDR + 0x1b] = 0x66; // LOW:0x66
			}
			// xprintf("VUSB: %x\n", menuData[MENU_SYS_ADDR + 0x1b]);
			sys_opt.usb_dev = USB_DEV_CHECK;
			break;
		default: xprintf("UNKNOWN DEV\n"); sys_opt.usb_dev = USB_DEV_DISABLED; return; break;
	}

	// attempt to close this if it's open, don't worry this is safe
	// FRESULT f_close_res;
	f_close(&cart_file);
	// xprintf("f_close result: %d\n", f_close_res);

	int ramdisk_ret = 0;
	if (sys_opt.usb_dev != USB_DEV_DISABLED && sys_opt.usb_dev != USB_DEV_CHECK) {
		ramdisk_ret = ramdiskmain(RAMDISK_NON_BLOCKING);
	}
	if (ramdisk_ret == 0 &&
		(sys_opt.usb_dev == USB_DEV_WAIT ||
		 sys_opt.usb_dev == USB_DEV_DISABLED ||
		 sys_opt.usb_dev == USB_DEV_CHECK)) {
		menuData[MENU_SYS_ADDR + 0x1c] = 'v'; // tell the Vectrex time to make one RPCFN call (wait/exit/run)
		menuData[MENU_SYS_ADDR + 0x1d] = 'x';
		return;
	} else {
		// handle errors, if we add some in ramdiskmain()
	}

	// remount the file system to pick up any changes
	// FRESULT f_mount_res;
	f_mount(&FatFs, "", 0);
	// xprintf("f_mount result: %d\n", f_mount_res);

	if (sys_opt.usb_dev == USB_DEV_RUN || sys_opt.usb_dev == USB_DEV_EJECT) {
		if (sys_opt.usb_dev == USB_DEV_RUN) xprintf("Vectrex asked to run\n");
		else if (sys_opt.usb_dev == USB_DEV_EJECT) xprintf("USB host ejected device\n");
		if (f_stat("/cart.bin", &cart_file_info) == FR_OK) {
			xprintf("Loading /cart.bin ...\n");
			if (f_open(&cart_file, "/cart.bin", FA_READ) == FR_OK) {
				romData=c_and_l.cartData; // Explicitly setting this here so we know WTF is going on in the background
				loadRom("/cart.bin");
				sys_opt.usb_dev = USB_DEV_RUN;
			}
		} else {
			xprintf("Sorry, didn't find /cart.bin\n");
			sys_opt.usb_dev = USB_DEV_DISABLED;

			(void) highScoreOpenFile(); // Create/Open highscore file

			romData=menuData; // Explicitly setting this here so we know WTF is going on in the background
			loadMenu();
			loadListing(menuDir, &c_and_l.listing, MENU_DATA_ADDR, 
									MENU_DATA_ADDR + MENU_DATA_SIZE, romData);
		}
	} else if (sys_opt.usb_dev == USB_DEV_EXIT) {
		xprintf("Exiting Developer mode\n");
		if (f_stat("/cart.bin", &cart_file_info) == FR_OK) {
			xprintf("Deleting /cart.bin ...\n");
			// FRESULT f_unlink_res;
			f_unlink("/cart.bin");
			// xprintf("f_unlink result: %d\n", f_unlink_res);
		}

		(void) highScoreOpenFile(); // Create/Open highscore file
		sys_opt.usb_dev = USB_DEV_DISABLED;
		romData=menuData; // Explicitly setting this here so we know WTF is going on in the background
		loadMenu();
		loadListing(menuDir, &c_and_l.listing, MENU_DATA_ADDR, 
		            MENU_DATA_ADDR + MENU_DATA_SIZE, romData);
	}

	menuData[0] = 'g';   // Fixup the copyright bytes now that we are exiting
	menuData[1] = ' ';   // |
}

// Handle an RPC event
void doHandleEvent(int data) {
	// xprintf("[E:%d,A:%02X]\n", data, (int)parmRam[254]);
	switch (data) {
		default:
		case 0: break;
		case 1: doChangeRom(menuDir, (int)parmRam[254]); break; // Starts changing ROM
		case 2: loadStreamData(0x4000, 1024+512); break;
		case 3: doUpDir(); break;
		case 4: updateAll(); break;
		case 5: rainbowStep((int)parmRam[254]); break;
		case 6: updateOne(); break;
		case 7: updateMulti(); break;
		case 8: ledsSetBrightness((int)parmRam[254]); break;
		case 9: loadVersions(); break;
		case 10: doRamDisk(); break;
		case 11: loadApp(); break;
		case 12: loadSysOpt(); break;
		case 13: dumpMemory(); break;
		case 14: highScoreSave(&parmRam[0]); break;
		case 15: doStartRom(); break; // Finishes changing ROM
		case 16: loadParmRam(); break;
		case 17: storeToRom(); break;
		case 18: settingsSave(&menuData[MENU_MEM_ADDR]); break;
	}
}

void doDbgHook(int adr, int data) {
	xprintf("R %x %x\n", adr, data);
}

void doLog(int data) {
	xprintf("%x\n", data);
}

int main(void) {
	void (*runptr)(void)=romemu;

	const struct rcc_clock_scale hse_8mhz_3v3_120MHz = { /* 120MHz */
		.pllm = 8,
		.plln = 240,
		.pllp = 2,
		.pllq = 5,
		.pllr = 0,
		.pll_source = RCC_CFGR_PLLSRC_HSE_CLK,
		.hpre = RCC_CFGR_HPRE_DIV_NONE,
		.ppre1 = RCC_CFGR_PPRE_DIV_4,
		.ppre2 = RCC_CFGR_PPRE_DIV_2,
		.voltage_scale = PWR_SCALE1,
		.flash_config = FLASH_ACR_ICEN | FLASH_ACR_DCEN | FLASH_ACR_LATENCY_3WS,
		.ahb_frequency  = 120000000,
		.apb1_frequency = 30000000,
		.apb2_frequency = 60000000,
	};

	rcc_clock_setup_pll(&hse_8mhz_3v3_120MHz);
	rcc_periph_clock_enable(RCC_GPIOA);
	rcc_periph_clock_enable(RCC_GPIOB);
	rcc_periph_clock_enable(RCC_GPIOC);
	rcc_periph_clock_enable(RCC_USART1);
	rcc_periph_clock_enable(RCC_SYSCFG);

	//Addressable LEDs - output
	gpio_mode_setup(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO13 | GPIO14);
	// gpio_set_output_options(GPIOB, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ,  GPIO13 | GPIO14);
	//LED - output
	gpio_mode_setup(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO0);
	//USB power - input
	gpio_mode_setup(GPIOA, GPIO_MODE_INPUT, GPIO_PUPD_PULLDOWN, GPIO9);

	//PB6/PB7: txd/rxd
	gpio_mode_setup(GPIOB, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO6 | GPIO7);
	gpio_set_af(GPIOB, GPIO_AF7, GPIO6 | GPIO7);

	usart_set_baudrate(USART1, 115200);
	usart_set_databits(USART1, 8);
	usart_set_stopbits(USART1, USART_STOPBITS_1);
	usart_set_mode(USART1, USART_MODE_TX);
	usart_set_parity(USART1, USART_PARITY_NONE);
	usart_set_flow_control(USART1, USART_FLOWCONTROL_NONE);
	/* Finally enable the USART. */
	usart_enable(USART1);
	xdev_out(uart_output_func);

	//Address lines - input (A0 - A14 & PB6)
	gpio_mode_setup(GPIOC, GPIO_MODE_INPUT, GPIO_PUPD_PULLDOWN,
		GPIO0|GPIO1|GPIO2|GPIO3|GPIO4|GPIO5|GPIO6|GPIO7|GPIO8|GPIO9|GPIO10|GPIO11|GPIO12|GPIO13|GPIO14|GPIO15);
	// gpio_mode_setup(GPIOB, GPIO_MODE_INPUT, GPIO_PUPD_NONE,
	// 	GPIO13|GPIO14);
	// IRQ
	gpio_mode_setup(GPIOB, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO9);

	//Data lines - output
	gpio_mode_setup(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE,
		GPIO0|GPIO1|GPIO2|GPIO3|GPIO4|GPIO5|GPIO6|GPIO7);

	//Control lines - input
	gpio_mode_setup(GPIOB, GPIO_MODE_INPUT, GPIO_PUPD_NONE,
		GPIO1|GPIO15);

	// dwt_enable_cycle_counter required for delay/millis
	// used instead of systick_handler to prevent interrupts from disrupting romemu.S loop
	dwt_enable_cycle_counter();

#if HW_VER == 255
	#error "USE_HW hardware version not specified, please specify e.g. USE_HW=v0.2 or USE_HW=v0.3"
#endif

	// TODO: load new options from VEXTREME/options.txt in key=val format
	sys_opt.size = sizeof(sys_opt);
	sys_opt.ver = 1;
// #if (HW_VER == 1)
// 	sys_opt.hw_ver = 0x000A; // v0.10
// #elif (HW_VER == 2)
// 	sys_opt.hw_ver = 0x0014; // v0.20
// #elif (HW_VER == 3)
// 	sys_opt.hw_ver = 0x001e; // v0.30
// #endif
	// For now, this is hard coded to determine LED operation, nothing more.
	// TODO: uncomment above for system hw_ver, and add .led_hw_ver for LED initialization.
	sys_opt.hw_ver = 0x0014; // v0.20
	sys_opt.sw_ver = 0x001e; // v0.30
	sys_opt.rgb_type = RGB_TYPE_10;
	sys_opt.usb_dev = USB_DEV_DISABLED;

	xprintf("\n");
	xprintf("[ VEXTREME booted ]\n");
	xprintf("  HW v%01d.%02d | SW v%01d.%02d\n", sys_opt.hw_ver >> 8, sys_opt.hw_ver & 0xFF, sys_opt.sw_ver >> 8, sys_opt.sw_ver & 0xFF);
	xprintf("  LED TYPE: ");
	// HW version < 0.30
	if (sys_opt.hw_ver < 0x001e) {
		// xprintf("HW VER < 0.30\n");
		if (sys_opt.rgb_type == RGB_TYPE_10) {
			xprintf("RGB_TYPE_10\n");
			ledsInitSW(10, GPIOB, GPIO14, GPIOB, GPIO13, RGB_BGR);
			ledsSetBrightness(50); // be careful not to set this too high when using white, those LEDs draw some power!!
		} else if (sys_opt.rgb_type == RGB_TYPE_4) {
			xprintf("RGB_TYPE_4\n");
			ledsInitSW(10, GPIOB, GPIO14, GPIOB, GPIO13, RGB_BGR);
			ledsSetBrightness(255); // we will be limiting to 4, it's ok to crank them all of the way up!
		} else if (sys_opt.rgb_type == RGB_TYPE_NONE) {
			xprintf("RGB_TYPE_NONE\n");
			ledsInitSW(10, GPIOB, GPIO14, GPIOB, GPIO13, RGB_BGR);
			ledsSetBrightness(0); // sleeper cart, you'll never see it coming _._
		}
	}
	// HW version >= 0.30
	else if (sys_opt.hw_ver >= 0x001e) { // >= 0.30
		// xprintf("HW VER >= 0.30\n");
		xprintf("type ignored, we only have 4!\n");
		ledsInitSW(4, GPIOB, GPIO14, GPIOB, GPIO13, RGB_BGR);
		ledsSetBrightness(255); // we will be limiting to 4, it's ok to crank them all of the way up!
	}

#if 0 // TEST LED CODE START
	while (1) {
		// color wipe back and fourth through the list of colors
		ledsClear();
		ledsSetBrightness(150);
		bool dir = true;
		for (int x = 1; x < sizeof(colors)/sizeof(*colors); x++) {
			colorWipe(dir, colors[x], 50);
			dir = !dir;
		}

		ledsClear();
		ledsSetBrightness(255);
		rainbowCycle(10);
		rainbowCycle(10);

		ledsClear();
		knightRider(6, 64, 4, 3, 9, 0xFF7700); // Cycles, Speed, Width, First, Last, RGB Color (original orange-red)
		knightRider(3, 32, 4, 3, 9, 0xFF00FF); // Cycles, Speed, Width, First, Last, RGB Color (purple)
		knightRider(3, 32, 4, 3, 9, 0x0000FF); // Cycles, Speed, Width, First, Last, RGB Color (blue)
		knightRider(3, 32, 5, 3, 9, 0x00FF00); // Cycles, Speed, Width, First, Last, RGB Color (green)
		knightRider(3, 32, 5, 3, 9, 0xFFFF00); // Cycles, Speed, Width, First, Last, RGB Color (yellow)
		knightRider(3, 32, 7, 3, 9, 0x00FFFF); // Cycles, Speed, Width, First, Last, RGB Color (cyan)
		knightRider(3, 32, 7, 3, 9, 0xFFFFFF); // Cycles, Speed, Width, First, Last, RGB Color (white)

		// Iterate through a whole rainbow of colors
		for(uint8_t j=0; j<252; j+=7) {
			knightRider(1, 16, 2, 0, 10, colorWheel(j)); // Cycles, Speed, Width, RGB Color
		}

		ledsClear();
		ledsSetBrightness(255);
		int y = 10;
		for (int x=0; x<10; x++) {
			theaterChaseRainbow(y);
			y += 5;
		}
	}
#endif // TEST LED CODE END

	// If USB power pin is high, boot into USB disk mode
	if (gpio_get(GPIOA, GPIO9)) {
		xprintf("[ Starting RAMDISK ]\n");
		ramdiskmain(RAMDISK_BLOCKING);
	}

	xprintf("[ Starting ROM Emulation ]\n");

	// FRESULT f_mount_res;
	f_mount(&FatFs, "", 0);
	// xprintf("f_mount result: %d\n", f_mount_res);

	// Create/Open highscore file
	// Ignore return value for now
	(void) highScoreOpenFile();

	// load settings from file
	SettingsRetVal sRet = settingsGet((char *)&settings);
	settingsReady = sRet == SETTINGS_SUCCESS;

	// set default settings for params used on STM side
	if (!settingsReady) {
		settings.led_mode = 1;
		settings.led_luma = 16;
	}

	// Give the cart some color, but after the USB process so we don't load down weak USB sources
	applyLedSettings(true);
	
	// set current directory
	if (settingsReady && isDirectoryExist(settings.directory)) { 
		strncpy(menuDir, settings.directory, sizeof(menuDir));
	} else {
		strncpy(menuDir, "/roms", sizeof(menuDir));
	}

	// Load the Menu
	romData=menuData; // Explicitly setting this here so we know WTF is going on in the background
	loadMenu();
	loadListing(menuDir, &c_and_l.listing, MENU_DATA_ADDR, 
	            MENU_DATA_ADDR + MENU_DATA_SIZE, romData);
	sys_opt.usb_dev = USB_DEV_DISABLED;

	// Load cart.bin and jump straight into Developer Mode if it exists
	if (f_stat("/cart.bin", &cart_file_info) == FR_OK) {
		xprintf("Loading /cart.bin ...\n");
		if (f_open(&cart_file, "/cart.bin", FA_READ) == FR_OK) {
			romData=c_and_l.cartData; // Explicitly setting this here so we know WTF is going on in the background
			loadRom("/cart.bin");
			sys_opt.usb_dev = USB_DEV_RUN;
			checkDevMode = 0;
		}
	}

	// Go emulate a ROM.
	SYSCFG_MEMRMP=0x3; //mak ram at 0
	runptr=(void*)(((int)runptr&0x1ffff)|1); //map to lower mem
	xprintf("Gonna start romemu at %08x\n", romemu);
	runptr();

	return 0;
}
