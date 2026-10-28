/*
 * vxcoop_handler.c - VX-COOP: VXT toolkit reference application, STM32 side.
 * Copyright (C) 2026 Caelotronics.
 * GPLv3.
 *
 * See vxcoop_handler.h for the build configuration and docs/VX-COOP/ for the
 * reference guide that derives each technique exercised here. The 6809
 * counterpart is code/app6809/VXCOOP/VXCOOP.asm.
 *
 * This file is organized for reading. Where a production application would
 * factor a computation out, it is kept local and visible, and each non-obvious
 * statement records the constraint it satisfies. The file has no dependency on
 * any application layer.
 *
 * PAGES. Button 4 advances the page; button 3 toggles within a page.
 *   0 LADDER   one-record long draws against chained draws, and the draw-scale
 *              ladder. The record readout is the subject: one picture, two costs.
 *   1 PROJECT  3D-to-2D projection under joystick yaw and pitch.
 *   2 CULL     backface culling on a convex mesh, toggleable, with the
 *              resulting record difference.
 *   3 WINDOW   edge occlusion: per-edge Cyrus-Beck clipping against a convex
 *              octagonal aperture.
 *   4 SOUND    three tone channels and noise, with channel-conflict resolution
 *              by priority.
 *   5 INPUT    the raw 6809-to-STM32 input block, byte by byte.
 *   6 CAL      per-unit calibration: values loaded once at boot, applied every
 *              frame, and written back only on a button press.
 */

#include "vxcoop_handler.h"

#include "../vxt/vxt_smart.h"
#include "../vxt/vxt_smart_text.h"
#include "../vxt/vxt_sound.h"
#include "../vxt/vxt_music.h"
#include "../vxt/vxt_input.h"
#include "../vxt/vxt_cal_load.h"
#include "../gamelib/gamelib_beam.h"
#include "../gamelib/gamelib_proj3d.h"

/* ===== example asset 1 of 3: the music data ================================
 * A three-channel song, converted offline from a .vpy note export by
 * tools/vpy_to_music.py. The source is docs/VX-COOP/music_example.vpy and the
 * generated data is vxcoop_music_example.c, which declares exactly one
 * symbol:
 *
 *     const VxtMusicSong vxcMusic_example;      <-- the data value
 *
 * Regenerate with:
 *     python3 tools/vpy_to_music.py docs/VX-COOP/music_example.vpy \
 *         code/stm32/vxcoop/vxcoop_music_example.c \
 *         code/stm32/vxcoop/vxcoop_music_example.h example vxc
 *
 * Size, as generated: 536 events across the three channels (212 / 201 / 123),
 * 855 frames at 50Hz, approximately 17.1 seconds, looping. Approximately 2.1KB
 * of flash and no RAM beyond the player's own few scalars.
 *
 * The other two example assets are the explosion effect (Section 4, search for
 * Example asset 2) and the stroke font used for on-screen text (Section 6,
 * Example asset 3).
 * ======================================================================== */
#include "vxcoop_music_example.h"

#include <math.h>

/* ===========================================================================
 * SECTION 0 - the two record budgets
 *
 * VXC_REGION_RECORDS is a MEMORY bound. The SmartList begins at
 * VXT_SMART_OFFSET ($0800) and the sound command block begins at
 * VXT_SND_OFFSET ($2000), so ($2000-$0800)/4 = 1536 records fit. Exceeding it
 * causes the draw list to overwrite the sound block.
 *
 * VXC_RECORDS_50HZ is the TIMING bound, and is approximately one third of the
 * memory bound. The two processors are serialized, the 6809 draws at a
 * measured 32.6 to 46.6 cycles per record, and a 50Hz frame allows roughly 20ms
 * of 1.5MHz 6809 time. The product is 510 to 730 records drawable per
 * frame. A frame that fits in memory but not in time produces flicker rather
 * than an incorrect image.
 *
 * Every page displays its live record count, because the platform has no
 * simulator for 6809 draw timing and the budget must therefore be measured.
 * ======================================================================== */
#define VXC_REGION_RECORDS   ((0x2000 - VXT_SMART_OFFSET) / 4)   /* = 1536 */
#define VXC_RECORDS_50HZ     600    /* midpoint of the measured 510-730 band */

/* Confirmed physical screen extents in phys units. The tube is PORTRAIT, so a
 * single half-extent constant serving both axes is incorrect. */
#define VXC_HALF_X           13500
#define VXC_HALF_Y           18000

/* Scale constants.
 *
 * RULE: a draw scale must be an integer multiple of the reposition scale.
 * Repositions land on multiples of VXC_POS_SCALE; a draw lands on multiples of
 * its own scale. Both 64 (2 x 32) and 32 land on the reposition grid, so a
 * drawn vertex and a repositioned vertex coincide. Scale 12 shares only a
 * factor of 4 with 32, so those two grids coincide only every 96 units, and a
 * vertex reached by each route can differ by up to about 22 units. Scale 12
 * remains correct for a self-contained chained figure that nothing else must
 * meet. */
#define VXC_POS_SCALE        0x20   /* 32 - the reposition grid               */
#define VXC_DRAW_SCALE       0x0C   /* 12 - inexpensive chained draws         */
#define VXC_BIG_DRAW_SCALE   0x40   /* 64 - single-record long draws          */

#define VXC_I_DIM            40
#define VXC_I_NORM           70
#define VXC_I_BRIGHT         100

/* ===========================================================================
 * SECTION 1 - page and input state
 *
 * Button semantics depend on whether the action is discrete or continuous, and
 * the two require different reads.
 *
 * A DISCRETE action - advancing a page, flipping a toggle - reads the raw
 * per-button edge byte parm[VXT_IN_BTN1_n] directly, which asserts for exactly
 * one frame per press.
 *
 * A CONTINUOUS action - adjusting a value while a button is held - must instead
 * test the button's bit in the raw VXT_IN_BTNS held bitmask, because the edge
 * byte asserts once and a held button then produces nothing further. Applying
 * the held pattern to a discrete action causes it to fire for every frame of a
 * press, approximately ten frames for a normal tap.
 * ======================================================================== */
#define VXC_PAGE_LADDER   0
#define VXC_PAGE_CUBE     1
#define VXC_PAGE_CULL     2
#define VXC_PAGE_WINDOW   3
#define VXC_PAGE_SOUND    4
#define VXC_PAGE_INPUT    5
#define VXC_PAGE_CAL      6
#define VXC_PAGE_COUNT    7

static int      vxc_page;
static uint32_t vxc_frame;

/* Records consumed by the page's own geometry, captured before any text is
 * emitted. See the readout at the end of Section 7. */
static int      vxc_content_records;

/* Per-page toggles, all driven by button 3 (discrete). */
static int vxc_ladder_chained;    /* 0 = single-record ladder, 1 = chained   */
static int vxc_cull_on = 1;       /* the CULL page's own toggle              */
static int vxc_window_on = 1;

/* On-screen title and hint lines. Button 2 toggles them on every page except
 * SOUND, where button 2 already triggers an effect. They cost approximately 200
 * records, a material fraction of the 50Hz budget, so being able to remove them
 * is both a practical necessity on the geometry pages and the clearest available
 * demonstration of what text costs: switch them off and the T readout drops by
 * approximately that amount. */
static int vxc_text_on = 1;

/* Whether culling is in effect for the frame being composed. DERIVED once per
 * frame from the page and the toggle, rather than by modifying vxc_cull_on.
 * The PROJECT page must display the full wireframe; were it to clear the CULL
 * page's toggle, paging back and forth would discard the user's setting. "What
 * the user selected" and "what this frame does" are distinct quantities and
 * require distinct variables. */
static int vxc_cull_active;

/* View angles in radians, driven by the joystick. */
static float vxc_yaw;
static float vxc_pitch;

/* ===========================================================================
 * SECTION 2 - the mesh: geometry, faces, and edge-to-face adjacency
 *
 * A cube is the appropriate mesh for demonstrating culling because it is CONVEX
 * and CLOSED, which is the necessary and sufficient condition for backface
 * culling to constitute exact hidden-line removal. On a non-convex model - a
 * panel mounted on a strut, a fin on a stalk - a face may be front-facing and
 * nevertheless occluded by another part of the same object, and no test on face
 * normals can detect that case.
 *
 * Each face carries a unit outward normal and d = dot(n, faceCentre), the
 * plane's geometric offset from the model origin. Store the raw d rather than a
 * threshold with the model scale already applied: the two are algebraically
 * equivalent, but a scale-baked threshold becomes invalid whenever a
 * scale constant is retuned.
 *
 * For this unit cube each face center lies one unit along its own normal, so
 * every d equals 1.0. That is a property of the cube and not a general result.
 *
 * A face normal must never be approximated as the direction from the model
 * centroid to the face. That approximation is valid only for a convex mesh
 * centered on its own origin. For a convex mesh no authored face list is
 * required at all: computing the exact convex hull of the vertex list yields
 * faces, outward normals and edge-to-face adjacency, and also serves as an
 * independent check on any face data supplied from elsewhere.
 * ======================================================================== */
#define VXC_CUBE_VERTS  8
#define VXC_CUBE_EDGES  12
#define VXC_CUBE_FACES  6

/* Local coordinates, half-extent 1. The axis names match
 * gamelibProject3DDeep()'s own parameters: X across, V vertical, U depth. */
static const float VXC_CUBE_X[VXC_CUBE_VERTS] = {
    -1.0f, 1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, -1.0f
};
static const float VXC_CUBE_V[VXC_CUBE_VERTS] = {
    -1.0f, -1.0f, 1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f
};
static const float VXC_CUBE_U[VXC_CUBE_VERTS] = {
    -1.0f, -1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f, 1.0f
};

static const uint8_t VXC_CUBE_EDGE[VXC_CUBE_EDGES][2] = {
    {0,1}, {1,2}, {2,3}, {3,0},      /* -U face ring */
    {4,5}, {5,6}, {6,7}, {7,4},      /* +U face ring */
    {0,4}, {1,5}, {2,6}, {3,7}       /* the four connecting edges */
};

/* Face outward normals (x, v, u) and their plane constants d. */
static const float VXC_FACE_NX[VXC_CUBE_FACES] = { 0.0f,  0.0f, 0.0f,  0.0f, -1.0f, 1.0f };
static const float VXC_FACE_NV[VXC_CUBE_FACES] = { 0.0f,  0.0f, -1.0f, 1.0f, 0.0f,  0.0f };
static const float VXC_FACE_NU[VXC_CUBE_FACES] = { -1.0f, 1.0f, 0.0f,  0.0f, 0.0f,  0.0f };
static const float VXC_FACE_D[VXC_CUBE_FACES]  = { 1.0f,  1.0f, 1.0f,  1.0f, 1.0f,  1.0f };

/* The two faces bordering each edge, in the same index order as
 * VXC_CUBE_EDGE. Derived mechanically: every edge of a cube is the
 * intersection of exactly the two faces whose fixed coordinate both endpoints
 * share. Verified by counting the entries rather than by trusting a stated
 * total: 12 edges x 2 = 24 incidences = 6 faces x 4 edges. */
static const uint8_t VXC_EDGE_FACES[VXC_CUBE_EDGES][2] = {
    {0,2}, {0,5}, {0,3}, {0,4},
    {1,2}, {1,5}, {1,3}, {1,4},
    {2,4}, {2,5}, {3,5}, {3,4}
};

/* Placement of the mesh in eye space, and the projection's own tuning. */
#define VXC_CUBE_SCALE    5500.0f
#define VXC_CUBE_POS_U    10000.0f   /* depth ahead of the screen plane      */
#define VXC_FOCAL         17400.0f   /* a tuning value, not a universal one  */

/* The near-plane singularity.
 *
 * gamelibProject3DDeep() computes scale = focal / (focal + u), placing the eye
 * at u = -focal. A vertex at or beyond the eye projects mirrored and greatly
 * magnified, with coordinates in the hundreds of thousands, rather than merely
 * moving off screen. Such coordinates render as long straight strays, and any
 * window or occlusion test applied to them is meaningless. Near-plane clipping
 * must therefore precede every such test.
 *
 * This mesh's nearest vertex lies at VXC_CUBE_POS_U - sqrt(3)*VXC_CUBE_SCALE,
 * approximately +472, so the guard below never triggers here. It is present
 * because the first model with a large depth extent will trigger it, and this
 * is the position in the pipeline at which it belongs. */
#define VXC_NEAR_U        (-VXC_FOCAL + 2000.0f)

/* int32_t rather than int, and the distinction is material: these are passed
 * by address to gamelibProject3DDeep(), whose out-parameters are int32_t*. A
 * mismatched pointer type compiles with a warning and then writes through a
 * pointer of the wrong width. */
static int32_t vxc_vis_y[VXC_CUBE_VERTS];
static int32_t vxc_vis_x[VXC_CUBE_VERTS];
static uint8_t vxc_face_front[VXC_CUBE_FACES];

/* Rotate a local vector by the same yaw-then-pitch transform that
 * gamelibProject3DDeep() applies to a point. The two must agree exactly:
 * culling computed under a different rotation convention from the projection
 * produces a result that is wrong but plausible. Reproduced from
 * gamelib_proj3d.c rather than re-derived. */
static void vxcRotate(float x, float v, float u,
                      float cy, float sy, float cp, float sp,
                      float *ox, float *ov, float *ou)
{
    float x1 = x * cy + u * sy;
    float u1 = -x * sy + u * cy;
    *ov = v * cp - u1 * sp;
    *ou = v * sp + u1 * cp;
    *ox = x1;
}

/* The exact front-facing test.
 *
 *     dot(n_rot, objectPos) + modelScale*d + focal*n_rot.u  <  0
 *
 * The test is perspective, not orthographic. The common shortcut
 * n_rot.u < someSmallConstant ignores the perspective spread, which is
 * approximately 6 degrees for an object of this scale at focal 17400, and
 * therefore leaves faces alive beyond the true silhouette; that error has been
 * measured at 4.63 percent of edge decisions. The exact test costs the same
 * few multiply-adds as the approximation. */
static void vxcCullFaces(float cy, float sy, float cp, float sp,
                         float posX, float posV, float posU)
{
    int i;
    for (i = 0; i < VXC_CUBE_FACES; i++) {
        float rx, rv, ru, t;
        vxcRotate(VXC_FACE_NX[i], VXC_FACE_NV[i], VXC_FACE_NU[i],
                  cy, sy, cp, sp, &rx, &rv, &ru);
        t = rx * posX
          + rv * posV
          + ru * (posU + VXC_FOCAL)
          + VXC_CUBE_SCALE * VXC_FACE_D[i];
        vxc_face_front[i] = (t < 0.0f) ? 1 : 0;
    }
}

/* Edge visibility from face visibility. The complete rule is:
 *
 *     an edge is visible iff EITHER of its two bordering faces is front-facing
 *
 * This handles the silhouette case, in which one bordering face faces forward
 * and the other away, by drawing the edge whole. No partial-edge splitting is
 * required once culling is performed per face rather than per vertex. */
static int vxcEdgeVisible(int e)
{
    if (!vxc_cull_active) return 1;
    return vxc_face_front[VXC_EDGE_FACES[e][0]]
        || vxc_face_front[VXC_EDGE_FACES[e][1]];
}

/* Project all eight vertices. Returns 0 if any vertex lies at or beyond the
 * near plane, in which case the caller must not draw the object at all. */
static int vxcProjectCube(float cy, float sy, float cp, float sp,
                          float posX, float posV, float posU)
{
    int i, ok = 1;
    for (i = 0; i < VXC_CUBE_VERTS; i++) {
        float rx, rv, ru;
        vxcRotate(VXC_CUBE_X[i] * VXC_CUBE_SCALE,
                  VXC_CUBE_V[i] * VXC_CUBE_SCALE,
                  VXC_CUBE_U[i] * VXC_CUBE_SCALE,
                  cy, sy, cp, sp, &rx, &rv, &ru);
        rx += posX;
        rv += posV;
        ru += posU;
        if (ru <= VXC_NEAR_U) ok = 0;
        /* The vertices are already rotated, so project with identity angles.
         * Rotating twice is a silent error. */
        gamelibProject3DDeep(ru, rv, rx, 1.0f, 0.0f, 1.0f, 0.0f,
                             VXC_FOCAL, &vxc_vis_y[i], &vxc_vis_x[i]);
    }
    return ok;
}

/* ===========================================================================
 * SECTION 3 - the aperture: convex-polygon classification and per-edge clipping
 *
 * This is edge occlusion: geometry visible only through an opening in the
 * foreground. It is distinct both from Section 2's backface culling, which
 * removes an object's own hidden half, and from shadow-volume occlusion, which
 * removes other geometry lying behind a solid.
 *
 * The procedure has two stages, because the inexpensive stage resolves most
 * frames:
 *   1. CLASSIFY the object's projected bounding circle against the aperture as
 *      INSIDE (draw unclipped, at no additional cost), CULL (emit nothing), or
 *      STRADDLE.
 *   2. For STRADDLE only, clip each edge individually.
 *
 * A STRADDLE classification does not imply that every edge crosses the
 * boundary. The classification tests a conservative bounding CIRCLE, so objects
 * are routinely classified STRADDLE with no edge crossing. Testing
 * each edge's own endpoints first, and paying the clipping cost only for edges
 * that cross, is therefore necessary; purchasing the expensive path
 * for an entire model in order to correct two edges is a common way to exceed
 * the record budget.
 * ======================================================================== */
#define VXC_WIN_INSIDE    0
#define VXC_WIN_STRADDLE  1
#define VXC_WIN_CULL      2

#define VXC_WIN_NPTS      8
static const int32_t VXC_WIN_X[VXC_WIN_NPTS] = {
     8315,  3444, -3444, -8315, -8315, -3444,  3444,  8315
};
static const int32_t VXC_WIN_Y[VXC_WIN_NPTS] = {
     4210, 10163, 10163,  4210, -4210, -10163, -10163, -4210
};

/* Signed distance from (px,py) to edge i's line, positive towards the interior.
 *
 * The inward normal is derived per edge and then sign-checked against the
 * origin, which lies inside this aperture, rather than assumed from a winding
 * direction. The cost is one additional dot product, and it removes an entire
 * class of sign error. */
static float vxcWinEdgeDist(int i, float px, float py)
{
    int j = (i + 1) % VXC_WIN_NPTS;
    float ax = (float)VXC_WIN_X[i], ay = (float)VXC_WIN_Y[i];
    float bx = (float)VXC_WIN_X[j], by = (float)VXC_WIN_Y[j];
    float nx = -(by - ay), ny = (bx - ax);
    float len;
    if (nx * (0.0f - ax) + ny * (0.0f - ay) < 0.0f) { nx = -nx; ny = -ny; }
    len = sqrtf(nx * nx + ny * ny);
    if (len == 0.0f) return 1.0e30f;
    return ((px - ax) * nx + (py - ay) * ny) / len;
}

/* For a CONVEX aperture: if the center lies at least r inside every edge, the
 * whole disc is inside; if it lies at least r outside any one edge, the whole
 * disc is outside. */
static int vxcWindowClassify(int32_t px, int32_t py, int32_t r)
{
    int i;
    float dmin = 1.0e30f;
    for (i = 0; i < VXC_WIN_NPTS; i++) {
        float d = vxcWinEdgeDist(i, (float)px, (float)py);
        if (d < dmin) dmin = d;
    }
    if (dmin <= -(float)r) return VXC_WIN_CULL;
    if (dmin >= (float)r)  return VXC_WIN_INSIDE;
    return VXC_WIN_STRADDLE;
}

static int vxcPointInWindow(int32_t py, int32_t px)
{
    int i;
    for (i = 0; i < VXC_WIN_NPTS; i++) {
        if (vxcWinEdgeDist(i, (float)px, (float)py) < 0.0f) return 0;
    }
    return 1;
}

/* Cyrus-Beck clip of one segment against the convex aperture. Returns 1 and the
 * clipped endpoints if any part survives, and 0 if the segment lies wholly
 * outside. The inward-normal derivation is identical to the classification
 * above, deliberately so: a single definition of the aperture geometry cannot
 * disagree with itself. */
static int vxcClipToWindow(int32_t y0, int32_t x0, int32_t y1, int32_t x1,
                          int32_t *oy0, int32_t *ox0,
                          int32_t *oy1, int32_t *ox1)
{
    float fx0 = (float)x0, fy0 = (float)y0;
    float dx = (float)(x1 - x0), dy = (float)(y1 - y0);
    float t0 = 0.0f, t1 = 1.0f;
    int i;

    for (i = 0; i < VXC_WIN_NPTS; i++) {
        int j = (i + 1) % VXC_WIN_NPTS;
        float ax = (float)VXC_WIN_X[i], ay = (float)VXC_WIN_Y[i];
        float bx = (float)VXC_WIN_X[j], by = (float)VXC_WIN_Y[j];
        float nx = -(by - ay), ny = (bx - ax);
        float num, den;
        if (nx * (0.0f - ax) + ny * (0.0f - ay) < 0.0f) { nx = -nx; ny = -ny; }
        num = (fx0 - ax) * nx + (fy0 - ay) * ny;   /* inward distance at t=0 */
        den = dx * nx + dy * ny;                   /* its derivative in t    */
        if (den == 0.0f) {
            if (num < 0.0f) return 0;              /* parallel and outside   */
            continue;
        }
        if (den > 0.0f) {                          /* entering this half-plane */
            float t = -num / den;
            if (t > t0) t0 = t;
        } else {                                   /* leaving it              */
            float t = -num / den;
            if (t < t1) t1 = t;
        }
        if (t0 > t1) return 0;
    }

    *ox0 = x0 + (int32_t)(dx * t0);
    *oy0 = y0 + (int32_t)(dy * t0);
    *ox1 = x0 + (int32_t)(dx * t1);
    *oy1 = y0 + (int32_t)(dy * t1);
    return 1;
}

/* ===========================================================================
 * SECTION 4 - SOUND: a three-channel song, an effect, and their conflict
 *
 * The AY-3-8912 provides three independent monophonic tone oscillators, one
 * noise generator routable to any of them, and a SINGLE mixer register
 * governing all six routings. Two consequences follow.
 *
 * First, because the mixer is one register, a statement of the form "channel B
 * now requires noise" cannot be expressed independently; the entire byte is
 * rewritten. Two callers each writing an independent conception of the mixer is
 * the characteristic failure, and its symptom is an effect whose routing is
 * reverted by a later write within the same frame.
 *
 * Second, the AY's registers are latched: once programmed they generate and
 * sustain the sound, freeing the host processor. Only changes should therefore
 * be queued. A frame containing no sound change costs the 6809 approximately 12
 * cycles, because vxt_sound.asm compares a sequence byte and returns.
 * Re-emitting an unchanged tone each frame discards that property entirely.
 *
 * What this page demonstrates. The song occupies all three channels. The
 * explosion effect needs a channel for a noise burst, so it borrows one from
 * the song and returns it afterwards. That is the whole conflict, and it is the
 * conflict every application with music and effects has to resolve.
 *
 * The resolution, in the order the frame performs it:
 *   1. vxtMusicUpdate() runs FIRST. The player performs one unconditional claim
 *      of the whole mixer register on its first call after vxtMusicPlay(), so an
 *      effect starting on that exact frame would otherwise have its correct
 *      noise routing reverted, and the song would then play that
 *      channel's next scheduled tone where the noise should have been.
 *   2. The effect asserts its own channel state AFTER the song, so on a shared
 *      channel the effect wins for the duration of its burst.
 *   3. When the burst ends, the effect restores the mixer to tone-only and calls
 *      vxtMusicReassertChannel(), which snaps the channel straight back to
 *      whatever the song's own timeline currently specifies. Without that call
 *      the channel would sit at the effect's final register values until the
 *      song's next scheduled note for it, which can be many frames later and
 *      reads as that channel falling out of time with the other two.
 * ======================================================================== */

/* ===== example asset 2 of 3: the explosion effect ==========================
 * A noise burst on one borrowed channel with a linear volume decay. No tone and
 * no AY hardware envelope register are involved; the amplitude ramp is computed
 * here, one value per frame.
 *
 * The borrowing contract, which is the point of the page:
 *   - The song owns all three channels and continues to play on all three.
 *   - The explosion takes ONE channel, the SECOND of the three (channel B), and
 *     takes it only for the frames it is sounding. Outside a burst it
 *     holds nothing and the song has all three channels back.
 *   - Channels A and C are never written by the effect, and the mixer
 *     recomputation leaves their tone routing untouched.
 *   - On expiry the channel is re-synced to the song. See the RE-SYNC block
 *     below, which is the half of this that is easy to leave out.
 * A single fixed channel is used rather than whichever channel happens to be
 * quietest: the choice must be predictable, because the re-sync and the mixer
 * recomputation both depend on knowing which channel to restore.
 *
 * The data values are the four constants below. To retune the effect, change
 * these and nothing else:
 *
 *     VXC_BOOM_FRAMES   total duration in frames
 *     VXC_BOOM_NTYPE    the AY 5-bit noise period; lower is brighter/hissier
 *     VXC_BOOM_VOL_MAX  the amplitude ceiling, 0-15
 *     VXC_BOOM_CHAN     which channel is borrowed
 *
 * The decay shape is `vol = framesRemaining >> 1`, clamped to VOL_MAX, so the
 * volume tracks the remaining duration and reaches zero exactly as the burst
 * expires. At 43 frames the ceiling is reached for the first 13 frames and the
 * ramp occupies the remaining 30.
 * ======================================================================== */
#define VXC_BOOM_FRAMES    0x2B   /* 43 frames at 50Hz, approximately 0.86s   */
#define VXC_BOOM_NTYPE     2      /* AY noise period: a deep, cannon-like hiss */
#define VXC_BOOM_VOL_MAX   0x0F   /* lower this to reduce the effect's volume  */
#define VXC_BOOM_CHAN      VXT_CH_B   /* the SECOND of the three channels.
                                       * VXT_CH_A/B/C are 0/1/2, so "channel B"
                                       * and "channel 2" counting from one are
                                       * the same channel; the symbol is used
                                       * rather than a literal to keep that
                                       * unambiguous. */

/* A second, simpler effect: a short tone sweep.
 *
 * It deliberately shares the explosion's channel, and that is a teaching choice
 * rather than normal practice. A real application assigns its effects to
 * different channels precisely so that they never contend - an explosion on the
 * second channel and a shorter effect on the third, for instance, never collide
 * and the priority rule never has to fire. That is the correct arrangement when
 * the channels are available.
 *
 * Two effects placed on one channel is what makes the priority rule observable,
 * and sooner or later an application has more effects than spare channels. With
 * both on the second channel: pressing 2 alone borrows it for a sweep, pressing 1
 * borrows it for an explosion and preempts a running sweep, and pressing 2 during
 * an explosion is REFUSED and counted. */
#define VXC_ZAP_FRAMES      10
#define VXC_ZAP_CHAN        VXT_CH_B   /* the same channel as the explosion */
#define VXC_ZAP_PERIOD_0    300   /* starting AY tone period; it sweeps upward */
#define VXC_ZAP_PERIOD_STEP 40
#define VXC_ZAP_AMP         13

/* Priorities. A request is granted only if it is at least equal to the
 * incumbent's, so a ZAP requested during a BOOM on the same channel is
 * discarded. That is the purpose of holding a priority at all. */
#define VXC_PRIO_MUSIC     0
#define VXC_PRIO_ZAP       1
#define VXC_PRIO_BOOM      2

#define VXC_OWNER_MUSIC    0
#define VXC_OWNER_ZAP      1
#define VXC_OWNER_BOOM     2

static uint8_t vxc_owner[3];        /* which source drives each channel       */
static uint8_t vxc_owner_prio[3];
static uint8_t vxc_owner_frames[3]; /* frames remaining before it is returned */
static int     vxc_snd_phase;       /* 0 reset, 1 start the song, 2 running */
static int     vxc_boom_count;      /* displayed, so a refusal is observable  */
static int     vxc_snd_pairs;       /* pairs emitted last frame, 14 = saturated */
static int     vxc_zap_refused;

/* Request channel `ch` for `owner` at `prio` for `frames`. Returns 1 when
 * granted. */
static int vxcSndRequest(int ch, uint8_t owner, uint8_t prio, uint8_t frames)
{
    if (vxc_owner_frames[ch] != 0 && vxc_owner_prio[ch] > prio) return 0;
    vxc_owner[ch] = owner;
    vxc_owner_prio[ch] = prio;
    vxc_owner_frames[ch] = frames;
    return 1;
}

/* The per-frame pair budget, and why the mixer write below is change-gated.
 *
 * vxtSoundReg() accepts at most VXT_SND_MAX_PAIRS (14) register/value pairs per
 * frame and discards anything beyond that. There is no error and no
 * flag. The costs are: vxtSoundTone() writes 3 pairs (fine period, coarse period,
 * amplitude), vxtSoundMixer() 1, and vxtSoundNoise() 1.
 *
 * The worst case on this page reaches the cap exactly:
 *
 *     song firing events on all three channels      3 x 3 = 9
 *     a channel returned this frame: mixer + reassert   1 + 3 = 4
 *                                                   ---------
 *                                                          13
 *     an effect granted on the same frame: noise + mixer  + 2  = 15
 *
 * The two writes past 14 would be discarded, and the one most likely to be lost
 * is the mixer's noise routing - which is audible as an explosion coming out as a
 * tone rather than noise, precisely the failure the shared-register rule above
 * warns about.
 *
 * Two mitigations are applied. First, the mixer is change-gated below, so a
 * recomputation that yields the same routing costs nothing - the same technique
 * vxtSmartIntensity() uses for the same reason. Second, the page displays the
 * pair count each frame, so saturation is observable rather than silent.
 *
 * If an application needs more, VXT_SND_MAX_PAIRS is the constant to
 * raise: the 6809 service loop terminates on a $FF register byte rather than a
 * count, so a longer block is serviced correctly with no assembly change, at the
 * cost of one Sound_Byte call per additional pair.
 * ======================================================================== */

/* Shadow of the last routing emitted. 0 = not yet valid, which forces
 * the first write of a run. It must be invalidated whenever something OTHER than
 * this function may have written the mixer - which is exactly once per song
 * start, when the player performs its own unconditional claim. */
static uint8_t vxc_mix_tone_shadow;
static uint8_t vxc_mix_noise_shadow;
static int     vxc_mix_valid;

static void vxcSndInvalidateMixer(void) { vxc_mix_valid = 0; }

/* The single point at which the mixer byte is computed, from the ownership of
 * all three channels simultaneously. Tone is enabled on every channel not
 * currently producing noise, and noise only on a channel owned by the explosion.
 *
 * vxtSoundMixer() accepts positive-logic enable masks and performs the
 * inversion, the AY's own mixer logic being inverted so that 0 enables, and it
 * force-clears bit 6. Bit 6 is the I/O port direction: setting it causes the AY
 * to drive the button lines and input reporting to fail. A raw
 * vxtSoundReg(7, x) provides no such protection. */
static void vxcSndEmitMixer(void)
{
    uint8_t tone = 0, noise = 0;
    int ch;
    for (ch = 0; ch < 3; ch++) {
        int boom = (vxc_owner_frames[ch] != 0 && vxc_owner[ch] == VXC_OWNER_BOOM);
        if (boom) noise |= (uint8_t)(VXT_MIX_NOISE_A << ch);
        else      tone  |= (uint8_t)(VXT_MIX_TONE_A << ch);
    }

    /* Change-gate: a recomputation yielding the same routing costs no pair. See
     * the pair-budget block above for why that matters. */
    if (vxc_mix_valid && tone == vxc_mix_tone_shadow
                      && noise == vxc_mix_noise_shadow) {
        return;
    }
    vxc_mix_tone_shadow = tone;
    vxc_mix_noise_shadow = noise;
    vxc_mix_valid = 1;

    /* vxtSoundMixer() accepts positive-logic enable masks and performs the
     * inversion, the AY's own mixer logic being inverted so that 0 enables, and
     * it force-clears bit 6. Bit 6 is the I/O port direction: setting it causes
     * the AY to drive the button lines and input reporting to fail. A raw
     * vxtSoundReg(7, x) provides no such protection. */
    vxtSoundMixer(tone, noise);
}

/* ===========================================================================
 * RE-SYNC: returning a borrowed channel to the song
 *
 * This is the part of channel borrowing that is easy to omit and whose absence
 * is audible, so it is worth setting out precisely what is and is not out of
 * step when an effect finishes.
 *
 * What never desyncs: the song's timeline. vxtMusicUpdate() is called every
 * frame unconditionally, whoever owns which channel. Inside it, each channel's
 * countdown is decremented and any event whose delta has elapsed is fired, which
 * both writes the AY and updates the player's own record of that channel's
 * current note. The sequencer therefore advances through the borrow exactly as
 * it would have done otherwise. The song does not lose its place, and nothing
 * needs rewinding or seeking.
 *
 * What does desync: the channel's hardware registers. While the effect owns the
 * channel it overwrites that channel's amplitude, and in the explosion's case
 * the mixer routes the channel to the noise generator instead of its tone
 * oscillator. When the effect expires, those registers still hold the effect's
 * final values. The player will not correct them, because it writes a channel
 * only when that channel has an event to fire - and the next event may be many
 * frames away. Until then the channel sits silent, or on a stale value, while
 * the other two continue. That is what is heard as one channel falling out of
 * time with the rest, and the further apart the song's events are on that
 * channel, the longer it lasts.
 *
 * THE RE-SYNC, therefore, is not a resynchronisation of time. It is the act of
 * bringing the hardware back into agreement with a timeline that was never
 * wrong. Two writes, in this order:
 *
 *   1. Restore the routing. Recompute the mixer from current ownership, so the
 *      returned channel is routed to its tone oscillator again. The other two
 *      channels are recomputed at the same time and are unaffected, having never
 *      been touched.
 *
 *   2. RE-assert the channel. vxtMusicReassertChannel(ch) writes the period and
 *      amplitude the sequencer currently holds for that channel, which is either
 *      a real note still sounding or a legitimate silence if the note happened
 *      to end during the burst. Either way the channel resumes at the position
 *      the song is at, on the frame the channel comes back, rather than
 *      at its next event.
 *
 * The order matters because the re-assert writes only the tone and amplitude
 * registers and never the mixer: re-emitting a tone into a channel still routed
 * to the noise generator would be inaudible.
 *
 * When no song is playing there is no sequencer to interrogate, and the
 * re-assert is a no-op by design. The channel's amplitude must then be zeroed
 * explicitly, or it holds the effect's final value indefinitely.
 *
 * One further ordering consequence, for the frames DURING a burst rather than at
 * its end: the player keeps writing the borrowed channel whenever the song has
 * an event for it. Those writes are harmless only because the effects assert
 * after the player within the same frame, so the effect's amplitude is the last
 * value written, and because the mixer is routing noise rather than tone on that
 * channel anyway. Reverse the two and the song audibly interrupts the effect.
 * ======================================================================== */

/* Advance one frame of sound. Must be called inside an already-open
 * vxtSoundBegin()/vxtSoundEnd() span: vxtSoundBegin() resets the pending-pair
 * buffer, so a second call within one frame discards whatever the
 * first caller queued. One span per frame is shared by every sound source. */
static void vxcSoundFrame(volatile uint8_t *parm)
{
    int ch;
    int mixer_dirty = 0;
    int expired;          /* bitmask of channels returned this frame */

    /* Start the song once. vxtMusicPlay() does not itself touch the AY; the
     * first vxtMusicUpdate() after it emits frame 0's events. */
    /* A TWO-phase start, and the phases must not be merged.
     *
     * Phase 1 takes ownership of the AY from whatever state the previously
     * running code left it in. Without it the opening of the first piece of audio
     * plays over stale registers - a live amplitude and tone period left by an
     * earlier tune stay audible until the player happens to write that channel,
     * which is heard as a scratch or a held tone and which disappears on a second
     * attempt once the registers have been overwritten.
     *
     * Phase 2 starts the song, on the NEXT frame. vxtSoundHardReset() costs 11 of
     * the 14 register pairs available in a frame and a three-channel song start
     * costs 9 or 10, so performing both in one frame overruns the block and the
     * surplus writes are discarded - which would reintroduce exactly the
     * defect the reset exists to remove. */
    if (vxc_snd_phase == 0) {
        vxtSoundHardReset();
        vxcSndInvalidateMixer();   /* the reset wrote the mixer itself */
        vxc_snd_phase = 1;
        return;                    /* nothing else this frame - see above */
    }
    if (vxc_snd_phase == 1) {
        vxc_snd_phase = 2;
        vxtMusicPlay(&vxcMusic_example);
        /* The player performs its own unconditional mixer claim on the first
         * update after Play(), so this function's shadow no longer describes the
         * register. Invalidate it, or the next recomputation would match the
         * stale shadow and skip a write that is needed. */
        vxcSndInvalidateMixer();
    }

    /* STEP 1 - the song updates FIRST, so that it consumes its one-shot mixer
     * claim before any effect this frame can contend with it. See this
     * section's header. */
    vxtMusicUpdate();

    /* STEP 2 - expire any borrowed channel and RE-SYNC it.
     *
     * Structured as three passes rather than one loop so that the ordering the
     * RE-SYNC block above requires holds even when two channels expire on the
     * same frame: decrement and record what expired, restore the routing once,
     * then re-assert each expired channel. Emitting the mixer inside the loop
     * would write register 7 once per expiring channel, and re-asserting before
     * the routing was restored would write a tone into a channel the mixer still
     * routes to the noise generator. */
    expired = 0;
    for (ch = 0; ch < 3; ch++) {
        if (vxc_owner_frames[ch] != 0) {
            vxc_owner_frames[ch]--;
            if (vxc_owner_frames[ch] == 0) {
                vxc_owner[ch] = VXC_OWNER_MUSIC;
                vxc_owner_prio[ch] = VXC_PRIO_MUSIC;
                expired |= (1 << ch);
            }
        }
    }
    if (expired) {
        /* 2a. Restore the routing, once, recomputed from current ownership. */
        vxcSndEmitMixer();
        /* 2b. THEN RE-SYNC each returned channel. */
        for (ch = 0; ch < 3; ch++) {
            if (!(expired & (1 << ch))) continue;
            if (vxtMusicIsPlaying()) {
                vxtMusicReassertChannel((vxtSndChan)ch);
            } else {
                /* No song, so no sequencer to interrogate: silence the channel
                 * explicitly. vxtMusicReassertChannel() returns without doing
                 * anything when no song is assigned, so omitting this branch
                 * would leave the channel holding the effect's final amplitude
                 * indefinitely. */
                vxtSoundReg((uint8_t)(VXT_AY_AMP_A + ch), 0);
            }
        }
    }

    /* STEP 3 - the effects assert LAST, so that on a shared channel the effect
     * wins for the duration of its burst. Both are discrete one-shots and
     * therefore read the raw edge byte. */
    if (parm[VXT_IN_BTN1_1]) {
        if (vxcSndRequest(VXC_BOOM_CHAN, VXC_OWNER_BOOM, VXC_PRIO_BOOM,
                          VXC_BOOM_FRAMES)) {
            vxc_boom_count++;
            /* The noise PERIOD is emitted once, here, and not in the per-frame
             * ramp below. It does not change for the duration of the burst, and
             * nothing else writes register 6, so re-emitting it every frame would
             * be 42 wasted register pairs - precisely the latched-register rule
             * this section's header states. Only the amplitude is per-frame. */
            vxtSoundNoise(VXC_BOOM_NTYPE);
            mixer_dirty = 1;
        }
    }
    if (parm[VXT_IN_BTN1_2]) {
        if (vxcSndRequest(VXC_ZAP_CHAN, VXC_OWNER_ZAP, VXC_PRIO_ZAP,
                          VXC_ZAP_FRAMES)) {
            mixer_dirty = 1;
        } else {
            /* Refused: outranked by a running explosion on the same channel.
             * This is the priority rule firing, and the counter on screen is
             * what makes it observable rather than merely asserted. */
            vxc_zap_refused++;
        }
    }

    /* The explosion's per-frame amplitude ramp - the decay shape described in
     * the example asset 2 block above. */
    if (vxc_owner_frames[VXC_BOOM_CHAN] != 0
     && vxc_owner[VXC_BOOM_CHAN] == VXC_OWNER_BOOM) {
        uint8_t vol = (uint8_t)(vxc_owner_frames[VXC_BOOM_CHAN] >> 1);
        if (vol > VXC_BOOM_VOL_MAX) vol = VXC_BOOM_VOL_MAX;
        vxtSoundReg((uint8_t)(VXT_AY_AMP_A + VXC_BOOM_CHAN), vol);
    }

    /* The sweep effect, while it holds its channel. */
    if (vxc_owner_frames[VXC_ZAP_CHAN] != 0
     && vxc_owner[VXC_ZAP_CHAN] == VXC_OWNER_ZAP) {
        uint16_t p = (uint16_t)(VXC_ZAP_PERIOD_0
                   + (VXC_ZAP_FRAMES - vxc_owner_frames[VXC_ZAP_CHAN])
                     * VXC_ZAP_PERIOD_STEP);
        vxtSoundTone(VXC_ZAP_CHAN, p, VXC_ZAP_AMP);
    }

    if (mixer_dirty) vxcSndEmitMixer();
}

/* Stop everything. Called when the page is left: the AY latches, so silence has
 * to be requested or the song and any burst in flight continue over the next
 * page. Must be inside an open sound span, exactly like vxcSoundFrame(). */
static void vxcSoundStop(void)
{
    /* Clear ownership BEFORE recomputing the mixer. vxcSndEmitMixer() derives the
     * routing from ownership, so calling it while an explosion was still recorded
     * as the owner would leave the channel routed to the noise generator. That is
     * inaudible here, the amplitudes having just been zeroed, but it would leave
     * the register in a state this page did not intend to leave behind. */
    vxc_owner_frames[0] = vxc_owner_frames[1] = vxc_owner_frames[2] = 0;
    vxc_owner[0] = vxc_owner[1] = vxc_owner[2] = VXC_OWNER_MUSIC;
    vxc_owner_prio[0] = vxc_owner_prio[1] = vxc_owner_prio[2] = VXC_PRIO_MUSIC;

    vxtMusicStop();          /* queues amp=0 on all three tone channels */
    vxtSoundSilence();
    vxcSndInvalidateMixer(); /* force the restore below to emit */
    vxcSndEmitMixer();       /* all three back to tone-only, no noise */
    vxc_snd_phase = 0;       /* re-entry resets the device again: whatever
                              * ran in between may have left it anywhere */
}

/* ===========================================================================
 * SECTION 5 - CALIBRATION: loaded once, applied per frame, saved on a press
 *
 * A calibration value measured on one physical unit describes that unit's own
 * analog behavior. Compiling a fitted constant into firmware fixes one
 * machine's figure permanently, and it is incorrect as soon as the cartridge is
 * moved to another unit. The values are therefore re-derived at each boot from
 * whatever /calmeas.csv is present on the SD card.
 *
 * THE I/O RULE. The 6809 waits inside its RAM-resident stub while the STM32
 * executes the frame handler, so anything that blocks the STM32 blanks the
 * display for its duration. An f_open, f_gets or f_write costs approximately
 * 100ms, which at 50Hz is five lost frames per frame. The symptom is FLICKER
 * rather than an incorrect image, which is why violations of this rule are not
 * evident from the display and survive inspection of the surrounding code.
 *
 *   - LOAD in the one-shot init handler (RPC 79) and cache in a static.
 *   - APPLY the cached value per frame; the setters themselves are inexpensive.
 *   - SAVE only in response to a button press, never on a timer or per frame.
 *
 * Verify this by walking the call graph rather than by reading, and note that a
 * grep confined to one function body is insufficient: see the audit recorded
 * above vxcoop_handler() in Section 7.
 *
 * The loaders' contract. Each loader returns 1 and fills its out-parameter only
 * when usable data was found, and otherwise returns 0 leaving the parameter
 * untouched, so that the caller supplies the fallback. "No data" and "measured
 * zero" must remain distinguishable; substituting another machine's fitted
 * constant is the error the mechanism exists to prevent.
 * ======================================================================== */
static int16_t vxc_cal_gain = 1000;   /* identity until data says otherwise */
static int16_t vxc_cal_off_y;
static int16_t vxc_cal_off_x;
static int8_t  vxc_cal_text_cross;    /* identity (0) until data says otherwise */
static int8_t  vxc_cal_text_along;
static int     vxc_cal_gain_loaded;
static int     vxc_cal_off_loaded;
static int     vxc_cal_save_ok = -1;  /* -1 = not attempted this boot */

/* Joystick-adjusted candidate, so that the page can demonstrate a save. */
static int16_t vxc_cal_candidate = 1000;

static void vxcCalLoadOnce(void)
{
    int16_t g, oy, ox;
    int8_t cross, along;

    /* Declare which file these readings come from before touching a loader.
     * This application is a pure CONSUMER of the standalone rig's own
     * measurements, so it names the rig's file and the rig's CHORD pad.
     * Stated explicitly rather than inherited: the STM32 is not reset when
     * the 6809 changes carts, so whichever cart ran before this one will
     * have left the loaders pointed at ITS file. A game shipping its own Cal
     * screen would name its own file here instead - see vxt_cal_load.h. */
    vxtCalLoadSetSource(VXT_CAL_RIG_FILE, VXT_CAL_RIG_TMP,
                        VXT_CAL_RIG_CHORD_PAD);
    if (vxtCalLoadDrawGain(&g)) { vxc_cal_gain = g; vxc_cal_gain_loaded = 1; }
    if (vxtCalLoadOffset(&oy, &ox)) {
        vxc_cal_off_y = oy; vxc_cal_off_x = ox; vxc_cal_off_loaded = 1;
    }
    if (vxtCalLoadTextComp(&cross, &along)) {
        vxc_cal_text_cross = cross; vxc_cal_text_along = along;
    }
    vxc_cal_candidate = vxc_cal_gain;
}

/* ===========================================================================
 * SECTION 6 - the pages, and the on-screen text
 *
 * ===== example asset 3 of 3: the stroke font ==============================
 *
 * All text on screen is drawn by vxt_smart_text (code/stm32/vxt/), which is a
 * toolkit module rather than an asset of this application. The data value is its
 * glyph table:
 *
 *     glyphs[] in vxt/vxt_smart_text.c     <-- the font data
 *
 * Each glyph is a short list of line-segment strokes on a grid three units wide
 * and five tall. For each stroke the pen performs a blanked move to the stroke's
 * start if it is not already there, then draws to its end. After a glyph's
 * strokes, one blanked move returns the pen to the glyph's own origin and one
 * further move advances it by a fixed glyph width, so the net displacement per
 * character is identical regardless of how many strokes the character required.
 * That invariant is what allows an entire string to be rendered as one
 * continuous pen path from a single recenter, rather than one recenter per
 * character.
 *
 * Text costs records, and the figures are larger than they appear. Each stroke
 * costs a blanked move plus a draw, so two records, and each glyph adds a return
 * to its origin and an advance. Counted against this font's own table, an
 * average upper-case character costs approximately 6 records and a 30-character
 * line approximately 125.
 *
 * The consequences were computed before this page layout was chosen, rather than
 * discovered on hardware:
 *
 *   - A three-line help block of 30-character lines, plus a title and a verbose
 *     readout, measured 641 records. That is at or above the entire 50Hz timing
 *     budget of 510 to 730 records BEFORE any geometry, and would therefore
 *     flicker unconditionally.
 *   - A twelve-line page describing every page and control measured 1501
 *     records, which exceeds even the 1536-record MEMORY bound. A full-screen
 *     help page is not affordable on this platform at any frame rate.
 *
 * What is affordable is a title, two short lines, and a compact readout, which
 * measures 263 to 288 records depending on the page and leaves 222 to 442 for
 * geometry. That is the layout below, and it is why the lines are short.
 *
 * Because 288 records is a material fraction of the budget, the title and the
 * two hint lines are TOGGLEABLE with button 2 on every page except SOUND, where
 * button 2 already triggers an effect. The compact readout is always drawn, so
 * that the cost of the text it accompanies can be observed directly: turn the
 * text off and T falls by approximately 200.
 *
 * The text is drawn AFTER the page's own geometry so that the record counter can
 * report the two separately. See the readout at the end of Section 7.
 * ======================================================================== */

/* Text layout. The confirmed screen extents are +-13500 in X and +-18000 in Y,
 * so these sit inside the usable area with margin. */
#define VXC_TXT_TITLE_Y     17000
#define VXC_TXT_LEFT_X     (-12800)
#define VXC_TXT_HELP_Y1    (-12200)   /* what the page shows                    */
#define VXC_TXT_HELP_Y2    (-14300)   /* which control does what                */
#define VXC_TXT_READOUT_Y  (-16600)

/* Text origins are in reposition-grid units, not physical units.
 *
 * vxtSmartTextBegin() multiplies both arguments by VXT_TEXT_POS_SCALE (32)
 * internally, because it positions the pen through the scale-32 reposition
 * routine. Passing a physical coordinate therefore places the text 32 times too
 * far out: a nominal y of 17000 lands at 544,000 physical units, far outside a
 * screen whose half-height is 18000, and nothing appears at all.
 *
 * These two helpers take PHYSICAL units, so that every call site in this file
 * uses the same units as the geometry around it, and perform the division here
 * once. The division truncates, so a coordinate that is not a whole number of
 * grid steps is rounded down by up to 31 units - immaterial for text placement,
 * but the reason the layout constants below are all multiples of 32 where it
 * matters. */
#define VXC_TXT_GRID(v)   ((int16_t)((v) / VXT_TEXT_POS_SCALE))

static void vxcLabel(int32_t y, int32_t x, const char *s, uint8_t intensity)
{
    vxtSmartTextSetIntensity(intensity);
    vxtSmartTextBegin(VXC_TXT_GRID(y), VXC_TXT_GRID(x));
    vxtSmartTextStr(s);
}

static void vxcValue(int32_t y, int32_t x, const char *s, int32_t v, uint8_t intensity)
{
    vxtSmartTextSetIntensity(intensity);
    vxtSmartTextBegin(VXC_TXT_GRID(y), VXC_TXT_GRID(x));
    vxtSmartTextStr(s);
    vxtSmartTextNumber32(v);
}

/* Per-page text: one line stating what the page shows, one naming its controls.
 * Button 4 advances the page on every page and is therefore not repeated in each
 * entry; it is stated on the LADDER page, which is where the demo starts.
 *
 * The lengths are held down deliberately - see the measured figures above. Every
 * string was costed against the font table before being chosen.
 *
 * The available glyph set is the default font's: upper case, digits, space, and
 * a small number of punctuation characters. Any character outside it draws as a
 * blank advance rather than failing, so these strings stay inside that set by
 * construction. */
static const char *VXC_PAGE_NAME[VXC_PAGE_COUNT] = {
    "LADDER", "PROJECT", "CULL", "WINDOW", "SOUND", "INPUT", "CAL"
};

typedef struct {
    const char *what;    /* what this page demonstrates */
    const char *ctrl;    /* which control does what     */
} VxcHelp;

static const VxcHelp VXC_HELP[VXC_PAGE_COUNT] = {
    /* LADDER  */ { "1 REC VS CHAIN",  "3 SWITCH  4 PAGE" },
    /* PROJECT */ { "3D PROJECTION",   "STICK ROTATE" },
    /* CULL    */ { "BACKFACE CULL",   "3 ON OFF  2 TEXT" },
    /* WINDOW  */ { "APERTURE CLIP",   "3 ON OFF  2 TEXT" },
    /* SOUND   */ { "SONG PLUS SFX",   "1 BOOM 2 SWP SHARE B" },
    /* INPUT   */ { "RAW 6809 INPUT",  "MOVE AND PRESS" },
    /* CAL     */ { "PER UNIT VALUES", "STICK ADJ  1 SAVE" },
};

static void vxcDrawHelp(int page)
{
    vxcLabel(VXC_TXT_TITLE_Y, VXC_TXT_LEFT_X, VXC_PAGE_NAME[page], VXC_I_BRIGHT);
    vxcLabel(VXC_TXT_HELP_Y1, VXC_TXT_LEFT_X, VXC_HELP[page].what, VXC_I_DIM);
    vxcLabel(VXC_TXT_HELP_Y2, VXC_TXT_LEFT_X, VXC_HELP[page].ctrl, VXC_I_DIM);
}

/* --- PAGE 0: the record cost of one picture drawn two ways -----------------
 *
 * Six horizontal lines of increasing length. Button 3 selects the technique.
 *
 * One record (default). gamelibDrawAutoLine() selects the least expensive
 * routine that reaches the line. A single SmartList record draws (dy,dx) as
 * (sy*s, sx*s) with |sy| and |sx| at most 100, so the scale s is constrained
 * only from below; any larger s is equally legal and still occupies one record.
 * The ladder exploits that freedom: scale 32 reaches 3200 units in 67 cycles
 * and scale 64 reaches 6400 in 97. A short edge that would otherwise have paid
 * scale 64 saves 30 cycles at the same single record, because Timer 1 counts
 * down from the SCALE rather than the line's length, so a 2000-unit edge at
 * scale 64 waits exactly as long as a 6400-unit one.
 *
 * CHAINED. The same lines constructed from short scale-12 segments via
 * gamelibChainDelta(). The record count rises accordingly. This is the cost of
 * obtaining precision by adding records, and the reason that practice is
 * avoided: the 6809 is the bottleneck and the STM32 occupies only 7 to 11
 * percent of a frame, so STM32 cycles should be spent and 6809 records
 * conserved.
 *
 * The tiered wait is the underlying 6809 mechanism, implemented in
 * vxt_smart.asm. Each draw routine starts Timer 1 and then waits out the beam
 * ramp, with a pad of (scale-9)/2 NOPs. Scale 12 therefore requires 1 NOP,
 * scale 32 requires 12, and scale 64 requires 27. A separate routine exists per
 * scale rather than one parameterized routine because the pad is fixed at
 * assembly time and cannot know which scale is loaded when it executes.
 * Under-padding is not a rounding error but the positional drift the pad exists
 * to prevent. The wait is the draw: Timer 1 runs for `scale` ticks because that
 * is the beam's sweep duration, so the NOPs are not overhead. For a scale chosen
 * at runtime a fourth option exists: SM_startDrawHuge_d polls the timer flag
 * instead of padding, which is correct at any scale at approximately 2.2 times
 * the dispatch cost. */
static const int32_t VXC_LADDER_LEN[6] = { 800, 1600, 2400, 3200, 4800, 6400 };

static void vxcPageLadder(void)
{
    int i;
    vxcLabel(15600, -11000, vxc_ladder_chained ? "CHAINED" : "ONE RECORD",
             VXC_I_BRIGHT);

    for (i = 0; i < 6; i++) {
        int32_t y = 9000 - (int32_t)i * 3000;
        int32_t x = -6000;
        if (vxc_ladder_chained) {
            /* Reposition, then traverse the length in scale-12 steps. */
            gamelibRepositionAbs(y, x);
            vxtSmartIntensity(VXC_I_NORM);
            vxtSmartScale(VXC_DRAW_SCALE);
            gamelibChainDelta(0, VXC_LADDER_LEN[i], 1);
            /* Close the run into blanked mode. If a render function's final
             * operation is a draw and the following code opens with an
             * expensive multi-record setup, the beam remains lit at that point
             * for the whole setup, dwelling longer than anywhere else in the
             * frame and appearing brighter. One blanked move prevents it. */
            gamelibBeamCloseRun();
        } else {
            gamelibDrawAutoLine(y, x, 0, VXC_LADDER_LEN[i],
                                VXC_I_NORM, VXC_BIG_DRAW_SCALE);
        }
        vxcValue(y + 700, 2400, "", VXC_LADDER_LEN[i], VXC_I_DIM);
    }
}

/* --- PAGES 1 to 3: projection, culling, and edge occlusion ---------------- */
static void vxcDrawCube(int clipToWindow)
{
    int e;
    int32_t actualY = 0, actualX = 0;
    int segCounter = 0;

    vxtSmartIntensity(VXC_I_NORM);

    for (e = 0; e < VXC_CUBE_EDGES; e++) {
        int a = VXC_CUBE_EDGE[e][0], b = VXC_CUBE_EDGE[e][1];
        int32_t y0 = vxc_vis_y[a], x0 = vxc_vis_x[a];
        int32_t y1 = vxc_vis_y[b], x1 = vxc_vis_x[b];

        if (!vxcEdgeVisible(e)) continue;

        if (clipToWindow) {
            /* Per-edge endpoint test first, per Section 3. An edge with both
             * endpoints inside requires no clipping, and most are. */
            if (!vxcPointInWindow(y0, x0) || !vxcPointInWindow(y1, x1)) {
                int32_t cy0, cx0, cy1, cx1;
                if (!vxcClipToWindow(y0, x0, y1, x1, &cy0, &cx0, &cy1, &cx1))
                    continue;                      /* wholly outside */
                y0 = cy0; x0 = cx0; y1 = cy1; x1 = cx1;
                /* A clipped edge no longer begins where the previous one ended
                 * and must therefore be drawn independently; it cannot extend
                 * the chain. That is the cost of clipping, and the reason it is
                 * applied per edge and only where required. */
                gamelibDrawAutoLine(y0, x0, y1 - y0, x1 - x0,
                                    VXC_I_NORM, VXC_BIG_DRAW_SCALE);
                segCounter = 0;
                actualY = y1; actualX = x1;
                continue;
            }
        }

        /* Unclipped edge. Culling has already broken chain adjacency, a culled
         * edge meaning that the next no longer begins where the last ended, so
         * each surviving edge repositions and then chains in bounded groups.
         * A genuine recenter every `groupSize` segments bounds analog
         * integrator drift without incurring a recenter per edge; a group size
         * of 1 is safe and expensive, and 0 forms one unbounded chain in which
         * drift accumulates. */
        if (segCounter == 0 || actualY != y0 || actualX != x0) {
            gamelibRepositionAbs(y0, x0);
            actualY = y0; actualX = x0;
            segCounter = 0;
        }
        vxtSmartScale(VXC_BIG_DRAW_SCALE);
        gamelibChainBigGrouped(y1 - y0, x1 - x0, VXC_BIG_DRAW_SCALE,
                               &actualY, &actualX, &segCounter, 4);
    }
    gamelibBeamCloseRun();
}

static void vxcDrawWindowOutline(void)
{
    int32_t py[VXC_WIN_NPTS], px[VXC_WIN_NPTS];
    int i;
    for (i = 0; i < VXC_WIN_NPTS; i++) { py[i] = VXC_WIN_Y[i]; px[i] = VXC_WIN_X[i]; }
    /* A closed perimeter from a single reposition. Its closing segment targets
     * the position reached rather than the ideal point 0, because
     * per-segment rounding drift accumulates around the loop and the closing
     * segment inherits the total, which appears as a corner that fails to
     * meet. */
    gamelibChainBigClosedPerimeter(py, px, VXC_WIN_NPTS,
                                   VXC_BIG_DRAW_SCALE, VXC_I_DIM);
}

static void vxcPageCube(volatile uint8_t *parm, int page)
{
    float cy, sy, cp, sp;
    int   ok;
    int8_t jx = vxtInJoyX(parm), jy = vxtInJoyY(parm);

    /* The magnitude is meaningful because the 6809 populated these axes via
     * Joy_Analog. The handler is unchanged if the 6809 uses Joy_Digital
     * instead, which reports only -1, 0 or +1; this integrates either. */
    if (jx > 8 || jx < -8) vxc_yaw   += (float)jx * 0.00035f;
    if (jy > 8 || jy < -8) vxc_pitch += (float)jy * 0.00035f;

    cy = cosf(vxc_yaw);   sy = sinf(vxc_yaw);
    cp = cosf(vxc_pitch); sp = sinf(vxc_pitch);

    ok = vxcProjectCube(cy, sy, cp, sp, 0.0f, 0.0f, VXC_CUBE_POS_U);
    vxcCullFaces(cy, sy, cp, sp, 0.0f, 0.0f, VXC_CUBE_POS_U);

    if (page == VXC_PAGE_WINDOW) vxcDrawWindowOutline();

    if (!ok) {
        /* Near-plane guard. Emitting nothing is the correct response: the
         * projected coordinates are invalid, and every test downstream of them
         * would be meaningless. */
        vxcLabel(0, -5000, "NEAR PLANE", VXC_I_BRIGHT);
        return;
    }

    if (page == VXC_PAGE_WINDOW) {
        /* Stage 1: classify the whole object once. A projected bounding radius
         * suffices, and it resolves most frames at no cost. */
        int cls = vxcWindowClassify(0, 0, 7800);
        if (cls == VXC_WIN_CULL) {
            vxcLabel(0, -3000, "CULLED", VXC_I_BRIGHT);
            return;
        }
        vxcDrawCube(vxc_window_on && cls == VXC_WIN_STRADDLE);
    } else {
        vxcDrawCube(0);
    }
}

/* --- PAGE 4: the song, the effects, and the channel they contend for ------
 *
 * The readout is the demonstration. OWN A/B/C name the current owner of each
 * channel (0 = the song, 1 = the sweep, 2 = the explosion), so pressing button 1
 * visibly transfers one channel away from the song for 43 frames and then
 * returns it. BOOM counts granted explosions and REFUSED counts sweep requests
 * rejected for being outranked, which is the priority rule becoming observable
 * rather than merely asserted.
 *
 * LOOP is the song's completed pass count, read from vxtMusicLoopCount() rather
 * than derived from a frame counter: playback advances on real elapsed time, so a
 * count computed from frames diverges whenever the frame rate departs from a
 * stable 50Hz, and on this platform it does. */
static void vxcPageSound(void)
{
    /* Laid out on three lines rather than seven, and with abbreviated labels,
     * because the readable version measured 392 records against 162 for this one.
     * With the title, hint and readout that was a 689-record frame against a
     * 50Hz budget of 510 to 730 - a text-only page can still exceed the budget,
     * and this one nearly did. The owner values are 0 for the song, 1 for the
     * sweep and 2 for the explosion. */
    vxcValue(13000, VXC_TXT_LEFT_X, "A ", (int32_t)vxc_owner[VXT_CH_A], VXC_I_NORM);
    vxcValue(13000, -9000, "B ", (int32_t)vxc_owner[VXT_CH_B], VXC_I_NORM);
    vxcValue(13000, -5000, "C ", (int32_t)vxc_owner[VXT_CH_C], VXC_I_NORM);
    vxcValue(9600, VXC_TXT_LEFT_X, "BOOM ", (int32_t)vxc_boom_count, VXC_I_NORM);
    vxcValue(9600, -4000, "REF ", (int32_t)vxc_zap_refused, VXC_I_NORM);
    vxcValue(6200, VXC_TXT_LEFT_X, "LOOP ", (int32_t)vxtMusicLoopCount(), VXC_I_NORM);
    vxcValue(6200, -5600, "PR ", (int32_t)vxc_snd_pairs, VXC_I_NORM);
}

/* --- PAGE 5: the raw input block ----------------------------------------- */
static void vxcPageInput(volatile uint8_t *parm)
{
    vxcValue(12000, -11000, "JX ", (int32_t)vxtInJoyX(parm), VXC_I_NORM);
    vxcValue(9600, -11000, "JY ", (int32_t)vxtInJoyY(parm), VXC_I_NORM);
    vxcValue(7200, -11000, "B1 ", (int32_t)parm[VXT_IN_BTN1_1], VXC_I_NORM);
    vxcValue(4800, -11000, "B2 ", (int32_t)parm[VXT_IN_BTN1_2], VXC_I_NORM);
    vxcValue(2400, -11000, "B3 ", (int32_t)parm[VXT_IN_BTN1_3], VXC_I_NORM);
    vxcValue(0, -11000, "B4 ", (int32_t)parm[VXT_IN_BTN1_4], VXC_I_NORM);
    vxcValue(-2400, -11000, "HELD ", (int32_t)parm[VXT_IN_BTNS], VXC_I_NORM);
    vxcValue(-4800, -11000, "EDGE ", (int32_t)parm[VXT_IN_EDGE], VXC_I_NORM);
}

/* --- PAGE 6: calibration ------------------------------------------------- */
static void vxcPageCal(volatile uint8_t *parm)
{
    int8_t jy = vxtInJoyY(parm);

    /* A continuous adjustment, and therefore read from the analog axis rather
     * than a button: an axis is inherently a held control and requires no
     * bitmask learning. A button-held adjustment would require the pattern
     * described in Section 1. */
    if (jy > 20 && vxc_cal_candidate < 1300) vxc_cal_candidate++;
    if (jy < -20 && vxc_cal_candidate > 700) vxc_cal_candidate--;

    /* Button 1 commits. One SD write, on an actual press, never per frame. */
    if (parm[VXT_IN_BTN1_1]) {
        vxc_cal_save_ok = vxtCalSaveRow("VXCOOP", 0, 0,
                                        0, 0,
                                        0, 0,
                                        0, 0,
                                        1000);
        /* drawGain is written as 1000, identity, because this row records a raw
         * measurement. A row measured through a correction is not a measurement
         * of the hardware, and every loader inspects this column to decide
         * whether to accept the row. The screen name "VXCOOP" is ignored by
         * every loader, each of which keys on its own screen identifier, so
         * this page cannot corrupt a calibration log. */
    }

    /* The inexpensive setters run per frame; the file reads do not. */
    gamelibBeamSetDrawGain(vxc_cal_candidate);
    gamelibBeamSetOffset(vxc_cal_off_y, vxc_cal_off_x);

    vxcValue(12000, -11000, "GAIN ", (int32_t)vxc_cal_candidate, VXC_I_BRIGHT);
    vxcLabel(9600, -11000, vxc_cal_gain_loaded ? "FROM CARD" : "DEFAULTED",
             VXC_I_NORM);
    vxcValue(7200, -11000, "OFFY ", (int32_t)vxc_cal_off_y, VXC_I_NORM);
    vxcValue(4800, -11000, "OFFX ", (int32_t)vxc_cal_off_x, VXC_I_NORM);
    vxcLabel(2400, -11000, vxc_cal_off_loaded ? "FROM CARD" : "DEFAULTED",
             VXC_I_NORM);
    if (vxc_cal_save_ok == 1) vxcLabel(-1200, -11000, "SAVED", VXC_I_BRIGHT);
    if (vxc_cal_save_ok == 0) vxcLabel(-1200, -11000, "SAVE FAILED", VXC_I_BRIGHT);

    /* A reference cross at nominal (0,0), the figure a center measurement uses:
     * a physical unit's true screen center need not coincide with the DAC's own
     * zero deflection. */
    gamelibDrawAutoLine(0, -1600, 0, 3200, VXC_I_DIM, VXC_BIG_DRAW_SCALE);
    gamelibDrawAutoLine(-1600, 0, 3200, 0, VXC_I_DIM, VXC_BIG_DRAW_SCALE);
}

/* ===========================================================================
 * SECTION 7 - the per-frame handler (RPC 78)
 *
 * This function and everything it calls constitute the frame path.
 *
 * I/O audit for this handler, performed by walking the call graph rather than
 * by grepping this function's body, a body-only grep reporting no hits here
 * because vxcPageCal() lies one level below:
 *
 *   vxcoop_handler -> vxcPageCal -> vxtCalSaveRow    blocking, but gated on a
 *                                                    button edge, which is the
 *                                                    permitted one-shot pattern
 *   no f_open, f_gets, f_read, f_write or vxtCalLoad* reachable
 *
 * Both questions must be asked separately: whether a call is reachable from the
 * frame path, and whether it is gated on a one-shot. Reachable and ungated is
 * the defect.
 *
 * Structure, in which the order is significant:
 *   1. read input from parmRam, which is write-only from the 6809 side: the
 *      6809 cannot read those bytes back, and the STM32 must not attempt to
 *      signal the 6809 through them, STM32-to-6809 data traveling in the
 *      served image
 *   2. advance application state
 *   3. one vxtSoundBegin()/vxtSoundEnd() span
 *   4. one vxtSmartBegin()/vxtSmartEnd() span, bounded by the region's own
 *      capacity and never by the outer full-image bound, since too large a
 *      bound permits one region to overwrite an adjacent one
 * ======================================================================== */
void vxcoop_handler(uint8_t id, volatile uint8_t *parm)
{
    (void)id;

    vxc_frame++;

    /* Discrete actions: page advance and per-page toggle, read directly from
     * the edge bytes. */
    if (parm[VXT_IN_BTN1_4]) vxc_page = (vxc_page + 1) % VXC_PAGE_COUNT;
    if (parm[VXT_IN_BTN1_3]) {
        switch (vxc_page) {
        case VXC_PAGE_LADDER: vxc_ladder_chained = !vxc_ladder_chained; break;
        case VXC_PAGE_CULL:   vxc_cull_on = !vxc_cull_on;               break;
        case VXC_PAGE_WINDOW: vxc_window_on = !vxc_window_on;           break;
        default: break;
        }
    }
    /* Button 2 toggles the on-screen text, except on SOUND where it triggers an
     * effect instead. The pages where that distinction is most likely to matter -
     * CULL and WINDOW, whose geometry competes with the text for the budget, and
     * SOUND, where button 2 does something else entirely - say so in their own
     * hint line. */
    if (parm[VXT_IN_BTN1_2] && vxc_page != VXC_PAGE_SOUND) {
        vxc_text_on = !vxc_text_on;
    }
    /* PROJECT displays the full wireframe; CULL and WINDOW honor the toggle.
     * The two mesh pages therefore differ in exactly one respect, which is what
     * makes the record readout a valid comparison. */
    vxc_cull_active = (vxc_page == VXC_PAGE_CUBE) ? 0 : vxc_cull_on;

    /* ONE sound span per frame, shared by every source. The song and both
     * effects are inside vxcSoundFrame(); nothing else opens a span. */
    vxtSoundBegin();
    if (vxc_page == VXC_PAGE_SOUND) {
        vxcSoundFrame(parm);
    } else if (vxc_snd_phase != 0) {
        /* Leaving the page must silence the song and any burst in flight. The AY
         * latches, so silence is something that has to be requested; omitting
         * this leaves the song audible over every other page. */
        vxcSoundStop();
    }
    /* vxtSoundEnd() returns the number of register pairs written, and the page
     * displays it. A value at VXT_SND_MAX_PAIRS means the frame saturated the
     * block and writes were discarded - see the pair-budget block. */
    vxc_snd_pairs = vxtSoundEnd();   /* a no-op when nothing was queued: the
                                      * sequence byte is left unchanged and the
                                      * 6809 skips servicing in ~12 cycles */

    vxtSmartBegin(VXT_SMART_OFFSET, VXC_REGION_RECORDS);
    gamelibBeamBegin(VXC_POS_SCALE, VXC_DRAW_SCALE);

    /* Re-apply the loaded calibration every frame, on every page - not just
     * while the CAL page happens to be on screen. gb_draw_gain/gb_move_offset
     * are sticky statics that gamelibBeamBegin() does not reset, so this is
     * not defending against a per-frame reset; it is defending against a
     * caller that visits VXC_PAGE_CAL, then leaves, and expects the loaded
     * (not the last-tried-candidate) values to still be in effect elsewhere.
     * Without this, any page other than CAL runs at whatever gain the CAL
     * page's live experimentation last left behind - identity (1000) if CAL
     * was never visited this boot at all - so a drawn edge and the
     * independently-repositioned vertex it must meet disagree by the
     * uncorrected ~4-7% draw-gain error the reference guide's Section 8
     * describes. vxcPageCal() itself overrides this with the live
     * vxc_cal_candidate afterward, further down in this same frame. */
    gamelibBeamSetDrawGain(vxc_cal_gain);
    gamelibBeamSetOffset(vxc_cal_off_y, vxc_cal_off_x);
    /* Same reasoning, for text: vxtSmartTextSetSkewComp() is sticky and
     * nothing else in this file ever called it, so every string drawn here
     * was rendering with zero skew compensation regardless of what
     * /calmeas.csv measured for this unit - visible as text that
     * leans, since the per-character horizontal drift this corrects for is
     * uncorrected without it. */
    vxtSmartTextSetSkewComp(vxc_cal_text_cross, vxc_cal_text_along);

    /* Beam priming. Measured on hardware, the first element drawn in a frame
     * lands significantly displaced, with 2.5 to 4.8 times the mean error of
     * every other item on the same screen. A recenter does not fully zero the
     * analog integrators in one operation, and the residual depends on the
     * beam's prior position, so it does not cancel out of the arithmetic as a
     * constant offset would. A small number of blanked cycles before any real
     * geometry absorbs the effect, at a cost of about 12 records. */
    gamelibBeamPrime(2);

    vxtSmartTextSetScale(VXT_TEXT_SCALE_DEFAULT);

    /* The page's own content is drawn FIRST and its record count captured before
     * any text is emitted, so that the readout can separate the two. Without
     * that separation the fixed cost of the title and help lines would be folded
     * into the figure the LADDER page exists to demonstrate. */
    switch (vxc_page) {
    case VXC_PAGE_LADDER: vxcPageLadder();                    break;
    case VXC_PAGE_CUBE:
    case VXC_PAGE_CULL:
    case VXC_PAGE_WINDOW: vxcPageCube(parm, vxc_page);        break;
    case VXC_PAGE_SOUND:  vxcPageSound();                     break;
    case VXC_PAGE_INPUT:  vxcPageInput(parm);                 break;
    case VXC_PAGE_CAL:    vxcPageCal(parm);                   break;
    default: break;
    }
    vxc_content_records = vxtSmartRecordCount();

    /* The title and the two hint lines together cost approximately 200 records,
     * which is why they are toggleable - see the example asset 3 block. */
    if (vxc_text_on) vxcDrawHelp(vxc_page);

    /* The record readout, and the instrument this whole application is built
     * around. The platform has no simulator for 6809 draw timing, so measurement
     * on the device is the only means of sizing a frame.
     *
     *   R  records consumed by the page's own geometry, captured before any text
     *   T  records for the whole frame, text included
     *
     * The two are reported separately because the text is a fixed overhead of
     * approximately 200 records that would otherwise be folded into the figure
     * the LADDER page exists to demonstrate. R is therefore the number to compare
     * between techniques, and T is the number to compare against the budget. */
    vxcValue(VXC_TXT_READOUT_Y, VXC_TXT_LEFT_X, "R ",
             (int32_t)vxc_content_records, VXC_I_DIM);
    vxcValue(VXC_TXT_READOUT_Y, -4800, "T ",
             (int32_t)vxtSmartRecordCount(), VXC_I_DIM);

    /* Both indicators are single characters, and that is deliberate rather than
     * terse. Spelling the over-budget condition out as a word costs 61 records
     * against 6, so a verbose warning would add materially to the overrun it
     * exists to report - a diagnostic must not perturb its own measurement in the
     * direction of the fault.
     *
     *   '!'  the frame is past the 50Hz timing budget and will flicker
     *   '?'  the list overflowed its MEMORY budget, so geometry was dropped
     *
     * Overflow is survivable, vxtSmartBegin() having reserved the terminator's
     * slot so that the 6809 always walks a properly terminated list, but it still
     * denotes discarded geometry. */
    if (vxtSmartRecordCount() > VXC_RECORDS_50HZ)
        vxcLabel(VXC_TXT_READOUT_Y, 3200, "!", VXC_I_BRIGHT);
    if (vxtSmartOverflowed())
        vxcLabel(VXC_TXT_READOUT_Y, 5600, "?", VXC_I_BRIGHT);

    vxtSmartEnd();
}

/* ===========================================================================
 * SECTION 8 - the one-shot init handler (RPC 79)
 *
 * A 6809 warm reset does not reset the STM32. Without a one-shot fired from the
 * 6809's own startup the STM32 resumes in its prior state: the frame counter
 * continues, the page remains where it was, and the application misreports its
 * own condition.
 *
 * This is also the only function permitted to perform blocking SD reads, per
 * Section 5.
 * ======================================================================== */
void vxcoop_init_handler(uint8_t id, volatile uint8_t *parm)
{
    (void)id;
    (void)parm;

    vxc_frame = 0;
    vxc_page = 0;
    vxc_yaw = 0.30f;
    vxc_pitch = 0.22f;
    vxc_ladder_chained = 0;
    vxc_cull_on = 1;
    vxc_window_on = 1;
    vxc_owner[0] = vxc_owner[1] = vxc_owner[2] = VXC_OWNER_MUSIC;
    vxc_owner_prio[0] = vxc_owner_prio[1] = vxc_owner_prio[2] = VXC_PRIO_MUSIC;
    vxc_owner_frames[0] = vxc_owner_frames[1] = vxc_owner_frames[2] = 0;
    vxc_boom_count = 0;
    vxc_snd_pairs = 0;
    vxc_snd_phase = 0;
    vxc_mix_valid = 0;
    vxc_zap_refused = 0;
    vxc_content_records = 0;
    vxc_text_on = 1;
    vxc_cal_save_ok = -1;

    vxcCalLoadOnce();   /* blocking file I/O, correct here and nowhere else */
}
