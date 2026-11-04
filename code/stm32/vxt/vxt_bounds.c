#include "vxt_bounds.h"

int vxtBoundsStep(vxtMovingBody *b, int32_t halfSize, vxtBoundMode mode)
{
    int32_t left, right, top, bottom;

    b->y += b->vy;
    b->x += b->vx;

    left   = -VXT_BOUNDS_HALF_X - halfSize;
    right  =  VXT_BOUNDS_HALF_X + halfSize;
    bottom = -VXT_BOUNDS_HALF_Y - halfSize;
    top    =  VXT_BOUNDS_HALF_Y + halfSize;

    switch (mode) {
    case VXT_BOUND_WRAP:
        /* Wrap on the FAR side (right - 2*(right-left)) rather than just
         * negating, so an object moving fast doesn't visibly "teleport"
         * to the exact opposite edge center - it reappears offset by
         * however far past the edge it had already traveled. */
        if (b->x > right)  b->x = left  + (b->x - right);
        if (b->x < left)   b->x = right - (left  - b->x);
        if (b->y > top)    b->y = bottom + (b->y - top);
        if (b->y < bottom) b->y = top    - (bottom - b->y);
        break;

    case VXT_BOUND_BOUNCE:
        if (b->x > right)  { b->x = right  - (b->x - right);  b->vx = -b->vx; }
        if (b->x < left)   { b->x = left   + (left - b->x);   b->vx = -b->vx; }
        if (b->y > top)    { b->y = top    - (b->y - top);    b->vy = -b->vy; }
        if (b->y < bottom) { b->y = bottom + (bottom - b->y); b->vy = -b->vy; }
        break;

    case VXT_BOUND_VANISH:
        if (b->x > right || b->x < left || b->y > top || b->y < bottom) {
            b->alive = 0;
        }
        break;

    case VXT_BOUND_CLAMP:
        if (b->x > right)  { b->x = right;  b->vx = 0; }
        if (b->x < left)   { b->x = left;   b->vx = 0; }
        if (b->y > top)    { b->y = top;    b->vy = 0; }
        if (b->y < bottom) { b->y = bottom; b->vy = 0; }
        break;
    }
    return b->alive;
}
