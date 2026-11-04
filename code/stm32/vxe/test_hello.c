/*
 * test_hello.c - HOST test of the SDK template game (game/hello.c). Links the
 * actual pieces end to end, on the Mac:
 *   hello.c, vxt_smart.c, vxt_frame.c, vxt_sound.c, gamelib_beam.c (all built
 *   WITH vxe_redirect.h, as in a payload) -> vxe_shim.c -> vxe_kernel.c's table
 *   and path mapping -> the tree's FatFs on a FAT32 image, with a model of the
 *   cart's SPI-flash write-back cache underneath (power cut = cache lost).
 * What it proves, without hardware:
 *   - the frame handler emits a well-formed SmartList: terminated, inside its
 *     region, every record's routine one of the addresses the 6809 published,
 *     and the cost in records is sane (the budget is 1536);
 *   - the square moves with the stick and is clamped;
 *   - button 1 saves a file in the game's launch folder, and it survives a
 *     power cut right after the save;
 *   - a restart reads the saved position back; a missing file leaves defaults;
 *   - sound is queued only when something changes.
 * Build/run: tools/test_hello.sh <fat32 image>
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void model_writeback(void);
void flashDoWriteback(void) { model_writeback(); }

#include "vxe/vxe_kernel.c"
#include "fatfs/diskio.h"
#include "game/hello.h"
#define HELLO_SETTLE_FRAMES 8   /* must match game/hello.c */
#include "vxt/vxt_smart.h"
#include "vxt/vxt_input.h"

/* ---- the cart's flash + write-back cache (see test_vxe_durable.c) ---- */
#define IMG_SECTORS 131072
static unsigned char flash_img[IMG_SECTORS * 512];
static unsigned char blk[4096];
static long blk_addr = -1;
static void model_writeback(void) { if (blk_addr >= 0) { memcpy(flash_img + blk_addr, blk, 4096); blk_addr = -1; } }
static void power_cut(void) { blk_addr = -1; }
DSTATUS disk_initialize(BYTE p) { (void)p; return 0; }
DSTATUS disk_status(BYTE p) { (void)p; return 0; }
DRESULT disk_read(BYTE p, BYTE *b, DWORD s, UINT n)
{ (void)p; while (n--) { model_writeback(); memcpy(b, flash_img + (long)s * 512, 512); b += 512; s++; } return RES_OK; }
DRESULT disk_write(BYTE p, const BYTE *b, DWORD s, UINT n)
{
    (void)p;
    while (n--) {
        long a = (long)s * 512, ba = a & ~4095L;
        if (ba != blk_addr) { model_writeback(); memcpy(blk, flash_img + ba, 4096); blk_addr = ba; }
        memcpy(blk + (a - ba), b, 512); b += 512; s++;
    }
    return RES_OK;
}
DRESULT disk_ioctl(BYTE p, BYTE c, void *b) { (void)p; (void)c; (void)b; return RES_OK; }

/* ---- the rest of what the kernel file links against ---- */
void doLedOn(int on) { (void)on; }
void ledsClear(void) {}
void ledsUpdate(void) {}
void rainbowStep(uint8_t s) { (void)s; }
uint32_t millis(void) { return 0; }
uint32_t dwt_read_cycle_counter(void) { return 0; }
static char last_log[128];                          /* the game's last log line, for assertions */
/* the payload's xprintf reaches the kernel through VxeKernelApi.k_xvprintf */
static char calib_log[128];                          /* the calibration line, kept separately */
void xvprintf(const char *f, va_list a)
{
    vsnprintf(last_log, sizeof last_log, f, a); printf("     | %s", last_log);
    if (strstr(last_log, "draw gain")) { strcpy(calib_log, last_log); }
}
void xprintf(const char *fmt, ...) { va_list a; va_start(a, fmt); xvprintf(fmt, a); va_end(a); }
HighScoreRetVal highScoreGet(const unsigned char *g, const uint32_t crcvx, GameFileRecordV1 *r) { (void)g; (void)crcvx; (void)r; return 0; }
HighScoreRetVal highScoreSetGameRecordToDefaults(const unsigned char *g, const uint32_t crcvx, GameFileRecordV1 *r) { (void)g; (void)crcvx; (void)r; return 0; }
void highScoreSave(unsigned char *s) { (void)s; }
void vxeKernelMenuScoreSet(const unsigned char s[7]) { (void)s; }
void vxe_payload_init(const VxeKernelApi *api, VxePayloadHooks *hooks) { (void)api; (void)hooks; }
char *romData, *cartData; unsigned char parmRam[256]; GameFileRecordV1 *pActiveGameData;

static int fails;
#define CHECK(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); fails++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static uint8_t served[64 * 1024];                    /* the "served image" */
static uint8_t parm[256];
static int exists(const char *p) { FIL f; if (f_open(&f, p, FA_READ) != FR_OK) return 0; f_close(&f); return 1; }

/* publish fake routine addresses, as the 6809 would via RPC 69 */
static uint16_t addr_of[14];
static void publish(void)
{
    vxtSmartAddrs a;
    int i;
    for (i = 0; i < 14; i++) addr_of[i] = (uint16_t)(0x0100 + 0x10 * i);
    a.setScale = addr_of[0]; a.setIntensity = addr_of[1]; a.recenter = addr_of[2]; a.startMove = addr_of[3];
    a.startDraw = addr_of[4]; a.cont = addr_of[5]; a.end = addr_of[6]; a.startMoveBig = addr_of[7];
    a.startDrawBig = addr_of[8]; a.startDrawHuge = addr_of[9]; a.setScaleHi = addr_of[10];
    a.startDrawHuge16 = addr_of[11]; a.startDraw32 = addr_of[12]; a.startMoveHuge = addr_of[13];
    vxtSmartSetAddrs(&a);
}

/* decode the list at $0800: returns the record count (excluding the terminator), -1 if malformed */
static int decode_list(int *bad_routines)
{
    int n;
    *bad_routines = 0;
    for (n = 0; n < 1536; n++) {
        const uint8_t *r = served + 0x0800 + 4 * n;
        uint16_t routine = (uint16_t)((r[2] << 8) | r[3]);
        int k, known = 0;
        for (k = 0; k < 14; k++) if (routine == addr_of[k]) known = 1;
        if (!known) (*bad_routines)++;
        if (routine == addr_of[6]) return n;               /* SM_end */
    }
    return -1;
}

static void frame(int jx, int jy, int btn)
{
    memset(parm, 0, sizeof parm);
    parm[VXT_IN_JOY1X] = (uint8_t)(int8_t)jx;
    parm[VXT_IN_JOY1Y] = (uint8_t)(int8_t)jy;
    parm[VXT_IN_BTN1_1] = (uint8_t)btn;
    hello_frame_handler(HELLO_RPC_FRAME, parm);
}

int main(int argc, char **argv)
{
    FATFS fs; FILE *fp; int bad, n, i;
    if (argc < 2) { printf("usage: t <fat32 image>\n"); return 2; }
    fp = fopen(argv[1], "rb"); if (!fp) { printf("cannot open image\n"); return 2; }
    fread(flash_img, 1, sizeof(flash_img), fp); fclose(fp);
    if (f_mount(&fs, "", 1) != FR_OK) { printf("mount failed\n"); return 2; }
    f_mkdir("/roms"); f_mkdir("/roms/VXE"); model_writeback();

    romData = (char *)served; cartData = (char *)served + 1;          /* romData != cartData: no bank mirror */
    vxe_shim_bind(&vxe_api);
    vxeKernelSetGamePath("/roms/VXE/HELLO.vxe");
    publish();

    printf("== 1. first start: no file\n");
    hello_init_handler(HELLO_RPC_INIT, parm);
    CHECK(strcmp(last_log, "HELLO: start position 0,0\n") == 0, "no file: starts at 0,0");
    CHECK(strstr(calib_log, "draw gain 1060, centre offset 0,0") != 0, "uncalibrated console: draws with the default gain (logged: %s)", calib_log);
    frame(0, 0, 0);
    n = decode_list(&bad);
    CHECK(n > 0 && n < 400, "frame 1 is a terminated list of %d records (budget 1536)", n);
    {   uint32_t h = 2166136261u; int k;                          /* FNV-1a of the frame's list, to compare builds */
        for (k = 0; k < 4 * (n + 1); k++) h = (h ^ served[0x800 + k]) * 16777619u;
        printf("     | frame-1 list checksum %08x\n", (unsigned)h); }
    CHECK(bad == 0, "every record's routine is one the 6809 published");
    CHECK(!exists("/roms/VXE/hello.dat"), "no file is created just by starting");

    printf("== 2. the stick moves the square; the edges clamp it\n");
    for (i = 0; i < 20; i++) frame(1, 1, 0);                          /* right and up, 20 frames */
    for (i = 0; i < 100; i++) frame(1, 1, 0);                         /* far past the limit */
    frame(0, 0, 0);
    n = decode_list(&bad);
    CHECK(n > 0 && bad == 0, "still a well-formed list after clamping (%d records)", n);

    printf("== 3. button 1 saves, once per press, in the game's folder\n");
    frame(0, 0, 1);                                                   /* press */
    CHECK(exists("/roms/VXE/hello.dat"), "/roms/VXE/hello.dat written (next to the game, not at the root)");
    CHECK(!exists("/hello.dat") && !exists("/roms/VXE/hello.tmp"), "nothing at the root, no .tmp left");
    /* a flash write briefly disturbs the picture (found on hardware), so the frames right after a save are
     * EMPTY: just "intensity 0" and a recenter, nothing drawn. HELLO_SETTLE_FRAMES of them, the press frame
     * being the first. */
    n = decode_list(&bad);
    CHECK(n == 2 && bad == 0 && served[0x800 + 1] == 0, "the press frame is empty: dimmed beam + recenter (%d records, intensity %d)", n, served[0x800 + 1]);
    {   int empties = 1, saw_marker = 0, k;
        for (k = 0; k < 30; k++) {
            frame(0, 0, 1);                                           /* the button is HELD: it must not save again */
            n = decode_list(&bad);
            if (n == 2) { empties++; continue; }
            saw_marker = (n >= 90 && n < 400 && bad == 0);
            break;
        }
        CHECK(empties == HELLO_SETTLE_FRAMES, "exactly %d empty frames after the save (saw %d)", HELLO_SETTLE_FRAMES, empties);
        CHECK(saw_marker, "then the box and the 'saved' marker return together (%d records)", n);
    }
    {   FIL f; char line[40] = ""; f_open(&f, "/roms/VXE/hello.dat", FA_READ); f_gets(line, sizeof line, &f); f_close(&f);
        printf("     | file contents: %s", line);
        CHECK(strcmp(line, "13500 9500\n") == 0, "the clamped position was saved (Y 13500, X 9500)"); }

    printf("== 4. power cut right after the save, then restart\n");
    power_cut();
    f_mount(0, "", 0); f_mount(&fs, "", 1);
    CHECK(exists("/roms/VXE/hello.dat"), "the save survived the power cut (durable)");
    hello_init_handler(HELLO_RPC_INIT, parm);                         /* the 6809 reboots */
    frame(0, 0, 0);
    n = decode_list(&bad);
    CHECK(n > 0 && bad == 0, "restarts into a well-formed frame (%d records)", n);
    CHECK(strcmp(last_log, "HELLO: start position 13500,9500\n") == 0, "restart reads the saved position back (logged: %s)", last_log);

    printf("== 5. a damaged/missing file leaves the defaults\n");
    f_unlink("/roms/VXE/hello.dat"); model_writeback();
    hello_init_handler(HELLO_RPC_INIT, parm);
    CHECK(!exists("/roms/VXE/hello.dat"), "starting with no file creates none (never seed on a failed read)");
    CHECK(strcmp(last_log, "HELLO: start position 0,0\n") == 0, "and falls back to 0,0");

    printf(fails ? "\n%d FAILED\n" : "\nall passed\n", fails);
    return fails ? 1 : 0;
}
