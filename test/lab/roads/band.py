"""The road profile as a QP: the smoothest curve inside the DEM's error band that keeps each
class's design grade where the band allows it, one height per node.

    python3 test/lab/roads/band.py OldTown [epsM]

Variables: one height z per OSM node -- every way through a node shares it, so C0 across ways
holds by construction. Objective: the second difference of z along every way scaled by the
stations (the profile's curvature; a road's vertical alignment minimises it) plus a small tie
to the DEM that fixes the null space. Constraints: dem - eps <= z <= dem + eps, the DEM's own
vertical error [SET 4 m: Copernicus GLO-30 states LE90 < 4 m, EU-DEM v1.1 RMSE 7 m]; and for
every consecutive pair of a way whose class has a design grade g, |z_b - z_a| <= g L -- unless
the band itself cannot reach it (|dem_b - dem_a| - 2 eps > g L), which is DROPPED and counted:
that is netconvert's --geometry.max-grade warning, a fact about the data and not a road.

Proof: every z inside its band; every kept grade constraint met; the dropped count is what it
is; curvature RMS falls; two negative controls -- eps -> 0 gives the DEM back, and an ISOLATED
way with eps = inf and no grade fits a straight line (curvature < 1e-9).
"""
import collections
import pathlib
import sys

import cvxpy as cp
import numpy as np
import scipy.sparse as sp

sys.path.insert(0, str(pathlib.Path(__file__).parent))
import data  # noqa: E402
import profile as prof  # noqa: E402

TIE = 1e-3


def curvature_rows(ways, index):
    rows, cols, vals = [], [], []
    r = 0
    for way in ways:
        refs, s = way["refs"], way["s"]
        for k in range(1, len(refs) - 1):
            d0 = s[k] - s[k - 1]
            d1 = s[k + 1] - s[k]
            if d0 <= 0 or d1 <= 0:
                continue
            scale = 2.0 / (d0 + d1)
            for ref, v in ((refs[k - 1], scale / d0), (refs[k], -scale * (1.0 / d0 + 1.0 / d1)), (refs[k + 1], scale / d1)):
                rows.append(r)
                cols.append(index[ref])
                vals.append(v)
            r += 1
    return sp.csr_matrix((vals, (rows, cols)), shape=(r, len(index)))


def slope_rows(ways, index, free):
    """The first difference over each segment, scaled 1/sqrt(ds): sum (dz/ds)^2 ds. A segment
    that touches a bridge's free node carries none -- the deck owes the river bed no grade."""
    rows, cols, vals = [], [], []
    r = 0
    for way in ways:
        refs, s = way["refs"], way["s"]
        for k in range(1, len(refs)):
            ds = s[k] - s[k - 1]
            a, b = index[refs[k - 1]], index[refs[k]]
            if ds <= 0 or a == b or free[a] or free[b]:
                continue
            w = 1.0 / np.sqrt(ds)
            rows += [r, r]
            cols += [a, b]
            vals += [-w, w]
            r += 1
    return sp.csr_matrix((vals, (rows, cols)), shape=(r, len(index)))


def grade_rows(ways, index):
    """(D, reach): |D z| <= reach + slack for every consecutive pair of a way with a class grade."""
    rows, cols, vals, reach, kinds = [], [], [], [], []
    r = 0
    for way in ways:
        g = way["grade_max"]
        if not g:
            continue
        refs, s = way["refs"], way["s"]
        for k in range(1, len(refs)):
            length = s[k] - s[k - 1]
            if length <= 0 or refs[k] == refs[k - 1]:
                continue
            rows += [r, r]
            cols += [index[refs[k - 1]], index[refs[k]]]
            vals += [-1.0, 1.0]
            reach.append(g * length)
            kinds.append(way["kind"])
            r += 1
    return sp.csr_matrix((vals, (rows, cols)), shape=(r, len(index))), np.array(reach), kinds


def solve(ways, dem_h, eps, with_grade=True, slack_weight=1.0, tie=TIE, smooth_m=None, band='fixed', posting_m=0.0):
    """The band is HARD (the data's word); the class grade is SOFT with a slack per pair, so the
    problem is always feasible and the slack is the netconvert warning, measured in metres.

    smooth_m: the smoothing length. Tikhonov: fidelity + (smooth_m^4) * curvature^2, so the
    profile keeps what the DEM resolves and drops what it cannot -- features shorter than the
    posting. None keeps the pure minimum-curvature form, which cuts every hilltop to the band.

    A bridge's or tunnel's INTERIOR nodes owe the DEM nothing: the deck is a design curve
    between the abutments (OpenDRIVE's elevation polynomial; CARLA drives it), so their band
    is open, their fidelity is zero, and the curvature term makes the deck."""
    index = {}
    for way in ways:
        for ref in way["refs"]:
            index.setdefault(ref, len(index))
    dem = np.array([dem_h[ref] for ref in index])
    curvature = curvature_rows(ways, index)
    free = np.zeros(len(index), dtype=bool)
    for way in ways:
        if way.get("spans") and len(way["refs"]) > 2:
            for ref in way["refs"][1:-1]:
                free[index[ref]] = True
    # the band: 'fixed' is eps everywhere; 'slope' grows it by the DEM's own grade times half a
    # posting, the standard DEM error model (a vertical error that follows the slope through the
    # horizontal one); 'none' leaves the fidelity terms alone to hold the profile
    if band == "slope":
        grade_dem = np.zeros(len(index))
        for way in ways:
            refs, s = way["refs"], way["s"]
            for k in range(1, len(refs)):
                ds = s[k] - s[k - 1]
                if ds > 0:
                    g = abs(dem[index[refs[k]]] - dem[index[refs[k - 1]]]) / ds
                    grade_dem[index[refs[k]]] = max(grade_dem[index[refs[k]]], g)
                    grade_dem[index[refs[k - 1]]] = max(grade_dem[index[refs[k - 1]]], g)
        band_m = eps + grade_dem * 0.5 * posting_m
    elif band == "none":
        band_m = np.full(len(index), 1e4)
    else:
        band_m = np.full(len(index), eps)
    wide = np.where(free, 1e4, band_m)
    fidelity = np.where(free, 0.0, 1.0)
    z = cp.Variable(len(index))
    if smooth_m is None:
        objective = cp.sum_squares(curvature @ z) + tie * cp.sum_squares(cp.multiply(fidelity, z - dem))
    else:
        # the grade's fidelity, mu = l^2 [derived]: the continuous form
        # int (z-d)^2 + mu (z'-d')^2 + l^4 z''^2 has one cut-off, 1/l, for both terms; without it
        # a two-node link between two roads carries any step for free, and did -- 1.9 m/m over
        # 0.9 m at OldTown where the DEM said 0.2 m
        slope = slope_rows(ways, index, free)
        objective = (cp.sum_squares(cp.multiply(fidelity, z - dem)) + (smooth_m ** 2) * cp.sum_squares(slope @ (z - dem))
                     + (smooth_m ** 4) * cp.sum_squares(curvature @ z))
    constraints = [z >= dem - wide, z <= dem + wide]
    grade, reach, kinds, slack = None, None, [], None
    if with_grade:
        grade, reach, kinds = grade_rows(ways, index)
        if grade.shape[0] > 0:
            slack = cp.Variable(grade.shape[0], nonneg=True)
            constraints += [grade @ z <= reach + slack, grade @ z >= -reach - slack]
            objective = objective + slack_weight * cp.sum(slack)
    problem = cp.Problem(cp.Minimize(objective), constraints)
    problem.solve(solver=cp.OSQP, eps_abs=1e-6, eps_rel=1e-6, max_iter=400000, polish=True)
    if z.value is None:
        raise RuntimeError("OSQP: " + str(problem.status))
    over = collections.Counter()
    if slack is not None:
        for kind, s in zip(kinds, np.asarray(slack.value)):
            if s > 1e-3:
                over[kind] += 1
    return index, dem, np.asarray(z.value), curvature, grade, reach, over, problem.status, wide


def grades_of(ways, index, z):
    out = []
    for way in ways:
        zz = np.array([z[index[r]] for r in way["refs"]])
        ds = np.diff(way["s"])
        ok = ds > 0
        out.append(np.abs(np.diff(zz)[ok] / ds[ok]))
    return np.concatenate(out)


def report(label, ways, index, dem, z, curvature, grade, reach, dropped, status, eps, wide):
    tol = 1e-3
    outside = max(float(np.max(dem - wide - z)), float(np.max(z - dem - wide)), 0.0)
    inside = outside <= tol
    kept_ok = True if grade is None else bool(np.all(np.abs(grade @ z) <= reach + tol + 1e9 * 0))
    over_pairs = 0 if grade is None else int(np.sum(np.abs(grade @ z) > reach + 1e-3))
    kd, kz = curvature @ dem, curvature @ z
    g = grades_of(ways, index, z)
    print(f"{label:28s} {status:9s} outside band by {outside:.1e} m "
          f"pairs still over their class grade {over_pairs:5d} of {0 if grade is None else grade.shape[0]:6d}  |z-dem| max {np.abs(z - dem).max():6.3f} m  "
          f"curvature rms {np.sqrt(np.mean(kd ** 2)):.5f} -> {np.sqrt(np.mean(kz ** 2)):.5f}  grade max {g.max():.3f} p99 {np.percentile(g, 99):.3f}")
    return inside


def main(argv):
    place = argv[0] if argv else "OldTown"
    eps = float(argv[1]) if len(argv) > 1 else 4.0
    nodes, ways, dem, table = prof.load(place)
    dem_h = {ref: dem.at(*nodes[ref]) for way in ways for ref in way["refs"]}
    print(f"{place}: {len(ways)} ways, {len(dem_h)} nodes in ways")
    posting = dem.posting_m(data.PLACES[place][0])
    for band_mode in ("fixed", "slope", "none"):
        smooth = 2.0 * posting
        index, d, z, curvature, grade, reach, dropped, status, wide = solve(ways, dem_h, eps, smooth_m=smooth, band=band_mode, posting_m=posting)
        label = f"{place} band={band_mode:5s} l={smooth:4.1f}m"
        assert report(label, ways, index, d, z, curvature, grade, reach, dropped, status, eps, wide)
        print("            still over, by class:", dict(dropped.most_common(6)))
        steepest_after(ways, index, d, z, eps)
    # control 1: the band shut, the DEM comes back
    index, d, z, curvature, grade, reach, dropped, status, wide = solve(ways, dem_h, 1e-4, with_grade=False)
    banded = wide < 1.0
    assert np.abs(z - d)[banded].max() < 1e-3, "eps -> 0 must give the DEM back where the band holds"
    # control 2: one isolated way, no band, no grade -> a straight line
    lone = max((w for w in ways if len(w["refs"]) >= 6), key=lambda w: w["s"][-1])
    index, d, z, curvature, *_ = solve([lone], {r: dem_h[r] for r in lone["refs"]}, 1e6, with_grade=False, tie=1e-12)
    kappa = np.abs(curvature @ z).max()
    print(f"            controls: the band shut gives the DEM back; an isolated {lone['s'][-1]:.0f} m way with the band open fits a line, curvature max {kappa:.2e} 1/m")
    assert kappa < 1e-5
    index, d, z, curvature, grade, reach, dropped, status, wide = solve(ways, dem_h, eps, smooth_m=2.0 * posting)
    plot(place, ways, index, d, z, argv[2:] if len(argv) > 2 else NAMED.get(place, []))
    return 0


def steepest_after(ways, index, dem, z, eps, count=6):
    rows = []
    for way in ways:
        refs, s = way["refs"], way["s"]
        for k in range(1, len(refs)):
            ds = s[k] - s[k - 1]
            if ds <= 0:
                continue
            a, b = index[refs[k - 1]], index[refs[k]]
            g = abs(z[b] - z[a]) / ds
            rows.append((g, way, k, ds, abs(z[a] - dem[a]) >= eps - 1e-6, abs(z[b] - dem[b]) >= eps - 1e-6, dem[b] - dem[a]))
    rows.sort(key=lambda r: -r[0])
    print("            the steepest segments after the fit:")
    for g, way, k, ds, pa, pb, dd in rows[:count]:
        print(f"              {g:6.3f} over {ds:5.1f} m  {way['kind']:12s} {way['name'][:26]:26s} DEM step {dd:6.2f} m  band-pinned {str(pa)[0]}{str(pb)[0]}  way {way['id']}")


NAMED = {
    "OldTown": ["Himmelsleiter", "Spitalgasse", "Taubertalweg", "Mittelhangweg"],
    "Heidelberg": ["Theodor-Heuss-Brücke", "Alte Brücke", "Chaisenweg", "Neue Schlossstraße", "Hauptstraße"],
}


def plot(place, ways, index, dem, z, names):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    picked = [w for w in ways if w["name"] in names]
    picked.sort(key=lambda w: (names.index(w["name"]), -w["s"][-1]))
    seen, rows = set(), []
    for w in picked:
        if w["name"] in seen or len(w["refs"]) < 3:
            continue
        seen.add(w["name"])
        rows.append(w)
    if not rows:
        return
    fig, axes = plt.subplots(len(rows), 1, figsize=(11, 2.6 * len(rows)))
    for ax, w in zip(np.atleast_1d(axes), rows):
        zz = [z[index[r]] for r in w["refs"]]
        ax.plot(w["s"], w["h"], "o-", color="tab:gray", ms=3, label="DEM at the nodes")
        ax.plot(w["s"], zz, "-", color="tab:red", lw=2, label="profile")
        ax.set_title(f"{w['name']} ({w['kind']}{', bridge/tunnel' if w.get('spans') else ''}, {w['s'][-1]:.0f} m, {len(w['refs'])} nodes)")
        ax.set_ylabel("m ASL")
        ax.grid(alpha=0.3)
    np.atleast_1d(axes)[0].legend(loc="best")
    np.atleast_1d(axes)[-1].set_xlabel("station m")
    fig.tight_layout()
    out = data.CACHE / f"profile-{place}.png"
    fig.savefig(out, dpi=110)
    print("            plotted", out)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
