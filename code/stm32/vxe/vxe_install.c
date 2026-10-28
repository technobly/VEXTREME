/*
 * vxe_install.c - KERNEL side: load a .vxe bundle picked in the menu (packer:
 * tools/make_vxe.py).
 *
 *  1. Check the bundle header (magic, version, header CRC, that it carries an image for
 *     this kernel's flash layout, sizes).
 *  2. Read the payload's own header from the bundle. If the slot already holds
 *     a valid payload with the same IDENTITY (crc32 field) and size, skip to 5.
 *  3. Read the whole STM part (the image for this kernel's layout) once and check its CRC and identity BEFORE
 *     touching flash - a damaged bundle never erases a working payload.
 *  4. Stop the running payload, erase flash sector 4 (+5 if needed), program
 *     the body, program the VxePayloadHeader LAST (until then the slot has no
 *     valid magic, so a power cut mid-install just leaves an empty slot that
 *     the kernel refuses at boot), reset the flash caches (both are enabled at
 *     120 MHz; stale lines of the old payload would otherwise survive), and
 *     verify the slot by CRC read back from flash.
 *  5. Copy the 6809 image into `dest`, check its CRC, restart the payload.
 *
 * A refusal at any step serves the message cartridge instead (vxeShowError), so the user sees the reason
 * on screen; the installed payload is left as it was.
 *
 * Runs inside doHandleEvent() (the menu's ROM launch), where the 6809 waits in
 * its RAM stub - the same window the SD load already uses. Erasing stalls
 * every flash fetch until it completes (the F411 has one bank); nothing
 * time-critical runs then. The cart LEDs show magenta while flash is written.
 * Kernel code; no payload code runs from step 4 until the restart.
 */
#include <string.h>
#include <stdbool.h>
#include "vxe_api.h"
#include "../xprintf.h"
#include "vxe_msgcart.h"

#ifndef VXE_SPLIT
#define VXE_SPLIT 0
#endif

#if VXE_SPLIT

uint32_t vxeCrc32(uint32_t crc, const void *data, uint32_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    int k;
    crc = ~crc;
    while (len--) {
        crc ^= *p++;
        for (k = 0; k < 8; k++) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

/* ---- refusing a .vxe: show WHY on the Vectrex screen --------------------------------------------------------
 * A refused game used to drop back to the menu with the reason on the debug console only. Now the kernel serves a
 * small message cartridge (vxe_msgcart.h, from code/app6809/VxeMsg/) with the reason written into its eight text
 * lines, and the user presses RESET to get the menu. The installed payload is never touched by a refusal. */
enum { VXE_E_FILE, VXE_E_ABI, VXE_E_LAYOUT, VXE_E_INSTALL };

static void vxeMsgPut(char *dest, int slot, const char *s)
{
    char *p = dest + VXE_MSG_OFS + slot * VXE_MSG_LEN;
    int n = 0;
    for (; s[n] && n < VXE_MSG_LEN - 1; n++) {
        char c = s[n];
        if (c >= 'a' && c <= 'z') c = (char)(c - 32);
        if (c < 0x20 || c > 0x5F) c = '-';             /* the BIOS font has capitals, digits and some punctuation */
        p[n] = c;
    }
    p[n] = (char)0x80;
}

/* "/roms/VXE/MYGAME.vxe" -> "MYGAME" (at most 16 characters) */
static void vxeMsgName(char *out, const char *path)
{
    const char *b = path, *q;
    unsigned len, n;
    for (q = path; *q; q++) if (*q == '/') b = q + 1;
    len = (unsigned)strlen(b);
    if (len > 4 && b[len - 4] == '.') len -= 4;
    if (len > 16) len = 16;
    for (n = 0; n < len; n++) out[n] = b[n];
    out[n] = 0;
}

/* `mask` is the bundle's VXE_LAYOUT_BIT_* set (VXE_E_LAYOUT only). Returns the image size, as a bundle load does. */
static unsigned vxeShowError(char *dest, unsigned destMax, int kind, const char *path, unsigned mask)
{
    char name[17];
    int i;
    if (destMax < sizeof(vxe_msgcart)) return 0;
    memcpy(dest, vxe_msgcart, sizeof(vxe_msgcart));
    for (i = 0; i < VXE_MSG_LINES; i++) vxeMsgPut(dest, i, "");   /* every slot ends in $80 */
    vxeMsgName(name, path);
    vxeMsgPut(dest, 0, name);
    switch (kind) {
    case VXE_E_LAYOUT:
        vxeMsgPut(dest, 1, "ONLY LOADS ON THE");
        vxeMsgPut(dest, 2, (mask & VXE_LAYOUT_BIT_DFU) ? "NON-UF2 KERNEL" : "UF2 KERNEL");
        vxeMsgPut(dest, 3, (mask & VXE_LAYOUT_BIT_DFU) ? "(192 KB PAYLOAD)" : "(128 KB PAYLOAD)");
        vxeMsgPut(dest, 4, VXE_LAYOUT_INDEX ? "THIS IS A NON-UF2" : "THIS IS THE UF2");
        vxeMsgPut(dest, 5, VXE_LAYOUT_INDEX ? "KERNEL (192 KB)" : "KERNEL (128 KB)");
        break;
    case VXE_E_ABI:
        vxeMsgPut(dest, 1, "BUILT FOR ANOTHER");
        vxeMsgPut(dest, 2, "KERNEL VERSION");
        vxeMsgPut(dest, 3, "REBUILD THE GAME");
        break;
    case VXE_E_INSTALL:
        vxeMsgPut(dest, 1, "COULD NOT BE");
        vxeMsgPut(dest, 2, "INSTALLED. CHECK");
        vxeMsgPut(dest, 3, "THE CARD, RETRY");
        break;
    default:
        vxeMsgPut(dest, 1, "CANNOT BE LOADED");
        vxeMsgPut(dest, 2, "FILE IS DAMAGED OR");
        vxeMsgPut(dest, 3, "IN AN OLD FORMAT");
        vxeMsgPut(dest, 4, "REBUILD THE .VXE");
        break;
    }
    vxeMsgPut(dest, 6, "PRESS RESET FOR");
    vxeMsgPut(dest, 7, "THE MENU");
    return (unsigned)sizeof(vxe_msgcart);
}

/* ---- internal flash: the slot's sectors (see vxe_api.h for the two layouts) ------- */
#if VXE_LAYOUT_UF2
#define VXE_SECTOR_A        5               /* 0x08020000, 128 KB: the whole slot */
#define VXE_SECTOR_A_SIZE   (128u * 1024u)
#else
#define VXE_SECTOR_A        4
#define VXE_SECTOR_A_SIZE   (64u * 1024u)   /* 0x08010000 */
#define VXE_SECTOR_B        5               /* 0x08020000, 128 KB */
#endif

#ifdef VXE_HOST_TEST
/* vxe/test_vxe_install.c simulates flash. */
void vxe_host_flash_begin(void);
int  vxe_host_flash_erase(int sector);
int  vxe_host_flash_word(uint32_t addr, uint32_t w);
void vxe_host_flash_end(void);
void ledsMagenta(void);
void applyLedSettings(bool initial);
#define vxeFlashBegin()      vxe_host_flash_begin()
#define vxeFlashErase(s)     vxe_host_flash_erase(s)
#define vxeFlashWord(a, w)   vxe_host_flash_word((a), (w))
#define vxeFlashEnd()        vxe_host_flash_end()
#else
#include <libopencm3/stm32/flash.h>
#include "../main.h"                /* ledsMagenta(), applyLedSettings() */

#define VXE_FLASH_SR_ERRORS (FLASH_SR_OPERR | FLASH_SR_WRPERR | FLASH_SR_PGAERR | FLASH_SR_PGPERR | (1u << 7) /* PGSERR */)

static void vxeFlashBegin(void)
{
    flash_unlock();
    FLASH_SR = FLASH_SR_EOP | VXE_FLASH_SR_ERRORS;          /* write-1-to-clear */
}
static int vxeFlashErase(int sector)
{
    flash_erase_sector((uint8_t)sector, FLASH_CR_PROGRAM_X32);   /* x32: VDD 2.7-3.6 V */
    return (FLASH_SR & VXE_FLASH_SR_ERRORS) == 0;
}
static int vxeFlashWord(uint32_t addr, uint32_t w)
{
    flash_program_word(addr, w);
    return (FLASH_SR & VXE_FLASH_SR_ERRORS) == 0;
}
static void vxeFlashEnd(void)
{
    flash_lock();
    /* The caches may hold lines of the PREVIOUS payload. RM0383: a cache may
     * only be reset while it is disabled. */
    FLASH_ACR &= ~(FLASH_ACR_ICEN | FLASH_ACR_DCEN);
    FLASH_ACR |=  (FLASH_ACR_ICRST | FLASH_ACR_DCRST);
    FLASH_ACR &= ~(FLASH_ACR_ICRST | FLASH_ACR_DCRST);
    FLASH_ACR |=  (FLASH_ACR_ICEN | FLASH_ACR_DCEN);
}
#endif

/* Reads [off, off+len) of the open bundle in chunks through buf, feeding each
 * chunk to fn. Chunks are multiples of 4 bytes and start at part offset 0. */
typedef int (*vxeChunkFn)(const uint8_t *chunk, uint32_t pos, uint32_t n, void *ctx);
static int vxeForChunks(FIL *f, uint32_t off, uint32_t len, char *buf, uint32_t bufMax, vxeChunkFn fn, void *ctx)
{
    uint32_t pos = 0, chunk = bufMax & ~3u;
    UINT br;
    if (chunk == 0 || f_lseek(f, off) != FR_OK) return 0;
    while (pos < len) {
        uint32_t n = (len - pos < chunk) ? (len - pos) : chunk;
        if (f_read(f, buf, n, &br) != FR_OK || br != n) return 0;
        if (!fn((const uint8_t *)buf, pos, n, ctx)) return 0;
        pos += n;
    }
    return 1;
}

typedef struct { uint32_t all, img; } vxeCrcs;
static int vxeCrcChunk(const uint8_t *c, uint32_t pos, uint32_t n, void *ctx)
{
    vxeCrcs *k = (vxeCrcs *)ctx;
    k->all = vxeCrc32(k->all, c, n);
    if (pos + n > sizeof(VxePayloadHeader)) {
        uint32_t skip = (pos >= sizeof(VxePayloadHeader)) ? 0 : (uint32_t)sizeof(VxePayloadHeader) - pos;
        k->img = vxeCrc32(k->img, c + skip, n - skip);
    }
    return 1;
}

static int vxeProgramChunk(const uint8_t *c, uint32_t pos, uint32_t n, void *ctx)
{
    uint32_t i;
    (void)ctx;
    for (i = 0; i < n; i += 4) {
        uint32_t w;
        if (pos + i < sizeof(VxePayloadHeader)) continue;        /* the header goes in LAST */
        memcpy(&w, c + i, 4);
        if (w == 0xFFFFFFFFu) continue;                           /* already erased */
        if (!vxeFlashWord(VXE_SLOT_ADDR + pos + i, w)) {
            xprintf("VXE: flash program error at %08x\n", (unsigned)(VXE_SLOT_ADDR + pos + i));
            return 0;
        }
    }
    return 1;
}

static int vxeInstall(FIL *f, const VxeBundleImage *im, const VxePayloadHeader *ph, char *buf, uint32_t bufMax)
{
    vxeCrcs k = { 0, 0 };
    uint32_t i, hw[sizeof(VxePayloadHeader) / 4];
    int ok;

    /* 3. integrity BEFORE erasing anything */
    if (!vxeForChunks(f, im->offset, im->size, buf, bufMax, vxeCrcChunk, &k)) {
        xprintf("VXE: cannot read the payload part\n"); return 0;
    }
    if (k.all != im->crc || k.img != ph->crc32) {
        xprintf("VXE: damaged bundle (part CRC %08x/%08x, identity %08x/%08x) - slot left as it is\n",
                (unsigned)k.all, (unsigned)im->crc, (unsigned)k.img, (unsigned)ph->crc32);
        return 0;
    }

    /* 4. erase, body, header last */
    xprintf("VXE: installing payload %08x (%u bytes)...\n", (unsigned)ph->crc32, (unsigned)im->size);
    vxeKernelStop();                         /* the old payload's code is about to vanish */
    ledsMagenta();
    vxeFlashBegin();
    ok = vxeFlashErase(VXE_SECTOR_A);
#ifdef VXE_SECTOR_B
    if (ok && im->size > VXE_SECTOR_A_SIZE) ok = vxeFlashErase(VXE_SECTOR_B);
#endif
    if (!ok) xprintf("VXE: flash erase error\n");
    if (ok) ok = vxeForChunks(f, im->offset, im->size, buf, bufMax, vxeProgramChunk, 0);
    if (ok) {
        memcpy(hw, ph, sizeof(hw));
        for (i = 0; ok && i < sizeof(hw) / 4; i++) ok = vxeFlashWord(VXE_SLOT_ADDR + 4 * i, hw[i]);
        if (!ok) xprintf("VXE: flash program error in the header\n");
    }
    vxeFlashEnd();
    applyLedSettings(false);
    if (!ok) return 0;

    /* verify: the slot must now hold exactly the bundle's bytes */
    if (vxeCrc32(0, VXE_SLOT_PTR, im->size) != im->crc) {
        xprintf("VXE: verify failed after programming\n"); return 0;
    }
    xprintf("VXE: payload installed and verified\n");
    return 1;
}

unsigned int vxeKernelLoadBundle(const char *path, char *dest, unsigned int destMax)
{
    FIL f;
    UINT br;
    VxeBundleHeader bh;
    VxePayloadHeader ph;
    const VxePayloadHeader *slot = (const VxePayloadHeader *)VXE_SLOT_PTR;
    const VxeBundleImage *im;
    int installed;
    int err = VXE_E_FILE;                    /* why a refusal happened: what the message cartridge says */
    unsigned mask = 0;

    if (f_open(&f, path, FA_READ) != FR_OK) {
        xprintf("VXE: cannot open %s\n", path); return vxeShowError(dest, destMax, VXE_E_FILE, path, 0);
    }

    /* 1. the bundle header */
    if (f_read(&f, &bh, sizeof(bh), &br) != FR_OK || br != sizeof(bh)
        || memcmp(bh.magic, VXE_BUNDLE_MAGIC, 4) != 0
        || bh.format_version != VXE_BUNDLE_VERSION || bh.header_size != VXE_BUNDLE_HEADER_SIZE
        || vxeCrc32(0, &bh, 124) != bh.header_crc) {
        xprintf("VXE: %s is not a valid bundle\n", path); goto fail;
    }
    if (bh.api_version < VXE_API_MIN_VERSION || bh.api_version > VXE_API_VERSION) {
        xprintf("VXE: payload ABI %d, this kernel runs %d-%d - rebuild the game\n", bh.api_version, VXE_API_MIN_VERSION, VXE_API_VERSION);
        err = VXE_E_ABI; goto fail;
    }
    {   /* which layouts the bundle carries: the flags must agree with the image table */
        unsigned want = bh.layouts, have = 0, i;
        for (i = 0; i < 2; i++) if (bh.image[i].size) have |= 1u << i;
        if (want == 0 || (want & ~3u) || want != have) { xprintf("VXE: bundle layout flags are inconsistent\n"); goto fail; }
        if (!(want & (1u << VXE_LAYOUT_INDEX))) {
            xprintf("VXE: %s is built for the %s kernel only, this is the %s kernel\n", path,
                    (want & VXE_LAYOUT_BIT_DFU) ? "dfu" : "uf2", VXE_LAYOUT_INDEX ? "dfu" : "uf2");
            err = VXE_E_LAYOUT; mask = want; goto fail;
        }
    }
    im = &bh.image[VXE_LAYOUT_INDEX];
    if (bh.m6809_size == 0 || bh.m6809_size > destMax || bh.m6809_offset < VXE_BUNDLE_HEADER_SIZE
        || im->size < sizeof(VxePayloadHeader) || im->size > VXE_SLOT_SIZE || (im->size & 3u)
        || im->offset < VXE_BUNDLE_HEADER_SIZE || im->load_addr != VXE_SLOT_ADDR) {
        xprintf("VXE: bundle sizes/offsets out of range\n"); goto fail;
    }

    /* 2. the payload's own header, and is it already installed? */
    if (f_lseek(&f, im->offset) != FR_OK || f_read(&f, &ph, sizeof(ph), &br) != FR_OK || br != sizeof(ph)
        || ph.magic != VXE_PAYLOAD_MAGIC || ph.header_size != sizeof(VxePayloadHeader)
        || ph.abi_version != bh.api_version
        || ph.image_end - VXE_SLOT_ADDR != im->size) {
        xprintf("VXE: the bundle's payload header is invalid\n"); goto fail;
    }
    installed = vxeHeaderOk(slot) && ph.crc32 != 0 && slot->crc32 == ph.crc32
                && slot->image_end == ph.image_end;
    if (installed) {
        xprintf("VXE: payload %08x already installed\n", (unsigned)ph.crc32);
    } else if (!vxeInstall(&f, im, &ph, dest, destMax)) {
        err = VXE_E_INSTALL; goto fail;
    }

    /* 5. the 6809 image */
    if (f_lseek(&f, bh.m6809_offset) != FR_OK || f_read(&f, dest, bh.m6809_size, &br) != FR_OK
        || br != bh.m6809_size || vxeCrc32(0, dest, bh.m6809_size) != bh.m6809_crc) {
        xprintf("VXE: cannot read the 6809 part\n"); goto fail;
    }
    f_close(&f);
    vxeKernelStart();                        /* fresh start for every launch */
    return bh.m6809_size;

fail:
    f_close(&f);
    return vxeShowError(dest, destMax, err, path, mask);
}

#else /* !VXE_SPLIT */

unsigned int vxeKernelLoadBundle(const char *path, char *dest, unsigned int destMax)
{
    (void)dest; (void)destMax;
    xprintf("VXE: %s - .vxe bundles need a split (VXE_SPLIT=1) kernel\n", path);
    return 0;
}

#endif /* VXE_SPLIT */
