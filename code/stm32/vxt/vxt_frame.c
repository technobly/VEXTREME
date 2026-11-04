/*
 * vxt_frame.c - VXT toolkit: vector-list writer (STM32 side). GPLv3.
 * See vxt_frame.h for the format and grounding notes.
 *
 * Two bugs found and fixed (root cause of Test 3's
 * random-line-from-center symptom):
 *
 * 1. POINTER/array extern mismatch (the fatal one). rom.h declares
 *    `extern char romData[]` (an ARRAY), but main.c:67 defines
 *    `char* romData = menuData;` (a POINTER). rom.h's declaration
 *    belongs to rom.c, which is not linked (rom.o is absent from the
 *    Makefile's OBJS). With the array declaration, romData[pos]=x
 *    compiles to a write at (address of the pointer variable)+pos -
 *    i.e., into unrelated firmware RAM - and never touches the served
 *    cart image at all. The 6809 was therefore drawing load-time
 *    padding (0x01) and, past $8000, floating-bus noise: the observed
 *    dot-at-center + random-edge lines. Fixed by declaring the symbols
 *    with their true types below instead of including rom.h.
 *
 * 2. Bank mirroring (would have bitten next). romemu.S (line 165)
 *    inverts the PB6/A15 address bit - a VEXTREME addition (Sprite_tm's
 *    original had no such inversion), which is why main.c duplicates
 *    <=32K images into both 32K banks at load time. Our draw loop
 *    (like VOOM's) drives PB6 low mid-frame via `clr VIA_port_b`,
 *    flipping which bank the 6809's data reads map to. Runtime-written
 *    frame data must therefore be mirrored into both banks - but only
 *    when the served image is the 64K cartData buffer; menuData is
 *    20K, and mirroring there would overflow it.
 *
 * PATCHED (Test 5 prep): vf_put() renamed to vxtImgPut() and
 * made non-static so vxt_sound.c can reuse it instead of duplicating the
 * mirroring rule above. Behaviour is identical identical to before -
 * only the name and linkage changed.
 */

#include "vxt_frame.h"

/* True definitions live in main.c (lines 67 and 82):
 *   char* romData  = menuData;         <- POINTER, repointed by doStartRom()
 *   char* cartData = c_and_l.cartData; <- 64K cart buffer
 * Do not include rom.h here - its `extern char romData[]` array
 * declaration mismatches the real pointer definition (bug #1 above). */
extern char* romData;
extern char* cartData;

static uint16_t vf_pos;
static int      vf_count;
static int      vf_overflow;

void vxtImgPut(uint16_t pos, uint8_t b)
{
    /* CHANGED 2026-10-24 (VXE Phase 2): read romData once. Behaviour is
     * identical (nothing repoints it during this call), but the char store
     * may alias the pointer variable itself, so the old form reloaded
     * romData after every store - and under VXE each read of romData is one
     * pointer hop further (vxe_redirect.h). This is the hottest toolkit path
     * (four calls per SmartList record): 8 loads per call became 6 (Phase 1:
     * 5). */
    char *r = romData;
    r[pos] = (char)b;
    if (r == cartData) {
        /* 64K buffer: mirror into the other 32K bank so the data is
         * visible regardless of PB6 state (bug #2 above). */
        r[pos ^ 0x8000] = (char)b;
    }
}

void vxtFrameBegin(uint16_t offset)
{
    vf_pos = offset;
    vf_count = 0;
    vf_overflow = 0;
}

int vxtFrameOverflowed(void)
{
    return vf_overflow;
}

void vxtFrameVector(uint8_t intensity, uint8_t scale, uint8_t coord1, uint8_t coord2)
{
    /* Hard bound - see VXT_FRAME_MAX_RECORDS in vxt_frame.h. Overrunning the
     * list into the sound block at $2000 corrupts the PSG command stream and
     * kills sound permanently (observed on hardware). Truncating the picture
     * is a far better failure mode, and vxtFrameOverflowed() makes it
     * detectable rather than baffling. */
    if (vf_count >= VXT_FRAME_MAX_RECORDS) {
        vf_overflow = 1;
        return;
    }
    vf_count++;
    vxtImgPut(vf_pos++, intensity);
    vxtImgPut(vf_pos++, scale);
    vxtImgPut(vf_pos++, coord1);
    vxtImgPut(vf_pos++, coord2);
}

void vxtFrameVectorAt(uint8_t intensity, int recenter,
                      uint8_t scale, int8_t coord1, int8_t coord2)
{
    /* Full 0-127 now that the recenter flag lives on bit 7 (see vxt_frame.h).
     * A recenter record is clamped to 126 so intensity|flag can never form
     * $FF, which ends the list. */
    if (intensity > 127) intensity = 127;
    if (recenter) {
        if (intensity > 126) intensity = 126;
        vxtFrameVector((uint8_t)(intensity | VXT_RECENTER_FLAG),
                       scale, (uint8_t)coord1, (uint8_t)coord2);
    } else {
        vxtFrameVector(intensity, scale, (uint8_t)coord1, (uint8_t)coord2);
    }
}

void vxtFrameMoveBig(int16_t totalY, int16_t totalX, uint8_t scale)
{
    /* Chain small safe moves. A position computed as `start + index*pitch`
     * packed into one int8_t wraps once the pitch grows; this
     * cannot. Also the only safe way to emit a move whose rate would exceed
     * 127 (e.g. inter-box pitch at a minimal duration). */
    while (totalY != 0 || totalX != 0) {
        int8_t sy = (int8_t)(totalY > 100 ? 100 : (totalY < -100 ? -100 : totalY));
        int8_t sx = (int8_t)(totalX > 100 ? 100 : (totalX < -100 ? -100 : totalX));
        vxtFrameVectorAt(0, 0, scale, sy, sx);
        totalY = (int16_t)(totalY - sy);
        totalX = (int16_t)(totalX - sx);
    }
}

void vxtFrameEnd(void)
{
    vxtImgPut(vf_pos++, 0xFF);
}
