/*
 * gamelib_clip.h - screen-window culling and clipping (STM32 side). GPLv3.
 * VXE SDK building block; EXTRACTED 2026-10-25 from a shipped game's cockpit-window code, with every
 * game-specific constant turned into a parameter. Each function was tested
 * against the original on hundreds of thousands of random inputs and matched
 * bit for bit, for both a circular and a polygon window.
 *
 * What it is for. A game that draws things through a window - a cockpit frame, a
 * circular viewport, a screen with a HUD box that must stay clear - needs three
 * questions answered cheaply, in this order:
 *   1. Is this whole object inside, outside, or straddling the window?
 *      (gamelibClassifyCircle: a bounding circle against the window. INSIDE objects
 *      take a fast unclipped path; CULLed ones cost nothing; only STRADDLERS pay for 2.)
 *   2. For a straddler, what part of each edge is visible?
 *      (gamelibClipSegment / gamelibClipSegmentHole)
 *   3. Is this one point inside? (gamelibPointInWindow)
 * and, for 3D, one more question that has to come BEFORE any window test:
 *   0. Does this edge cross the camera's near plane? (gamelibClipEdgeNear) - vertices
 *      behind the eye project mirrored and hugely magnified, so window tests on them
 *      are meaningless (docs/vxe/CONVENTIONS.md, convention 14).
 *
 * WINDOWS. Either a disc centred on the beam origin (0,0), or a CONVEX polygon given
 * as vertex arrays in either winding. A polygon must contain the origin: inward
 * normals are oriented by checking them against the origin, not by assuming a
 * winding (this project was bitten by sign assumptions more than once). If your window
 * is not centred on the origin, subtract its centre from every coordinate you pass in.
 * An optional rectangular HOLE (a HUD box that hides what is behind it) is handled by
 * the *Hole* variants; pass NULL for none.
 *
 * Units are whatever your coordinates are (the original: physical beam units, about +-13500 x
 * +-18000). All arithmetic is single-precision float, as in the original.
 */
#ifndef GAMELIB_CLIP_H
#define GAMELIB_CLIP_H

#include <stdint.h>

/* Results of gamelibClassifyCircle . */
#define GL_CLIP_CULL      0   /* wholly outside (or hidden behind the hole): draw nothing   */
#define GL_CLIP_INSIDE    1   /* wholly inside: draw with no clipping                       */
#define GL_CLIP_STRADDLE  2   /* crosses the window (or the hole's edge): clip each edge    */

/* An axis-aligned rectangle, inclusive: x0 <= x <= x1, ybot <= y <= ytop. */
typedef struct {
    int32_t x0, x1;
    int32_t ybot, ytop;
} GlRect;

/* A window: a disc centred on the origin, or a convex polygon containing the origin. */
#define GL_WINDOW_DISC    0
#define GL_WINDOW_CONVEX  1
typedef struct {
    int kind;                       /* GL_WINDOW_DISC or GL_WINDOW_CONVEX          */
    int32_t radius;                 /* DISC: its radius                            */
    const int32_t *px, *py;         /* CONVEX: the n vertices (any winding)        */
    int n;
} GlWindow;

/* Does the circle (centre px,py; radius r) overlap the rectangle? Returns nonzero if so,
 * and sets *fullyInside to 1 when the rectangle completely covers the circle. */
int gamelibCircleRectOverlap(const GlRect *rect, int32_t px, int32_t py, int32_t r, int *fullyInside);

/* Classify a bounding circle against the window and the optional hole (NULL = none):
 * GL_CLIP_CULL / GL_CLIP_INSIDE / GL_CLIP_STRADDLE. A circle wholly covered by the hole is
 * CULL; one overlapping the hole's edge is at least STRADDLE even if it never touches the
 * window boundary. */
int gamelibClassifyCircle(const GlWindow *w, const GlRect *hole, int32_t px, int32_t py, int32_t r);

/* Is the point (py, px) visible: inside the window and not inside the hole (NULL = none)?
 * NOTE the argument order, y first, as everywhere in this toolkit. */
int gamelibPointInWindow(const GlWindow *w, const GlRect *hole, int32_t py, int32_t px);

/* Clip the segment (y0,x0)-(y1,x1) to the window (Cyrus-Beck for a polygon, a quadratic
 * for a disc). Returns 1 with the clipped endpoints, or 0 if no part is inside. */
int gamelibClipSegment(const GlWindow *w,
                       int32_t y0, int32_t x0, int32_t y1, int32_t x1,
                       int32_t *oy0, int32_t *ox0, int32_t *oy1, int32_t *ox1);

/* The same, minus the hole: the visible part of the segment may be one piece or TWO (the hole
 * can split it). Returns the number of pieces (0, 1 or 2), written to segY0[k] ... segX1[k] for
 * k < the return value; each array needs room for 2. */
int gamelibClipSegmentHole(const GlWindow *w, const GlRect *hole,
                           int32_t y0, int32_t x0, int32_t y1, int32_t x1,
                           int32_t segY0[2], int32_t segX0[2], int32_t segY1[2], int32_t segX1[2]);

/* Clip one 3D edge against the camera's near plane, given its two already-PROJECTED endpoints
 * (ya,xa / yb,xb: screen units from gamelibProject3DDeep or similar) and their camera-space depths
 * (za, zb), the projection's focalLength and the nearPlaneZ to clip at (the original uses -0.75 * focalLength).
 * The eye sits at z = -focalLength, so anything at or beyond it projects mirrored.
 * Returns 0 if both ends are behind the near plane (draw nothing), 1 if both are in front (the
 * output equals the input), 2 if the edge was cut (one output end moved to the plane). The caller
 * adds any camera shake AFTER this call (the original added it inside). */
int gamelibClipEdgeNear(int32_t ya, int32_t xa, float za,
                        int32_t yb, int32_t xb, float zb,
                        float focalLength, float nearPlaneZ,
                        int32_t *oy0, int32_t *ox0, int32_t *oy1, int32_t *ox1);

#endif /* GAMELIB_CLIP_H */
