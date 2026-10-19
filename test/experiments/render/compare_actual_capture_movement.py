"""Actual native capture versus complete selected surfaces and Fine at equal rays."""

import argparse
import hashlib
import json
import platform
import resource
import sys
import tempfile
import time
from pathlib import Path

import numpy as np
from check_building_capture_inputs import check, load
from native_capture_meshes import (
    BINDING_COLUMNS,
    REPRESENTATIONS,
    independent_depth,
    mesh,
    sample,
)
from triangle_image_reference import (
    average_samples,
    depth_difference,
    difference,
    image_rays,
    panel,
    prepare_source,
    projected_bounds,
    relight,
)

SAMPLE_LIMIT = 16384
CONTROL_PAIR_LIMIT = 8388608


def views(document, call, positions):
    eye = np.asarray(call["capture"]["eye_offset_ecef"], float)
    up = np.asarray(document["anchor_ecef"], float)
    up /= np.linalg.norm(up)
    forward = (positions.min(0) + positions.max(0)) / 2 - eye
    forward /= np.linalg.norm(forward)
    right = np.cross(forward, up)
    right /= np.linalg.norm(right)
    upward = np.cross(right, forward)
    basis = np.stack((right, upward, forward))
    angle = np.radians(5)
    turned = np.stack(
        (
            np.cos(angle) * right - np.sin(angle) * forward,
            upward,
            np.sin(angle) * right + np.cos(angle) * forward,
        )
    )
    projection = (call["capture"]["focal_px"], 1280, 720)
    return {
        "still": (eye, basis, *projection),
        "right6m": (eye + 6 * right, basis, *projection),
        "right6m-turn5deg": (eye + 6 * right, turned, *projection),
    }


def errors(reference, candidate, meshes):
    a, b = reference[0], candidate[0]
    common = np.isfinite(a) & np.isfinite(b)
    result = depth_difference(a, b, common)
    result.update(
        lost_samples=int((np.isfinite(a) & ~np.isfinite(b)).sum()),
        gained_samples=int((~np.isfinite(a) & np.isfinite(b)).sum()),
    )
    if common.any():
        first = meshes[0]["binding"][reference[1][common]]
        second = meshes[1]["binding"][candidate[1][common]]
        known = (first[:, 1] >= 0) & (second[:, 1] >= 0)
        result["different_known_source_samples"] = int(
            (known & (first[:, 1] != second[:, 1])).sum()
        )
        result["max_normal_unit_distance"] = float(
            np.linalg.norm(
                reference[2]["normal"][common] - candidate[2]["normal"][common], axis=1
            ).max()
        )
        for field in ("colour", "uv", "metres"):
            result[f"max_{field}_payload_component_difference"] = float(
                abs(reference[2][field][common] - candidate[2][field][common]).max()
            )
    return result


def changed_rays(first, second, meshes):
    a, b = np.isfinite(first[0]), np.isfinite(second[0])
    changed = a != b
    ids = np.flatnonzero(a & b)
    owners = [
        s["binding"][h[1][ids], 1] for s, h in zip(meshes, (first, second), strict=True)
    ]
    changed[ids] |= (abs(first[0][ids] - second[0][ids]) > 1e-6) | (
        owners[0] != owners[1]
    )
    return np.flatnonzero(changed)


def verify_rays(meshes, hits, origins, directions, critical):
    controls = []
    for source, hit in zip(meshes, hits, strict=True):
        valid = np.flatnonzero(np.isfinite(hit[0]))
        selected = np.unique(
            np.r_[
                critical,
                np.linspace(0, len(origins) - 1, min(32, len(origins))).astype(int),
                valid[np.linspace(0, len(valid) - 1, min(32, len(valid))).astype(int)],
            ]
        )
        if len(selected) * len(source["indices"]) > CONTROL_PAIR_LIMIT:
            raise ValueError("independent control triangle-pair budget exhausted")
        control = independent_depth(
            source["positions"][source["indices"]],
            origins[selected],
            directions[selected],
        )
        expected = hit[0][selected]
        both = np.isfinite(control) & np.isfinite(expected)
        if not np.array_equal(np.isfinite(control), np.isfinite(expected)):
            raise ValueError("independent ray coverage differs")
        np.testing.assert_allclose(control[both], expected[both], rtol=1e-10, atol=1e-8)
        controls.append(
            {
                "rays": len(selected),
                "all_critical_rays": len(critical),
                "triangle_pairs": len(selected) * len(source["indices"]),
                "maximum_depth_difference_m": float(
                    abs(control[both] - expected[both]).max(initial=0)
                ),
            }
        )
    return controls


def capture_changes(meshes, hits):
    records = []
    for ray in changed_rays(hits[1], hits[2], meshes[1:]):
        row = {"ray": int(ray)}
        for name, source, hit in zip(
            REPRESENTATIONS[1:], meshes[1:], hits[1:], strict=True
        ):
            face = hit[1][ray]
            row[name] = {
                "depth_m": float(hit[0][ray]) if face >= 0 else None,
                "binding": [int(v) if v >= 0 else None for v in source["binding"][face]]
                if face >= 0
                else None,
            }
        records.append(row)
    return records


def compare_view(meshes, prepared, view, output, name, suns):
    boxes = np.array([projected_bounds(s["positions"], view) for s in meshes])
    low = np.maximum(np.floor(boxes[:, 0].min(0)).astype(int), 0)
    high = np.minimum(np.ceil(boxes[:, 1].max(0)).astype(int), view[3:])
    width, height = high - low
    if width <= 0 or height <= 0 or 4 * width * height > SAMPLE_LIMIT:
        raise ValueError("nonempty ROI within declared sample budget required")
    origins, directions = image_rays(view, np.array([low, high]), width, height)
    hits, seconds, saved = [], [], {}
    for representation, source, index in zip(
        REPRESENTATIONS, meshes, prepared, strict=True
    ):
        began = time.perf_counter()
        hit = sample(source, index, origins, directions)
        seconds.append(time.perf_counter() - began)
        hits.append(hit)
        saved[representation + "_depth"] = hit[0]
        saved[representation + "_face"] = hit[1]
        for field, values in hit[2].items():
            saved[representation + "_" + field] = values
    if not np.isfinite(hits[0][0]).any():
        raise ValueError("empty Fine reference is not evidence")
    comparisons = [(0, 1), (1, 2), (0, 2)]
    critical = np.unique(
        np.concatenate(
            [
                changed_rays(hits[a], hits[b], (meshes[a], meshes[b]))
                for a, b in comparisons
            ]
        )
    )
    controls = verify_rays(meshes, hits, origins, directions, critical)
    report = {
        f"{REPRESENTATIONS[a]}_to_{REPRESENTATIONS[b]}": errors(
            hits[a], hits[b], (meshes[a], meshes[b])
        )
        for a, b in comparisons
    }
    images = []
    for sun in suns:
        light = {}
        for representation, hit in zip(REPRESENTATIONS, hits, strict=True):
            values = np.column_stack(
                (relight(hit[2], sun, hit[0]), np.isfinite(hit[0]))
            )
            light[representation] = average_samples(values, width, height, 2)
        images.append(light)
    panel(output / f"{name}.png", images, name)
    np.savez_compressed(
        output / f"{name}.npz", origins=origins, directions=directions, **saved
    )
    report.update(
        roi_xyxy=[low.tolist(), high.tolist()],
        full_image=[1280, 720],
        samples_per_pixel=4,
        sample_rays=len(origins),
        ray_query_s=seconds,
        independent_controls=controls,
        capture_changes=capture_changes(meshes, hits),
        pose={
            "eye_offset_ecef": view[0].tolist(),
            "basis": view[1].tolist(),
            "focal_px": view[2],
        },
        lights=[
            {
                f"{REPRESENTATIONS[a]}_to_{REPRESENTATIONS[b]}": difference(
                    light[REPRESENTATIONS[a]], light[REPRESENTATIONS[b]]
                )
                for a, b in comparisons
            }
            for light in images
        ],
    )
    return report


def run(args):
    began = time.perf_counter()
    document = load(args.input)
    check(document)
    calls = [
        call for result in document["results"] for call in result["projection_calls"]
    ]
    if len(calls) != 1:
        raise ValueError("one explicit original projection required per comparison")
    call = calls[0]
    meshes = [mesh(call, name) for name in REPRESENTATIONS]
    prepared = [prepare_source(s) for s in meshes]
    args.output.mkdir(parents=True, exist_ok=False)
    anchor = np.asarray(document["anchor_ecef"], float)
    up = anchor / np.linalg.norm(anchor)
    east = np.cross([0, 0, 1], up)
    east /= np.linalg.norm(east)
    suns = [up + east, up - east]
    records = {
        name: compare_view(meshes, prepared, view, args.output, name, suns)
        for name, view in views(document, call, meshes[0]["positions"]).items()
    }
    lookup = {row["index"]: row for row in document["sources"]}
    members = [
        {
            "slot": source["slot"],
            "member": call["members"][source["slot"]],
            "original": lookup.get(call["members"][source["slot"]]["source"]),
            "emission": source["emission"],
        }
        for source in call["capture"]["sources"]
    ]
    report = {
        "input_sha256": hashlib.sha256(args.input.read_bytes()).hexdigest(),
        "native_provenance": document["provenance"],
        "projection": {
            k: v
            for k, v in call["capture"].items()
            if k not in ("sources", "native_capture")
        },
        "members": members,
        "views": records,
        "costs": {
            "platform": platform.platform(),
            "triangles": [len(s["indices"]) for s in meshes],
            "array_bytes": [sum(a.nbytes for a in s.values()) for s in meshes],
            "binding_columns": BINDING_COLUMNS,
            "sample_limit_per_view": SAMPLE_LIMIT,
            "triangle_limit_per_mesh": 131072,
            "independent_pair_limit_per_mesh_view": CONTROL_PAIR_LIMIT,
            "ray_reader_batch": 32,
            "total_seconds": time.perf_counter() - began,
            "process_peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
            * (1024 if sys.platform.startswith("linux") else 1)
            if sys.platform.startswith("linux") or sys.platform == "darwin"
            else None,
        },
        "scope": "Fixed original capture, declared cell-facing diagnostic orientation and follow-up poses; 2x2 same-ray front-facing opaque control, interpolated native fields and Lambert colour factors. Raw UV/material payload differences do not measure texture error. No UV texture/PBR, runtime culling/opacity qualification, near-field, continuous movement radius, GPU or world admission. Array bytes exclude JSON, R-tree, Python/allocator scratch; peak is process-wide, not incremental. Triangle bindings retain -1 for unknown identities; JSON change records use null. Depth differences above 1e-6 m select independent control rays, not an acceptance tolerance; every coverage/source difference is also checked.",
    }
    np.savez_compressed(
        args.output / "triangle-bindings.npz",
        **{name: s["binding"] for name, s in zip(REPRESENTATIONS, meshes, strict=True)},
    )
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    return report


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    arguments = parser.parse_args()
    if not arguments.output.resolve().is_relative_to(
        Path(tempfile.gettempdir()).resolve()
    ):
        parser.error("diagnostic output must stay in system temp")
    result = run(arguments)
    print(json.dumps({"output": str(arguments.output), "costs": result["costs"]}))
