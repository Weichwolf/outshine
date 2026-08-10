#!/usr/bin/env python3
"""|drawn surface - height oracle| along the local vertical, in metres: the budget, term by term,
and the measured residual beside it.

TWO EVALUATORS STAND BEHIND ONE SURFACE. The BUILDER (world/ChunkMesh.h) projects every node through
geodetic->ECEF in double, subtracts the chunk origin and stores the offset as float32; the vertex
shader adds float32(origin - eye) and the rasteriser interpolates the triangle linearly in ECEF. The
ORACLE (world/ChunkSurface.h, ChunkCellHeight) interpolates the same node heights linearly in
Mercator fraction, in float32. Every term below is a difference between those two paths -- the node
heights are the same float32 values on both sides and cancel. The sum over the envelope is
kSurfaceAgreementM; anything above it is a second interpolant, not arithmetic.

THE MEASUREMENT it is checked against: each `plumb-i-j` scene stands eyeM above the oracle's answer
and looks along -up, so kNearM/depth is the drop to the drawn surface. A plane is fitted over the
centre patch and evaluated at the boresight, which removes the perspective/slope coupling that biases
a plain centre average; the fit rms is the check that the patch really was one triangle. `origin` is
the same scene at the tile centre, where the oracle returns the chunk origin's own node height.

  sim/tools/surface_budget.py <run-dir> [mods-dir]

<run-dir> holds, per place, `<place>-<i>-<j>.log`, `<place>-origin.log` and `<place>/plumb-<i>-<j>.f32`
as `gpu_walk <place> <scene>` writes them.
"""
import json, math, pathlib, re, sys

A, E2 = 6378137.0, 0.00669437999014
Z, CELLS = 14, 128
EARTH_EQ = 40075016.686
DPSI = 2.0 * math.pi / 2 ** 21          # one z14/128 chunk cell, in Mercator radians
NEAR_M = 0.05                           # Renderer's reversed-Z near plane
PLACES = ("preikestolen", "ardeche", "badwater")


def ecef(lat, lon, alt):
    p, l = math.radians(lat), math.radians(lon)
    n = A / math.sqrt(1.0 - E2 * math.sin(p) ** 2)
    return ((n + alt) * math.cos(p) * math.cos(l),
            (n + alt) * math.cos(p) * math.sin(l),
            (n * (1 - E2) + alt) * math.sin(p))


def up(lat, lon):
    p, l = math.radians(lat), math.radians(lon)
    return (math.cos(p) * math.cos(l), math.cos(p) * math.sin(l), math.sin(p))


def half_ulp(v):
    v = abs(v)
    return 0.0 if v == 0 else 2.0 ** (math.floor(math.log2(v)) - 24)


def f32_vertical(d, u):
    """The vertical part of a float32 store of an ECEF vector: each component rounds at its own ULP."""
    return sum(half_ulp(d[k]) * abs(u[k]) for k in range(3))


def merid(lat):
    p = math.radians(lat)
    return A * (1 - E2) / (1 - E2 * math.sin(p) ** 2) ** 1.5


def cell_m(lat):
    return EARTH_EQ * math.cos(math.radians(lat)) / 2 ** Z / CELLS


def tile_of(lat, lon):
    n = 2 ** Z
    return (int((lon + 180.0) / 360.0 * n),
            int((1.0 - math.asinh(math.tan(math.radians(lat))) / math.pi) / 2.0 * n))


def tile_centre(tx, ty):
    n = 2 ** Z
    return (math.degrees(math.atan(math.sinh(math.pi * (1.0 - 2.0 * (ty + 0.5) / n)))),
            (tx + 0.5) / n * 360.0 - 180.0)


TERMS = ("oracleF32", "oracleInner", "vertexF32", "eyeF32", "gpuAdd", "mercator", "chord")


def terms(lat, lon, h, eye_m, slope_deg, h_origin):
    tx, ty = tile_of(lat, lon)
    clat, clon = tile_centre(tx, ty)
    o = ecef(clat, clon, h_origin)
    p = ecef(lat, lon, h)
    e = ecef(lat, lon, h + eye_m)
    u = up(lat, lon)
    d = [p[k] - o[k] for k in range(3)]
    de = [o[k] - e[k] for k in range(3)]
    rel = [p[k] - e[k] for k in range(3)]
    cell = cell_m(lat)
    tan_s = math.tan(math.radians(slope_deg))
    return {
        # the oracle's `ha + (l1*db + l2*dc)` rounds at the corner height's magnitude
        "oracleF32": half_ulp(h),
        # ...and its addend rounds at the cell's own drop
        "oracleInner": half_ulp(cell * tan_s),
        # ChunkVtx.pos: the builder's double ECEF offset, stored float32
        "vertexF32": f32_vertical(d, u),
        # Tile.a = float32(origin - eye), added to every vertex of the tile in the shader
        "eyeF32": f32_vertical(de, u),
        # ...and that sum itself rounds, at the camera-relative distance of the sample
        "gpuAdd": f32_vertical(rel, u),
        # oracle linear in Mercator fraction vs builder linear in ECEF: the second-order part of
        # ground-distance(isometric latitude), turned into height by the slope
        "mercator": (DPSI ** 2 / 8.0) * merid(lat) * tan_s *
                    abs(math.sin(math.radians(lat)) * math.cos(math.radians(lat))),
        # the builder's triangle is a chord plane; the oracle's height is geodetic, i.e. on the arc
        "chord": cell ** 2 / (4.0 * merid(lat)),
    }, math.sqrt(sum(x * x for x in d))


def envelope():
    """The header constant: the same terms at this surface's own limits, anywhere on Earth.

    |h| <= 4096 m; the camera stands in the sampled tile, so both ECEF offsets are under 2048 m (half
    a z14 diagonal is 1729 m at the equator, plus relief); slope <= 80 deg; worst cell 19.10 m."""
    sq3 = math.sqrt(3.0)                      # |u_x|+|u_y|+|u_z| <= sqrt(3) for a unit vector
    tan_s = math.tan(math.radians(80.0))
    cell = cell_m(0.0)
    return {"oracleF32": half_ulp(4096.0),
            "oracleInner": half_ulp(cell * tan_s),
            "vertexF32": sq3 * half_ulp(2048.0),
            "eyeF32": sq3 * half_ulp(2048.0),
            "gpuAdd": sq3 * half_ulp(2048.0),
            "mercator": (DPSI ** 2 / 8.0) * merid(45.0) * 0.5 * tan_s,
            "chord": cell ** 2 / (4.0 * merid(0.0))}


def residual(path, eye_m, fov_deg, patch=8):
    """Drawn surface minus oracle at the boresight, and the slope and fit rms that check the patch."""
    import numpy as np
    d = np.fromfile(path, dtype=np.float32)
    side = int(round(math.sqrt(d.size)))
    d = d.reshape(side, side)
    c, h = side // 2, patch // 2
    sub = d[c - h:c + h, c - h:c + h].astype(np.float64)
    if not np.all(sub > 1e-6):
        return None
    drop = NEAR_M / sub
    t = math.tan(math.radians(fov_deg) / 2.0)
    px = np.arange(c - h, c + h)
    xn = (2.0 * (px + 0.5) / side - 1.0) * t
    yn = (1.0 - 2.0 * (px + 0.5) / side) * t
    m = np.stack([np.ones(drop.size), (drop * xn[None, :]).ravel(),
                  (drop * yn[:, None]).ravel()], axis=1)
    sol, *_ = np.linalg.lstsq(m, drop.ravel(), rcond=None)
    rms = float(np.sqrt(np.mean((m @ sol - drop.ravel()) ** 2)))
    return sol[0] - eye_m, rms, math.degrees(math.atan(math.hypot(sol[1], sol[2])))


def ground_m(log):
    return float(re.search(r"stand groundM=(-?[\d.e+]+)", pathlib.Path(log).read_text()).group(1))


def main(runs, mods):
    runs = pathlib.Path(runs)
    worst, violations = 0.0, 0
    for place in PLACES:
        scenes = {s["id"]: s for s in
                  json.loads((pathlib.Path(mods) / place / "mod.json").read_text())["scenes"]}
        h_origin = ground_m(runs / (place + "-origin.log"))
        print("%s   chunk origin node height %.3f m (measured, scene `origin`)" % (place, h_origin))
        pmax, bmin, rmsmax = 0.0, 1e9, 0.0
        for j in range(4):
            for i in range(4):
                sid = "plumb-%d-%d" % (i, j)
                s = scenes[sid]
                r = residual(str(runs / place / (sid + ".f32")), s["eyeM"], s["fovDeg"])
                if r is None:
                    print("   %-11s UNRESOLVED" % sid)
                    violations += 1
                    continue
                res, rms, slope = r
                h = ground_m(runs / ("%s-%d-%d.log" % (place, i, j)))
                t, off = terms(s["lat"], s["lon"], h, s["eyeM"], slope, h_origin)
                tot = sum(t.values())
                pmax, bmin, rmsmax = max(pmax, abs(res)), min(bmin, tot), max(rmsmax, rms)
                if abs(res) > tot:
                    violations += 1
                print("   %-11s h %9.3f m  slope %5.2f deg  |off| %6.1f m   budget %.3e   "
                      "|resid| %.3e %s" % (sid, h, slope, off, tot, abs(res),
                                           "VIOLATED" if abs(res) > tot else ""))
        print("   worst |resid| %.3e m   tightest budget %.3e m   headroom x%.2f   fit rms max %.1e"
              % (pmax, bmin, bmin / pmax if pmax else 0.0, rmsmax))
        worst = max(worst, pmax)
    e = envelope()
    print("\nenvelope: z14 grid %d, |h| <= 4096 m, camera in the sampled tile, slope <= 80 deg" % CELLS)
    for k in TERMS:
        print("   %-12s %.4e" % (k, e[k]))
    print("   kSurfaceAgreementM = %.4e m   worst measured |resid| %.3e m (x%.1f)   violations %d"
          % (sum(e.values()), worst, sum(e.values()) / worst, violations))
    return 1 if violations else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1], sys.argv[2] if len(sys.argv) > 2 else
                  pathlib.Path(__file__).resolve().parents[2] / "mods"))
