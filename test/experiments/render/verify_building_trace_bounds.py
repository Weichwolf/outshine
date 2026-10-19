"""Original native hit controls and structural Whole-capture trace work."""

import argparse
import hashlib
import json
import tempfile
import time
from collections import Counter
from pathlib import Path

import numpy as np
from building_trace_bounds import (
    local_part_bounds,
    ray_candidates,
    source_local_rays,
    union_bound,
)
from compare_cube_capture_masks import load_original, native_rectangle
from cube_capture_mask import occupied_tiles, tile_shape


def bind_surfaces(decoded, trace, native):
    if any(native[key] != trace[key] for key in ("parent", "anchor_ecef", "view")):
        raise ValueError("native source controls and trace frame/view mismatch")
    for i, (summary, source, actual) in enumerate(
        zip(decoded["summaries"], trace["sources"], native["sources"], strict=True)
    ):
        for key in ("index", "cell", "id", "kind", "bounds", "shell_error_m"):
            if source[key] != actual[key]:
                raise ValueError("native source-control identity/metadata mismatch")
        surface = actual["surface"]
        if surface is not None and summary["supported"]:
            np.testing.assert_array_equal(
                summary["frame"],
                [surface[k] for k in ("origin", "east", "north", "up")],
            )
            if (
                summary["projectable"] != surface["projectable"]
                or summary["minimum_height_m"] != surface["minimum_height_m"]
            ):
                raise ValueError("native source model/minimum height mismatch")
    return len(decoded["records"])


def native_controls(sources):
    counts, unknown = Counter(), []
    for source in sources:
        surface = source["surface"]
        if surface is None or not surface["projectable"]:
            counts["not_projectable_sources"] += 1
            continue
        parts = local_part_bounds(surface, padding_m=source["shell_error_m"])
        if parts is None:
            unknown.append(source["index"])
            continue
        bound = union_bound(parts)
        rays = surface["rays"]
        origins, directions = source_local_rays(
            surface, [r["origin"] for r in rays], [r["direction"] for r in rays]
        )
        low, high = (
            np.array([r["minimum"] for r in rays]),
            np.array([r["maximum"] for r in rays]),
        )
        selected = ray_candidates(bound, origins, directions, minimum=low, maximum=high)
        hit = np.array([r["hit"] is not None for r in rays])
        if (hit & ~selected).any():
            raise ValueError(
                f"local source bound rejects original native hit: {source['index']}"
            )
        native_points = [r["hit"]["point"] for r in rays if r["hit"] is not None]
        if native_points:
            local_points, _ = source_local_rays(
                surface, native_points, np.zeros((len(native_points), 3))
            )
            hit_parts = [r["hit"]["part"] for r in rays if r["hit"] is not None]
            point_boxes = parts[hit_parts]
            if not (
                (local_points >= point_boxes[:, 0])
                & (local_points <= point_boxes[:, 1])
            ).all():
                raise ValueError(
                    f"native hit point escapes its actual local part: {source['index']}"
                )
            counts["native_hit_points_inside_actual_part"] += len(native_points)
        for part in {r["hit"]["part"] for r in rays if r["hit"] is not None}:
            ids = np.array(
                [r["hit"] is not None and r["hit"]["part"] == part for r in rays]
            )
            if not ray_candidates(
                parts[part],
                origins[ids],
                directions[ids],
                minimum=low[ids],
                maximum=high[ids],
            ).all():
                raise ValueError(
                    f"local part bound rejects original native hit: {source['index']}/{part}"
                )
        counts.update(
            sources=1,
            parts=len(parts),
            native_rays=len(rays),
            native_hits=int(hit.sum()),
            rejected_native_misses=int((~hit & ~selected).sum()),
            retained_native_misses=int((~hit & selected).sum()),
            logical_source_union_bytes=bound.nbytes,
            logical_part_array_bytes=parts.nbytes,
        )
    return {
        "counts": dict(counts),
        "unbounded_original_source_indices": unknown,
        "rejected_native_hits": 0,
        "rejected_actual_native_hit_parts": 0,
    }


def capture_directions(tiles, size, axis, sign):
    """Original pixel centres, at most sixteen original blocks / 65,536 rays."""
    tiles = sorted(tiles)
    for first in range(0, len(tiles), 16):
        chunks = []
        for tile in tiles[first : first + 16]:
            y, x = tile
            height, width = tile_shape(tile, size)
            yy, xx = np.indices((height, width))
            directions = np.zeros((height * width, 3))
            directions[:, axis] = sign
            directions[:, (axis + 1) % 3] = (
                2.0 * (x * 64 + xx.ravel() + 0.5) / size - 1.0
            )
            directions[:, (axis + 2) % 3] = (
                2.0 * (y * 64 + yy.ravel() + 0.5) / size - 1.0
            )
            chunks.append(directions)
        yield np.concatenate(chunks)


def cell_capture_membership(decoded, trace):
    """Distinguish the Whole diagnostic opportunity from actual cell consumers."""
    eye = np.asarray(trace["view"]["eye_ecef"]) - decoded["anchor_ecef"]
    targets = {
        i
        for i, source in enumerate(decoded["summaries"])
        if source["projectable"]
        and ((eye >= source["bounds"][0]) & (eye <= source["bounds"][1])).all()
    }
    projected, meshed = [], []
    for index, result in enumerate(trace["results"][1:], 1):
        for group, call in enumerate(result["projection_calls"]):
            members = targets & {m["source"] for m in call["members"]}
            if call["accepted"] and members:
                projected.append(
                    {
                        "result_index": index,
                        "cell": result["cell"],
                        "group": group,
                        "sources": sorted(members),
                    }
                )
        for call in result["mesh_calls"]:
            if call["source"] in targets:
                meshed.append({"cell": result["cell"], **call})
    return {
        "eye_containing_original_sources": sorted(targets),
        "accepted_cell_projection_groups_with_those_sources": projected,
        "original_cell_mesh_calls_for_those_sources": meshed,
    }


def eye_containing_work(decoded, trace, sources):
    eye = np.asarray(trace["view"]["eye_ecef"]) - decoded["anchor_ecef"]
    size = int(np.ceil(4 * trace["view"]["focal_px"] / trace["view"]["error_px"]))
    results = []
    for group, call in enumerate(trace["results"][0]["projection_calls"]):
        if not call["accepted"]:
            continue
        chosen = {}
        for member in call["members"]:
            index = member["source"]
            summary = decoded["summaries"][index]
            if (
                not summary["projectable"]
                or not (
                    (eye >= summary["bounds"][0]) & (eye <= summary["bounds"][1])
                ).all()
            ):
                continue
            source = sources[index]
            parts = local_part_bounds(
                source["surface"], padding_m=source["shell_error_m"]
            )
            if parts is None:
                continue
            chosen[index] = (source["surface"], parts, union_bound(parts), Counter())
        if not chosen:
            continue
        for axis in range(3):
            for sign in (-1, 1):
                rectangles = [
                    native_rectangle(
                        decoded["summaries"][m["source"]]["bounds"],
                        eye,
                        axis,
                        sign,
                        size,
                    )
                    for m in call["members"]
                    if m["detail"] != 0
                    and decoded["summaries"][m["source"]]["projectable"]
                ]
                for directions in capture_directions(
                    occupied_tiles(rectangles, size), size, axis, sign
                ):
                    for index, (surface, parts, bound, work) in chosen.items():
                        local_origin, local_direction = source_local_rays(
                            surface, eye, directions
                        )
                        selected = ray_candidates(bound, local_origin, local_direction)
                        kept_parts = 0
                        union_parts = np.zeros(len(directions), dtype=bool)
                        for part in parts:
                            kept = ray_candidates(part, local_origin, local_direction)
                            kept_parts += int(kept.sum())
                            union_parts |= kept
                        work.update(
                            original_rays=len(directions),
                            original_part_calls_lower_bound=len(directions)
                            * len(parts),
                            source_local_union_kept_rays=int(selected.sum()),
                            source_local_union_part_calls_upper_bound=int(
                                selected.sum()
                            )
                            * len(parts),
                            separate_part_kept_rays=int(union_parts.sum()),
                            separate_part_calls_upper_bound=kept_parts,
                        )
        for index, (_, parts, bound, work) in chosen.items():
            results.append(
                {
                    "source": index,
                    "group": group,
                    "parts": len(parts),
                    "source_union_logical_payload_bytes": bound.nbytes,
                    "separate_parts_logical_payload_bytes": parts.nbytes,
                    "padding_m": sources[index]["shell_error_m"],
                    "work": dict(work),
                }
            )
    return results


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--trace", type=Path, required=True)
    parser.add_argument("--surfaces", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--count-whole-rays", action="store_true")
    args = parser.parse_args()
    target, temporary = args.output.resolve(), Path(tempfile.gettempdir()).resolve()
    if (
        target.exists()
        or target == temporary
        or not target.is_relative_to(temporary)
        or target.is_relative_to(Path(__file__).resolve().parents[3])
    ):
        parser.error("fresh explicit system-temp output outside checkout required")
    decoded, trace = load_original(args.input, args.trace)
    native = json.loads(args.surfaces.read_bytes())
    bound_sources = bind_surfaces(decoded, trace, native)
    start = time.perf_counter()
    controls = native_controls(native["sources"])
    work = (
        eye_containing_work(decoded, trace, native["sources"])
        if args.count_whole_rays
        else []
    )
    result = {
        "scope": "Original native metadata/frames and successful native hit/part controls. Source-local outer union after RequireShapes is compared with existing global source AABBs and optional separate-part boxes. Counter lower bounds use the actual Whole capture grid only for projectable source boxes containing the eye: every original hierarchy traversal reaches those leaves and visits all parts. Guard counts use maximum=+infinity, hence conservative geometric opportunities, not native times, actual new Trace results, world/framebuffer or device acceptance. Source/part order, geometry, IDs and preparation errors remain unchanged by the proposed placement; a native port still needs its own arithmetic/output/memory proof.",
        "parent": native["parent"],
        "bound_original_sources": bound_sources,
        "native_controls": controls,
        "eye_containing_source_work": work,
        "cell_capture_membership": cell_capture_membership(decoded, trace),
        "seconds_python_diagnostic": time.perf_counter() - start,
        "inputs_sha256": {
            str(p): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in (args.input, args.trace, args.surfaces)
        },
        "code_sha256": {
            n: hashlib.sha256(Path(__file__).with_name(n).read_bytes()).hexdigest()
            for n in (
                Path(__file__).name,
                "building_trace_bounds.py",
                "compare_cube_capture_masks.py",
                "cube_capture_mask.py",
                "building_plan_inputs.py",
            )
        },
    }
    target.mkdir(parents=True)
    (target / "report.json").write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps({"parent": result["parent"], "controls": controls, "work": work}),
        flush=True,
    )
