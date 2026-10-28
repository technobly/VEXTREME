/*
 * gamelib_clip.c - see gamelib_clip.h. GPLv3.
 *
 * EXTRACTED from a shipped game's window-clipping code. The arithmetic is kept
 * in exactly the original's order and types so the results are bit-identical
 * (verified against the original); only the constants became parameters. Do not "tidy" the float
 * expressions: a reassociated sum is a different number.
 */
#include <math.h>
#include "gamelib_clip.h"

int gamelibCircleRectOverlap(const GlRect *rect, int32_t px, int32_t py, int32_t r, int *fullyInside)
{
    int32_t clampedX = px, clampedY = py;
    float dx, dy, dist;
    if (clampedX < rect->x0) clampedX = rect->x0;
    if (clampedX > rect->x1) clampedX = rect->x1;
    if (clampedY < rect->ybot) clampedY = rect->ybot;
    if (clampedY > rect->ytop) clampedY = rect->ytop;
    dx = (float)(px - clampedX);
    dy = (float)(py - clampedY);
    dist = sqrtf(dx * dx + dy * dy);
    *fullyInside = 0;
    if (px >= rect->x0 && px <= rect->x1
     && py >= rect->ybot && py <= rect->ytop) {
        /* centre inside the rectangle: fully covered only if every side is at least r away */
        float m = (float)(px - rect->x0);
        float d;
        d = (float)(rect->x1 - px); if (d < m) m = d;
        d = (float)(py - rect->ybot); if (d < m) m = d;
        d = (float)(rect->ytop - py); if (d < m) m = d;
        *fullyInside = (m >= (float)r);
        return 1;
    }
    return (dist < (float)r);
}

int gamelibClassifyCircle(const GlWindow *w, const GlRect *hole, int32_t px, int32_t py, int32_t r)
{
    float dmin;
    int boxOverlap, boxFullyInside;
    if (w->kind == GL_WINDOW_DISC) {
        dmin = (float)w->radius - sqrtf((float)px * (float)px + (float)py * (float)py);
    } else {
        int i;
        dmin = 1.0e30f;
        for (i = 0; i < w->n; i++) {
            int j = (i + 1) % w->n;
            float ax = (float)w->px[i], ay = (float)w->py[i];
            float bx = (float)w->px[j], by = (float)w->py[j];
            float nx = -(by - ay), ny = (bx - ax);   /* a normal to the edge */
            float len, d;
            if (nx * (0.0f - ax) + ny * (0.0f - ay) < 0.0f) { nx = -nx; ny = -ny; }   /* point it at the origin */
            len = sqrtf(nx * nx + ny * ny);
            if (len == 0.0f) continue;
            d = ((float)px - ax) * nx + ((float)py - ay) * ny;   /* +inward */
            d /= len;
            if (d < dmin) dmin = d;
        }
    }
    if (dmin <= -(float)r) return GL_CLIP_CULL;       /* outside the window: the hole is irrelevant */

    if (hole) {
        boxOverlap = gamelibCircleRectOverlap(hole, px, py, r, &boxFullyInside);
        if (boxOverlap) {
            if (boxFullyInside) return GL_CLIP_CULL;       /* hidden behind the hole */
            return GL_CLIP_STRADDLE;                       /* pokes past the hole's edge */
        }
    }

    if (dmin >= (float)r) return GL_CLIP_INSIDE;
    return GL_CLIP_STRADDLE;
}

int gamelibClipSegment(const GlWindow *w,
                       int32_t y0, int32_t x0, int32_t y1, int32_t x1,
                       int32_t *oy0, int32_t *ox0, int32_t *oy1, int32_t *ox1)
{
    float fx0 = (float)x0, fy0 = (float)y0;
    float dx = (float)(x1 - x0), dy = (float)(y1 - y0);
    float t0 = 0.0f, t1 = 1.0f;
    if (w->kind == GL_WINDOW_DISC) {
        float R = (float)w->radius;
        float a = dx * dx + dy * dy;
        float b = 2.0f * (fx0 * dx + fy0 * dy);
        float c = fx0 * fx0 + fy0 * fy0 - R * R;
        float disc = b * b - 4.0f * a * c;
        float sq, tA, tB;
        if (disc < 0.0f) return 0;     /* the line misses the disc */
        sq = sqrtf(disc);
        tA = (-b - sq) / (2.0f * a);
        tB = (-b + sq) / (2.0f * a);
        if (tA > t0) t0 = tA;
        if (tB < t1) t1 = tB;
        if (t0 > t1) return 0;
    } else {
        int i;
        for (i = 0; i < w->n; i++) {
            int j = (i + 1) % w->n;
            float ax = (float)w->px[i], ay = (float)w->py[i];
            float bx = (float)w->px[j], by = (float)w->py[j];
            float nx = -(by - ay), ny = (bx - ax);
            float num, den, t;
            if (nx * (0.0f - ax) + ny * (0.0f - ay) < 0.0f) { nx = -nx; ny = -ny; }
            num = (fx0 - ax) * nx + (fy0 - ay) * ny;   /* >= 0 means P0 is inside this edge */
            den = dx * nx + dy * ny;
            if (den == 0.0f) {
                if (num < 0.0f) return 0;     /* parallel to the edge and outside it */
                continue;
            }
            t = -num / den;
            if (den > 0.0f) { if (t > t0) t0 = t; }    /* entering */
            else            { if (t < t1) t1 = t; }    /* leaving  */
            if (t0 > t1) return 0;
        }
    }
    *ox0 = (int32_t)(fx0 + t0 * dx); *oy0 = (int32_t)(fy0 + t0 * dy);
    *ox1 = (int32_t)(fx0 + t1 * dx); *oy1 = (int32_t)(fy0 + t1 * dy);
    return 1;
}

/* The parameter range [tIn, tOut] over which the segment is INSIDE the rectangle; 0 if never. */
static int gl_segment_rect_range(const GlRect *rect, float x0, float y0, float dx, float dy, float *tIn, float *tOut)
{
    float t0 = 0.0f, t1 = 1.0f;
    if (dx == 0.0f) {
        if (x0 < (float)rect->x0 || x0 > (float)rect->x1) return 0;
    } else {
        float ta = ((float)rect->x0 - x0) / dx;
        float tb = ((float)rect->x1 - x0) / dx;
        if (ta > tb) { float tmp = ta; ta = tb; tb = tmp; }
        if (ta > t0) t0 = ta;
        if (tb < t1) t1 = tb;
        if (t0 > t1) return 0;
    }
    if (dy == 0.0f) {
        if (y0 < (float)rect->ybot || y0 > (float)rect->ytop) return 0;
    } else {
        float ta = ((float)rect->ybot - y0) / dy;
        float tb = ((float)rect->ytop - y0) / dy;
        if (ta > tb) { float tmp = ta; ta = tb; tb = tmp; }
        if (ta > t0) t0 = ta;
        if (tb < t1) t1 = tb;
        if (t0 > t1) return 0;
    }
    *tIn = t0; *tOut = t1;
    return 1;
}

int gamelibClipSegmentHole(const GlWindow *w, const GlRect *hole,
                           int32_t y0, int32_t x0, int32_t y1, int32_t x1,
                           int32_t segY0[2], int32_t segX0[2], int32_t segY1[2], int32_t segX1[2])
{
    int32_t cy0, cx0, cy1, cx1;
    float fx0, fy0, dx, dy, tIn, tOut;
    int n = 0;
    if (!gamelibClipSegment(w, y0, x0, y1, x1, &cy0, &cx0, &cy1, &cx1))
        return 0;
    if (!hole) {
        segY0[0] = cy0; segX0[0] = cx0; segY1[0] = cy1; segX1[0] = cx1;
        return 1;
    }
    fx0 = (float)cx0; fy0 = (float)cy0;
    dx = (float)(cx1 - cx0); dy = (float)(cy1 - cy0);
    if (!gl_segment_rect_range(hole, fx0, fy0, dx, dy, &tIn, &tOut)) {
        segY0[0] = cy0; segX0[0] = cx0; segY1[0] = cy1; segX1[0] = cx1;     /* never meets the hole */
        return 1;
    }
    if (tIn > 0.0f) {                      /* the piece before the hole */
        segY0[n] = cy0; segX0[n] = cx0;
        segY1[n] = (int32_t)(fy0 + tIn * dy); segX1[n] = (int32_t)(fx0 + tIn * dx);
        n++;
    }
    if (tOut < 1.0f) {                     /* the piece after the hole */
        segY0[n] = (int32_t)(fy0 + tOut * dy); segX0[n] = (int32_t)(fx0 + tOut * dx);
        segY1[n] = cy1; segX1[n] = cx1;
        n++;
    }
    return n;
}

int gamelibPointInWindow(const GlWindow *w, const GlRect *hole, int32_t py, int32_t px)
{
    if (hole && px >= hole->x0 && px <= hole->x1
             && py >= hole->ybot && py <= hole->ytop) return 0;       /* inside the hole: hidden */
    if (w->kind == GL_WINDOW_DISC) {
        float dx = (float)px, dy = (float)py;
        return (dx * dx + dy * dy) <= (float)w->radius * (float)w->radius;
    } else {
        int i;
        for (i = 0; i < w->n; i++) {
            int j = (i + 1) % w->n;
            float ax = (float)w->px[i], ay = (float)w->py[i];
            float bx = (float)w->px[j], by = (float)w->py[j];
            float nx = -(by - ay), ny = (bx - ax);
            if (nx * (0.0f - ax) + ny * (0.0f - ay) < 0.0f) { nx = -nx; ny = -ny; }
            if (((float)px - ax) * nx + ((float)py - ay) * ny < 0.0f) return 0;   /* outside this edge */
        }
        return 1;
    }
}

int gamelibClipEdgeNear(int32_t ya, int32_t xa, float za,
                        int32_t yb, int32_t xb, float zb,
                        float focalLength, float nearPlaneZ,
                        int32_t *oy0, int32_t *ox0, int32_t *oy1, int32_t *ox1)
{
    int aIn = (za > nearPlaneZ), bIn = (zb > nearPlaneZ);
    float wxa, wya, wxb, wyb, t, wx, wy, s;
    if (!aIn && !bIn) return 0;
    *oy0 = ya; *ox0 = xa;
    *oy1 = yb; *ox1 = xb;
    if (aIn && bIn) return 1;
    /* Un-project both ends back to camera space, cut at the near plane, re-project the cut point. */
    wxa = (float)xa * (focalLength + za) / focalLength;
    wya = (float)ya * (focalLength + za) / focalLength;
    wxb = (float)xb * (focalLength + zb) / focalLength;
    wyb = (float)yb * (focalLength + zb) / focalLength;
    t = (nearPlaneZ - za) / (zb - za);
    wx = wxa + t * (wxb - wxa);
    wy = wya + t * (wyb - wya);
    s = focalLength / (focalLength + nearPlaneZ);
    if (aIn) { *oy1 = (int32_t)(wy * s); *ox1 = (int32_t)(wx * s); }
    else     { *oy0 = (int32_t)(wy * s); *ox0 = (int32_t)(wx * s); }
    return 2;
}
