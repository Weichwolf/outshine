"""Original #800 capture groups: admitted rays, exact bitmaps and logical costs."""

import argparse
import hashlib
import json
import math
import tempfile
import time
from pathlib import Path

import numpy as np
from building_plan_inputs import read_plans
from cube_capture_mask import capture_rectangle, occupied_tiles, tile_shape, word_masks


def native_rectangle(bounds, eye, axis, sign, size):
    """Literal current Capture.cpp Project, independently of the exact reference."""
    corners = (
        np.array(
            [
                [x, y, z]
                for x in bounds[:, 0]
                for y in bounds[:, 1]
                for z in bounds[:, 2]
            ]
        )
        - eye
    )
    depth = corners[:, axis] * sign
    right, up = corners[:, (axis + 1) % 3], corners[:, (axis + 2) % 3]
    sides = np.array([depth - right, depth + right, depth - up, depth + up])
    if depth.max() <= 0 or (sides.max(axis=1) < 0).any():
        return None
    if depth.min() <= 0:
        return (0, 0, size, size)
    x, y = right / depth, up / depth
    if x.min() > 1 or x.max() < -1 or y.min() > 1 or y.max() < -1:
        return None
    lower = lambda v: max(
        0, min(size, math.floor((np.clip(v, -1, 1) + 1) * 0.5 * size) - 1)
    )
    upper = lambda v: max(
        0, min(size, math.ceil((np.clip(v, -1, 1) + 1) * 0.5 * size) + 1)
    )
    return lower(x.min()), lower(y.min()), upper(x.max()), upper(y.max())


def load_original(manifest_path, trace_path):
    manifest, trace = (
        json.loads(path.read_bytes()) for path in (manifest_path, trace_path)
    )
    index = manifest_path.parent / manifest["index"]
    entry = next(
        row for row in manifest["native_packages"] if row["file"] == index.name
    )
    raw = index.read_bytes()
    if hashlib.sha256(raw).hexdigest() != entry["sha256"]:
        raise ValueError("original source-index SHA mismatch")
    decoded = read_plans(raw)
    if (
        manifest["parent"] != trace["parent"]
        or tuple(trace["anchor_ecef"]) != decoded["anchor_ecef"]
    ):
        raise ValueError("native trace/index parent or frame mismatch")
    for i, (record, summary, source) in enumerate(
        zip(decoded["records"], decoded["summaries"], trace["sources"], strict=True)
    ):
        if (
            source["index"] != i
            or record["source_id"] != (source["kind"], source["id"])
            or record["cell"] != source["cell"]
            or summary["shell_error_m"] != source["shell_error_m"]
        ):
            raise ValueError("native source identity/cell/error mismatch")
        if source["bounds"] is None:
            if summary["envelope"] is not None:
                raise ValueError("unknown native source envelope became known")
        else:
            np.testing.assert_array_equal(
                summary["envelope"], [source["bounds"]["min"], source["bounds"]["max"]]
            )
    return decoded, trace


def face_rectangles(summaries, members, eye, axis, sign, size, *, include_fine):
    original, demand, work = (
        [],
        [],
        {"projected_boxes": 0, "fine_members": 0, "native_exact_rect_differences": 0},
    )
    for member in members:
        source = summaries[member["source"]]
        work["fine_members"] += member["detail"] == 0
        if not source["projectable"]:
            continue
        bounds = source.get("bounds")
        if member["detail"] != 0:
            original.append(
                native_rectangle(bounds, eye, axis, sign, size)
                if bounds is not None
                else (0, 0, size, size)
            )
        if member["detail"] == 0 and not include_fine:
            continue
        exact = capture_rectangle(bounds, eye, axis, sign, size)
        demand.append(exact)
        work["projected_boxes"] += 1
        if bounds is not None:
            work["native_exact_rect_differences"] += exact != native_rectangle(
                bounds, eye, axis, sign, size
            )
    return original, demand, work


def dense_oracle(masks, demand, size):
    """Independent byte-per-pixel rectangle fill, checked against every row bit."""
    dense = np.zeros((size, size), dtype=bool)
    for rectangle in demand:
        if rectangle is not None:
            left, top, right, bottom = rectangle
            dense[top:bottom, left:right] = True
    count = 0
    digest = hashlib.sha256()
    for (y, x), rows in masks.items():
        height, width = tile_shape((y, x), size)
        selected = dense[y * 64 : y * 64 + height, x * 64 : x * 64 + width]
        packed = np.packbits(selected, axis=1, bitorder="little")
        packed = np.pad(packed, ((0, 0), (0, 8 - packed.shape[1])))
        actual = rows.astype("<u8", copy=False).view(np.uint8).reshape(height, 8)
        if not np.array_equal(actual, packed):
            raise ValueError(
                "word bitmap differs from independent pixel rectangle union"
            )
        digest.update(actual.tobytes())
        count += int(selected.sum())
    return {
        "oracle_selected_rays": count,
        "bitmap_sha256": digest.hexdigest(),
        "diagnostic_dense_array_bytes": dense.nbytes,
    }


def compare_face(summaries, members, eye, axis, sign, size, *, include_fine, verify):
    start = time.perf_counter()
    original, demand, projected = face_rectangles(
        summaries, members, eye, axis, sign, size, include_fine=include_fine
    )
    project_s = time.perf_counter() - start
    start = time.perf_counter()
    tiles = occupied_tiles(original, size)
    masks, work = word_masks(tiles, demand, size)
    mask_s = time.perf_counter() - start
    if work["budget_exhausted"]:
        raise ValueError(
            "real comparison exhausted its bitmap budget; no exact-union claim"
        )
    result = {
        "axis": axis,
        "sign": sign,
        "projection_work": projected,
        "mask_work": work,
        "python_projection_s_diagnostic": project_s,
        "python_mask_s_diagnostic": mask_s,
        "native_sample_grid_allocations_unchanged": len(tiles),
        "native_sample_grid_logical_bytes_unchanged": work["baseline_rays"] * 56,
        "native_sample_grid_peak_logical_bytes_unchanged": max(
            (math.prod(tile_shape(tile, size)) * 56 for tile in tiles), default=0
        ),
    }
    if verify:
        result.update(dense_oracle(masks, demand, size))
        if result["oracle_selected_rays"] != work["selected_rays"]:
            raise ValueError("oracle bit count differs")
    return result


def compare(manifest, trace_path, *, include_fine=False, verify=False):
    data, trace = load_original(manifest, trace_path)
    eye = np.asarray(trace["view"]["eye_ecef"]) - data["anchor_ecef"]
    size = math.ceil(4 * trace["view"]["focal_px"] / trace["view"]["error_px"])
    rows = []
    for row in trace["results"]:
        groups = []
        for index, call in enumerate(row["projection_calls"]):
            if not call["accepted"]:
                groups.append({"index": index, "native_accepted": False})
                continue
            faces = [
                compare_face(
                    data["summaries"],
                    call["members"],
                    eye,
                    axis,
                    sign,
                    size,
                    include_fine=include_fine,
                    verify=verify,
                )
                for axis in range(3)
                for sign in (-1, 1)
            ]
            groups.append(
                {
                    "index": index,
                    "native_accepted": True,
                    "members": len(call["members"]),
                    "faces": faces,
                }
            )
        rows.append(
            {
                "cell": row["cell"],
                "original_native_triangles": row["fallback_triangles"],
                "groups": groups,
            }
        )
    return {
        "parent": trace["parent"],
        "source_records": len(data["records"]),
        "dimension": size,
        "include_fine_in_demand": include_fine,
        "rows": rows,
        "inputs_sha256": {
            str(path): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in (manifest, trace_path)
        },
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--trace", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--include-fine-in-demand", action="store_true")
    parser.add_argument("--verify-bitmaps", action="store_true")
    args = parser.parse_args()
    target, temporary = args.output.resolve(), Path(tempfile.gettempdir()).resolve()
    if (
        target.exists()
        or target == temporary
        or not target.is_relative_to(temporary)
        or target.is_relative_to(Path(__file__).resolve().parents[3])
    ):
        parser.error("fresh explicit system-temp output outside checkout required")
    result = compare(
        args.input,
        args.trace,
        include_fine=args.include_fine_in_demand,
        verify=args.verify_bitmaps,
    )
    result["scope"] = (
        "Frozen actual native projection groups and original raw surface Bounds_, not padded SourceEnvelopeBounds. Exact-dyadic projection/row masks measure possible ray admission only. Fine-only sample omission has an open native error/InvalidSamples contract; Fine remains a ray occluder inside admitted demand. No source/mesh removal, native output/time/GPU/device acceptance. Native sample-grid allocation/init stays unchanged; mask storage is additional. Python times are contended diagnostics."
    )
    result["code_sha256"] = {
        name: hashlib.sha256(Path(__file__).with_name(name).read_bytes()).hexdigest()
        for name in (
            Path(__file__).name,
            "cube_capture_mask.py",
            "building_plan_inputs.py",
        )
    }
    target.mkdir(parents=True)
    (target / "report.json").write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps(
            {
                "parent": result["parent"],
                "rows": len(result["rows"]),
                "include_fine": result["include_fine_in_demand"],
            }
        ),
        flush=True,
    )
