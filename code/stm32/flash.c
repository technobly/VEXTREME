/*
 *  Copyright (C) 2015 Jeroen Domburg <jeroen at spritesmods.com>
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <stdlib.h>

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/spi.h>
#include <libopencm3/usb/usbd.h>
#include <libopencm3/usb/msc.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/systick.h>
#include "flash.h"
#include "xprintf.h"

static volatile uint32_t flashBlkAge;
static uint8_t flashBlkData[4096];
static int flashBlkAddr=-1;

// SPI flash - W25Q128VF
// CS -  50 - PA15 - SPI1_NSS
// DO -  56 - PB4  - SPI1_MISO
// CLK - 55 - PB3  - SPI1_SCK
// DI -  57 - PB5  - SPI1_MOSI
// This flash has 4K sectors, divided in 256-byte pages.
// This means that if we write a (512-byte) sector, we need to
// read the 4k sector, erase it, modify the 512 bytes in the RAM
// buffer, then write the buffer back to the recently-erased sector.

// We actually do some caching here: as soon as the first 512-byte sector
// of a 4K page is written, we read the 4K sector into RAM and modify the RAM
// buffer to reflect the 512byte write. We don't write it back yet, there may
// be more writes incoming. If they do, we again modify the in-memory page. If
// there's a write to a different page or if 300ms seconds have passed, we
// write back the in-RAM page.

// To be called every 50th of a second
void flashTick() {
	flashBlkAge++;
	if (flashBlkAddr!=-1 && flashBlkAge>15) {
		flashDoWriteback();
		xprintf("Auto writeback after timeout\n");

		// We arrive here after each file copied to the cart or when the mounted drive is ejected.
	}
}

static void flashCs(int i) {
	if (i) gpio_set(GPIOA, GPIO15); else gpio_clear(GPIOA, GPIO15);
}

__attribute__((unused))
static void flashEraseChip(void) {
	int status;
	xprintf("Erasing chip...\n");
	//Send write enable
	flashCs(0);
	spi_xfer(SPI1, 6);
	flashCs(1);

	//Erase block
	flashCs(0);
	spi_xfer(SPI1, 0xc7); //chip erase
	flashCs(1);

	//Wait till erase is done
	do {
		flashCs(0);
		spi_xfer(SPI1, 5); //read status 1
		status=spi_xfer(SPI1, 0);
		flashCs(1);
	} while (status&1);
	xprintf("Erasing chip done.\n");
}

void flashInit(void) {
	static bool flash_init = false; // only allow flash to be initialized once
	if (flash_init) {
		return;
	}
	flash_init = true;

	int id, mf;
	gpio_mode_setup(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO15);
	gpio_mode_setup(GPIOB, GPIO_MODE_AF, GPIO_PUPD_NONE,
			GPIO3 | GPIO4 | GPIO5);
	gpio_set_af(GPIOB, GPIO_AF5, GPIO3 | GPIO4 | GPIO5);

	rcc_periph_clock_enable(RCC_SPI1);
	SPI1_I2SCFGR = 0;
	spi_init_master(SPI1, SPI_CR1_BAUDRATE_FPCLK_DIV_2, SPI_CR1_CPOL_CLK_TO_0_WHEN_IDLE, SPI_CR1_CPHA_CLK_TRANSITION_1, 
		SPI_CR1_DFF_8BIT,SPI_CR1_MSBFIRST);
	spi_enable_software_slave_management(SPI1);
	spi_set_nss_high(SPI1);
	spi_enable(SPI1);
	//Wait till SPI chip has started working
	do {
		flashCs(0);
		spi_xfer(SPI1, 0x90);
		spi_xfer(SPI1, 0);
		spi_xfer(SPI1, 0);
		spi_xfer(SPI1, 0);
		mf=spi_xfer(SPI1, 0);
		id=spi_xfer(SPI1, 0);
		flashCs(1);
	} while (mf==0 || mf==0xff || id==0 || id==0xff);
	xprintf("Flash inited (id/mf %x/%x).\n", id, mf);

	// Maybe you need to uncomment this to recover your SPI flash from being corrupted :)
	// flashEraseChip();
}


void flashDoWriteback() {
	int x, i, status;
	if (flashBlkAddr==-1) return;
	xprintf("Doing writeback on blk addr %x\n", flashBlkAddr);

//	printf("Writeback data:\n");
//	for (x=0; x<4096; x+=32) put_dump(flashBlkData+x, flashBlkAddr+x, 32, DW_CHAR);

	//Send write enable
	flashCs(0);
	spi_xfer(SPI1, 6);
	flashCs(1);

	//Erase block
	flashCs(0);
	spi_xfer(SPI1, 0x20); //sector erase
	spi_xfer(SPI1, (flashBlkAddr>>16)&0xff);
	spi_xfer(SPI1, (flashBlkAddr>>8)&0xff);
	spi_xfer(SPI1, (flashBlkAddr)&0xff);
	flashCs(1);

	//Wait till erase is done
	do {
		flashCs(0);
		spi_xfer(SPI1, 5); //read status 1
		status=spi_xfer(SPI1, 0);
		flashCs(1);
	} while (status&1);
//	xprintf("Blk erased.\n");

	//Write blocks
	for (i=0; i<4096; i+=256) {
		//Send write enable
		flashCs(0);
		spi_xfer(SPI1, 6);
		flashCs(1);

		//Send sector
		flashCs(0);
		spi_xfer(SPI1, 0x2); //page program
		spi_xfer(SPI1, (flashBlkAddr>>16)&0xff);
		spi_xfer(SPI1, (flashBlkAddr>>8)&0xff);
		spi_xfer(SPI1, (flashBlkAddr)&0xff);
		for (x=0; x<256; x++) {
			spi_xfer(SPI1, flashBlkData[x+i]);
		}
		flashCs(1);

		//Wait till write is done
		do {
			flashCs(0);
			spi_xfer(SPI1, 5); //read status 1
			status=spi_xfer(SPI1, 0);
			flashCs(1);
		} while (status&1);

		flashBlkAddr+=256;
	}
//	xprintf("Blk written.\n");
	gpio_clear(GPIOB, GPIO0); //turn off LED

	flashBlkAddr=-1;
}

int flashReadBlk(uint32_t lba, uint8_t *copy_to) {
	uint32_t addr=(lba*512);
	int x;
//	xprintf("Read LBA %d\n", lba);
	flashDoWriteback();
	flashCs(0);
	spi_xfer(SPI1, 0x0B); //fast read
	spi_xfer(SPI1, (addr>>16)&0xff);
	spi_xfer(SPI1, (addr>>8)&0xff);
	spi_xfer(SPI1, (addr)&0xff);
	spi_xfer(SPI1, 0); //dummy
	for (x=0; x<512; x++) copy_to[x]=spi_xfer(SPI1, 0);
	flashCs(1);
	return 0;
}

int flashWriteBlk(uint32_t lba, const uint8_t *copy_from) {
	uint32_t addr=(lba*512);
	uint32_t blkaddr=addr&(~4095);
	int x;

//	xprintf("Write to LBA %d addr %x:\n", lba, addr);
//	for (x=0; x<512; x+=32) put_dump(copy_from+x, addr+x, 32, DW_CHAR);


//	xprintf("Write lba %d (addr %x blkaddr %x)\n", lba, addr, blkaddr);

	if (blkaddr != (uint32_t)flashBlkAddr) {
		//Write back data in block cache, if any
		flashDoWriteback();
		gpio_set(GPIOB, GPIO0); //turn on LED
		//Read orig data into block cache
		flashCs(0);
		spi_xfer(SPI1, 0x0B); //fast read
		spi_xfer(SPI1, (blkaddr>>16)&0xff);
		spi_xfer(SPI1, (blkaddr>>8)&0xff);
		spi_xfer(SPI1, (blkaddr)&0xff);
		spi_xfer(SPI1, 0); //dummy
		for (x=0; x<4096; x++) flashBlkData[x]=spi_xfer(SPI1, 0);
		flashCs(1);

		flashBlkAddr=blkaddr;
	}

	//Modify data to reflect new sector
//	xprintf("Modifying block buffer for %x at %x\n", flashBlkAddr, (addr&(7<<9)));
	for (x=0; x<512; x++) {
		flashBlkData[x+(addr&(7<<9))]=copy_from[x];
	}
	flashBlkAge=0;

	return 0;
}


