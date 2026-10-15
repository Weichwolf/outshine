"""Cache actual nested fern/grass products and equal-output assembly measurements."""

import argparse
import json
import subprocess
import time
import tracemalloc
from pathlib import Path

import numpy as np
from fern_maps import write_maps
from fern_mesh import fern_mesh, pinna_mesh
from fern_plan import prepare
from ground_cover_scene import tuft_poses
from outshine_ref.vegetation import GroundCoverPlan
from outshine_ref.vegetation.graminoid import graminoid_tuft_mesh


def geometry(mesh):
    v, f = mesh["vertices"], mesh["faces"]
    points = v[f]
    cross = np.cross(points[:, 1] - points[:, 0], points[:, 2] - points[:, 0])
    assert all(np.isfinite(a).all() for a in mesh.values())
    assert (np.einsum("ij,ikj->ik", cross, mesh["normals"].reshape(-1, 3, 3)) > 0).all()
    return {
        "vertices": len(v),
        "triangles": len(f),
        "payload_bytes": sum(a.nbytes for a in mesh.values()),
        "surface_area_m2": float(np.linalg.norm(cross, axis=1).sum() / 2),
        "min_area2_m2": float(np.linalg.norm(cross, axis=1).min()),
        "normal_unit_error": float(
            abs(np.linalg.norm(mesh["normals"], axis=1) - 1).max()
        ),
        "bounds_m": [v.min(axis=0).tolist(), v.max(axis=0).tolist()],
    }


def compare(plan, pinna):
    times = {key: [] for key in ("batch", "individual")}
    reference = None
    for i in range(6):
        for method in list(times)[:: 1 if i % 2 else -1]:
            start = time.perf_counter()
            mesh = fern_mesh(plan, pinna, method=method)
            times[method].append(time.perf_counter() - start)
            if reference is None:
                reference = mesh
            for key, value in reference.items():
                assert (
                    value.dtype == mesh[key].dtype
                    and value.tobytes() == mesh[key].tobytes()
                ), key
    trace = {}
    for method in times:
        tracemalloc.start()
        mesh = fern_mesh(plan, pinna, method=method)
        retained, peak = tracemalloc.get_traced_memory()
        tracemalloc.stop()
        trace[method] = {
            "retained_traced_bytes": retained,
            "peak_traced_bytes": peak,
            "output_bytes": sum(a.nbytes for a in mesh.values()),
        }
    return {
        "equal_complete_arrays": True,
        "times_s": times,
        "separate_untimed_tracemalloc": trace,
        "work": {
            "source_pinna_reused": 133,
            "guide_evaluations": {"batch": 7, "individual": 140},
        },
        "scope": "Same whole plant and triangles. Traced/registered NumPy memory, not all allocator calls or copy traffic; full process RSS measured separately.",
    }


def population(output, kind, seed, density, count):
    plan = GroundCoverPlan(seed, density)
    sites = plan.sites((-2, -2), (2, 2))
    poses = tuft_poses(sites)
    poses["variants"] %= count
    if kind == "grass":
        poses["scales"] = np.repeat(poses["scales"][:, None], 3, axis=1) * (1, 1, 0.65)
    np.savez_compressed(output / (kind + "-population.npz"), **poses)
    return {
        "plan": plan.recipe(),
        "roots": len(poses["ids"]),
        "variant_counts": np.bincount(poses["variants"], minlength=count).tolist(),
        "payload_bytes": sum(a.nbytes for a in poses.values()),
    }


def run(output):
    output.mkdir(parents=True, exist_ok=True)
    rows = []
    comparison = None
    for variant in range(4):
        plan = prepare(221 + variant)
        (output / f"fern-{variant}-plan.json").write_text(
            json.dumps(plan, indent=2) + "\n"
        )
        for detail, segments, rib, sides in (("near", 6, 24, 6), ("field", 2, 10, 4)):
            start = time.perf_counter()
            pinna = pinna_mesh(
                plan,
                segments,
                rib_segments=8 if detail == "near" else 3,
                rib_sides=sides,
            )
            mesh = fern_mesh(plan, pinna, rib_segments=rib)
            elapsed = time.perf_counter() - start
            np.savez_compressed(output / f"pinna-{variant}-{detail}.npz", **pinna)
            np.savez_compressed(output / f"fern-{variant}-{detail}.npz", **mesh)
            rows.append(
                {
                    "variant": variant,
                    "detail": detail,
                    "mesh_s": elapsed,
                    **geometry(mesh),
                }
            )
            if variant == 0 and detail == "near":
                comparison = compare(plan, pinna)
    grasses = []
    for i in range(8):
        mesh = graminoid_tuft_mesh(17 + i, segments=3)
        data = {
            "vertices": mesh["vertices"],
            "faces": mesh["faces"].astype(np.int32),
            "normals": mesh["normals"],
            "corner_uv": mesh["uv"][mesh["faces"].ravel()],
            "surface": np.full(len(mesh["faces"]), 1 + i % 3, np.uint8),
        }
        np.savez_compressed(output / f"grass-{i}.npz", **data)
        grasses.append(len(mesh["faces"]))
    populations = {
        kind: population(output, kind, seed, density, count)
        for kind, seed, density, count in (
            ("grass", 113, 150, 8),
            ("fern", 339, 0.85, 4),
        )
    }
    total = sum(
        n * f
        for n, f in zip(populations["grass"]["variant_counts"], grasses, strict=True)
    )
    total += sum(
        populations["fern"]["variant_counts"][row["variant"]] * row["triangles"]
        for row in rows
        if row["detail"] == "field"
    )
    write_maps(output)
    result = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "models": rows,
        "populations": populations,
        "tile_instanced_triangles": total,
        "comparison": comparison,
        "scope": "4x4m authored fern/grass study, no calibrated habitat or botanical species admission.",
    }
    (output / "summary.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args().output)
