"""Compare same-topology local sweeps and the coupled weighted quadratic control."""

import argparse
import hashlib
import json
import os
import platform
import subprocess
import time
from pathlib import Path

import numpy as np
from ground_contact import ContactLimits
from ground_contact_checks import qualify_contact
from ground_contact_inputs import load_context
from ground_interior import relax_contact, relax_interior
from ground_interior_validation import bridge_composition, validate, validation_inputs
from ground_refinement import vertical_lod_difference
from ground_rim_contact import build_rim_contact
from ground_rim_fit import fit_rim_heights
from outshine_ref.contracts import Mesh
from outshine_ref.transport.heights import HeightBudget
from replay_canonical_bridge_ports import allocations
from replay_canonical_ports import process_peak, serializable


def coupled_control(context, base):
    mesh = base["mesh"]
    n = len(base["coarse_mesh"].vertices)
    v = mesh.vertices.copy()
    v[n:, 2] = context.sample_z(v[n:, :2])
    p = v[mesh.triangles]
    areas = np.cross(p[:, 1] - p[:, 0], p[:, 2] - p[:, 0])[:, 2] / 2
    weights = np.zeros(len(v))
    count = len(context.road.triangles)
    np.add.at(weights, mesh.triangles[count:].ravel(), np.repeat(areas[count:] / 3, 3))
    weights[:n] = weights.max()
    weights /= weights.sum()
    fit = fit_rim_heights(
        Mesh(v, mesh.triangles),
        np.broadcast_to(np.eye(2) / (2 / 3), (len(mesh.triangles), 2, 2)),
        {i: float(v[i, 2]) for i in range(n)},
        weights=weights,
        maximum_change_m=8.0,
        budget=HeightBudget(time_limit_s=0.25, max_vertices=1024, max_faces=4096),
    )
    if fit.mesh is None:
        raise ValueError("coupled control did not solve:" + str(fit.report))
    bound = vertical_lod_difference(
        base["coarse_mesh"], fit.mesh, base["refinement_roots"]
    )
    return base | {
        "mesh": fit.mesh,
        "interior_fit": dict(fit.report),
        "refinement": base["refinement"] | {"maximum_vertical_LOD_difference_m": bound},
    }


def stage(context, base, name):
    cpu, wall = time.process_time(), time.perf_counter()
    if name == "coupled":
        result = coupled_control(context, base)
    elif name == "scalar4":
        n = len(base["coarse_mesh"].vertices)
        mesh, report = relax_interior(
            base["mesh"],
            context.sample_z,
            fixed_vertices=n,
            insertion_counts=[1] * (len(base["mesh"].vertices) - n),
            protected_faces=len(context.road.triangles),
            maximum_sweeps=4,
        )
        bound = vertical_lod_difference(
            base["coarse_mesh"], mesh, base["refinement_roots"]
        )
        result = base | {
            "mesh": mesh,
            "interior_fit": report,
            "refinement": base["refinement"]
            | {"maximum_vertical_LOD_difference_m": bound},
        }
    else:
        result = relax_contact(
            context.road,
            base,
            context.sample_z,
            maximum_sweeps=int(name.removeprefix("batch")),
        )
    return result, {
        "cpu_s": time.process_time() - cpu,
        "wall_s": time.perf_counter() - wall,
    }


def signature(result):
    mesh = result["mesh"]
    return hashlib.sha256(
        mesh.vertices.tobytes() + mesh.triangles.tobytes()
    ).hexdigest()


def replay(args):
    args.output.mkdir(parents=True, exist_ok=False)
    context = load_context()
    base = build_rim_contact(
        context.road,
        context.road_dem_z,
        context.sample_z,
        limits=ContactLimits(margins_m=(15.0,)),
    )
    qualify_contact(context.road, base)
    names = ("scalar4", "batch2", "batch4", "batch8", "coupled")
    results = {name: stage(context, base, name)[0] for name in names}
    expected = {name: signature(result) for name, result in results.items()}
    samples = {name: [] for name in names}
    for turn in range(args.rounds):
        for name in names if turn % 2 == 0 else reversed(names):
            result, cost = stage(context, base, name)
            if signature(result) != expected[name]:
                raise ValueError("timed geometry changed")
            samples[name].append(cost)
    traces = {}
    if args.trace:
        import memray

        for name in names:
            path = args.output / (name + ".bin")
            with memray.Tracker(
                str(path), trace_python_allocators=True, native_traces=True
            ):
                value, _ = stage(context, base, name)
            if signature(value) != expected[name]:
                raise ValueError("traced geometry changed")
            traces[name] = allocations(path) | {
                "scope": "Warm same-base interior method, including target acquisition, setup, final gradient postcheck, owned output and geometric LOD update. External complete contact checks/validation/export/plot/source preparation excluded; allocation bytes are not copied bytes."
            }
    inputs = validation_inputs(context, base)
    methods = {}
    arrays = {
        "road_vertices": context.road.vertices,
        "road_faces": context.road.triangles,
        "coarse_vertices": base["coarse_mesh"].vertices,
        "coarse_faces": base["coarse_mesh"].triangles,
        "refinement_roots": base["refinement_roots"],
        "road_vertex_map": base["road_vertex_map"],
        "outer_vertex_ids": base["outer_vertex_ids"],
        "prior_z": base["prior_z"],
        "domain_xy": np.asarray(base["domain"].exterior.coords),
        "rim_corner_ids": base["rim_profile"]["corner_vertex_ids"],
        "ordered_rim_ids": base["rim_profile"]["ordered_vertex_ids"],
    }
    arrays.update(
        {key: value for key, value in inputs.items() if isinstance(value, np.ndarray)}
    )
    for name, result in {"base": base, **results}.items():
        mesh = result["mesh"]
        methods[name] = {
            "quality": qualify_contact(context.road, result),
            "validation": validate(mesh, inputs),
            "fit": result.get("interior_fit"),
            "refinement": result["refinement"],
            "runs": samples.get(name),
            "allocations": traces.get(name),
            "artifact_sha256": signature(result),
        }
        arrays[name + "_vertices"], arrays[name + "_faces"] = (
            mesh.vertices,
            mesh.triangles,
        )
    bridge, bridge_arrays = bridge_composition()
    arrays.update(bridge_arrays)
    peak, peak_source = process_peak()
    np.savez(args.output / "geometry.npz", **arrays)
    report = {
        "schema": "outshine/terrain-interior-heights/1",
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "worktree_status": subprocess.check_output(
            ["git", "status", "--porcelain"], text=True
        ),
        "platform": platform.platform(),
        "python": platform.python_version(),
        "load_average": os.getloadavg(),
        "process_nice": os.getpriority(os.PRIO_PROCESS, 0),
        "threads": {
            key: os.environ.get(key)
            for key in ("OPENBLAS_NUM_THREADS", "OMP_NUM_THREADS")
        },
        "inputs": context.metadata,
        "source_binding": context.report["binding"],
        "source_state": context.report["state"],
        "source_terminals": context.report["terminal_values"],
        "source_preparation_cpu_s": context.preparation_cpu_s,
        "methods": methods,
        "bridges": bridge,
        "process_peak_rss_bytes_before_plot": peak,
        "process_peak_source": peak_source,
        "stored_array_payload_bytes": sum(value.nbytes for value in arrays.values()),
        "scope": "Same315vertices/564faces,187fixed coarse/source/rim values and128movable interior observations. Whole soil gradient2/3, no added topology. Coupled8m observation box is nonbinding in this case; rim2m contract unchanged.45barycentric samples per410soil faces plus410centroids are sampled controls, not continuous DEM bounds. Coordinate method can stall without joint optimality. Bridge composition uses its own unchanged original source field/frame. No original-grid/neighbour/C1/Source/World/cache/device/frame admission. Total copied bytes unmeasured.",
    }
    (args.output / "report.json").write_text(
        json.dumps(serializable(report), indent=2, default=lambda v: v.tolist()) + "\n"
    )
    if args.plot:
        from ground_interior_plot import draw

        draw(context, base, results, inputs, args.output)
    print(
        json.dumps(
            {
                name: {
                    "cpu_ms": [v["cpu_s"] * 1000 for v in samples.get(name, [])],
                    "validation": value["validation"],
                    "allocations": value["allocations"],
                }
                for name, value in methods.items()
            }
        )
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--rounds", type=int, default=3)
    parser.add_argument("--trace", action="store_true")
    parser.add_argument("--plot", action="store_true")
    args = parser.parse_args()
    if not 1 <= args.rounds <= 16:
        parser.error("rounds must be1..16")
    replay(args)


if __name__ == "__main__":
    main()
