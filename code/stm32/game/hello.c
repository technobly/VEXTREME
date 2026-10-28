/*
 * hello.c - HELLO, the SDK template game for the VXE loader. GPLv3.
 * See hello.h and docs/vxe/SDK_GUIDE.md. Built WITH vxe/vxe_redirect.h, so the
 * f_open/xprintf/flashDoWriteback calls below reach the kernel through the
 * VxeKernelApi table without this file knowing.
 *
 * What it shows, each in the place a real game puts it:
 *   hello_init_handler()   once per 6809 boot - reset state, ALL file reads
 *   hello_frame_handler()  every frame - input, one tick, compose the list, sound
 *   hello_save()           an explicit user action - the only file write
 * The file is "/hello.dat" and lands in the folder the game was launched from
 * (vxe/vxe_kernel.c maps root-level game files there).
 *
 * It also applies this console's CALIBRATION, which every game should apply: a Vectrex's
 * analog parts age, so no two draw alike. Two corrections matter here
 * (gamelib_beam.h has the measurements): a DRAWN line covers about 5% less distance
 * than a blanked move for the same command (the "draw gain" - without it every edge
 * ends short and the corners of a box do not meet), and the screen's centre is
 * offset from the DAC's zero (the "centre offset" - without it a game's margins are
 * uneven). They are measured once per console by the calibration screens, stored in
 * /calmeas.csv at the card root (written by the calibration rig, CAL.vxe; shared by every game), loaded once in the init
 * handler and applied every frame.
 */
#include <stdint.h>
#include <string.h>
#include "hello.h"
#include "../fatfs/ff.h"
#include <stdint.h>                /* flash.h uses uint32_t/uint8_t without including this itself */
#include "../flash.h"
#include "../xprintf.h"
#include "../vxt/vxt_smart.h"
#include "../vxt/vxt_input.h"
#include "../vxt/vxt_sound.h"
#include "../vxt/vxt_cal_load.h"
#include "../gamelib/gamelib_beam.h"

/* ---- geometry, in physical beam units (the screen is about +-13500 x +-18000) ---- */
#define HELLO_HALF        2500      /* half the square's side                          */
#define HELLO_INNER       1000      /* half-side of the "saved!" marker square         */
#define HELLO_STEP         200      /* beam units moved per frame while the stick is held */
#define HELLO_LIMIT_X     9500      /* keep the square well inside the visible area     */
#define HELLO_LIMIT_Y    13500
#define HELLO_INTENSITY     80      /* 0-127 */
#define HELLO_SETTLE_FRAMES  8      /* empty frames after a flash write - see hello_frame_handler() */

/* ---- the SmartList region this game writes: the $0800-$2000 span (the
 *      sound region starts at $2000), so 1536 four-byte records. A game that
 *      shares the span between several draw passes must give each its own
 *      sub-capacity (docs/vxe/CONVENTIONS.md convention 6). ---- */
#define HELLO_REGION_OFFSET   VXT_SMART_OFFSET
#define HELLO_REGION_RECORDS  ((0x2000 - VXT_SMART_OFFSET) / 4)

/* ---- beam scales: position moves at 0x20, drawing at 0x0C - the pairing the
 *      toolkit's examples use ---- */
#define HELLO_POS_SCALE    0x20
#define HELLO_DRAW_SCALE   0x0C

#define HELLO_FILE   "/hello.dat"
#define HELLO_TMP    "/hello.tmp"

/* ---- calibration: the rig's /calmeas.csv, which stays at the card root for every game
 *      (see vxe/vxe_kernel.c) and is the loaders' default source (vxt_cal_load.h). A game
 *      with its own calibration screen would call vxtCalLoadSetSource() here instead. ---- */
#ifndef HELLO_DRAW_GAIN_DEFAULT
#define HELLO_DRAW_GAIN_DEFAULT 1060   /* per-1000; a typical console until this one is measured */
#endif

/* ---- game state. Statics are cheap here: a payload gets a fixed 16 KB RAM
 *      window, and this uses a few dozen bytes. ---- */
static int32_t  hello_y, hello_x;      /* the square's centre                           */
static uint8_t  hello_prev_btn;        /* button 1 last frame, for edge detection       */
static uint8_t  hello_marker;          /* frames left to show the "saved" marker        */
static uint8_t  hello_beep;            /* frames left of the save beep                  */
static uint8_t  hello_settle;          /* empty frames still to send after a flash write */
static uint8_t  hello_snd_ready;       /* 0 until the AY has been taken over            */
static int16_t  hello_draw_gain;       /* this console's measured draw gain, per-1000   */
static int16_t  hello_off_y, hello_off_x;   /* ... and its centre offset, beam units    */

/* ---- tiny text <-> int helpers (avoid newlib's printf/strtol: size) ---- */
static char *hello_put_int(char *p, int32_t v)
{
    char tmp[12];
    int n = 0;
    uint32_t u = (v < 0) ? (uint32_t)(-v) : (uint32_t)v;
    if (v < 0) *p++ = '-';
    do { tmp[n++] = (char)('0' + u % 10); u /= 10; } while (u);
    while (n) *p++ = tmp[--n];
    return p;
}

static const char *hello_get_int(const char *s, int32_t *out)
{
    int neg = 0;
    int32_t v = 0;
    int digits = 0;
    while (*s == ' ') s++;
    if (*s == '-') { neg = 1; s++; }
    while (*s >= '0' && *s <= '9') { v = v * 10 + (*s - '0'); s++; digits++; }
    if (!digits) return 0;
    *out = neg ? -v : v;
    return s;
}

static int32_t hello_clamp(int32_t v, int32_t lim)
{
    return v < -lim ? -lim : (v > lim ? lim : v);
}

/* ---- the one file write: tmp -> close -> unlink -> rename, then flush. A direct
 *      overwrite would leave a truncated file if the power went mid-write. The
 *      final flashDoWriteback() matters: the cart's storage is flash behind a
 *      one-block RAM cache, and without it the rename can be lost at power-off
 *      (the kernel also does this for you under VXE; a game that must run
 *      standalone should do it itself). ONE-SHOT path only - ~100 ms. ---- */
static int hello_save(void)
{
    FIL f;
    char buf[32];
    char *p = buf;
    UINT wrote = 0, n;
    int ok;

    p = hello_put_int(p, hello_y);
    *p++ = ' ';
    p = hello_put_int(p, hello_x);
    *p++ = '\n';
    n = (UINT)(p - buf);

    if (f_open(&f, HELLO_TMP, FA_CREATE_ALWAYS | FA_WRITE) != FR_OK) return 0;
    if (f_write(&f, buf, n, &wrote) != FR_OK || wrote != n) { f_close(&f); return 0; }
    if (f_close(&f) != FR_OK) return 0;
    (void)f_unlink(HELLO_FILE);          /* this FatFs's f_rename does not replace */
    ok = (f_rename(HELLO_TMP, HELLO_FILE) == FR_OK);
    flashDoWriteback();
    return ok;
}

/* Once per 6809 boot (cold or warm: a 6809 reset does NOT reset the STM32, so
 * everything a game keeps in RAM must be rebuilt here). The place for all file
 * reads; none on the frame path (convention 19). Reading never creates a file:
 * a missing or unreadable file just leaves the defaults. */
void hello_init_handler(uint8_t id, volatile uint8_t *parm)
{
    FIL f;
    char line[40];
    (void)id; (void)parm;

    hello_y = 0; hello_x = 0;
    hello_prev_btn = 0; hello_marker = 0; hello_beep = 0; hello_settle = 0; hello_snd_ready = 0;

    /* This console's calibration, read once here and cached (no file I/O on the
     * frame path). Each loader returns 0 and leaves its value alone when the console has
     * not been measured, so the defaults below stand until someone runs a calibration. */
    hello_draw_gain = HELLO_DRAW_GAIN_DEFAULT;
    (void)vxtCalLoadDrawGain(&hello_draw_gain);
    hello_off_y = 0; hello_off_x = 0;
    (void)vxtCalLoadOffset(&hello_off_y, &hello_off_x);
    xprintf("HELLO: draw gain %d, centre offset %d,%d\n", (int)hello_draw_gain, (int)hello_off_y, (int)hello_off_x);

    if (f_open(&f, HELLO_FILE, FA_READ) == FR_OK) {
        if (f_gets(line, (int)sizeof line, &f) != 0) {
            int32_t y, x;
            const char *p = hello_get_int(line, &y);
            if (p && hello_get_int(p, &x)) {
                hello_y = hello_clamp(y, HELLO_LIMIT_Y);
                hello_x = hello_clamp(x, HELLO_LIMIT_X);
            }
        }
        f_close(&f);
    }
    xprintf("HELLO: start position %d,%d\n", (int)hello_y, (int)hello_x);
}

/* A square: four INDEPENDENT edges, each with its own reposition, so the corners
 * cannot drift apart (convention 12: independent edges cost ~9-14 records each,
 * chained ones ~3 - fine for 8 edges; a model with hundreds of edges would chain
 * with gamelibChainBigGrouped instead). gamelibDrawHugeLine() picks a safe scale
 * for any length. */
static void hello_square(int32_t cy, int32_t cx, int32_t h, uint8_t intensity)
{
    gamelibDrawHugeLine(cy - h, cx - h,   0,  2 * h, intensity);   /* bottom, left -> right */
    gamelibDrawHugeLine(cy - h, cx + h,  2 * h,   0, intensity);   /* right,  up            */
    gamelibDrawHugeLine(cy + h, cx + h,   0, -2 * h, intensity);   /* top,    right -> left */
    gamelibDrawHugeLine(cy + h, cx - h, -2 * h,   0, intensity);   /* left,   down          */
}

void hello_frame_handler(uint8_t id, volatile uint8_t *parm)
{
    int jx = vxtInJoyX(parm), jy = vxtInJoyY(parm);
    uint8_t btn = vxtInBtn1(parm) ? 1 : 0;
    (void)id;

    /* ---- one tick of game logic ---- */
    if (jx) hello_x += (jx > 0 ? HELLO_STEP : -HELLO_STEP);       /* digital stick: only the sign matters */
    if (jy) hello_y += (jy > 0 ? HELLO_STEP : -HELLO_STEP);
    hello_x = hello_clamp(hello_x, HELLO_LIMIT_X);
    hello_y = hello_clamp(hello_y, HELLO_LIMIT_Y);

    if (btn && !hello_prev_btn) {                 /* button 1 pressed this frame: SAVE */
        if (hello_save()) { hello_marker = 25; hello_beep = 8; hello_settle = HELLO_SETTLE_FRAMES; }
    }
    hello_prev_btn = btn;

    /* ---- after a FLASH WRITE, draw nothing for a few frames. Found on hardware: erasing and
     *      programming the cart's flash chip briefly disturbs the Vectrex's picture - a partial
     *      ghost of the box flashes near the centre for a moment after the write, whatever else
     *      the frame contains (reading the file, or merely pausing 100 ms, did NOT do it; sound,
     *      the second box and the beam state were each ruled out). Sending a few empty frames
     *      lets it pass unseen; 3 were enough on the test console, 8 are used for margin. The
     *      beam is dimmed so the empty frames leave no dot at the centre. A real game should
     *      also save at a quiet moment (between levels, at game over), not mid-play. ---- */
    if (hello_settle) {
        hello_settle--;
        vxtSmartBegin(HELLO_REGION_OFFSET, HELLO_REGION_RECORDS);
        vxtSmartIntensity(0);
        vxtSmartRecenter();
        vxtSmartEnd();
        return;                   /* the marker and the beep start when the screen is back */
    }

    /* ---- compose the frame's SmartList ---- */
    vxtSmartBegin(HELLO_REGION_OFFSET, HELLO_REGION_RECORDS);   /* the bound is the REAL capacity */
    gamelibBeamBegin(HELLO_POS_SCALE, HELLO_DRAW_SCALE);        /* once per frame */
    gamelibBeamSetDrawGain(hello_draw_gain);                    /* the cached calibration: cheap setters, */
    gamelibBeamSetOffset(hello_off_y, hello_off_x);             /* applied every frame (no file I/O here) */
    gamelibBeamPrime(2);          /* the first element of a frame lands slightly off: make it a throwaway */
    hello_square(hello_y, hello_x, HELLO_HALF, HELLO_INTENSITY);
    if (hello_marker) {
        hello_square(hello_y, hello_x, HELLO_INNER, HELLO_INTENSITY);
        hello_marker--;
    }
    vxtSmartRecenter();           /* end every frame homed, then terminate the list */
    vxtSmartEnd();                /* (vxtSmartRecordCount() tells you what a frame cost) */

    /* ---- sound: queue only what CHANGED this frame ---- */
    vxtSoundBegin();
    if (!hello_snd_ready) {
        vxtSoundHardReset();      /* take the AY over from whatever ran before; must be alone in its frame */
        hello_snd_ready = 1;
    } else if (hello_beep == 8) {
        vxtSoundMixer(VXT_MIX_TONE_A, 0);          /* args are register-bit masks: pass VXT_MIX_* straight */
        vxtSoundTone(VXT_CH_A, 300, 12);
        hello_beep--;
    } else if (hello_beep) {
        if (--hello_beep == 0) vxtSoundSilence();
    }
    vxtSoundEnd();
}
