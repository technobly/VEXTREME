/*
 * test_vxe_durable.c - HOST test for the VXE durability fix (vxe_kernel.c,
 * k_f_*_durable).
 *
 * Models flash.c's storage exactly: ONE 4 KB block in a RAM write-back cache,
 * written back when another block is written or ANY block is read, or by
 * flashDoWriteback() - and LOST at power-off. Two small stand-in games save a
 * config file the way a real game does (write a .tmp, delete the old file,
 * rename), through the kernel's file table and path mapping and the tree's
 * FatFs, then the power is cut right after the save and the card is remounted
 * from "flash" only:
 *   - PLAIN game (never calls flashDoWriteback) on the OLD table entries
 *     (plain close/rename/unlink): must LOSE the save, reproducing the state
 *     seen on hardware (a .tmp with no final file);
 *   - PLAIN game on the real (durable) table: the save must survive;
 *   - CAREFUL game (calls flashDoWriteback itself, as hello.c does) on the OLD
 *     table: must survive - the game-level fix alone is enough, which is what
 *     a standalone build with no VXE kernel relies on.
 *
 * Build and run: tools/test_vxe_durable.sh.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* flashDoWriteback() is called by vxe_kernel.c: give it the model's flush. */
static void model_writeback(void);
void flashDoWriteback(void) { model_writeback(); }

#include "vxe/vxe_kernel.c"
#include "fatfs/diskio.h"

/* ---- flash.c's cache, modelled ---- */
#define IMG_SECTORS 131072                      /* 64 MB */
static unsigned char flash_img[IMG_SECTORS * 512];
static unsigned char blk[4096];
static long blk_addr = -1;                      /* byte address of the cached 4 KB block, -1 = clean */

static void model_writeback(void)
{
    if (blk_addr < 0) return;
    memcpy(flash_img + blk_addr, blk, 4096);
    blk_addr = -1;
}
static void power_cut(void) { blk_addr = -1; }  /* the RAM cache is simply gone */

DSTATUS disk_initialize(BYTE p) { (void)p; return 0; }
DSTATUS disk_status(BYTE p) { (void)p; return 0; }
DRESULT disk_read(BYTE p, BYTE *b, DWORD s, UINT n)
{
    (void)p;
    while (n--) { model_writeback(); memcpy(b, flash_img + (long)s * 512, 512); b += 512; s++; }   /* flashReadBlk */
    return RES_OK;
}
DRESULT disk_write(BYTE p, const BYTE *b, DWORD s, UINT n)
{
    (void)p;
    while (n--) {                                                                                  /* flashWriteBlk */
        long addr = (long)s * 512, ba = addr & ~4095L;
        if (ba != blk_addr) { model_writeback(); memcpy(blk, flash_img + ba, 4096); blk_addr = ba; }
        memcpy(blk + (addr - ba), b, 512);
        b += 512; s++;
    }
    return RES_OK;
}
DRESULT disk_ioctl(BYTE p, BYTE c, void *b) { (void)p; (void)c; (void)b; return RES_OK; }   /* as ff_diskio.c */

/* ---- the rest of what vxe_kernel.c links against ---- */
void doLedOn(int on) { (void)on; }
void ledsClear(void) {}
void ledsUpdate(void) {}
void rainbowStep(uint8_t s) { (void)s; }
uint32_t millis(void) { return 0; }
uint32_t dwt_read_cycle_counter(void) { return 0; }
void xvprintf(const char *f, va_list a) { (void)f; (void)a; }
HighScoreRetVal highScoreGet(const unsigned char *g, const uint32_t crcvx, GameFileRecordV1 *r) { (void)g; (void)crcvx; (void)r; return 0; }
HighScoreRetVal highScoreSetGameRecordToDefaults(const unsigned char *g, const uint32_t crcvx, GameFileRecordV1 *r) { (void)g; (void)crcvx; (void)r; return 0; }
void highScoreSave(unsigned char *s) { (void)s; }
void vxeKernelMenuScoreSet(const unsigned char s[7]) { (void)s; }
void vxe_payload_init(const VxeKernelApi *api, VxePayloadHooks *hooks) { (void)api; (void)hooks; }
char *romData, *cartData; unsigned char parmRam[256]; GameFileRecordV1 *pActiveGameData;

static FATFS fs;
static int exists(const char *p) { FIL f; if (f_open(&f, p, FA_READ) != FR_OK) return 0; f_close(&f); return 1; }

/* A game's save, written against the kernel table it was given - exactly the call pattern of a
 * real game's config save: write a .tmp, close, delete the old file, rename the .tmp over it.
 * `careful` adds the flashDoWriteback() a careful game makes at the end. */
static void game_save(const VxeKernelApi *t, int careful, const char *text)
{
    FIL f; UINT w;
    t->k_f_open(&f, "/cfg.tmp", FA_CREATE_ALWAYS | FA_WRITE);
    t->k_f_write(&f, text, (UINT)strlen(text), &w);
    t->k_f_close(&f);
    t->k_f_unlink("/cfg.dat");
    t->k_f_rename("/cfg.tmp", "/cfg.dat");
    if (careful) t->k_flashDoWriteback();
}
/* ...and its boot: recover a leftover .tmp whose final file is missing (a read follows, so it lands). */
static void game_boot(const VxeKernelApi *t)
{
    FIL f; char line[64];
    if (t->k_f_open(&f, "/cfg.dat", FA_READ) == FR_OK) { t->k_f_close(&f); return; }
    if (t->k_f_open(&f, "/cfg.tmp", FA_READ) == FR_OK) { t->k_f_close(&f); t->k_f_rename("/cfg.tmp", "/cfg.dat"); }
    if (t->k_f_open(&f, "/cfg.dat", FA_READ) == FR_OK) { t->k_f_gets(line, (int)sizeof line, &f); t->k_f_close(&f); }
}

static int run(const char *label, const VxeKernelApi *table, int careful)
{
    FIL f; UINT w; char line[64];
    int cfg, tmp, ok_text = 0;

    /* start: the card as the hardware left it - a .tmp present, no final file */
    f_mount(&fs, "", 1);
    f_unlink("/roms/VXE/cfg.dat"); f_unlink("/roms/VXE/cfg.tmp");
    f_open(&f, "/roms/VXE/cfg.tmp", FA_CREATE_ALWAYS | FA_WRITE);
    f_write(&f, "old=1\n", 6, &w); f_close(&f);
    model_writeback();

    vxeKernelSetGamePath("/roms/VXE/GAME.bin");
    game_boot(table);
    game_save(table, careful, "value=5,-7\n");   /* the LAST thing before power-off */

    power_cut();
    f_mount(0, "", 0); f_mount(&fs, "", 1);      /* power back on: FatFs re-reads everything from flash */
    cfg = exists("/roms/VXE/cfg.dat"); tmp = exists("/roms/VXE/cfg.tmp");
    if (cfg && f_open(&f, "/roms/VXE/cfg.dat", FA_READ) == FR_OK) {
        f_gets(line, (int)sizeof line, &f); f_close(&f);
        ok_text = strcmp(line, "value=5,-7\n") == 0;
    }
    printf("%-34s after power cut: cfg.dat=%s cfg.tmp=%s%s\n", label, cfg ? "yes" : "no", tmp ? "yes" : "no",
           cfg ? (ok_text ? "  (holds the saved values)" : "  (WRONG values)") : "");
    f_mount(0, "", 0);
    return cfg && !tmp && ok_text;
}

int main(void)
{
    VxeKernelApi old_table;
    FRESULT r;
    int fails = 0;

    /* _USE_MKFS is 0 in this tree, so the volume comes from a FAT32 image made
     * elsewhere (VXE_TEST_IMG), loaded straight into the model's "flash". */
    {
        FILE *fp = fopen(getenv("VXE_TEST_IMG") ? getenv("VXE_TEST_IMG") : "", "rb");
        if (!fp) { printf("set VXE_TEST_IMG to a FAT32 image (e.g. hdiutil create -size 64m -fs \"MS-DOS FAT32\" -layout NONE)\n"); return 2; }
        fread(flash_img, 1, sizeof(flash_img), fp); fclose(fp);
    }
    if ((r = f_mount(&fs, "", 1)) != FR_OK) { printf("mount failed %d\n", r); return 2; }
    f_mkdir("/roms"); f_mkdir("/roms/VXE");
    f_mount(0, "", 0); model_writeback();

    old_table = vxe_api;                        /* the entries before the durability fix */
    old_table.k_f_close  = f_close;
    old_table.k_f_sync   = f_sync;
    old_table.k_f_rename = k_f_rename;
    old_table.k_f_unlink = k_f_unlink;

    if (run("PLAIN game, OLD table:", &old_table, 0)) { printf("FAIL: the old table did NOT reproduce the hardware loss - the model is wrong\n"); fails++; }
    else                                               printf("ok   plain game + plain table reproduces the hardware state (cfg lost at power-off)\n");
    if (!run("PLAIN game, DURABLE table:", &vxe_api, 0)) { printf("FAIL: the durable table still loses the save\n"); fails++; }
    else                                                  printf("ok   durable table keeps the save across a power cut\n");
    if (!run("CAREFUL game, OLD table:", &old_table, 1)) { printf("FAIL: a game that calls flashDoWriteback still loses the save\n"); fails++; }
    else                                                  printf("ok   a game that writes back itself keeps the save even with the plain table\n");
    return fails;
}
