/*
 * vxe_kernel.c - KERNEL side of the VXE boundary (vxe_api.h): builds the
 * VxeKernelApi table, starts the payload, routes RPC 64-79 to it, and maps a
 * game's own files into the folder the game was launched from.
 *
 * Game files live beside the game: a game launched from
 * /roms/VXE/ that opens "/game.dat" gets /roms/VXE/game.dat. Only a
 * root-level path is mapped ("/name", no further '/'), and only while the game
 * was launched from a folder other than the root, so /cart.bin developer mode
 * keeps using the root as before. The CONSOLE CALIBRATION files are exempt and
 * stay at the root, shared by every game: calmeas.csv (the cal rig's
 * measurement of this console) and cdfmeas.csv (the per-console correction
 * file a game's calibration screen writes and any game may read; the name is
 * historical), plus their
 * .tmp save files - moving them into one game's folder would leave the other
 * games uncalibrated.
 */
#include <string.h>
#include <stdbool.h>              /* led.h uses `bool` without including this itself */
#include <libopencm3/cm3/dwt.h>
#include "vxe_api.h"
#include "../flash.h"
#include "../led.h"
#include "../delay.h"
#include "../xprintf.h"

/* The kernel's own definitions (main.c, highscore.c) - declared with their
 * exact types (docs/vxe/CONVENTIONS.md convention 3). */
extern char* romData;
extern char* cartData;
extern unsigned char parmRam[256];
extern GameFileRecordV1 *pActiveGameData;
void doLedOn(int on);                                     /* main.c */

static const char *const vxe_shared_files[] = {
    "calmeas.csv", "calmeas.tmp", "cdfmeas.csv", "cdfmeas.tmp"
};

static char vxe_game_dir[_MAX_LFN + 1];   /* "" = the root */

void vxeKernelSetGamePath(const char *romPath)
{
    const char *slash = strrchr(romPath, '/');
    size_t n = slash ? (size_t)(slash - romPath) : 0;
    if (n >= sizeof(vxe_game_dir)) n = 0;     /* cannot happen with FatFs paths; fall back to root */
    memcpy(vxe_game_dir, romPath, n);
    vxe_game_dir[n] = '\0';
}

/* Returns the path to use: `path` itself, or `buf` holding <game dir><path>. */
static const TCHAR *vxeMapPath(const TCHAR *path, TCHAR *buf, size_t bufLen)
{
    unsigned i;
    if (vxe_game_dir[0] == '\0' || path[0] != '/' || strchr(path + 1, '/') != NULL) return path;
    for (i = 0; i < sizeof(vxe_shared_files) / sizeof(vxe_shared_files[0]); i++) {
        if (strcmp(path + 1, vxe_shared_files[i]) == 0) return path;
    }
    if (strlen(vxe_game_dir) + strlen(path) + 1 > bufLen) return path;
    strcpy(buf, vxe_game_dir);
    strcat(buf, path);
    return buf;
}

#define VXE_PATH_BUF (_MAX_LFN + 1 + 32)

static FRESULT k_f_open(FIL *fp, const TCHAR *path, BYTE mode)
{
    TCHAR b[VXE_PATH_BUF];
    return f_open(fp, vxeMapPath(path, b, sizeof(b)), mode);
}
static FRESULT k_f_rename(const TCHAR *path_old, const TCHAR *path_new)
{
    TCHAR bo[VXE_PATH_BUF], bn[VXE_PATH_BUF];
    return f_rename(vxeMapPath(path_old, bo, sizeof(bo)), vxeMapPath(path_new, bn, sizeof(bn)));
}
static FRESULT k_f_unlink(const TCHAR *path)
{
    TCHAR b[VXE_PATH_BUF];
    return f_unlink(vxeMapPath(path, b, sizeof(b)));
}

/* DURABILITY (found on hardware). The cart's
 * storage is SPI flash behind a ONE-block (4 KB) RAM write-back cache
 * (flash.c): FatFs's CTRL_SYNC is a no-op (ff_diskio.c), and the timed flush
 * (flashTick) only runs in the USB mass-storage loop, never while a game runs.
 * A dirty block is written back only when another block is written or ANY block
 * is read. So the last block a game's save touches - the directory entry of its
 * final rename - stays in RAM, and is lost if the Vectrex is powered off from
 * inside the game. Seen as a .tmp file with no final file: a game that never
 * calls flashDoWriteback() leaves exactly that behind, so this predates VXE.
 * Every payload file operation that can complete a change now
 * writes the block back before returning; on a clean cache flashDoWriteback()
 * returns at once, so a game's own explicit calls cost nothing extra. Kernel code
 * keeps calling FatFs directly and is unchanged. Host model test (flash.c's
 * cache, power cut after the save): vxe/test_vxe_durable.c. */
static FRESULT k_f_close_durable(FIL *fp)
{
    FRESULT r = f_close(fp);
    flashDoWriteback();
    return r;
}
static FRESULT k_f_sync_durable(FIL *fp)
{
    FRESULT r = f_sync(fp);
    flashDoWriteback();
    return r;
}
static FRESULT k_f_rename_durable(const TCHAR *path_old, const TCHAR *path_new)
{
    FRESULT r = k_f_rename(path_old, path_new);
    flashDoWriteback();
    return r;
}
static FRESULT k_f_unlink_durable(const TCHAR *path)
{
    FRESULT r = k_f_unlink(path);
    flashDoWriteback();
    return r;
}

/* The 1.0 firmware keys a high-score record by name AND the ROM's CRC (crcvx). A .vxe game keeps the
 * two-argument calls; these wrappers add the CRC main.c computed (or left at its 0xFFFFFFFF default for
 * a bundle) for the current load, so the payload's lookup matches the kernel's own at load time. */
static uint32_t vxe_game_crcvx = 0xFFFFFFFFu;
void vxeKernelSetCrcvx(uint32_t crcvx) { vxe_game_crcvx = crcvx; }
static HighScoreRetVal k_highScoreGet_w(const unsigned char *pGame, GameFileRecordV1 *pGameRecord)
{
    return highScoreGet(pGame, vxe_game_crcvx, pGameRecord);
}
static HighScoreRetVal k_highScoreSetGameRecordToDefaults_w(const unsigned char *pGame, GameFileRecordV1 *pGameRecord)
{
    return highScoreSetGameRecordToDefaults(pGame, vxe_game_crcvx, pGameRecord);
}

static const VxeKernelApi vxe_api = {
    .k_version                          = VXE_API_VERSION,
    .k_size                             = sizeof(VxeKernelApi),
    .k_romData_pp                       = &romData,
    .k_cartData_pp                      = &cartData,
    .k_parmRam_p                        = &parmRam,
    .k_pActiveGameData_pp               = &pActiveGameData,
    .k_f_open                           = k_f_open,
    .k_f_close                          = k_f_close_durable,
    .k_f_gets                           = f_gets,
    .k_f_write                          = f_write,
    .k_f_sync                           = k_f_sync_durable,
    .k_f_rename                         = k_f_rename_durable,
    .k_f_unlink                         = k_f_unlink_durable,
    .k_flashDoWriteback                 = flashDoWriteback,
    .k_doLedOn                          = doLedOn,
    .k_ledsClear                        = ledsClear,
    .k_ledsUpdate                       = ledsUpdate,
    .k_rainbowStep                      = rainbowStep,
    .k_millis                           = millis,
    .k_dwt_read_cycle_counter           = dwt_read_cycle_counter,
    .k_xvprintf                         = xvprintf,
    .k_highScoreGet                     = k_highScoreGet_w,
    .k_highScoreSetGameRecordToDefaults = k_highScoreSetGameRecordToDefaults_w,
    .k_highScoreSave                    = highScoreSave,
    .k_menuScoreSet                     = vxeKernelMenuScoreSet,
};

static VxePayloadHooks vxe_hooks;

#ifndef VXE_SPLIT
#define VXE_SPLIT 0
#endif

#if VXE_SPLIT
/* The payload is a separate image in flash sectors 4-5. Call it only
 * if its header is believable: an erased slot reads all $FF, a stale or
 * foreign image may have any bytes at all, and jumping into either is a hard
 * fault with the screen dark. Refusing just leaves RPC 64-79 unhandled - the
 * multicart, its menu and every plain ROM keep working. */
int vxeHeaderOk(const VxePayloadHeader *h)
{
    uint32_t start = h->start & ~1u;                             /* drop the Thumb bit */
    uint32_t end   = h->image_end;
    uint32_t ram0  = h->ram_start, ram1 = h->ram_end;

    if (h->magic != VXE_PAYLOAD_MAGIC) return 0;
    if (h->abi_version < VXE_API_MIN_VERSION || h->abi_version > VXE_API_VERSION) return 0;   /* too old, or built for a newer kernel */
    if (h->header_size != sizeof(VxePayloadHeader)) return 0;
    if (end <= VXE_SLOT_ADDR + sizeof(VxePayloadHeader) || end > VXE_SLOT_ADDR + VXE_SLOT_SIZE) return 0;
    if (start < VXE_SLOT_ADDR + sizeof(VxePayloadHeader) || start >= end) return 0;
    if ((h->start & 1u) == 0) return 0;                                      /* must be a Thumb address */
    if (ram0 < VXE_PAYLOAD_RAM || ram1 < ram0 || ram1 > VXE_PAYLOAD_RAM + VXE_PAYLOAD_RAM_SZ) return 0;
    return 1;
}

void vxeKernelStart(void)
{
    const VxePayloadHeader *h = (const VxePayloadHeader *)VXE_SLOT_PTR;
    vxe_hooks.rpc_dispatch = 0;
    if (!vxeHeaderOk(h)) {
        xprintf("VXE: no valid payload at %08x (magic %08x) - running without one\n",
                (unsigned)VXE_SLOT_ADDR, (unsigned)h->magic);
        return;
    }
    xprintf("VXE: payload ABI %d, %d bytes, RAM %d bytes\n", h->abi_version,
            (int)(h->image_end - VXE_SLOT_ADDR),
            (int)(h->ram_end - h->ram_start));
#ifdef VXE_HOST_TEST
    vxe_host_start(h);                  /* host tests cannot jump into ARM code */
#else
    ((void (*)(const VxeKernelApi *, VxePayloadHooks *))(uintptr_t)h->start)(&vxe_api, &vxe_hooks);
#endif
}
#else
/* Single image: the payload is linked in, so call it directly. */
void vxeKernelStart(void)
{
    vxe_hooks.rpc_dispatch = 0;
    vxe_payload_init(&vxe_api, &vxe_hooks);
}
#endif

void vxeKernelDispatch(int id)
{
    if (vxe_hooks.rpc_dispatch) vxe_hooks.rpc_dispatch(id);
}

/* Forget the running payload before its flash is rewritten
 * (vxe_install.c); vxeKernelStart() brings the new one up. */
void vxeKernelStop(void)
{
    vxe_hooks.rpc_dispatch = 0;
}
