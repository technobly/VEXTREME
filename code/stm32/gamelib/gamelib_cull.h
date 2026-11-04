/*
 * gamelib_cull.h - rotation and exact perspective BACKFACE culling (STM32 side). GPLv3.
 * VXE SDK building block; EXTRACTED 2026-10-25 from a shipped game, the focal length turned into a parameter.
 * Tested bit for bit against the original functions.
 *
 * What this does and does not do. Backface culling drops the edges of a solid that face away from
 * the camera. It is exactly hidden-line removal only for a CONVEX closed mesh (a cube, a rock, a
 * convex hull). For a non-convex model - wings on struts, fins on stalks, a flat panel visible
 * from both sides - a front-facing face can still be hidden behind another part, and face normals
 * cannot tell; that needs real occlusion testing. Do not claim exactness you cannot deliver
 * (docs/vxe/CONVENTIONS.md, convention 17, has the full story, including the recipe that
 * took an asteroid from 5% wrong edges to 0.00%).
 *
 * The recipe, in short:
 *   1. Per model, once, offline: the faces' outward unit normals fnx/fny/fnz and each face's plane
 *      constant fd = dot(normal, any point on the face). tools/obj_cull_tool.py computes exact
 *      convex hulls (so a convex mesh needs no face export) and validates them by ray casting.
 *      Store the RAW geometric d - never a value with the model's scale folded in: scale constants
 *      get retuned all the time, and a baked threshold goes stale without any error.
 *   2. Per frame: gamelibCullFaces() once per object -> visible[face].
 *   3. Per edge: it is drawn iff EITHER of its two bordering faces is front-facing
 *      (gamelibEdgeVisible) - the closed-manifold rule. Tabulate each edge's two face indices offline.
 *
 * The test is perspective, not orthographic. gamelibProject3DDeep() puts the eye at z = -focalLength,
 * so "normal.z < constant" is wrong; the exact test is
 *        dot(n_rot, objectPos) + modelScale*d + focalLength*n_rot.z  <  0
 * which costs the same handful of multiply-adds.
 *
 * Culling breaks SmartList CHAIN adjacency (a culled edge means the next edge no longer starts
 * where the last one ended), so chained draw paths lose some free continuations. The record count
 * still drops by about half on a convex mesh.
 */
#ifndef GAMELIB_CULL_H
#define GAMELIB_CULL_H

#include <stdint.h>

/* Rotate (x,y,z) by yaw then pitch. cy,sy / cp,sp are the cosines and sines, precomputed once per
 * object. Right-handed: x' = x*cy + z*sy ; z1 = -x*sy + z*cy ; y' = y*cp - z1*sp ;
 * z' = y*sp + z1*cp. */
void gamelibRotateYawPitch(float x, float y, float z,
                           float cy, float sy, float cp, float sp,
                           float *ox, float *oy, float *oz);

/* Roll first (cr,sr: about the view axis), then yaw/pitch as above. */
void gamelibRotateFull(float x, float y, float z,
                       float cy, float sy, float cp, float sp,
                       float cr, float sr,
                       float *ox, float *oy, float *oz);

/* Per-frame face visibility for ONE object. fnx/fny/fnz/fd are the model's nFaces face normals and
 * plane constants (raw d, see above); posX/Y/Z the object's position in CAMERA space (the same space
 * gamelibProject3DDeep() takes); modelScale the object's scale factor; focalLength the projection's
 * (the original: 17400). Writes visible[i] = 1 if face i is front-facing, else 0. */
void gamelibCullFaces(const float *fnx, const float *fny, const float *fnz,
                      const float *fd, int nFaces,
                      float posX, float posY, float posZ,
                      float modelScale, float focalLength,
                      float cy, float sy, float cp, float sp,
                      float cr, float sr,
                      uint8_t *visible);

/* An edge borders exactly two faces of a closed mesh; it is visible iff either is front-facing. */
static inline int gamelibEdgeVisible(const uint8_t *visible, uint8_t faceA, uint8_t faceB)
{
    return visible[faceA] || visible[faceB];
}

#endif /* GAMELIB_CULL_H */
