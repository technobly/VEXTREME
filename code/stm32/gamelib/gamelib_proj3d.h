/*
 * gamelib_proj3d.h - VEXTREME game library: yaw/pitch rotation + weak-
 * perspective projection, and straight-edge linear interpolation.
 *
 * STM32-side only, no shared state, no vxt_smart dependency - pure math.
 * Extracted from a game's radar-style element, where it remains the only
 * caller so far. Known cosmetic left/right curvature asymmetry in the
 * weak-perspective model, unaddressed.
 */
#ifndef GAMELIB_PROJ3D_H
#define GAMELIB_PROJ3D_H

#include <stdint.h>

/* Rotates a rest-orientation point (faceX fixed per "plane"/face, v =
 * vertical, u = depth-within-face) by yaw (around the vertical axis) then
 * pitch (around the horizontal axis), then projects with weak perspective
 * (linear depth scale, NO division). yaw/pitch are passed as PRECOMPUTED
 * sin/cos (cy,sy = cos/sin yaw; cp,sp = cos/sin pitch) so this can be
 * called per-vertex without re-deriving them each time. `perspD` is the
 * weak-perspective reference distance - larger flattens the depth effect,
 * smaller exaggerates it; tune per use, this function has no default. */
void gamelibProject3D(float u, float v, float faceX,
                      float cy, float sy, float cp, float sp,
                      float perspD, int32_t *outY, int32_t *outX);

/* Linear interpolation along a straight chord between two ALREADY-PROJECTED
 * screen points (a, b): maps a local coordinate `coord` in the symmetric
 * range [-halfExtent, +halfExtent] to the corresponding point on that
 * chord. Used to derive intermediate points (e.g. interior line endpoints)
 * that must land exactly ON a straight border already computed via
 * project3D() at its two endpoints, rather than re-projecting through the
 * full (quadratic-along-an-edge) 3D math, which would not agree with a
 * straight chord - see the reference doc's "why lerpEdge, not project3D,
 * for interior points" section. */
int32_t gamelibLerpEdge(int32_t a, int32_t b, int32_t coord, int32_t halfExtent);

/* Same yaw-then-pitch rotation as gamelibProject3D(), but TRUE
 * perspective divide (scale = focalLength/(focalLength+z)) instead of the
 * weak-perspective linear term - for a deep game-scene volume (world
 * depths up to ~200,000 units), where the weak-perspective quadratic term
 * can go negative or non-monotonic. A different function, not a
 * modification of gamelibProject3D() - that one stays exactly as-is for
 * shallow, already-locked elements. `focalLength` is an explicit
 * parameter (no file-scope default), matching gamelibProject3D()'s own
 * perspD convention - ~17,400 is one game's first-pass value, tuned for a
 * 6s/50Hz/200,000-unit closing profile, not a universal constant. */
void gamelibProject3DDeep(float u, float v, float faceX,
                          float cy, float sy, float cp, float sp,
                          float focalLength, int32_t *outY, int32_t *outX);

#endif
