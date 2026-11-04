/*
 * vxe_api.h - the boundary between the VEXTREME firmware (the KERNEL) and a
 * co-op game's STM32 code (the PAYLOAD)..
 *
 * A payload is the game plus the VXT toolkit (vxt/, gamelib/) and its own
 * libc/libm. Everything it needs from the firmware goes through ONE table,
 * VxeKernelApi, handed to vxe_payload_init() at start; everything the firmware
 * needs from the payload comes back through VxePayloadHooks. Payload sources
 * are NOT edited for this: vxe_redirect.h (force-included into every payload
 * object) renames each kernel symbol they use to a vxe_* stand-in, and
 * vxe_shim.c forwards those through the table. `make` checks that the payload
 * objects link on their own (payload-check), so a symbol missing from this
 * table fails the build instead of linking silently against the kernel.
 *
 * The payload is linked separately from the kernel and loaded from a .vxe. The table is append-only: add
 * new entries at the end and bump VXE_API_VERSION; do not reorder.
 *
 * Every member is prefixed k_ so no member name collides with a symbol that
 * vxe_redirect.h renames.
 */
#ifndef VXE_API_H
#define VXE_API_H

#include <stdint.h>
#include <stdbool.h>      /* the 1.0 highscore.h uses bool without including it */
#include <stdarg.h>
#include "../fatfs/ff.h"
#include "../highscore.h"

#define VXE_API_VERSION     3
/* The oldest payload ABI this kernel still runs: 3 (the VEXTREME 1.0 firmware base).
 *  - v2 was a short-lived development build whose high-score calls took an extra crcvx argument. v3
 *    returns them to the two-argument form, so a v2 payload and a v3 kernel disagree about the
 *    arguments and must not mix.
 *  - The firmware's high-score record also changed (GameFileRecordV1: different field order and sizes,
 *    keyed by name + ROM CRC) and pActiveGameData points at it, so a v1 payload that read the record's
 *    fields would read the wrong bytes.
 * Older payloads are refused; rebuild them against this kernel. The payload-facing high-score CALLS keep
 * their two-argument form: the kernel supplies the CRC itself (vxeKernelSetCrcvx), so a game never
 * depends on how the firmware keys its records. */
#define VXE_API_MIN_VERSION 3

/* The RPC block doHandleEvent() forwards to the payload (the
 * same values as vxt_rpc.h's VXT_RPC_TOOLKIT_FIRST/LAST, restated here so the
 * kernel does not include the toolkit). */
#define VXE_RPC_FIRST 64
#define VXE_RPC_LAST  79

typedef struct {
    uint16_t k_version;                 /* VXE_API_VERSION of the kernel   */
    uint16_t k_size;                    /* sizeof(VxeKernelApi) there      */

    /* Shared memory. Pointers TO the kernel's own variables, because the
     * kernel repoints romData (menu vs cart) after the payload has started.
     * Types match main.c / highscore.c exactly (docs/vxe/CONVENTIONS.md convention 3).   */
    char           **k_romData_pp;       /* main.c: char* romData            */
    char           **k_cartData_pp;      /* main.c: char* cartData (64 KB)   */
    unsigned char  (*k_parmRam_p)[256];  /* main.c: unsigned char parmRam[256] */
    GameFileRecordV1 **k_pActiveGameData_pp; /* highscore.c: pActiveGameData   */

    /* FatFs. A game's own root-level files ("/game.dat") are opened in the
     * folder the game was launched from; console calibration files stay at
     * the root (vxe_kernel.c, vxeMapPath()).                                */
    FRESULT (*k_f_open)(FIL *fp, const TCHAR *path, BYTE mode);
    FRESULT (*k_f_close)(FIL *fp);
    TCHAR  *(*k_f_gets)(TCHAR *buff, int len, FIL *fp);
    FRESULT (*k_f_write)(FIL *fp, const void *buff, UINT btw, UINT *bw);
    FRESULT (*k_f_sync)(FIL *fp);
    FRESULT (*k_f_rename)(const TCHAR *path_old, const TCHAR *path_new);
    FRESULT (*k_f_unlink)(const TCHAR *path);

    /* SPI flash write-back (flash.c). One-shot paths only (convention 19). */
    void (*k_flashDoWriteback)(void);

    /* Cartridge LEDs (led.c, main.c). */
    void (*k_doLedOn)(int on);
    void (*k_ledsClear)(void);
    void (*k_ledsUpdate)(void);
    void (*k_rainbowStep)(uint8_t step);

    /* Time. */
    uint32_t (*k_millis)(void);                  /* delay.c                  */
    uint32_t (*k_dwt_read_cycle_counter)(void);  /* libopencm3               */

    /* Debug output (xprintf.c). */
    void (*k_xvprintf)(const char *fmt, va_list arp);

    /* The multicart's SD high-score store (highscore.c). */
    HighScoreRetVal (*k_highScoreGet)(const unsigned char *pGame, GameFileRecordV1 *pGameRecord);
    HighScoreRetVal (*k_highScoreSetGameRecordToDefaults)(const unsigned char *pGame, GameFileRecordV1 *pGameRecord);
    void            (*k_highScoreSave)(unsigned char *pScore);

    /* The menu's "native register" score (menuData[MENU_SYS_ADDR + 0x10],
     * 7 bytes: 6 ASCII digits + $80) - what the menu shows after exiting. */
    void (*k_menuScoreSet)(const unsigned char score7[7]);
} VxeKernelApi;

typedef struct {
    /* RPC 64-79 (doHandleEvent()'s toolkit block). NULL = no payload. */
    void (*rpc_dispatch)(int id);
} VxePayloadHooks;

/* Exactly one per payload (vxe/vxe_payload.c). */
void vxe_payload_init(const VxeKernelApi *api, VxePayloadHooks *hooks);

/* ---- the payload as its own image (VXE_SPLIT=1) ---------------------------
 * Two flash layouts, chosen at build time (Makefile VXE_LAYOUT=uf2|dfu, passed as -DVXE_LAYOUT_UF2=1|0):
 *
 *   uf2  The VEXTREME 1.0 UF2 bootloader owns sector 0. The kernel runs from 0x08004000 (sectors 1-3 and,
 *        if it is bigger than 48 KB, the start of sector 4); the payload owns sector 5 only:
 *        VXE_SLOT_ADDR 0x08020000, 128 KB. Flash can only be erased a whole sector at a time, so the
 *        unused rest of sector 4 stays unused.
 *   dfu  No bootloader (flashed with dfu-util). The kernel owns sectors 0-3 (64 KB, so it must be built
 *        small, -Os) and the payload owns sectors 4-5: VXE_SLOT_ADDR 0x08010000, 192 KB.
 *
 * In both, the payload has a fixed RAM window at the top of RAM (VXE_PAYLOAD_RAM). The linker scripts
 * (stm32f411rct6-kernel-<layout>.ld, vxe/vxe_payload_<layout>.ld) and tools/make_vxe.py (--layout) must
 * agree with these numbers. A bundle records the load address of each image it carries and the kernel
 * installs only the image linked for its own layout, so a payload can never be installed where it was
 * not linked to run. */
#include "../vxe_config.h"
#if !VXE_ENABLED
#error "the VXE sources are only part of a VXE=1 build (the Makefile passes -DVXE_ENABLED=1); a VXE=0 build is the regular firmware"
#endif
#ifndef VXE_LAYOUT_UF2
#error "define VXE_LAYOUT_UF2 as 1 (UF2 bootloader, 128 KB slot) or 0 (no bootloader, 192 KB slot) - the Makefile does"
#endif
#if VXE_LAYOUT_UF2
#define VXE_SLOT_ADDR       0x08020000u
#define VXE_SLOT_SIZE       (128u * 1024u)
#define VXE_LAYOUT_INDEX    0               /* this kernel's entry in VxeBundleHeader.image[] */
#else
#define VXE_SLOT_ADDR       0x08010000u
#define VXE_SLOT_SIZE       (192u * 1024u)
#define VXE_LAYOUT_INDEX    1
#endif
#define VXE_PAYLOAD_RAM     0x2001C000u
#define VXE_PAYLOAD_RAM_SZ  (16u * 1024u)
#define VXE_PAYLOAD_MAGIC   0x50455856u     /* "VXEP" read as a little-endian word */

/* Where the kernel READS the slot. The real address on the cart; a host test
 * points it at a simulated flash array (vxe/test_vxe_install.c). */
#ifndef VXE_SLOT_PTR
#define VXE_SLOT_PTR        ((const uint8_t *)VXE_SLOT_ADDR)
#endif

/* The first bytes of the slot. The kernel calls `start` only if the magic,
 * the ABI version and the pointers check out; an erased slot (all $FF) or a
 * foreign image is refused and the multicart simply runs without a payload.
 * `crc32` is 0 until the packer fills it in (0 = not checked). */
typedef struct {
    uint32_t magic;                     /* VXE_PAYLOAD_MAGIC                      */
    uint16_t abi_version;               /* VXE_API_VERSION the payload was built against */
    uint16_t header_size;               /* sizeof(VxePayloadHeader)               */
    uint32_t image_end;                 /* address of the first flash byte after the image */
    uint32_t crc32;                     /* IDENTITY: CRC32 of the image after this header,
                                         * stamped by tools/make_vxe.py; 0 = not set        */
    uint32_t ram_start;                 /* the payload's RAM use: [ram_start,     */
    uint32_t ram_end;                   /*   ram_end) - must lie inside the window */
    uint32_t start;                     /* Thumb address of
                                         * void start(const VxeKernelApi *, VxePayloadHooks *) */
} VxePayloadHeader;
/* The address fields are uint32_t, not C pointers, so this is a fixed 28-byte
 * on-disk format on any host (a 64-bit host test read it wrongly with pointers). */

/* ---- the .vxe bundle (tools/make_vxe.py writes it) ------------------------
 * One file per co-op game: the 6809 cartridge image and the STM32 payload(s).
 * 128-byte header, little-endian, then the parts. The 6809 image is the same
 * for every flash layout. The STM32 payload is linked to run at one fixed
 * address, so a bundle carries one image per flash layout it was built for
 * (up to two: [0] uf2, [1] dfu). `layouts` says which are present; the kernel
 * installs the one for its own layout and refuses a bundle that has none.
 * The packer stamps each payload's IDENTITY (CRC32 of the payload after its
 * header) into the payload header's crc32; the kernel skips installing a
 * payload whose identity matches the one already in the slot. CRC32 = zlib's.
 * Format version 2 (version 1 held a single image and is no longer read). */
#define VXE_BUNDLE_MAGIC        "VXE1"
#define VXE_BUNDLE_VERSION      2
#define VXE_BUNDLE_HEADER_SIZE  128
#define VXE_LAYOUT_BIT_UF2      0x1u        /* VxeBundleHeader.layouts: an image for the uf2 layout is present */
#define VXE_LAYOUT_BIT_DFU      0x2u        /*                          an image for the dfu layout is present */
typedef struct {
    uint32_t load_addr;                 /* where the image is linked to run: 0 if absent, else that layout's slot */
    uint32_t offset, size, crc;         /* the STM32 image in the file; size 0 = absent */
} __attribute__((packed)) VxeBundleImage;
typedef struct {
    char     magic[4];                  /*   0 "VXE1"                         */
    uint16_t format_version;            /*   4 VXE_BUNDLE_VERSION             */
    uint16_t header_size;               /*   6 VXE_BUNDLE_HEADER_SIZE         */
    uint16_t api_version;               /*   8 the payload's ABI              */
    uint16_t layouts;                   /*  10 VXE_LAYOUT_BIT_* of the images present   */
    uint32_t m6809_offset, m6809_size, m6809_crc;   /* 12, 16, 20              */
    VxeBundleImage image[2];            /*  24 [0] uf2 layout, [1] dfu layout            */
    char     m6809_name[24];            /*  56 e.g. "GAME6809.bin"            */
    char     title[24];                 /*  80 informational                  */
    uint32_t reserved[5];               /* 104                                */
    uint32_t header_crc;                /* 124 CRC32 of bytes 0..123          */
} __attribute__((packed)) VxeBundleHeader;

/* Kernel side, vxe/vxe_install.c (split builds). Loads the bundle at `path`:
 * installs its payload into the slot if it is not already there, copies the
 * 6809 image into dest (at most destMax bytes), restarts the payload. Returns
 * the 6809 image size, or 0 on any failure (nothing is started then). */
unsigned int vxeKernelLoadBundle(const char *path, char *dest, unsigned int destMax);
int vxeHeaderOk(const VxePayloadHeader *h);       /* vxe_kernel.c */
uint32_t vxeCrc32(uint32_t crc, const void *data, uint32_t len);   /* zlib-compatible */

/* Payload side (vxe_shim.c): binds the stand-ins to the table. */
void vxe_shim_bind(const VxeKernelApi *api);

/* Kernel side (vxe_kernel.c). */
void vxeKernelSetGamePath(const char *romPath);   /* every ROM load      */
void vxeKernelSetCrcvx(uint32_t crcvx);           /* every ROM load: the high-score key's CRC half */
void vxeKernelStart(void);                        /* once, at boot       */
void vxeKernelDispatch(int id);                   /* RPC 64-79           */
void vxeKernelStop(void);                         /* before the slot is rewritten */
void vxeKernelMenuScoreSet(const unsigned char score7[7]);   /* main.c   */

#endif /* VXE_API_H */
