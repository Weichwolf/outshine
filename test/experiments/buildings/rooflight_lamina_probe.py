"""Equal-sheet construction costs and actual full/adaptive/negative render inputs."""

import argparse
import hashlib
import json
import time
import tracemalloc
from functools import lru_cache
from pathlib import Path

import numpy as np
from demand import View, displacement_error
from evidence import revision
from facade_parts import append
from industrial_hall_details import products
from industrial_rooflight_probe import storage_bytes
from outshine_ref.buildings.rooflight_panels import (
    DEPTH_M,
    closed,
    corners,
    place,
    triangle_surface,
)
from outshine_ref.buildings.rooflight_panels import template as full_template
from recipe_hall_operators import inputs_for
from recipe_rooflight_operators import prepare
from recipe_rooflight_products import assemble as full_assembly
from rooflight_lamina import DISTANCE_BOUND_M, assemble, panels, selection, template
from rooflight_surface_probe import mapped
from verify_industrial_halls import sources
from verify_recipe_halls import mesh_equal


@lru_cache(maxsize=32, typed=True)
def filtered_template(width, length, slope):
    full = full_template(width, length, slope)
    selected = full["normals"][full["faces"][:, 0], 2] != 0
    raw = {"vertices": full["vertices"], "faces": full["faces"][selected]}
    value = triangle_surface(raw, 10)
    value["materials"] = full["materials"][selected]
    for array in value.values():
        array.flags.writeable = False
    return value


def placed(fields, build):
    return [
        place(
            p,
            build(
                p["bounds"][2] - p["bounds"][0],
                p["bounds"][3] - p["bounds"][1],
                p["slope"],
            ),
        )
        for p in fields
    ]


def scalar_selection(fields, view):
    result = []
    for item in fields:
        top = corners(item)
        error = displacement_error(
            np.r_[top, top - [0, 0, DEPTH_M]], view, DISTANCE_BOUND_M
        )
        result.append(
            {
                "id": item["id"],
                "lamina": error <= view.detail_error_px,
                "geometry_bound_px": error if np.isfinite(error) else None,
            }
        )
    return tuple(result)


def measured(call):
    began = time.perf_counter()
    value = call()
    cold = (time.perf_counter() - began) * 1000
    times = []
    for _ in range(21):
        began = time.perf_counter()
        value = call()
        times.append((time.perf_counter() - began) * 1000)
    tracemalloc.start()
    value = call()
    _, peak = tracemalloc.get_traced_memory()
    tracemalloc.stop()
    return value, {
        "cold_ms": cold,
        "warm_ms": times,
        "median_ms": float(np.median(times)),
        "traced_peak_bytes": peak,
    }


def construct(fields, view, method):
    if method == "full":
        return closed(fields)
    if method == "filtered":
        return placed(fields, filtered_template)
    if method == "direct":
        return placed(fields, template)
    return panels(fields, view)[0]


def allocations(fields, view, method, output):
    import memray

    output.mkdir(parents=True, exist_ok=True)
    results = []
    for state in ("cold", "warm"):
        if state == "cold":
            template.cache_clear()
            full_template.cache_clear()
            filtered_template.cache_clear()
        path = output / f"{len(fields)}-{method}-{state}.bin"
        with memray.Tracker(path, trace_python_allocators=True, native_traces=True):
            actual = construct(fields, view, method)
        expected = construct(fields, view, method)
        for a, b in zip(actual, expected, strict=True):
            mesh_equal(a, b)
        reader = memray.FileReader(path)
        free = {
            memray.AllocatorType.FREE,
            memray.AllocatorType.PYMALLOC_FREE,
            memray.AllocatorType.MUNMAP,
        }
        events, requested = 0, 0
        for event in reader.get_allocation_records():
            if event.allocator not in free and event.size > 0:
                events += event.n_allocations
                requested += event.size
        results.append(
            {
                "state": state,
                "profiler": "memray",
                "version": memray.__version__,
                "trace_python_allocators": True,
                "native_traces": True,
                "allocation_events": events,
                "cumulative_requested_bytes": requested,
                "tracked_peak_heap_bytes": reader.metadata.peak_memory,
                "trace": str(path.resolve()),
                "trace_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                "all_output_arrays_dtypes_exact": True,
            }
        )
    return results


def costs(output, proof, *, profiles=False):
    report = []
    for row, neighbors in sources()[:3]:
        began = time.perf_counter()
        run = prepare().evaluate(
            inputs_for(row, neighbors), outputs=("programme", "rooflights", "shell")
        )
        prep = (time.perf_counter() - began) * 1000
        fields = run.outputs["rooflights"].value
        centre = np.r_[np.asarray(row["polygon"].centroid.coords[0]), 0]
        view = View(centre + [0, -1000, 600], centre, width=1920, height=1080)
        chosen = selection(fields, view)
        if not all(p["lamina"] for p in chosen):
            raise ValueError("Comparison view did not admit every field")
        if scalar_selection(fields, view) != chosen:
            raise ValueError("Scalar/batched selection differs")
        direct, filtered = placed(fields, template), placed(fields, filtered_template)
        for a, b in zip(direct, filtered, strict=True):
            mesh_equal(a, b)
        entries = []
        for method in ("full", "filtered", "direct", "adaptive"):
            template.cache_clear()
            full_template.cache_clear()
            filtered_template.cache_clear()

            parts, numbers = measured(
                lambda method=method, fields=fields, view=view: construct(
                    fields, view, method
                )
            )
            if profiles:
                numbers["allocation_profiles"] = allocations(
                    fields, view, method, output / "profiles"
                )
            entries.append(
                numbers
                | {
                    "method": method,
                    "triangles": sum(len(p["faces"]) for p in parts),
                    "components": len(parts),
                    "known_transformed_position_normal_arrays": 2 * len(parts),
                    "known_transformed_position_normal_bytes": sum(
                        p[k].nbytes for p in parts for k in ("vertices", "normals")
                    ),
                    "unique_array_bytes": storage_bytes(parts),
                    "known_intermediate_vertex_matmul_bytes": sum(
                        p["vertices"].nbytes for p in parts
                    ),
                }
            )
        decision = []
        for name, fn in (("scalar", scalar_selection), ("batched", selection)):
            _, timing = measured(
                lambda fn=fn, fields=fields, view=view: fn(fields, view)
            )
            decision.append(timing | {"method": name})
        report.append(
            {
                "source_id": row["id"],
                "fields": len(fields),
                "prepare_complete_body_ms": prep,
                "graph_nodes": run.stats["evaluated_nodes"],
                "construction": entries,
                "selection": decision,
            }
        )
    (output / "costs.json").write_text(
        json.dumps(
            proof
            | {
                "cases": report,
                "scope": "Normal-priority shared-host scoped cold/21-warm panel emission and decision timing. Direct/filtered sheets are array/dtype-identical; full is the zero-error geometry baseline, adaptive includes selection. Prepared full body/CSG is separately measured and retained, not removed. Known array/copy work and traced peak only; total native/allocation/traffic and GPU/frame costs unmeasured.",
            },
            indent=2,
        )
        + "\n"
    )


def model(record, row, run, view, method, target, proof):
    if method == "full":
        raw, _, _ = full_assembly(run, view, opened=1, staff_angle=90)
        decisions = selection(run.outputs["rooflights"].value, view)
    elif method == "adaptive":
        raw, decisions, _ = assemble(run, view, opened=1, staff_angle=90)
    else:
        plan = run.outputs["programme"].value.plan
        plan = dict(plan, entry=dict(plan["entry"], angle_deg=90))
        parts, _ = products(plan, 1, 1, view)
        panes = placed(run.outputs["rooflights"].value, template)
        raw = append(run.outputs["shell"].value.body, [*parts, *panes])
        decisions = selection(run.outputs["rooflights"].value, view)
    mesh, source = mapped(row, raw, record["city"])
    target.mkdir(parents=True, exist_ok=True)
    path = target / (record["city"] + "-mesh.npz")
    np.savez_compressed(path, **mesh)
    value = (
        record
        | proof
        | {
            "mesh": str(path.resolve()),
            "mesh_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
            "source": source,
            "triangles": len(mesh["faces"]),
            "procedures": {
                "method": method,
                "fields": decisions,
                "prepared_graph_nodes": run.stats["evaluated_nodes"],
                "retained_body_triangles": len(
                    run.outputs["shell"].value.body["faces"]
                ),
            },
            "scope": "Actual retained v2 body and source-owned render panels. Adaptive admits only bounded sheets; forced is a deliberate unadmitted negative. Collision/full source body unchanged. No general shadow/radiance/night/native/whole-building LOD admission.",
        }
    )
    (target / (record["city"] + "-geometry.json")).write_text(
        json.dumps(value, indent=2, allow_nan=False) + "\n"
    )
    return mesh, source


def models(references, output, proof):
    for i, (row, neighbors) in enumerate(sources()[:3]):
        run = prepare().evaluate(
            inputs_for(row, neighbors), outputs=("programme", "rooflights", "shell")
        )
        original = json.loads(
            (references / f"Rooflight{i}WholeGlazed-geometry.json").read_text()
        )
        camera = dict(original["camera"])
        eye, target = np.asarray(camera["eye"]), np.asarray(camera["target"])
        camera["eye"] = (target + 4 * (eye - target)).tolist()
        record = original | {"city": f"Rooflight{i}FarGlazed", "camera": camera}
        view = View(
            np.asarray(camera["eye"]),
            target,
            width=1920,
            height=1080,
            fov_deg=camera["vertical_fov_deg"],
        )
        for method in ("full", "adaptive"):
            model(record, row, run, view, method, output / method, proof)
        if i == 2:
            near = json.loads(
                (references / "Rooflight2NearGlazed-geometry.json").read_text()
            )
            c = near["camera"]
            v = View(
                np.asarray(c["eye"]),
                np.asarray(c["target"]),
                width=1920,
                height=1080,
                fov_deg=c["vertical_fov_deg"],
            )
            for method in ("full", "adaptive", "forced"):
                model(near, row, run, v, method, output / method, proof)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--references", type=Path)
    parser.add_argument("--profiles", action="store_true")
    args = parser.parse_args()
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError("Freeze lamina source before evidence")
    args.output.mkdir(parents=True, exist_ok=True)
    costs(args.output, proof, profiles=args.profiles)
    if args.references:
        models(args.references, args.output, proof)
