/*
 * vxt_bounds.h - VXT toolkit: optional screen-boundary behavior helper.
 *
 * not part of vxt_scene/vxt_smart - this is a small, optional convenience
 * for game logic that moves objects around the screen. The compositor has
 * no concept of velocity or "off-screen"; it only draws whatever positions
 * you give it. This module answers ONE question - "what happens when a
 * moving object crosses the screen edge" - so every game doesn't have to
 * re-derive the same four boundary checks by hand.
 *
 * Screen extents used here are the confirmed values (Chapter 4 sec.4.0.1):
 * X: +-13500, Y: +-18000, phys units, origin at screen center, +Y=up.
 */
#ifndef VXT_BOUNDS_H
#define VXT_BOUNDS_H

#include <stdint.h>

#define VXT_BOUNDS_HALF_X  13500
#define VXT_BOUNDS_HALF_Y  18000

typedef enum {
    VXT_BOUND_WRAP,    /* Asteroids: exits one edge, reappears on the
                        * opposite edge at the same coordinate on the
                        * other axis. */
    VXT_BOUND_BOUNCE,  /* Pong: the velocity component perpendicular to
                        * the edge it hit is negated; position is
                        * reflected back inside the boundary rather than
                        * clipped exactly at it (avoids sticking exactly
                        * on the edge for one frame). */
    VXT_BOUND_VANISH,  /* Bullets/projectiles: once fully off-screen, the
                        * body is marked dead (alive=0) and the caller is
                        * expected to stop drawing/updating it. Position
                        * is not corrected - it's meant to be discarded. */
    VXT_BOUND_CLAMP    /* Player-controlled objects that must never leave
                        * the visible area (e.g. a paddle): position is
                        * clamped to the boundary, velocity zeroed on the
                        * clamped axis (prevents "pushing into the wall"
                        * from re-accumulating speed once released). */
} vxtBoundMode;

typedef struct {
    int32_t y, x;     /* position, phys units, origin at screen center */
    int32_t vy, vx;   /* velocity, phys units/frame */
    int     alive;    /* 1 = still active. VANISH mode clears this; other
                       * modes never touch it - set it yourself at spawn. */
} vxtMovingBody;

/* Advances b->y/b->x by b->vy/b->vx, then applies `mode`'s boundary
 * behavior using an axis-aligned bounding radius `halfSize` (so a body's
 * edge, not just its center point, is what's checked against the screen
 * edge - a size-0 body is checked as a point).
 *
 * Returns b->alive (1 = still active, 0 = should be removed - only ever
 * 0 under VXT_BOUND_VANISH once it's fully exited the screen). */
int vxtBoundsStep(vxtMovingBody *b, int32_t halfSize, vxtBoundMode mode);

#endif /* VXT_BOUNDS_H */
