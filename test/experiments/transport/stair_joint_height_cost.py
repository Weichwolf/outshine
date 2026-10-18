"""Complete bounded height fits on identical verified whole-source geometry."""

import argparse
import hashlib
import json
import resource
import time
from pathlib import Path

import numpy as np
from camera_height_inputs import digest
from outshine_ref.contracts import Mesh
from outshine_ref.transport import HeightBudget, fit_surface_heights
from stair_closed_height_rounding import normalized_grade
from stair_free_terrain_cost import preload
from stair_joint_height_fit import fit_joint_levels, level_groups
from stair_module_experiment import write_json
from stair_module_inputs import inputs
from stair_module_roads import face_frames
from stair_quantized_joint_heights import fit_quantized_levels
from stair_seed_crossings import crossings_for
from stair_seed_geometry import plans_for, prepared
from stair_terrain_experiment import frozen


def height_inputs(args):
    context = preload(args)
    data, ground = context[4], context[5]
    recipe, rows, _context, sources, sample, _layer = inputs(args.fixture)
    if recipe.get("junction_height_mode") != "quantized":
        raise ValueError("verified quantized joint seed required for this comparison")
    planning = prepared(rows, sources, ground)
    plans = plans_for(rows, planning[0], crossings_for(rows, sources, recipe))
    vertices, faces = data["surface_vertices"], data["surface_faces"]
    if not np.array_equal(data["targets"], sample(vertices[:, :2])):
        raise ValueError("whole original DEM height targets changed")
    surface = Mesh(np.c_[vertices[:, :2], data["targets"]], faces)
    frames = face_frames(rows, vertices[:, :2], faces, data["owners"], reserve=0.0002)
    groups = level_groups(vertices[:, :2], plans)
    return surface, frames, plans, groups, data


def fit(method, surface, frames, plans, groups):
    if method == "quantized":
        return fit_quantized_levels(surface, frames, plans)
    budget = HeightBudget(max_vertices=4096, max_faces=8192)
    if method == "joint":
        return fit_joint_levels(surface, frames, plans, budget)
    if method != "fixed_priors":
        raise ValueError("known bounded height method required")
    pins = {
        int(i): float(plan["height_m"])
        for plan, group in zip(plans, groups, strict=True)
        for i in group
    }
    return fit_surface_heights(surface, frames, fixed=pins, budget=budget), None


def observation(result, levels, surface, frames, groups, saved, output, arrays):
    record = {
        "solver_report": dict(result.report),
        "mesh_returned": result.mesh is not None,
    }
    if result.mesh is None:
        return {**record, "valid_absolute_D_Q": False}
    xyz, faces = result.mesh.vertices, surface.triangles
    grades = [normalized_grade(xyz, faces, frames, cast) for cast in (False, True)]
    level_error = max(float(np.ptp(xyz[g, 2])) for g in groups)
    signature = hashlib.sha256(xyz.tobytes()).hexdigest()
    if signature not in arrays:
        path = output / (signature + ".npz")
        np.savez_compressed(path, vertices=xyz, faces=faces)
        arrays[signature] = {"file": path.name, "sha256": digest(path)}
    return {
        **record,
        "vertices_sha256": signature,
        "maximum_reserved_normalized_D_Q": grades,
        "flat_level_error_m": level_error,
        "valid_absolute_D_Q": all(np.isfinite(g) and g <= 1 for g in grades)
        and level_error == 0,
        "maximum_DEM_height_change_m": float(
            abs(xyz[:, 2] - surface.vertices[:, 2]).max()
        ),
        "same_verified_body_surface": np.array_equal(xyz, saved["surface_vertices"]),
        "levels_m": None if levels is None else np.asarray(levels).tolist(),
    }


def run(args):
    commit = frozen()
    if (
        args.output.exists()
        or not args.output.resolve().is_relative_to(Path("/tmp"))
        or args.repeats not in (1, 3, 5)
    ):
        raise ValueError("fresh explicit /tmp output and bounded repetitions required")
    surface, frames, plans, groups, saved = height_inputs(args)
    # Imports are outside fit measurement, like source/body verification.
    import clarabel
    from scipy.optimize import milp

    if not callable(milp) or not hasattr(clarabel, "DefaultSolver"):
        raise ValueError("both numerical solver backends required")
    original = surface.vertices.copy()
    signature = hashlib.sha256(
        surface.vertices.tobytes() + surface.triangles.tobytes() + frames.tobytes()
    ).hexdigest()
    args.output.mkdir()
    measurements, arrays = {}, {}
    for method in ("fixed_priors", "joint", "quantized"):
        records = []
        for _repeat in range(args.repeats):
            began = time.perf_counter()
            result, levels = fit(method, surface, frames, plans, groups)
            seconds = time.perf_counter() - began
            records.append(
                {
                    "seconds": seconds,
                    **observation(
                        result,
                        levels,
                        surface,
                        frames,
                        groups,
                        saved,
                        args.output,
                        arrays,
                    ),
                }
            )
        measured = {"unprofiled": records}
        if args.profile:
            import memray

            with memray.Tracker(
                str(args.output / (method + "-profile.bin")),
                trace_python_allocators=True,
                native_traces=False,
            ):
                began = time.perf_counter()
                result, levels = fit(method, surface, frames, plans, groups)
                seconds = time.perf_counter() - began
            measured["profiled"] = {
                "seconds": seconds,
                **observation(
                    result, levels, surface, frames, groups, saved, args.output, arrays
                ),
            }
        measurements[method] = measured
    if not np.array_equal(original, surface.vertices):
        raise ValueError("height fitting changed immutable original targets")
    report = {
        "commit": commit,
        "input_sha256": signature,
        "body_sha256": digest(args.products / "module.npz"),
        "fixture_sha256": digest(args.fixture),
        "vertices": len(surface.vertices),
        "faces": len(surface.triangles),
        "level_groups": len(groups),
        "level_vertices": sum(len(g) for g in groups),
        "input_array_bytes": surface.vertices.nbytes
        + surface.triangles.nbytes
        + frames.nbytes,
        "group_membership_bytes": sum(g.nbytes for g in groups),
        "measurements": measurements,
        "artifacts": arrays,
        "peak_process_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * 1024,
        "scope": "Identical complete verified whole-source XY/faces, original DEM targets and reserved anisotropic frames. Complete height-fit call timed/traced, including flat-region membership, sparse matrices, solver and its candidate checks. Source/body preload and imports, independent observation/export excluded. Every bounded result retained, including absent or Q-invalid candidates. No optimality, copied-byte, native runtime/device or equal-success comparison claim. Process RSS includes preload; per-trace peaks are incremental.",
    }
    write_json(args.output / "comparison.json", report)
    print(
        json.dumps(
            {"commit": commit, "input_sha256": signature, "methods": list(measurements)}
        ),
        flush=True,
    )
    return report


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("resident", "fixture", "products", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--prior-module", type=Path, action="append", default=[])
    parser.add_argument("--repeats", type=int, choices=(1, 3, 5), default=3)
    parser.add_argument("--profile", action="store_true")
    resource.setrlimit(resource.RLIMIT_CPU, (180, 180))
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3,) * 2)
    run(parser.parse_args())
