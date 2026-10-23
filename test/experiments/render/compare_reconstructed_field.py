"""One actual prepared-field view; unresolved work is visible and disqualifies it."""

import argparse
import itertools
import json
import subprocess
import time
from pathlib import Path

import numpy as np
from compare_directed_groups import (
    background,
    kept_visibility,
    retained_subset,
    transformed,
)
from compare_plane_cloud import image_roi
from full_view_reference import load, views
from PIL import Image
from process_memory import peak_rss_bytes
from reconstructed_field_query import active_bounds, trace, validate
from triangle_image_reference import average_samples, image_rays, prepare_source
from urban_parent_light import display, light_at, shade


def field_shadow(field, points, profile):
    light, _, limit = light_at(points, profile)
    depth, _, unknown, work = trace(field, points, light, limit, minimum=0.001)
    occluded = np.isfinite(depth)
    unresolved = np.isfinite(unknown) & ~occluded
    visibility = (~occluded).astype(float)
    visibility[unresolved] = np.nan
    return visibility, unresolved, work


def candidate(field, retained, prepared, origins, directions, ground, profile):
    started = time.perf_counter()
    stop, receivers, direct, other = background(
        retained, prepared, origins, directions, ground, profile
    )
    depth, normal, unknown, primary_work = trace(field, origins, directions, stop)
    primary_bad = np.isfinite(unknown) & (unknown <= np.minimum(depth, stop))
    hit = np.isfinite(depth) & ~primary_bad
    colour = np.zeros_like(origins)
    transmission = (~hit).astype(float)
    visibility = np.ones(len(origins))
    work = {"primary": primary_work}
    bad = primary_bad.copy()
    if hit.any():
        points = origins[hit] + depth[hit, None] * directions[hit]
        light_visibility, missing, light_work = field_shadow(
            field, points + normal[hit] * 0.001, profile
        )
        kept = kept_visibility(retained, prepared, points, normal[hit], profile)
        light_visibility[kept == 0] = 0
        bad[np.flatnonzero(hit)[missing & (kept != 0)]] = True
        colour[hit] = shade(
            points, normal[hit], -directions[hit], 0.6, light_visibility, profile
        )
        work["surface_shadow"] = light_work
    receiver_ids = np.flatnonzero(np.isfinite(stop) & ~hit & ~primary_bad)
    if len(receiver_ids):
        s, missing, shadow_work = field_shadow(field, receivers[receiver_ids], profile)
        relevant = np.any(direct[receiver_ids] != 0, axis=1)
        missing &= relevant
        s[~relevant] = 1
        visibility[receiver_ids] = s
        bad[receiver_ids[missing]] = True
        work["receiver_shadow"] = shadow_work
    colour[primary_bad], transmission[primary_bad] = np.nan, np.nan
    rgb = colour + transmission[:, None] * (visibility[:, None] * direct + other)
    rgb[bad] = np.nan
    return (
        rgb,
        {
            "colour": colour,
            "transmission": transmission,
            "receiver_visibility": visibility,
            "candidate_depth": depth,
            "unknown_primary_depth": unknown,
            "unresolved": bad,
            "primary_unresolved": primary_bad,
            "stop": stop,
            "direct": direct,
            "other": other,
        },
        {
            "seconds": time.perf_counter() - started,
            "primary_unresolved_rays": int(primary_bad.sum()),
            "all_unresolved_rays": int(bad.sum()),
            "work": work,
        },
    )


def support_for_roi(field, source, ground, profile):
    bounds = active_bounds(field)
    low, high = (
        np.minimum(bounds[0], source["positions"].min(0)),
        np.maximum(bounds[1], source["positions"].max(0)),
    )
    corners = np.array(list(itertools.product(*zip(low, high, strict=True))))
    light = np.array(profile["direction"])
    shadow = corners - (corners[:, 2] - ground)[:, None] / light[2] * light
    return {"positions": np.concatenate((corners, shadow))}


def save_images(original, candidate_rgb, unresolved, view, box, width, height, output):
    original_pixel = display(average_samples(original, width, height, 2))
    candidate_pixel = display(
        average_samples(np.nan_to_num(candidate_rgb), width, height, 2)
    )
    missing = average_samples(unresolved.astype(float), width, height, 2) > 0
    candidate_pixel[missing] = [255, 0, 255]
    error = np.abs(original_pixel.astype(int) - candidate_pixel.astype(int))
    error_image = np.clip(error * 4, 0, 255).astype(np.uint8)
    error_image[missing] = [255, 0, 255]
    for name, values in [
        ("original", original_pixel),
        ("field", candidate_pixel),
        ("error", error_image),
    ]:
        image = np.zeros((view[4], view[3], 3), np.uint8)
        image[box[0, 1] : box[1, 1], box[0, 0] : box[1, 0]] = values
        Image.fromarray(image).save(output / f"{name}.png")
    qualified = ~missing
    return {
        "unresolved_pixels": int(missing.sum()),
        "qualified_pixels": int(qualified.sum()),
        "max_rgb8_error_on_qualified_pixels": int(error[qualified].max())
        if qualified.any()
        else None,
        "global_image_qualified": not missing.any(),
    }


def run(args):
    args.output.mkdir(parents=True, exist_ok=False)
    started = time.perf_counter()
    with np.load(args.field, allow_pickle=False) as archive:
        field = {key: archive[key] for key in archive.files}
    validate(field)
    loaded = time.perf_counter()
    field_load_seconds = loaded - started
    original, document, context, _ = load(args.input)
    source = transformed(original, field["basis"], field["anchor"])
    retained = retained_subset(source, field["needs_sources"])
    source_index = prepare_source(source)
    kept_index = prepare_source(retained) if len(retained["indices"]) else None
    reference_loaded = time.perf_counter()
    pose = views(document, context)["moving-023"]
    view = (
        pose[0] @ field["basis"].T - field["anchor"],
        pose[1] @ field["basis"].T,
        *pose[2:],
    )
    ground = float(field["source_bounds"][0, 2] - 1)
    profile = {
        "kind": "sun",
        "direction": [0.45, -0.5, 0.7],
        "intensity": 3.0,
        "ambient": 0.1,
        "sky": [0.1] * 3,
    }
    box = image_roi(support_for_roi(field, source, ground, profile), view)
    if box is None:
        raise ValueError("existing original case must be visible")
    box = np.array(
        [np.maximum(box[0] - 1, [0, 0]), np.minimum(box[1] + 1, [view[3], view[4]])],
        int,
    )
    width, height = (box[1] - box[0]).astype(int)
    origins, directions = image_rays(view, box, width, height, 2)
    directions /= np.linalg.norm(directions, axis=1)[:, None]
    started = time.perf_counter()
    _, _, a_direct, a_other = background(
        source, source_index, origins, directions, ground, profile
    )
    reference = a_direct + a_other
    reference_seconds = time.perf_counter() - started
    rgb, raw, report = candidate(
        field, retained, kept_index, origins, directions, ground, profile
    )
    np.savez_compressed(
        args.output / "raw.npz",
        original=reference,
        candidate=rgb,
        roi=box,
        origins=origins,
        directions=directions,
        **raw,
    )
    report.update(
        save_images(
            reference, rgb, raw["unresolved"], view, box, width, height, args.output
        )
    )
    report.update(
        pose="moving-023",
        light="day",
        roi=box.tolist(),
        rays=len(origins),
        field_load_and_validate_seconds=field_load_seconds,
        reference_load_prepare_seconds=reference_loaded - loaded,
        reference_seconds=reference_seconds,
        field_array_bytes=sum(a.nbytes for a in field.values()),
        retained_original_triangles=len(retained["indices"]),
        retained_original_array_bytes=sum(a.nbytes for a in retained.values()),
        process_peak_rss_bytes=peak_rss_bytes(),
        producer=subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        source_queries_during_candidate=0,
    )
    report["scope"] = (
        "One actual ROI screening case, not full-frame/FPS/PBR/World admission. Magenta pixels retain unresolved demand; raw NaNs are explicit missing radiance. Candidate consumes only ready field and one retained incomplete original source. Comparison loads the original population for its opponent/ROI and retained-source extraction, never as candidate field/stops. Field bytes exclude still-required unresolved finer products. A structural failure stops before night/backlight or parameter/GPU rounds."
    )
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--field", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
