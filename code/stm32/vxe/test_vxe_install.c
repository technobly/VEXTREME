/*
 * test_vxe_install.c - HOST test for the .vxe loader (vxe/vxe_install.c).
 * Compiles the actual vxe_install.c and
 * vxe_kernel.c (VXE_SPLIT=1) with the internal flash slot simulated as NOR
 * flash (erase -> $FF, programming can only clear bits) and bundles made by
 * tools/make_vxe.py (BIG is a payload padded past 64 KB, so the
 * two-sector path is covered; B and C are real games), read through the
 * tree's FatFs from a FAT32 image.
 *
 * Usage: t <fat32 image> <BIG.vxe> <B.vxe> <C.vxe> <ONLYOTHER.vxe> <ONLYMINE.vxe>
 * (BIG, B and C carry a payload for each flash layout; ONLYOTHER / ONLYMINE carry just one.)
 * Build: see tools/test_vxe_install.sh.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#ifndef VXE_LAYOUT_UF2
#error "compile with -DVXE_LAYOUT_UF2=1 (UF2 layout, sector 5 only) or =0 (no-bootloader layout, sectors 4-5)"
#endif
#if VXE_LAYOUT_UF2
#define HOST_SLOT_SIZE (128 * 1024)
#define BIG_ERASES     1          /* one sector holds the whole slot */
#define OTHER_LAYOUT_ADDR 0x08010000u
#else
#define HOST_SLOT_SIZE (192 * 1024)
#define BIG_ERASES     2          /* a payload over 64 KB needs sectors 4 and 5 */
#define OTHER_LAYOUT_ADDR 0x08020000u
#endif
static unsigned char host_slot[HOST_SLOT_SIZE];
#define VXE_SLOT_PTR ((const uint8_t *)host_slot)

static const void *started;                     /* last payload header vxeKernelStart() accepted */
static int starts;
static void vxe_host_start(const void *h) { started = h; starts++; }

#include "vxe/vxe_kernel.c"
#include "vxe/vxe_install.c"
#include "fatfs/diskio.h"

/* ---- simulated internal flash ---- */
static int erases, words, word_limit = -1, unlocked;
void vxe_host_flash_begin(void) { unlocked = 1; }
void vxe_host_flash_end(void)   { unlocked = 0; }
int vxe_host_flash_erase(int sector)
{
    if (!unlocked) { printf("  !! erase while locked\n"); return 0; }
    erases++;
#if VXE_LAYOUT_UF2
    if (sector == 5) memset(host_slot, 0xFF, sizeof(host_slot));
    else { printf("  !! erase of sector %d - outside the slot (sector 5 only in the UF2 layout)\n", sector); return 0; }
#else
    if (sector == 4) memset(host_slot, 0xFF, 64 * 1024);
    else if (sector == 5) memset(host_slot + 64 * 1024, 0xFF, 128 * 1024);
    else { printf("  !! erase of sector %d - outside the slot\n", sector); return 0; }
#endif
    return 1;
}
int vxe_host_flash_word(uint32_t addr, uint32_t w)
{
    uint32_t off = addr - VXE_SLOT_ADDR, old;
    if (!unlocked || (addr & 3) || off >= sizeof(host_slot)) { printf("  !! bad program at %08x\n", addr); return 0; }
    if (word_limit >= 0 && words >= word_limit) return 0;     /* "power cut": nothing more is written */
    memcpy(&old, host_slot + off, 4);
    old &= w;                                                  /* NOR: bits only go 1 -> 0 */
    memcpy(host_slot + off, &old, 4);
    words++;
    return 1;
}
void ledsMagenta(void) {}
void applyLedSettings(bool initial) { (void)initial; }

/* ---- disk + kernel stubs ---- */
static unsigned char *img; static long img_len;
DSTATUS disk_initialize(BYTE p) { (void)p; return 0; }
DSTATUS disk_status(BYTE p) { (void)p; return 0; }
DRESULT disk_read(BYTE p, BYTE *b, DWORD s, UINT n) { (void)p; memcpy(b, img + (long)s * 512, (size_t)n * 512); return RES_OK; }
DRESULT disk_write(BYTE p, const BYTE *b, DWORD s, UINT n) { (void)p; memcpy(img + (long)s * 512, b, (size_t)n * 512); return RES_OK; }
DRESULT disk_ioctl(BYTE p, BYTE c, void *b)
{
    (void)p;
    if (c == GET_SECTOR_COUNT) { *(DWORD *)b = (DWORD)(img_len / 512); return RES_OK; }
    if (c == GET_SECTOR_SIZE) { *(WORD *)b = 512; return RES_OK; }
    if (c == GET_BLOCK_SIZE) { *(DWORD *)b = 1; return RES_OK; }
    return RES_OK;
}
void flashDoWriteback(void) {}
void doLedOn(int on) { (void)on; }
void ledsClear(void) {}
void ledsUpdate(void) {}
void rainbowStep(uint8_t s) { (void)s; }
uint32_t millis(void) { return 0; }
uint32_t dwt_read_cycle_counter(void) { return 0; }
void xvprintf(const char *f, va_list a) { (void)f; (void)a; }
/* The kernel's xprintf (xprintf.c is not linked). MUST be defined here: macOS libc has an
 * unrelated xprintf(3) with another signature, and linking to it crashes. */
void xprintf(const char *fmt, ...) { va_list a; va_start(a, fmt); printf("     | "); vprintf(fmt, a); va_end(a); }
HighScoreRetVal highScoreGet(const unsigned char *g, const uint32_t crcvx, GameFileRecordV1 *r) { (void)g; (void)crcvx; (void)r; return 0; }
HighScoreRetVal highScoreSetGameRecordToDefaults(const unsigned char *g, const uint32_t crcvx, GameFileRecordV1 *r) { (void)g; (void)crcvx; (void)r; return 0; }
void highScoreSave(unsigned char *s) { (void)s; }
void vxeKernelMenuScoreSet(const unsigned char s[7]) { (void)s; }
void vxe_payload_init(const VxeKernelApi *api, VxePayloadHooks *hooks) { (void)api; (void)hooks; }
char *romData, *cartData; unsigned char parmRam[256]; GameFileRecordV1 *pActiveGameData;

static int fails;
#define CHECK(c, ...) do { if (c) { printf("ok   "); } else { printf("FAIL "); fails++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static unsigned char *slurp(const char *p, long *n)
{
    FILE *f = fopen(p, "rb"); unsigned char *b;
    if (!f) { printf("cannot open %s\n", p); exit(2); }
    fseek(f, 0, SEEK_END); *n = ftell(f); rewind(f);
    b = malloc((size_t)*n); fread(b, 1, (size_t)*n, f); fclose(f);
    return b;
}
static void put(const char *fatpath, const unsigned char *d, long n)
{
    FIL f; UINT w;
    if (f_open(&f, fatpath, FA_CREATE_ALWAYS | FA_WRITE) != FR_OK || f_write(&f, d, (UINT)n, &w) != FR_OK || (long)w != n) {
        printf("cannot write %s\n", fatpath); exit(2);
    }
    f_close(&f);
}
static uint32_t le32(const unsigned char *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }

/* the bundle's header: image[] starts at byte 24, 16 bytes per entry ({load, offset, size, crc}) */
#define IMG_AT         (24 + 16 * VXE_LAYOUT_INDEX)
#define OTHER_IMG_AT   (24 + 16 * (1 - VXE_LAYOUT_INDEX))
static void fix_hdr_crc(unsigned char *b) { uint32_t c = vxeCrc32(0, b, 124); memcpy(b + 124, &c, 4); }

/* the bundle's own parts, for comparison */
typedef struct { unsigned char *b; long n; const unsigned char *m6809, *stm; uint32_t n6809, nstm; } bundle;
static bundle open_bundle(const char *p)
{
    bundle x; x.b = slurp(p, &x.n);
    x.m6809 = x.b + le32(x.b + 12); x.n6809 = le32(x.b + 16);
    x.stm = x.b + le32(x.b + IMG_AT + 4);   x.nstm = le32(x.b + IMG_AT + 8);    /* this kernel's layout */
    return x;
}

static char dest[64 * 1024];
/* A refusal now serves the message cartridge (vxe_msgcart.h) instead of returning 0; LINE(i) reads its text slot i. */
#define REFUSED(r)  ((r) == sizeof(vxe_msgcart))
#define LINE(i)     ((const char *)dest + VXE_MSG_OFS + (i) * VXE_MSG_LEN)
static int line_is(int i, const char *want)         /* the slot holds `want`, then $80 */
{ size_t n = strlen(want); return memcmp(LINE(i), want, n) == 0 && (unsigned char)LINE(i)[n] == 0x80; }
static unsigned run(const char *path) { erases = words = 0; starts = 0; started = 0; return vxeKernelLoadBundle(path, dest, sizeof(dest)); }

int main(int argc, char **argv)
{
    FATFS fs; bundle big, bb, cc; unsigned r; unsigned char saved[sizeof(host_slot)];
    if (argc < 7) { printf("usage: t <image> <BIG.vxe> <B.vxe> <C.vxe> <ONLYOTHER.vxe> <ONLYMINE.vxe>\n"); return 2; }
    img = slurp(argv[1], &img_len);
    if (f_mount(&fs, "", 1) != FR_OK) { printf("mount failed\n"); return 2; }
    big = open_bundle(argv[2]); bb = open_bundle(argv[3]); cc = open_bundle(argv[4]);
    f_mkdir("/roms"); f_mkdir("/roms/VXE");
    put("/roms/VXE/BIG.vxe", big.b, big.n); put("/roms/VXE/B.vxe", bb.b, bb.n); put("/roms/VXE/C.vxe", cc.b, cc.n);
    memset(host_slot, 0xFF, sizeof(host_slot));            /* an erased slot */

    printf("== 1. the big (two-sector) payload into an empty slot\n");
    r = run("/roms/VXE/BIG.vxe");
    CHECK(r == big.n6809, "returns the 6809 size (%u)", r);
    CHECK(memcmp(dest, big.m6809, big.n6809) == 0, "6809 image copied into the cart buffer");
    CHECK(memcmp(host_slot, big.stm, big.nstm) == 0, "slot holds the bundle's payload byte for byte");
    CHECK(erases == BIG_ERASES, "erased the slot's sector(s) (payload %u B): %d erases, expected %d", big.nstm, erases, BIG_ERASES);
    CHECK(starts == 1 && started == host_slot, "payload started once, from the slot");

    printf("== 2. the big payload again (already installed)\n");
    r = run("/roms/VXE/BIG.vxe");
    CHECK(r == big.n6809 && erases == 0 && words == 0, "no erase, no programming (erases %d, words %d)", erases, words);
    CHECK(starts == 1, "payload restarted for the new launch");

    printf("== 3. switch to B, then C\n");
    r = run("/roms/VXE/B.vxe");
    CHECK(r == bb.n6809 && memcmp(host_slot, bb.stm, bb.nstm) == 0, "B installed (%u B payload)", bb.nstm);
    CHECK(memcmp(dest, bb.m6809, bb.n6809) == 0, "B's 6809 image served");
    r = run("/roms/VXE/C.vxe");
    CHECK(r == cc.n6809 && memcmp(host_slot, cc.stm, cc.nstm) == 0, "C installed (%u B payload)", cc.nstm);
    CHECK(erases == 1, "C fits in the first sector: only one erase (%d)", erases);

    printf("== 4. a damaged bundle must not touch the slot\n");
    memcpy(saved, host_slot, sizeof(saved));
    {   bundle bad = open_bundle(argv[2]);
        bad.b[le32(bad.b + IMG_AT + 4) + 5000] ^= 0x40;    /* one bit inside the big payload */
        put("/roms/VXE/BAD.vxe", bad.b, bad.n);
        r = run("/roms/VXE/BAD.vxe");
        CHECK(REFUSED(r) && erases == 0 && words == 0 && memcmp(saved, host_slot, sizeof(saved)) == 0,
              "refused; slot unchanged (erases %d, words %d)", erases, words);
        CHECK(starts == 0, "nothing started");
        bad.b[le32(bad.b + IMG_AT + 4) + 5000] ^= 0x40;
        bad.b[82] ^= 1;                                     /* the title: covered by the header CRC */
        put("/roms/VXE/BADH.vxe", bad.b, bad.n);
        r = run("/roms/VXE/BADH.vxe");
        CHECK(REFUSED(r) && erases == 0 && memcmp(saved, host_slot, sizeof(saved)) == 0, "bad header CRC refused");
    }

    printf("== 5. a payload built for a newer kernel ABI is refused\n");
    {   bundle nw = open_bundle(argv[2]); uint32_t c;
        nw.b[8] = VXE_API_VERSION + 1;                      /* bundle api_version */
        c = vxeCrc32(0, nw.b, 124); memcpy(nw.b + 124, &c, 4);
        put("/roms/VXE/NEW.vxe", nw.b, nw.n);
        r = run("/roms/VXE/NEW.vxe");
        CHECK(REFUSED(r) && erases == 0 && memcmp(saved, host_slot, sizeof(saved)) == 0, "refused before any flash operation");
    }


    printf("== 5b. a payload built for an older, incompatible ABI is refused\n");
    {   bundle od = open_bundle(argv[2]); uint32_t c;
        od.b[8] = VXE_API_MIN_VERSION - 1;                  /* bundle api_version */
        c = vxeCrc32(0, od.b, 124); memcpy(od.b + 124, &c, 4);
        put("/roms/VXE/OLD.vxe", od.b, od.n);
        r = run("/roms/VXE/OLD.vxe");
        CHECK(REFUSED(r) && erases == 0 && memcmp(saved, host_slot, sizeof(saved)) == 0, "refused before any flash operation");
    }

    printf("== 5c. a bundle whose image for THIS layout was linked for the other slot is refused\n");
    {   bundle ol = open_bundle(argv[2]); uint32_t addr = OTHER_LAYOUT_ADDR;
        memcpy(ol.b + IMG_AT, &addr, 4);                    /* this layout's image[] load address */
        fix_hdr_crc(ol.b);
        put("/roms/VXE/OTHER.vxe", ol.b, ol.n);
        r = run("/roms/VXE/OTHER.vxe");
        CHECK(REFUSED(r) && erases == 0 && memcmp(saved, host_slot, sizeof(saved)) == 0, "refused before any flash operation");
    }

    printf("== 5d. a bundle carrying only the OTHER layout's payload is refused, slot untouched\n");
    {   bundle oo = open_bundle(argv[5]);
        CHECK((oo.b[10] | oo.b[11] << 8) == (VXE_LAYOUT_INDEX ? 0x1 : 0x2), "(the test bundle's layouts flags are %u)", oo.b[10] | oo.b[11] << 8);
        put("/roms/VXE/ONLYOTHER.vxe", oo.b, oo.n);
        r = run("/roms/VXE/ONLYOTHER.vxe");
        CHECK(REFUSED(r) && erases == 0 && words == 0 && memcmp(saved, host_slot, sizeof(saved)) == 0, "refused; slot unchanged (erases %d, words %d)", erases, words);
        CHECK(starts == 0, "nothing started");
        CHECK(line_is(0, "ONLYOTHER") && line_is(1, "ONLY LOADS ON THE"), "message names the game and says what it needs");
        CHECK(line_is(2, VXE_LAYOUT_INDEX ? "UF2 KERNEL" : "NON-UF2 KERNEL") && line_is(3, VXE_LAYOUT_INDEX ? "(128 KB PAYLOAD)" : "(192 KB PAYLOAD)"),
              "message names the kernel the game needs");
        CHECK(line_is(4, VXE_LAYOUT_INDEX ? "THIS IS A NON-UF2" : "THIS IS THE UF2") && line_is(7, "THE MENU"), "message says which kernel this is, and how to leave");
        CHECK(memcmp(dest, vxe_msgcart, VXE_MSG_OFS) == 0 && memcmp(dest + VXE_MSG_OFS + VXE_MSG_LEN * VXE_MSG_LINES, vxe_msgcart + VXE_MSG_OFS + VXE_MSG_LEN * VXE_MSG_LINES, sizeof(vxe_msgcart) - VXE_MSG_OFS - VXE_MSG_LEN * VXE_MSG_LINES) == 0,
              "everything outside the text slots is the message cartridge as built");
        {   int i; int ok = 1;
            for (i = 0; i < VXE_MSG_LINES; i++) { const unsigned char *q = (const unsigned char *)LINE(i); int k = 0; while (k < VXE_MSG_LEN && q[k] != 0x80) k++; if (k >= VXE_MSG_LEN) ok = 0; }
            CHECK(ok, "every text slot ends in $80 (the BIOS stops there)");
        }
    }

    printf("== 5e. a bundle carrying only THIS layout's payload installs\n");
    {   bundle om = open_bundle(argv[6]);
        put("/roms/VXE/ONLYMINE.vxe", om.b, om.n);
        r = run("/roms/VXE/ONLYMINE.vxe");
        CHECK(r == om.n6809 && memcmp(host_slot, om.stm, om.nstm) == 0 && starts == 1, "installed and started");
    }

    printf("== 5f. layouts flags that disagree with the image table are refused\n");
    memcpy(saved, host_slot, sizeof(saved));
    {   unsigned t; static const unsigned bad_flags[] = { 0u, 3u, 4u, 0x8000u };
        for (t = 0; t < sizeof(bad_flags) / sizeof(bad_flags[0]); t++) {
            bundle fl = open_bundle(argv[2]); char nm[40]; uint16_t v = (uint16_t)bad_flags[t];
            /* argv[2] is a dual bundle (flags 3). Case "3" leaves the flags and empties one table entry instead. */
            if (bad_flags[t] == 3u) { memset(fl.b + OTHER_IMG_AT, 0, 16); }    /* flags say both, table says one */
            else memcpy(fl.b + 10, &v, 2);
            fix_hdr_crc(fl.b);
            sprintf(nm, "/roms/VXE/FLAG%u.vxe", t); put(nm, fl.b, fl.n);
            r = run(nm);
            CHECK(REFUSED(r) && erases == 0 && words == 0 && memcmp(saved, host_slot, sizeof(saved)) == 0,
                  "case %u refused before any flash operation", t);
        }
    }

    printf("== 5g. a dual-image bundle installs the image for THIS layout (and not the other)\n");
    {   bundle d = open_bundle(argv[2]); const unsigned char *oth = d.b + le32(d.b + OTHER_IMG_AT + 4); uint32_t on = le32(d.b + OTHER_IMG_AT + 8);
        run("/roms/VXE/BIG.vxe");
        CHECK(memcmp(host_slot, d.stm, d.nstm) == 0, "slot holds this layout's image (%u B)", d.nstm);
        CHECK(on != 0 && le32(d.b + OTHER_IMG_AT) != le32(d.b + IMG_AT), "the other image exists (%u B) at a different slot address", on);
        (void)oth;
    }

    printf("== 6. power cut in the middle of programming the big payload\n");
    run("/roms/VXE/C.vxe");                                 /* a different payload in the slot first, so BIG really installs */
    word_limit = 20000;                                     /* well inside the body */
    r = run("/roms/VXE/BIG.vxe");
    word_limit = -1;
    CHECK(REFUSED(r), "install reports failure");
    CHECK(!vxeHeaderOk((const VxePayloadHeader *)host_slot), "slot header NOT valid (magic %08x) - the kernel will refuse it at boot",
          le32(host_slot));
    CHECK(starts == 0, "nothing started");
    r = run("/roms/VXE/BIG.vxe");                           /* power back, pick the big payload again */
    CHECK(r == big.n6809 && memcmp(host_slot, big.stm, big.nstm) == 0, "next launch reinstalls it cleanly");

    printf("== 7. header-last ordering (a cut after the whole body, before the header)\n");
    run("/roms/VXE/C.vxe");                              /* something else in the slot first */
    word_limit = 0;                                         /* count the body's words first */
    {   int body; erases = words = 0; word_limit = -1;
        /* body words that are not $FFFFFFFF in the big payload, after its 28-byte header */
        body = 0;
        for (uint32_t i = 28; i < big.nstm; i += 4) if (le32(big.stm + i) != 0xFFFFFFFFu) body++;
        word_limit = body;                                  /* everything but the header */
        r = run("/roms/VXE/BIG.vxe");
        word_limit = -1;
        CHECK(REFUSED(r) && !vxeHeaderOk((const VxePayloadHeader *)host_slot),
              "body fully written (%d words) but no valid header -> refused", body);
        CHECK(memcmp(host_slot + 28, big.stm + 28, big.nstm - 28) == 0, "(the body itself was complete)");
    }

    printf(fails ? "\n%d FAILED\n" : "\nall passed\n", fails);
    return fails ? 1 : 0;
}
