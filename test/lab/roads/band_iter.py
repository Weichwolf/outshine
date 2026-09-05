"""The profile as a C++ can compute it: conjugate gradients on the Tikhonov normal equations.

    python3 test/lab/roads/band_iter.py OldTown [epsM]

band.py proves the profile as a QP (OSQP). This is the same problem in the form the engine
will carry: (W + l^4 K^T K) z = W dem, where W is the fidelity weight per node -- 1 for a node
that owes the DEM its height, 0 for a bridge's or tunnel's interior node whose deck is free --
solved by conjugate gradients in a FIXED number of iterations in a DECLARED order (so it is
deterministic), with the band applied by clamping between outer rounds. Where no bound is
active the two are the same problem, and the proof is the difference.

Proof: max |z_cg - z_qp| under 1 cm on every node; the iteration count that reaches it.
"""
import pathlib
import sys
import time

import numpy as np
import scipy.sparse as sp

sys.path.insert(0, str(pathlib.Path(__file__).parent))
import band  # noqa: E402
import data  # noqa: E402
import profile as prof  # noqa: E402


def normal_system(ways, index, dem, smooth_m):
    n = len(index)
    curvature = band.curvature_rows(ways, index)
    fidelity = np.ones(n)
    for way in ways:
        if way.get("spans") and len(way["refs"]) > 2:
            for ref in way["refs"][1:-1]:
                fidelity[index[ref]] = 0.0
    free = fidelity == 0.0
    slope = band.slope_rows(ways, index, free)
    a = sp.diags(fidelity) + (smooth_m ** 2) * (slope.T @ slope) + (smooth_m ** 4) * (curvature.T @ curvature)
    b = fidelity * dem + (smooth_m ** 2) * (slope.T @ (slope @ dem))
    return a.tocsr(), b, fidelity


def pcg(a, b, x0, iterations):
    """Jacobi-preconditioned conjugate gradients, fixed iteration count, declared order."""
    inv = 1.0 / a.diagonal()
    x = x0.copy()
    r = b - a @ x
    zv = inv * r
    p = zv.copy()
    rz = float(r @ zv)
    for _ in range(iterations):
        ap = a @ p
        pap = float(p @ ap)
        if pap <= 0.0:
            break
        alpha = rz / pap
        x += alpha * p
        r -= alpha * ap
        zv = inv * r
        rz_next = float(r @ zv)
        if rz_next <= 0.0:
            break
        p = zv + (rz_next / rz) * p
        rz = rz_next
    return x


def direct(a, b, dem, fidelity, eps, rounds=16):
    """A sparse direct solve of the normal equations (what Eigen's SparseLU does in C++), then
    the band by an ACTIVE SET: a node the solve puts outside its band is pinned to the band's
    edge (its row becomes an identity row) and the system solved again; a pinned node whose
    residual gradient points back inside the band is RELEASED -- without the release the set
    can settle on a point that is not the QP's (measured: 0.74 m off at OldTown). At most
    `rounds` rounds. With no bound active the first solve IS the QP's KKT point."""
    from scipy.sparse.linalg import spsolve
    n = len(dem)
    pinned = np.full(n, np.nan)
    z = dem.copy()
    for round_ in range(rounds):
        a_r = a.tolil(copy=True)
        b_r = b.copy()
        held = ~np.isnan(pinned)
        for k in np.where(held)[0]:
            a_r.rows[k] = [k]
            a_r.data[k] = [1.0]
            b_r[k] = pinned[k]
        z = spsolve(a_r.tocsc(), b_r)
        gradient = a @ z - b
        low, high = dem - eps, dem + eps
        outside = (fidelity > 0) & ((z < low - 1e-9) | (z > high + 1e-9)) & ~held
        release = held & (((pinned <= low + 1e-9) & (gradient < -1e-9)) | ((pinned >= high - 1e-9) & (gradient > 1e-9)))
        if not outside.any() and not release.any():
            return z, round_ + 1, int(held.sum())
        pinned[outside] = np.clip(z[outside], low[outside], high[outside])
        pinned[release] = np.nan
    return z, rounds, int((~np.isnan(pinned)).sum())


def main(argv):
    place = argv[0] if argv else "OldTown"
    eps = float(argv[1]) if len(argv) > 1 else 4.0
    nodes, ways, dem, table = prof.load(place)
    dem_h = {ref: dem.at(*nodes[ref]) for way in ways for ref in way["refs"]}
    posting = dem.posting_m(data.PLACES[place][0])
    smooth = 2.0 * posting
    t0 = time.time()
    index, d, z_qp, curvature, grade, reach, over, status, wide = band.solve(ways, dem_h, eps, with_grade=False, smooth_m=smooth)
    t_qp = time.time() - t0
    a, b, fidelity = normal_system(ways, index, d, smooth)
    print(f"{place}: {len(index)} nodes, smoothing length {smooth:.1f} m, OSQP {t_qp:.1f} s, bounds active on {int(np.sum(np.abs(z_qp - d) > eps - 1e-3))} nodes")
    t0 = time.time()
    z, rounds, pinned = direct(a, b, d, fidelity, eps)
    worst = float(np.abs(z - z_qp).max())
    print(f"   direct solve: {rounds} round(s), {pinned} node(s) pinned to the band, max |z - z_qp| {worst:.5f} m, rms {np.sqrt(np.mean((z - z_qp) ** 2)):.6f} m ({time.time() - t0:.2f} s)")
    proved = worst < 0.01
    print(f"   {'PROOF' if proved else 'NOT PROVED'}: the direct solve {'stands' if proved else 'does not stand'} within 1 cm of the QP at {place}")
    for iterations in (32, 64, 128, 256, 512, 1024):
        t0 = time.time()
        x = pcg(a, b, d, iterations)
        x = np.where(fidelity > 0, np.clip(x, d - eps, d + eps), x)
        w = float(np.abs(x - z_qp).max())
        print(f"   PCG {iterations:5d} iterations: max |z - z_qp| {w:8.4f} m, rms {np.sqrt(np.mean((x - z_qp) ** 2)):.5f} m ({time.time() - t0:.2f} s)")
        if w < 0.01:
            print(f"   PROOF: Jacobi-PCG reaches 1 cm of the QP in {iterations} iterations at {place}")
            break
    return 0 if proved else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
