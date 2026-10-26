"""Same cutout/carrier/grid, vector GEOS CDT versus shared projected strip builder."""

import argparse
import json
import time
import tracemalloc
from itertools import pairwise
from pathlib import Path

import numpy as np
import shapely
from cutout_shapes import contour
from outshine_ref.geometry import projected_height_surface
from outshine_ref.vegetation import Blade, BladeCutout
from outshine_ref.vegetation.cutout_mesh import map_surface, planar_mesh


def shared(cutout, along, across):
    b = cutout.blade
    y = np.r_[-b.petiole, np.linspace(0, b.length, along + 1)]
    x = np.linspace(-b.width / 2, b.width / 2, 2 * across + 1)
    raw = []
    for first, last in pairwise(x):
        part = cutout.parameter_domain_m.intersection(
            shapely.box(first, -b.petiole, last, b.length)
        )
        if part.is_empty or part.area == 0:
            continue
        mesh, report = projected_height_surface(
            part, (0, 0), (0, 1), np.c_[y, np.zeros(len(y))]
        )
        if mesh is None:
            raise ValueError(report["reason"])
        raw.append(mesh.vertices[mesh.triangles][:, :, :2])
    planar = planar_mesh(np.concatenate(raw), cutout, 8192, 8192)
    if planar is None:
        raise ValueError("complete shared product exceeds capacity")
    return map_surface(cutout, *planar)


def residual(cutout, mesh):
    parameters = mesh["parameters"][mesh["faces"]]
    q = parameters.mean(axis=1)
    truth = cutout.blade.evaluate(q[:, 0], q[:, 1], np.where(q[:, 1] < 0, -1, 1))[0]
    affine = mesh["vertices"][mesh["faces"]].mean(axis=1)
    return float(np.linalg.norm(truth - affine, axis=1).max())


def run(output):
    blade = Blade(lobes=0, lobe_depth=0)
    cutout = BladeCutout(blade, contour(blade, 71))
    records = []
    for along, across in ((16, 2), (32, 2), (64, 3)):
        for repeat in range(4):
            order = ("grid", "shared") if repeat % 2 == 0 else ("shared", "grid")
            for method in order:
                start = time.perf_counter()
                try:
                    mesh = (
                        cutout.mesh(along, across)
                        if method == "grid"
                        else shared(cutout, along, across)
                    )
                    record = {
                        "valid": True,
                        "faces": len(mesh["faces"]),
                        "payload_bytes": sum(a.nbytes for a in mesh.values()),
                        "max_centroid_residual_m": residual(cutout, mesh),
                    }
                except ValueError as failure:
                    record = {"valid": False, "reason": str(failure)}
                records.append(
                    {
                        "method": method,
                        "along": along,
                        "across": across,
                        "repeat": repeat,
                        "seconds": time.perf_counter() - start,
                        **record,
                    }
                )
    tracemalloc.start()
    mesh = cutout.mesh(64, 3)
    live, peak = tracemalloc.get_traced_memory()
    blocks = sum(
        stat.count for stat in tracemalloc.take_snapshot().statistics("filename")
    )
    tracemalloc.stop()
    result = {
        "rows": records,
        "traced_live_bytes": live,
        "traced_peak_bytes": peak,
        "traced_live_blocks": blocks,
        "returned_array_bytes": sum(a.nbytes for a in mesh.values()),
        "scope": "Same cutout/carrier and requested grid. GEOS scratch excluded from traced allocation counts. Finite centroid residuals are diagnostics, not global surface/LOD error bounds. Times include independent residual check; failure is retained, never ranked as faster.",
    }
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args().output)
