#!/usr/bin/env python3
"""
Rigorous OBJ -> VEXTREME cull-data tool.

Deliberately avoids the centroid-direction heuristic that produced a WRONG
answer earlier (it can only detect a majority bias, never prove one face
wrong). Instead:

  1. Merge duplicate-position vertices, drop degenerate faces, fan-triangulate
     any remaining polygons.
  2. Orient every face consistently by BFS propagation across shared edges
     (topological, no geometry assumption at all).
  3. Fix the GLOBAL orientation by SIGNED VOLUME (divergence theorem) - exact
     for any closed mesh, convex or not. Positive volume <=> outward winding.
  4. Validate by RAY CASTING from the real eye position at many yaw angles:
     compare normal-based front-facing classification against actual
     occlusion. This is ground truth.

Axis convention: Blender OBJ export is Z-up; VEXTREME is Y-up.
    game_X = obj_X,  game_Y = -obj_Z,  game_Z = obj_Y
"""
import sys, math
from collections import defaultdict


# ---------- vector helpers ----------
def sub(a, b): return (a[0]-b[0], a[1]-b[1], a[2]-b[2])
def add(a, b): return (a[0]+b[0], a[1]+b[1], a[2]+b[2])
def mul(a, s): return (a[0]*s, a[1]*s, a[2]*s)
def dot(a, b): return a[0]*b[0]+a[1]*b[1]+a[2]*b[2]
def cross(a, b):
    return (a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0])
def length(a): return math.sqrt(dot(a, a))
def norm(a):
    m = length(a)
    return mul(a, 1.0/m) if m > 1e-12 else (0.0, 0.0, 0.0)


# ---------- load ----------
def load_obj(path):
    verts, faces = [], []
    with open(path) as fh:
        for line in fh:
            line = line.strip()
            if line.startswith("v "):
                p = line.split()[1:4]
                verts.append(tuple(float(x) for x in p))
            elif line.startswith("f "):
                idx = [int(tok.split("/")[0]) - 1 for tok in line.split()[1:]]
                faces.append(idx)
    return verts, faces


def to_game(verts):
    """Blender Z-up -> VEXTREME Y-up."""
    return [(v[0], -v[2], v[1]) for v in verts]


# ---------- clean ----------
def merge_and_clean(verts, faces, tol=1e-4):
    """Merge coincident vertices, drop degenerate faces, triangulate."""
    canon, remap = [], []
    for v in verts:
        hit = None
        for j, c in enumerate(canon):
            if abs(v[0]-c[0]) < tol and abs(v[1]-c[1]) < tol and abs(v[2]-c[2]) < tol:
                hit = j
                break
        if hit is None:
            canon.append(v)
            hit = len(canon) - 1
        remap.append(hit)

    tris, dropped = [], 0
    for f in faces:
        nf = []
        for vi in f:
            m = remap[vi]
            if not nf or nf[-1] != m:
                nf.append(m)
        while len(nf) > 1 and nf[0] == nf[-1]:
            nf.pop()
        if len(nf) < 3:
            dropped += 1
            continue
        for k in range(1, len(nf)-1):          # fan-triangulate
            tris.append((nf[0], nf[k], nf[k+1]))
    return canon, tris, remap, dropped


# ---------- orient ----------
def orient_consistently(faces):
    """BFS-propagate a consistent winding across shared edges. Purely
    topological - makes no assumption about shape."""
    faces = [tuple(f) for f in faces]
    edge_faces = defaultdict(list)
    for fi, f in enumerate(faces):
        for i in range(3):
            edge_faces[frozenset((f[i], f[(i+1) % 3]))].append(fi)

    oriented = list(faces)
    visited = set()
    flipped = []
    components = 0

    for seed in range(len(faces)):
        if seed in visited:
            continue
        components += 1
        stack, visited = [seed], visited | {seed}
        while stack:
            fi = stack.pop()
            f = oriented[fi]
            dirs = {(f[i], f[(i+1) % 3]) for i in range(3)}
            for i in range(3):
                key = frozenset((f[i], f[(i+1) % 3]))
                for nb in edge_faces[key]:
                    if nb == fi or nb in visited:
                        continue
                    g = oriented[nb]
                    gdirs = {(g[j], g[(j+1) % 3]) for j in range(3)}
                    # consistent  <=>  shared edge traversed in OPPOSITE dirs
                    if dirs & gdirs:
                        oriented[nb] = (g[0], g[2], g[1])
                        flipped.append(nb)
                    visited.add(nb)
                    stack.append(nb)
    return oriented, flipped, components


def signed_volume(verts, faces):
    """Divergence theorem. >0 <=> faces wound outward (CCW from outside)."""
    tot = 0.0
    for f in faces:
        a, b, c = verts[f[0]], verts[f[1]], verts[f[2]]
        tot += dot(a, cross(b, c))
    return tot / 6.0


def fix_global(verts, faces):
    vol = signed_volume(verts, faces)
    if vol < 0:
        return [(f[0], f[2], f[1]) for f in faces], vol, True
    return faces, vol, False


# ---------- verify ----------
def check_manifold(faces):
    und = defaultdict(int)
    dir_ = defaultdict(int)
    for f in faces:
        for i in range(3):
            a, b = f[i], f[(i+1) % 3]
            und[frozenset((a, b))] += 1
            dir_[(a, b)] += 1
    not2 = {k: v for k, v in und.items() if v != 2}
    same_dir = {k: v for k, v in dir_.items() if v > 1}
    missing_rev = [k for k in dir_ if (k[1], k[0]) not in dir_]
    return not2, same_dir, missing_rev, und


def face_normals(verts, faces):
    out = []
    for f in faces:
        a, b, c = verts[f[0]], verts[f[1]], verts[f[2]]
        out.append(norm(cross(sub(b, a), sub(c, a))))
    return out


# ---------- ray cast ground truth ----------
def ray_tri(orig, d, v0, v1, v2):
    """Moller-Trumbore. Returns t or None."""
    e1, e2 = sub(v1, v0), sub(v2, v0)
    p = cross(d, e2)
    det = dot(e1, p)
    if abs(det) < 1e-9:
        return None
    inv = 1.0/det
    tv = sub(orig, v0)
    u = dot(tv, p) * inv
    if u < -1e-6 or u > 1+1e-6:
        return None
    q = cross(tv, e1)
    v = dot(d, q) * inv
    if v < -1e-6 or u+v > 1+1e-6:
        return None
    t = dot(e2, q) * inv
    return t if t > 1e-6 else None


def rot_y(v, cy, sy):
    """Matches cdfShowcaseRotate(): x' = x*cy + z*sy ; z' = -x*sy + z*cy."""
    return (v[0]*cy + v[2]*sy, v[1], -v[0]*sy + v[2]*cy)


def validate(verts, faces, normals, scale, focal, yaws):
    """Compare normal-based front-face test vs. real occlusion."""
    eye = (0.0, 0.0, -focal)
    total_mismatch = 0
    report = []
    for yaw in yaws:
        r = math.radians(yaw)
        cy, sy = math.cos(r), math.sin(r)
        rv = [rot_y(mul(v, scale), cy, sy) for v in verts]
        rn = [rot_y(n, cy, sy) for n in normals]

        mism = []
        for fi, f in enumerate(faces):
            a, b, c = rv[f[0]], rv[f[1]], rv[f[2]]
            centre = mul(add(add(a, b), c), 1.0/3.0)
            # normal test, orthographic (what the firmware does)
            ortho_front = rn[fi][2] < 0.0
            # ray cast from eye to face centre; any closer hit => occluded
            d = sub(centre, eye)
            occluded = False
            for gj, g in enumerate(faces):
                if gj == fi:
                    continue
                t = ray_tri(eye, d, rv[g[0]], rv[g[1]], rv[g[2]])
                if t is not None and t < 1.0 - 1e-4:
                    occluded = True
                    break
            truly_visible = (not occluded) and ortho_front
            if ortho_front != (not occluded):
                mism.append((fi, ortho_front, not occluded, centre[1]))
        total_mismatch += len(mism)
        report.append((yaw, len(mism), mism))
    return total_mismatch, report


# ---------- emit ----------
def emit_c(prefix, verts, faces, normals, und_edges):
    edges = sorted(tuple(sorted(tuple(k))) for k in und_edges)
    ef = defaultdict(list)
    for fi, f in enumerate(faces):
        for i in range(3):
            ef[tuple(sorted((f[i], f[(i+1) % 3])))].append(fi)

    print(f"\n#define {prefix}_FACE_COUNT  {len(faces)}")
    print(f"#define {prefix}_EDGE_COUNT  {len(edges)}")

    print(f"\nstatic const uint8_t {prefix}_FACES[{prefix}_FACE_COUNT][3] = {{")
    for i in range(0, len(faces), 6):
        print("    " + " ".join("{%d,%d,%d}," % f for f in faces[i:i+6]))
    print("};")

    print(f"\nstatic const float {prefix}_FACE_NX[{prefix}_FACE_COUNT] = {{")
    for i in range(0, len(normals), 8):
        print("    " + " ".join("%.4ff," % n[0] for n in normals[i:i+8]))
    print("};")
    print(f"static const float {prefix}_FACE_NZ[{prefix}_FACE_COUNT] = {{")
    for i in range(0, len(normals), 8):
        print("    " + " ".join("%.4ff," % n[2] for n in normals[i:i+8]))
    print("};")

    print(f"\nstatic const uint8_t {prefix}_EDGES[{prefix}_EDGE_COUNT][2] = {{")
    for i in range(0, len(edges), 8):
        print("    " + " ".join("{%d,%d}," % e for e in edges[i:i+8]))
    print("};")
    print(f"\nstatic const uint8_t {prefix}_EDGE_FACES[{prefix}_EDGE_COUNT][2] = {{")
    for i in range(0, len(edges), 8):
        row = edges[i:i+8]
        print("    " + " ".join("{%d,%d}," % (ef[e][0], ef[e][1]) for e in row))
    print("};")

    print(f"\nstatic const float {prefix}_VERTS[{prefix}_VERT_COUNT][3] = {{")
    for v in verts:
        print("    {%.4ff, %.4ff, %.4ff}," % v)
    print("};")


def run(path, prefix, scale, focal=17400.0, show_c=True):
    print("=" * 72)
    print(f"{path}   ->   {prefix}")
    print("=" * 72)
    raw_v, raw_f = load_obj(path)
    print(f"raw: {len(raw_v)} verts, {len(raw_f)} faces")

    gv = to_game(raw_v)
    verts, tris, remap, dropped = merge_and_clean(gv, raw_f)
    dupes = len(gv) - len(verts)
    print(f"merged: {len(verts)} verts ({dupes} duplicate(s) removed), "
          f"{len(tris)} triangles ({dropped} degenerate face(s) dropped)")

    tris, flipped, comps = orient_consistently(tris)
    print(f"BFS orientation: {len(flipped)} face(s) flipped for consistency, "
          f"{comps} connected component(s)")

    tris, vol, gflip = fix_global(verts, tris)
    print(f"signed volume: {vol:.1f}  -> global flip applied: {gflip}")

    not2, same_dir, missing_rev, und = check_manifold(tris)
    V, E, F = len(verts), len(und), len(tris)
    print(f"manifold check: edges bordered by !=2 faces: {len(not2)}; "
          f"same-direction conflicts: {len(same_dir)}; "
          f"missing reverse: {len(missing_rev)}")
    print(f"Euler V-E+F = {V}-{E}+{F} = {V-E+F}  (2 == closed surface)")

    normals = face_normals(verts, tris)

    yaws = list(range(0, 360, 30))
    nmis, rep = validate(verts, tris, normals, scale, focal, yaws)
    print(f"\nray-cast validation ({len(yaws)} yaw angles, eye at z=-{focal:.0f}, "
          f"scale {scale}):")
    print(f"  total normal-vs-occlusion mismatches: {nmis}")
    for yaw, n, mism in rep:
        if n:
            ys = ", ".join(f"f{fi}(front={a},vis={b},cy={cyy:.0f})"
                           for fi, a, b, cyy in mism[:6])
            print(f"    yaw {yaw:3d}: {n} mismatch(es)  {ys}")
    if nmis == 0:
        print("  PERFECT - normal test agrees with true occlusion at every angle")

    if show_c:
        print(f"\n#define {prefix}_VERT_COUNT  {len(verts)}")
        emit_c(prefix, verts, tris, normals, und)
    return verts, tris, normals


if __name__ == "__main__":
    import argparse
    ap = argparse.ArgumentParser(description="Clean an OBJ mesh, orient its faces, validate the cull data by ray "
                                 "casting, and print the C arrays (see docs/vxe/CONVENTIONS.md, convention 17).")
    ap.add_argument("obj", help="path to a closed-mesh .obj file")
    ap.add_argument("prefix", help="prefix for the generated C names, e.g. ROCK")
    ap.add_argument("scale", type=float, help="the model scale factor your game applies (used by the ray-cast validation)")
    ap.add_argument("--focal", type=float, default=17400.0, help="projection focal length (default 17400)")
    ap.add_argument("--no-c", action="store_true", help="validate only; do not print the C arrays")
    a = ap.parse_args()
    run(a.obj, a.prefix, a.scale, a.focal, not a.no_c)
