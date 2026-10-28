/*
 * test_vxe_map_path.c - HOST test for vxe_kernel.c's game-file mapping (which
 * folder a game's file lands in). Compiles the actual vxe_kernel.c, with the
 * FatFs calls it makes stubbed to record the final path.
 *
 *   cc -I. -I../libopencm3/include -DSTM32F4 -o /tmp/t vxe/test_vxe_map_path.c && /tmp/t
 *
 * (run from code/stm32). Exits non-zero on the first failure.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "vxe/vxe_kernel.c"     /* the code under test, statics included */

/* ---- stubs for everything vxe_kernel.c links against ---- */
static char last_path[400], last_path2[400];
FRESULT f_open(FIL *fp, const TCHAR *path, BYTE mode) { (void)fp; (void)mode; strcpy(last_path, path); return FR_OK; }
FRESULT f_close(FIL *fp) { (void)fp; return FR_OK; }
TCHAR *f_gets(TCHAR *b, int n, FIL *fp) { (void)b; (void)n; (void)fp; return 0; }
FRESULT f_write(FIL *fp, const void *b, UINT n, UINT *w) { (void)fp; (void)b; (void)n; (void)w; return FR_OK; }
FRESULT f_sync(FIL *fp) { (void)fp; return FR_OK; }
FRESULT f_rename(const TCHAR *a, const TCHAR *b) { strcpy(last_path, a); strcpy(last_path2, b); return FR_OK; }
FRESULT f_unlink(const TCHAR *p) { strcpy(last_path, p); return FR_OK; }
void flashDoWriteback(void) {}
void doLedOn(int on) { (void)on; }
void ledsClear(void) {}
void ledsUpdate(void) {}
void rainbowStep(uint8_t s) { (void)s; }
uint32_t millis(void) { return 0; }
uint32_t dwt_read_cycle_counter(void) { return 0; }
void xvprintf(const char *f, va_list a) { (void)f; (void)a; }
static uint32_t seen_crcvx_get, seen_crcvx_def;
HighScoreRetVal highScoreGet(const unsigned char *g, const uint32_t crcvx, GameFileRecordV1 *r) { (void)g; seen_crcvx_get = crcvx; (void)r; return 0; }
HighScoreRetVal highScoreSetGameRecordToDefaults(const unsigned char *g, const uint32_t crcvx, GameFileRecordV1 *r) { (void)g; seen_crcvx_def = crcvx; (void)r; return 0; }
void highScoreSave(unsigned char *s) { (void)s; }
void vxeKernelMenuScoreSet(const unsigned char s[7]) { (void)s; }
void vxe_payload_init(const VxeKernelApi *api, VxePayloadHooks *hooks) { (void)api; (void)hooks; }
char *romData, *cartData;
unsigned char parmRam[256];
GameFileRecordV1 *pActiveGameData;

static int fails;
static void expect_open(const char *rom, const char *path, const char *want)
{
    FIL f;
    vxeKernelSetGamePath(rom);
    k_f_open(&f, path, 0);
    if (strcmp(last_path, want) != 0) { printf("FAIL rom=%-28s open %-16s -> %-26s want %s\n", rom, path, last_path, want); fails++; }
    else                                printf("ok   rom=%-28s open %-16s -> %s\n", rom, path, last_path);
}

int main(void)
{
    /* a game launched from a folder: its own files go beside it */
    expect_open("/roms/VXE/GAMEA.bin", "/score.dat",  "/roms/VXE/score.dat");
    expect_open("/roms/VXE/GAMEA.bin", "/prefs.cfg", "/roms/VXE/prefs.cfg");
    expect_open("/roms/VXE/GAMEA.bin", "/key.dat","/roms/VXE/key.dat");
    expect_open("/roms/VXE/GAMEB.bin",  "/other.cfg",  "/roms/VXE/other.cfg");
    expect_open("/roms/VXE/GAMEC.bin","/third.cfg",   "/roms/VXE/third.cfg");
    /* ...except the console calibration, shared at the root */
    expect_open("/roms/VXE/GAMEA.bin", "/cdfmeas.csv","/cdfmeas.csv");
    expect_open("/roms/VXE/GAMEA.bin", "/cdfmeas.tmp","/cdfmeas.tmp");
    expect_open("/roms/VXE/GAMEB.bin",  "/cdfmeas.csv","/cdfmeas.csv");
    expect_open("/roms/VXE/GAMEA.bin", "/calmeas.csv","/calmeas.csv");
    expect_open("/roms/VXE/GAMEA.bin", "/calmeas.tmp","/calmeas.tmp");
    /* launched from the root (/cart.bin developer mode): unchanged */
    expect_open("/cart.bin",             "/score.dat",  "/score.dat");
    /* deeper folders, and paths that are not root-level, are left alone */
    expect_open("/roms/a/b/GAME.bin",    "/score.dat",  "/roms/a/b/score.dat");
    expect_open("/roms/VXE/GAMEA.bin", "/sub/x.csv",  "/sub/x.csv");
    expect_open("/roms/VXE/GAMEA.bin", "rel.csv",     "rel.csv");
    /* a later root launch must clear the folder again */
    expect_open("/roms/VXE/GAMEA.bin", "/score.dat",  "/roms/VXE/score.dat");
    expect_open("/cart.bin",             "/score.dat",  "/score.dat");

    /* rename maps BOTH paths the same way (the .tmp save pattern) */
    vxeKernelSetGamePath("/roms/VXE/GAMEB.bin");
    k_f_rename("/other.tmp", "/other.cfg");
    if (strcmp(last_path, "/roms/VXE/other.tmp") || strcmp(last_path2, "/roms/VXE/other.cfg")) {
        printf("FAIL rename -> %s, %s\n", last_path, last_path2); fails++;
    } else printf("ok   rename /other.tmp -> /other.cfg  =>  %s -> %s\n", last_path, last_path2);
    k_f_rename("/cdfmeas.tmp", "/cdfmeas.csv");
    if (strcmp(last_path, "/cdfmeas.tmp") || strcmp(last_path2, "/cdfmeas.csv")) {
        printf("FAIL shared rename -> %s, %s\n", last_path, last_path2); fails++;
    } else printf("ok   rename (shared) stays at the root: %s -> %s\n", last_path, last_path2);
    k_f_unlink("/third.cfg");
    if (strcmp(last_path, "/roms/VXE/third.cfg")) { printf("FAIL unlink -> %s\n", last_path); fails++; }
    else printf("ok   unlink /third.cfg -> %s\n", last_path);

    /* the game keeps the two-argument high-score calls; the kernel adds the load's CRC */
    vxeKernelSetCrcvx(0xFFFFFFFFu);
    vxe_api.k_highScoreGet((const unsigned char *)"X", NULL);
    vxe_api.k_highScoreSetGameRecordToDefaults((const unsigned char *)"X", NULL);
    if (seen_crcvx_get != 0xFFFFFFFFu || seen_crcvx_def != 0xFFFFFFFFu) { printf("FAIL default crcvx %08x/%08x\n", seen_crcvx_get, seen_crcvx_def); fails++; }
    else printf("ok   high-score calls carry the kernel's default CRC (%08x)\n", seen_crcvx_get);
    vxeKernelSetCrcvx(0xA1B2C3D4u);
    vxe_api.k_highScoreGet((const unsigned char *)"X", NULL);
    vxe_api.k_highScoreSetGameRecordToDefaults((const unsigned char *)"X", NULL);
    if (seen_crcvx_get != 0xA1B2C3D4u || seen_crcvx_def != 0xA1B2C3D4u) { printf("FAIL set crcvx %08x/%08x\n", seen_crcvx_get, seen_crcvx_def); fails++; }
    else printf("ok   after vxeKernelSetCrcvx the calls carry %08x\n", seen_crcvx_get);

    printf(fails ? "\n%d FAILED\n" : "\nall passed\n", fails);
    return fails ? 1 : 0;
}
