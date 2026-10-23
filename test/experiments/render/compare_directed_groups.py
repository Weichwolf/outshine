"""Evaluate an original-source grouped transport case before native field upload."""

import argparse
import itertools
import json
import time
from pathlib import Path

import numpy as np
from compare_plane_cloud import image_roi
from directed_group_transport import prepare_view, render_groups
from full_view_reference import load, views
from native_capture_meshes import fields_at
from PIL import Image
from process_memory import peak_rss_bytes
from triangle_image_reference import (
    average_samples,
    image_rays,
    intersect_source,
    prepare_source,
)
from urban_parent_layer import compose
from urban_parent_light import display, light_at, shade


def transformed(source, basis, anchor):
    return source | {
        "positions": source["positions"].astype(float) @ basis.T - anchor,
        "vertex_normal": source["vertex_normal"] @ basis.T,
    }


def retained_subset(source, identities):
    faces = np.flatnonzero(np.isin(source["binding"][:, 1], identities))
    indices = source["indices"][faces]
    unique, inverse = np.unique(indices, return_inverse=True)
    return {
        key: value[faces]
        if key == "binding"
        else inverse.reshape(-1, 3)
        if key == "indices"
        else value[unique]
        for key, value in source.items()
    }


def opaque_stops(source, prepared, origins, directions, ground):
    if len(source["indices"]):
        depth, faces = intersect_source(source, origins, directions, prepared=prepared)
    else:
        depth = np.full(len(origins), np.inf)
        faces = np.full(len(origins), -1, np.int32)
    ground_t = np.divide(
        ground - origins[:, 2],
        directions[:, 2],
        out=np.full(len(origins), np.inf),
        where=directions[:, 2] < 0,
    )
    ground_t = np.where(ground_t > 0.1, ground_t, np.inf)
    use = np.isfinite(depth) & (depth < ground_t)
    stop = np.minimum(depth, ground_t)
    points = origins + np.where(np.isfinite(stop), stop, 0)[:, None] * directions
    normals = np.tile([0.0, 0.0, 1.0], (len(stop), 1))
    if use.any():
        normals[use] = fields_at(source, origins, directions, depth, faces)["normal"][
            use
        ]
    return stop, points, normals


def kept_visibility(source, prepared, points, normals, profile):
    light, _energy, limit = light_at(points, profile)
    visibility = np.ones(len(points))
    if prepared is not None and len(points):
        locations, rays, _ = prepared["ray"].intersects_location(
            points + normals * 0.001, light, multiple_hits=False
        )
        distance = np.linalg.norm(
            locations - (points[rays] + normals[rays] * 0.001), axis=1
        )
        visibility[rays[distance < limit[rays]]] = 0
    return visibility


def background(source, prepared, origins, directions, ground, profile):
    stop, points, normals = opaque_stops(source, prepared, origins, directions, ground)
    receivers = np.isfinite(stop)
    direct = np.zeros_like(origins)
    other = np.broadcast_to(np.asarray(profile["sky"]), origins.shape).copy()
    visible = kept_visibility(
        source, prepared, points[receivers], normals[receivers], profile
    )
    ambient = shade(
        points[receivers],
        normals[receivers],
        -directions[receivers],
        0.6,
        np.zeros(receivers.sum()),
        profile,
    )
    lit = shade(
        points[receivers],
        normals[receivers],
        -directions[receivers],
        0.6,
        visible,
        profile,
    )
    direct[receivers] = lit - ambient
    other[receivers] = ambient
    return stop, points + normals * 0.001, direct, other


def profile_for(arrays, name):
    centre = (
        (arrays["cells"].min(0) + arrays["cells"].max(0) + 1) * arrays["pitch_m"] / 2
    )
    if name == "night":
        return {
            "kind": "point",
            "position": centre + [0, 0, 1000],
            "intensity": 2e8,
            "ambient": 0.002,
            "sky": [0.002] * 3,
        }
    return {
        "kind": "sun",
        "direction": [0.45, -0.5 if name == "day" else 0.5, 0.7],
        "intensity": 3.0,
        "ambient": 0.1,
        "sky": [0.1] * 3,
    }


def comparison_support(arrays, source, ground, profile):
    low = np.minimum(
        source["positions"].min(0), (arrays["cells"] * arrays["pitch_m"]).min(0)
    )
    high = np.maximum(
        source["positions"].max(0), ((arrays["cells"] + 1) * arrays["pitch_m"]).max(0)
    )
    corners = np.array(list(itertools.product(*zip(low, high, strict=True))))
    if profile["kind"] == "sun":
        light = np.asarray(profile["direction"], float)
        shadows = corners - (corners[:, 2] - ground)[:, None] / light[2] * light
    else:
        light = np.asarray(profile["position"], float)
        if light[2] <= high[2]:
            raise ValueError(
                "initial finite shadow-support control requires light above field"
            )
        shadows = (
            light
            + (corners - light)
            * ((ground - light[2]) / (corners[:, 2] - light[2]))[:, None]
        )
    return {"positions": np.concatenate((corners, shadows))}


def run_case(
    arrays, source, retained, reference_index, retained_index, view, profile, output
):
    ground = float((arrays["cells"] * arrays["pitch_m"])[:, 2].min() - 1)
    box = image_roi(comparison_support(arrays, source, ground, profile), view)
    if box is None:
        raise ValueError("first grouped comparison requires a visible original cell")
    box = box + np.array([[-1, -1], [1, 1]])
    box = np.array(
        [np.maximum(box[0], [0, 0]), np.minimum(box[1], [view[3], view[4]])], int
    )
    width, height = (box[1] - box[0]).astype(int)
    origins, directions = image_rays(view, box, width, height, 2)
    directions = directions / np.linalg.norm(directions, axis=1)[:, None]
    started = time.perf_counter()
    reference_stop, _a_points, a_direct, a_other = background(
        source, reference_index, origins, directions, ground, profile
    )
    original = a_direct + a_other
    original_seconds = time.perf_counter() - started
    started = time.perf_counter()
    stop, points, direct, other = background(
        retained, retained_index, origins, directions, ground, profile
    )
    coefficients, setup_work = prepare_view(
        arrays,
        view[0],
        profile,
        lambda centres: kept_visibility(
            retained, retained_index, centres, np.zeros_like(centres), profile
        ),
    )
    setup_seconds = time.perf_counter() - started
    started = time.perf_counter()
    c, t, s, work = render_groups(
        arrays, coefficients, origins, directions, stop, points, profile
    )
    candidate = compose(c, t, s, direct, other)
    render_seconds = time.perf_counter() - started
    output.mkdir(parents=True, exist_ok=False)
    np.savez_compressed(
        output / "raw.npz",
        original=original,
        candidate=candidate,
        colour=c,
        transmission=t,
        receiver_visibility=s,
        reference_stop=reference_stop,
        stop=stop,
        origins=origins,
        directions=directions,
        receiver_points=points,
        direct=direct,
        other=other,
        roi=box,
        width=width,
        height=height,
        **{"field_" + key: value for key, value in coefficients.items()},
    )
    for label, values in [("original", original), ("grouped", candidate)]:
        image = np.zeros((view[4], view[3], 3), np.uint8)
        image[box[0, 1] : box[1, 1], box[0, 0] : box[1, 0]] = display(
            average_samples(values, width, height, 2)
        )
        Image.fromarray(image).save(output / f"{label}.png")
    return {
        "roi": box.tolist(),
        "rays": len(origins),
        "original_seconds": original_seconds,
        "setup_seconds": setup_seconds,
        "render_seconds": render_seconds,
        "max_linear_rgb_error": float(np.abs(original - candidate).max()),
        "mean_linear_rgb_error": float(np.abs(original - candidate).mean()),
        "max_rgb8_error": int(
            np.abs(
                display(average_samples(original, width, height, 2)).astype(int)
                - display(average_samples(candidate, width, height, 2)).astype(int)
            ).max()
        ),
        "setup_work": setup_work,
        "work": work,
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--field", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    started = time.perf_counter()
    with np.load(args.field, allow_pickle=False) as data:
        arrays = {key: data[key] for key in data.files}
    field_load_seconds = time.perf_counter() - started
    started = time.perf_counter()
    original, document, context, _ = load(args.input)
    source = transformed(original, arrays["basis"], arrays["anchor"])
    retained = retained_subset(source, arrays["needs_sources"])
    reference_load_seconds = time.perf_counter() - started
    started = time.perf_counter()
    reference_index = prepare_source(source)
    retained_index = prepare_source(retained) if len(retained["indices"]) else None
    report = {
        "index_prepare_seconds": time.perf_counter() - started,
        "field_load_seconds": field_load_seconds,
        "reference_load_and_retained_extract_seconds": reference_load_seconds,
        "field_array_bytes": sum(a.nbytes for a in arrays.values()),
        "retained_array_bytes": sum(a.nbytes for a in retained.values()),
        "reference_array_bytes": sum(a.nbytes for a in source.values()),
        "total_allocation_count": None,
        "total_physical_copied_bytes": None,
        "retained_original_sources": len(arrays["needs_sources"]),
        "retained_original_triangles": len(retained["indices"]),
        "cases": [],
    }
    for name in ["moving-023", "static-025"]:
        pose = views(document, context)[name]
        view = (
            (pose[0] @ arrays["basis"].T - arrays["anchor"]),
            pose[1] @ arrays["basis"].T,
            *pose[2:],
        )
        for light in ["day", "back", "night"]:
            row = run_case(
                arrays,
                source,
                retained,
                reference_index,
                retained_index,
                view,
                profile_for(arrays, light),
                args.output / f"{name}-{light}",
            )
            report["cases"].append({"pose": name, "light": light, **row})
            print(name, light, row["max_linear_rgb_error"], flush=True)
    report["process_peak_rss_bytes"] = peak_rss_bytes()
    report["scope"] = (
        "Homogeneous two-sided flake model with one explicit neutral GGX material; no native facade/material or opaque-world qualification. Candidate receives only retained fallback/ground stops, never complete original depth. ROI diagnostic, not full-frame image or FPS evidence."
    )
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
