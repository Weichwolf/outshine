"""Count guaranteed first-source work on actual original cell capture grids."""

import argparse
import hashlib
import json
import struct
import tempfile
from collections import Counter
from pathlib import Path

import numpy as np
from building_capture_prefix import first_source, native_tree
from building_trace_bounds import (
    local_part_bounds,
    ray_candidates,
    source_local_rays,
    union_bound,
)
from compare_cube_capture_masks import load_original, native_rectangle
from cube_capture_mask import occupied_tiles
from sample_building_capture import without_timing
from verify_building_trace_bounds import bind_surfaces, capture_directions


def guards(source):
    surface = source["surface"]
    parts = local_part_bounds(surface, padding_m=source["shell_error_m"])
    return parts, union_bound(parts)


def verify_callbacks(tree, nodes, order, flags, members, bounds, native, eye):
    headers = {}
    for local, global_source in enumerate(members):
        surface = native["sources"][global_source]["surface"]
        if surface is None:
            continue
        frame = np.asarray([surface[k] for k in ("origin", "east", "north", "up")])
        header = np.r_[frame.ravel(), bounds[local].ravel()].astype("<f8").tobytes()
        headers.setdefault(header.hex(), []).append(local)
    counts = Counter()
    for ray, witness in tree["first_callbacks"].items():
        coordinates = np.frombuffer(bytes.fromhex(ray), dtype="<f8")
        np.testing.assert_array_equal(coordinates[:3], eye)
        if (
            struct.unpack("<d", bytes.fromhex(witness["minimum_hex"]))[0] != 0
            or struct.unpack("<d", bytes.fromhex(witness["maximum_hex"]))[0] != np.inf
        ):
            raise ValueError("witness is not before every possible closer hit")
        first, _ = first_source(nodes, order, flags, eye, coordinates[None, 3:])
        candidates = headers.get(witness["header_hex"], [])
        if int(first[0]) not in candidates or "has_hit" not in witness:
            raise ValueError(
                "first-source replay/actual native header or return mismatch"
            )
        counts["native_first_callbacks"] += 1
        counts["uniquely_identified_first_callbacks"] += len(candidates) == 1
        source = native["sources"][members[first[0]]]
        _, bound = guards(source)
        origins, directions = source_local_rays(
            source["surface"], eye, coordinates[None, 3:]
        )
        retained = bool(ray_candidates(bound, origins, directions)[0])
        if witness["has_hit"]:
            along, _, _, _, part, _, _ = struct.unpack(
                "<4dQQB", bytes.fromhex(witness["hit_hex"])
            )
            if not retained or not 0 <= part < len(source["surface"]["shapes"]):
                raise ValueError("local guard rejects actual first native hit/part")
            if not np.isfinite(along) or along < 0:
                raise ValueError("invalid native Hit ABI interpretation")
            counts["retained_actual_native_prefix_hits"] += 1
        elif not retained:
            counts["guard_rejected_actual_native_prefix_misses"] += 1
    return counts


def measure_call(decoded, trace, native, members, bounds, nodes, order, flags, cache):
    eye = np.asarray(trace["view"]["eye_ecef"]) - decoded["anchor_ecef"]
    size = int(np.ceil(4 * trace["view"]["focal_px"] / trace["view"]["error_px"]))
    counts, per_source = Counter(), {}
    for axis in range(3):
        for sign in (-1, 1):
            rectangles = [
                native_rectangle(bounds[i], eye, axis, sign, size)
                for i, member in enumerate(members)
                if member["detail"] != 0 and flags[i]
            ]
            for directions in capture_directions(
                occupied_tiles(rectangles, size), size, axis, sign
            ):
                first, tests = first_source(nodes, order, flags, eye, directions)
                counts.update(
                    original_cell_rays=len(first),
                    original_prefix_ray_entry_calls=int(tests.sum()),
                    original_first_eligible_source_calls=int((first >= 0).sum()),
                )
                for local in np.unique(first[first >= 0]):
                    index = members[local]["source"]
                    selected = first == local
                    source = native["sources"][index]
                    if index not in cache:
                        cache[index] = guards(source)
                    parts, bound = cache[index]
                    work = per_source.setdefault(index, Counter())
                    work["first_eligible_source_calls"] += int(selected.sum())
                    if parts is None:
                        work["unknown_guard_delegated_calls"] += int(selected.sum())
                        continue
                    origins, vectors = source_local_rays(
                        source["surface"], eye, directions[selected]
                    )
                    union = ray_candidates(bound, origins, vectors)
                    part_calls = sum(
                        int(ray_candidates(part, origins, vectors).sum())
                        for part in parts
                    )
                    work.update(
                        known_original_part_calls=len(union) * len(parts),
                        source_union_rejected_calls=int((~union).sum()),
                        source_union_part_calls=int(union.sum()) * len(parts),
                        separate_box_part_calls=part_calls,
                        union_box_tests=len(union),
                        separate_part_box_tests=len(union) * len(parts),
                    )
    for work in per_source.values():
        counts.update(
            {k: v for k, v in work.items() if k != "first_eligible_source_calls"}
        )
    return dict(counts), {str(i): dict(work) for i, work in per_source.items()}


def provenance(args):
    names = (
        "measure_building_capture_prefix.py",
        "building_capture_prefix.py",
        "building_trace_bounds.py",
        "compare_cube_capture_masks.py",
        "cube_capture_mask.py",
        "sample_building_capture.py",
        "verify_building_trace_bounds.py",
        "building_plan_inputs.py",
        "gdb_capture_prefix_inputs.py",
    )
    return {
        "inputs_sha256": {
            str(p): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in (
                args.input,
                args.trace,
                args.surfaces,
                args.trees,
                args.native_result,
            )
        },
        "code_sha256": {
            name: hashlib.sha256(
                Path(__file__).with_name(name).read_bytes()
            ).hexdigest()
            for name in names
        },
    }


def run(args):
    output = args.output.resolve()
    temporary = Path(tempfile.gettempdir()).resolve()
    if (
        output.exists()
        or output == temporary
        or not output.is_relative_to(temporary)
        or output.is_relative_to(Path(__file__).resolve().parents[3])
    ):
        raise ValueError("fresh explicit system-temp result outside checkout required")
    decoded, trace = load_original(args.input, args.trace)
    native = json.loads(args.surfaces.read_bytes())
    snapshots = json.loads(args.trees.read_bytes())
    actual = json.loads(args.native_result.read_bytes())
    bind_surfaces(decoded, trace, native)
    reader = Path(__file__).with_name("gdb_capture_prefix_inputs.py")
    if snapshots["reader_sha256"] != hashlib.sha256(reader.read_bytes()).hexdigest():
        raise ValueError("native snapshot reader version differs from this audit")
    if snapshots["native_exit"] != 0 or without_timing(actual) != without_timing(trace):
        raise ValueError(
            "native snapshot run changed original output or did not finish"
        )
    calls = [
        (result, group, call)
        for result, row in enumerate(trace["results"])
        for group, call in enumerate(row["projection_calls"])
    ]
    if any(not call["accepted"] for _, _, call in calls):
        raise ValueError(
            "failed capture groups need a separate error-aware prefix audit"
        )
    total, controls, rows, cache = Counter(), Counter(), [], {}
    eye = np.asarray(trace["view"]["eye_ecef"]) - decoded["anchor_ecef"]
    for (result, group, call), tree in zip(calls, snapshots["trees"], strict=True):
        nodes, order, bounds = native_tree(tree)
        members = call["members"]
        indices = np.asarray([member["source"] for member in members])
        flags = np.asarray([decoded["summaries"][i]["projectable"] for i in indices])
        np.testing.assert_array_equal(
            bounds, [decoded["summaries"][i]["bounds"] for i in indices]
        )
        controls.update(
            verify_callbacks(tree, nodes, order, flags, indices, bounds, native, eye)
        )
        controls["byte_identical_native_trees"] += 1
        if result == 0:
            continue
        counts, sources = measure_call(
            decoded, trace, native, members, bounds, nodes, order, flags, cache
        )
        total.update(counts)
        rows.append(
            {
                "result": result,
                "cell": trace["results"][result]["cell"],
                "group": group,
                "counts": counts,
                "sources": sources,
            }
        )
    result = {
        "scope": "Original native cell grids and byte-verified native hierarchy. Only the first eligible callback per ray is counted, before any closer hit can exist. Known local outer bounds expose geometric part-loop avoidance opportunities after RequireShapes, not native candidate times, full native output equivalence or device/world acceptance. Later callbacks remain uncounted; do not call this the total source/part work or multiply by sampled CPU shares.",
        "parent": trace["parent"],
        "sources_bound": len(decoded["records"]),
        "controls": dict(controls),
        "counts": dict(total),
        "groups": rows,
        **provenance(args),
    }
    output.mkdir(parents=True)
    (output / "report.json").write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps(
            {
                "parent": result["parent"],
                "controls": dict(controls),
                "counts": dict(total),
            }
        )
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("input", "trace", "surfaces", "trees", "native-result", "output"):
        parser.add_argument(f"--{name}", type=Path, required=True)
    run(parser.parse_args())
