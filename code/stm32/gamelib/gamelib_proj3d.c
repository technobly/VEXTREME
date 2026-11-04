/*
 * gamelib_proj3d.c - see gamelib_proj3d.h. Extracted verbatim (math
 * unchanged) from a game's own project3D()/lerpEdge(). Only change:
 * PERSP_D (a file-scope #define in the original) and a hardcoded ±3200
 * box half-extent are now explicit parameters (perspD, halfExtent)
 * instead of baked-in constants, so a different game element's depth
 * range or local coordinate extent doesn't reuse the original's
 * own tuning values.
 */
#include "gamelib_proj3d.h"

void gamelibProject3D(float u, float v, float faceX,
                      float cy, float sy, float cp, float sp,
                      float perspD, int32_t *outY, int32_t *outX)
{
    float x1, z1, y1, y2, z2, x2, scale;

    /* yaw: rotate (X,Z) around Y */
    x1 = faceX * cy + u * sy;
    z1 = -faceX * sy + u * cy;
    y1 = v;

    /* pitch: rotate (Y,Z) around X */
    y2 = y1 * cp - z1 * sp;
    z2 = y1 * sp + z1 * cp;
    x2 = x1;

    scale = 1.0f - z2 / perspD;
    *outX = (int32_t)(x2 * scale);
    *outY = (int32_t)(y2 * scale);
}

int32_t gamelibLerpEdge(int32_t a, int32_t b, int32_t coord, int32_t halfExtent)
{
    return a + (b - a) * (coord + halfExtent) / (2 * halfExtent);
}

/* Added for a second game element - see gamelib_proj3d.h for why this is a
 * separate function rather than a gamelibProject3D() modification. Same
 * yaw/pitch rotation, only the scale term differs (true divide vs. weak
 * perspective's linear term). */
void gamelibProject3DDeep(float u, float v, float faceX,
                          float cy, float sy, float cp, float sp,
                          float focalLength, int32_t *outY, int32_t *outX)
{
    float x1, z1, y1, y2, z2, x2, scale;

    /* yaw: rotate (X,Z) around Y */
    x1 = faceX * cy + u * sy;
    z1 = -faceX * sy + u * cy;
    y1 = v;

    /* pitch: rotate (Y,Z) around X */
    y2 = y1 * cp - z1 * sp;
    z2 = y1 * sp + z1 * cp;
    x2 = x1;

    scale = focalLength / (focalLength + z2);
    *outX = (int32_t)(x2 * scale);
    *outY = (int32_t)(y2 * scale);
}
