/*
 * gamelib_cull.c - see gamelib_cull.h. GPLv3.
 * EXTRACTED from a shipped game with the focal length as a parameter. Arithmetic kept in
 * the original's order for bit-identical results; verified against the original.
 */
#include "gamelib_cull.h"

void gamelibRotateYawPitch(float x, float y, float z,
                           float cy, float sy, float cp, float sp,
                           float *ox, float *oy, float *oz)
{
    float x1 =  x * cy + z * sy;
    float z1 = -x * sy + z * cy;
    *ox = x1;
    *oy = y * cp - z1 * sp;
    *oz = y * sp + z1 * cp;
}

void gamelibRotateFull(float x, float y, float z,
                       float cy, float sy, float cp, float sp,
                       float cr, float sr,
                       float *ox, float *oy, float *oz)
{
    float rx = x * cr - y * sr;
    float ry = x * sr + y * cr;
    gamelibRotateYawPitch(rx, ry, z, cy, sy, cp, sp, ox, oy, oz);
}

void gamelibCullFaces(const float *fnx, const float *fny, const float *fnz,
                      const float *fd, int nFaces,
                      float posX, float posY, float posZ,
                      float modelScale, float focalLength,
                      float cy, float sy, float cp, float sp,
                      float cr, float sr,
                      uint8_t *visible)
{
    int i;
    for (i = 0; i < nFaces; i++) {
        float rx, ry, rz, t;
        gamelibRotateFull(fnx[i], fny[i], fnz[i], cy, sy, cp, sp, cr, sr, &rx, &ry, &rz);
        t = rx * posX
          + ry * posY
          + rz * (posZ + focalLength)
          + modelScale * fd[i];
        visible[i] = (t < 0.0f);
    }
}
