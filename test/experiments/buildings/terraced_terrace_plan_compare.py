"""Same-product source-plan replay and scoped allocation/work comparison."""

import argparse
import gc
import hashlib
import importlib.util
import json
import sys
import time
from dataclasses import asdict
from pathlib import Path
from unittest.mock import patch

import numpy as np
import terraced_terrace_frame as current_frame
import terraced_terrace_plan as current_plan
from evidence import revision
from terraced_mass_plan import plan as mass_plan
from terraced_mass_sources import sources
from terraced_terrace_geometry import TerraceProducts


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    sys.modules[name] = value
    spec.loader.exec_module(value)
    return value


def equal_arrays(left, right):
    if left.keys() != right.keys():
        raise ValueError("Terrace replay changed array fields")
    for key in left:
        if not np.array_equal(left[key], right[key]):
            raise ValueError("Terrace replay changed " + key)


def physical_replay(before, after):
    a, b = TerraceProducts(before), TerraceProducts(after)
    equal_arrays(a.facade.coarse.mesh, b.facade.coarse.mesh)
    np.testing.assert_array_equal(
        a.facade.coarse.triangle_owners, b.facade.coarse.triangle_owners
    )
    if a.facade.coarse.owner_ids != b.facade.coarse.owner_ids:
        raise ValueError("Terrace replay changed direct face ownership")
    for x, y in zip(a.components, b.components, strict=True):
        if x.source != y.source:
            raise ValueError("Terrace replay changed a shared component descriptor")
        equal_arrays(x.mesh, y.mesh)
    equal_arrays(a.draw(False), b.draw(False))
    near_a, near_b = a.facade.physical(), b.facade.physical()
    for name in ("shell", "stairs", "landing"):
        x, y = getattr(near_a.floors, name), getattr(near_b.floors, name)
        equal_arrays(x.mesh, y.mesh)
        for key in ("vertex_roots", "source_indices", "source_triangles"):
            np.testing.assert_array_equal(getattr(x, key), getattr(y, key))
        if (x.volume_m3, x.area_m2, x.source_ids) != (
            y.volume_m3,
            y.area_m2,
            y.source_ids,
        ):
            raise ValueError("Terrace replay changed a physical/provenance measurement")
    equal_arrays(near_a.frames, near_b.frames)
    equal_arrays(a.draw(True), b.draw(True))


def measure(mass, planning, frame, trace):
    timings, value = [], None
    for _ in range(3):
        del value
        gc.collect()
        cpu, start = time.process_time(), time.perf_counter()
        value = planning.plan(mass)
        timings.append(
            {"wall_s": time.perf_counter() - start, "cpu_s": time.process_time() - cpu}
        )
    del value
    gc.collect()
    import memray

    with memray.Tracker(trace, trace_python_allocators=True, native_traces=False):
        result = planning.plan(mass)
    calls = {
        "profile_resolutions": 0,
        "world_window_cuts": 0,
        "world_cut_input_coordinate_bytes": 0,
    }
    resolved, placed = planning.resolved, frame.placed_cut

    def resolve(polygon):
        calls["profile_resolutions"] += 1
        return resolved(polygon)

    def place(cut, core):
        import shapely

        calls["world_window_cuts"] += 1
        calls["world_cut_input_coordinate_bytes"] += shapely.get_coordinates(
            cut.polygon
        ).nbytes
        return placed(cut, core)

    with (
        patch.object(planning, "resolved", resolve),
        patch.object(frame, "placed_cut", place),
    ):
        counted = planning.plan(mass)
    if asdict(counted) != asdict(result):
        raise ValueError("Work replay changed the complete logical plan")
    return result, {"timings": timings, "trace": str(trace), "work": calls}


def compare(args):
    old_plan = module("terrace_plan_reference", args.reference_plan)
    old_frame = module("terrace_frame_reference", args.reference_frame)
    old_plan.world_prepared = old_frame.world_prepared
    rows = []
    for index, row in enumerate(sources()):
        for kind in ("cascade", "paired-wings"):
            mass = mass_plan(row, kind)
            prefix = args.traces / f"{index}-{kind}"
            before, a = measure(
                mass, old_plan, old_frame, prefix.with_name(prefix.name + "-before.bin")
            )
            after, b = measure(
                mass,
                current_plan,
                current_frame,
                prefix.with_name(prefix.name + "-after.bin"),
            )
            if asdict(before) != asdict(after):
                raise ValueError(
                    "Prepared source/storey/window/portal/route fields changed"
                )
            physical_replay(before, after)
            rows.append(
                {
                    "source": index,
                    "source_id": mass.identifier,
                    "programme": kind,
                    "before": a,
                    "after": b,
                    "logical_exact": True,
                    "physical_exact": True,
                }
            )
            print(
                json.dumps(
                    {"source": index, "programme": kind, "physical_exact": True}
                ),
                flush=True,
            )
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reference-plan", type=Path, required=True)
    parser.add_argument("--reference-frame", type=Path, required=True)
    parser.add_argument("--traces", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if not all(
        p.resolve().is_relative_to(Path("/tmp")) for p in (args.traces, args.output)
    ):
        parser.error("--traces/--output require paths under /tmp")
    proof = revision()
    if proof["experiment_tree_dirty"]:
        parser.error("Freeze the comparison implementation first")
    args.traces.mkdir(parents=True, exist_ok=True)
    rows = compare(args)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(
        json.dumps(
            proof
            | {
                "records": rows,
                "reference_sha256": {
                    str(p): hashlib.sha256(p.read_bytes()).hexdigest()
                    for p in (args.reference_plan, args.reference_frame)
                },
                "scope": "Same logical and physical products on eight actual programmes. Three unprofiled plan builds per variant; mass/imports/source acquisition excluded. Separate Memray Python/native allocation traces, no native stacks; work counters are separate replays. No radiance/shadow/native/frame/cache/device admission, no whole-city asset validation.",
            },
            indent=2,
        )
        + "\n"
    )


if __name__ == "__main__":
    main()
