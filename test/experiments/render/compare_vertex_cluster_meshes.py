"""One actual original/control/centre/QEF view with retained C/T/S diagnostics."""

import argparse
import itertools
import json
import subprocess
import time
from pathlib import Path

import numpy as np
from compare_directed_groups import kept_visibility, retained_subset, transformed
from compare_plane_cloud import image_roi
from field_scale_projection import displacement_bound
from full_view_reference import load, views
from native_capture_meshes import fields_at
from packed_normals import unpack
from PIL import Image, ImageDraw
from process_memory import peak_rss_bytes
from source_plane_mesh import factorized_positions
from triangle_image_reference import (
    average_samples,
    image_rays,
    intersect_source,
    prepare_source,
)
from urban_parent_light import display, shade


def ready_source(arrays, retained):
    """Expand diagnostic corners only; no native attribute-sharing/layout claim."""
    positions = arrays["positions"][arrays["indices"]].reshape(-1, 3)
    count = len(positions)
    source = {
        "positions": positions,
        "indices": np.arange(count, dtype=np.int64).reshape(-1, 3),
        "vertex_normal": np.repeat(unpack(arrays["face_normal_words"]), 3, axis=0),
        "uv": np.zeros((count, 2)),
        "metres": np.zeros((count, 2)),
        "colour": np.ones((count, 4)),
    }
    result = {
        k: np.concatenate((v, retained[k] + count if k == "indices" else retained[k]))
        for k, v in source.items()
    }
    return result


def render(source, prepared, origins, directions, ground, profile):
    started = time.perf_counter()
    depth, faces = intersect_source(source, origins, directions, prepared=prepared)
    floor = np.divide(
        ground - origins[:, 2],
        directions[:, 2],
        out=np.full(len(origins), np.inf),
        where=directions[:, 2] < 0,
    )
    floor = np.where(floor > 0.1, floor, np.inf)
    hit = np.isfinite(depth) & (depth < floor)
    stop = np.minimum(depth, floor)
    valid = np.isfinite(stop)
    points = origins + np.where(valid, stop, 0)[:, None] * directions
    normals = np.tile([0.0, 0.0, 1.0], (len(origins), 1))
    if hit.any():
        normals[hit] = fields_at(
            source, origins[hit], directions[hit], depth[hit], faces[hit]
        )["normal"]
    visibility = np.ones(len(origins))
    visibility[valid] = kept_visibility(
        source, prepared, points[valid], normals[valid], profile
    )
    direct = np.zeros_like(origins)
    other = np.broadcast_to(np.asarray(profile["sky"]), origins.shape).copy()
    ambient = shade(
        points[valid],
        normals[valid],
        -directions[valid],
        0.6,
        np.zeros(valid.sum()),
        profile,
    )
    lit = shade(
        points[valid],
        normals[valid],
        -directions[valid],
        0.6,
        np.ones(valid.sum()),
        profile,
    )
    direct[valid], other[valid] = lit - ambient, ambient
    colour = np.zeros_like(origins)
    colour[hit] = other[hit] + visibility[hit, None] * direct[hit]
    transmission = (~hit).astype(float)
    rgb = colour + transmission[:, None] * (visibility[:, None] * direct + other)
    return {
        "rgb": rgb,
        "colour": colour,
        "transmission": transmission,
        "receiver_visibility": visibility,
        "direct": direct,
        "other": other,
        "source_hit": hit,
        "first_depth": stop,
        "source_depth": depth,
        "normal": normals,
    }, {
        "render_seconds": time.perf_counter() - started,
        "source_hit_rays": int(hit.sum()),
    }


def pixel_fields(raw, width, height):
    rgb = display(average_samples(raw["rgb"], width, height, 2))
    alpha = average_samples(raw["source_hit"].astype(float), width, height, 2)[..., 0]
    return rgb, alpha


def errors(reference, candidate, width, height):
    a, aa = pixel_fields(reference, width, height)
    b, ba = pixel_fields(candidate, width, height)
    difference = abs(a.astype(float) - b.astype(float))
    receiver = (
        (~reference["source_hit"])
        & (~candidate["source_hit"])
        & np.isfinite(reference["first_depth"])
        & np.isfinite(candidate["first_depth"])
    )
    common = reference["source_hit"] & candidate["source_hit"]
    depth = abs(reference["source_depth"][common] - candidate["source_depth"][common])
    angle = np.degrees(
        np.arccos(
            np.clip(
                np.einsum(
                    "ij,ij->i", reference["normal"][common], candidate["normal"][common]
                ),
                -1,
                1,
            )
        )
    )
    return {
        "max_rgb8_error": int(difference.max()),
        "rgb8_rmse": float(np.sqrt((difference**2).mean())),
        "changed_rgb8_pixels": int((difference.max(2) > 0).sum()),
        "alpha_l1_pixel_units": float(abs(ba - aa).sum()),
        "lost_half_coverage_pixels": int(((aa >= 0.5) & (ba < 0.5)).sum()),
        "gained_half_coverage_pixels": int(((aa < 0.5) & (ba >= 0.5)).sum()),
        "common_ground_receiver_rays": int(receiver.sum()),
        "changed_receiver_shadow_rays": int(
            (
                reference["receiver_visibility"][receiver]
                != candidate["receiver_visibility"][receiver]
            ).sum()
        ),
        "common_source_hit_max_depth_change_m": float(depth.max())
        if len(depth)
        else None,
        "common_source_hit_max_normal_angle_degrees": float(angle.max())
        if len(angle)
        else None,
    }


def save_images(raw, view, box, output):
    width, height = box[1] - box[0]
    names = ("original", "control", "centre", "qef")
    zoom = 8
    panel_w, panel_h = int(width * zoom + 16), int(height * zoom + 36)
    contact = Image.new("RGB", (panel_w * 4, panel_h * 2), (32, 32, 32))
    draw = ImageDraw.Draw(contact)
    for column, name in enumerate(names):
        rgb, _ = pixel_fields(raw[name], width, height)
        full = np.zeros((view[4], view[3], 3), np.uint8)
        full[box[0, 1] : box[1, 1], box[0, 0] : box[1, 0]] = rgb
        Image.fromarray(full).save(output / f"{name}.png")
        crop = Image.fromarray(rgb).resize(
            (int(width * zoom), int(height * zoom)), Image.Resampling.NEAREST
        )
        contact.paste(crop, (column * panel_w + 8, 28))
        draw.text((column * panel_w + 8, 6), name, fill="white")
        reference_name = "original" if name in ("original", "control") else "control"
        reference, _ = pixel_fields(raw[reference_name], width, height)
        delta = np.clip(
            abs(rgb.astype(int) - reference.astype(int)) * 4, 0, 255
        ).astype(np.uint8)
        contact.paste(
            Image.fromarray(delta).resize(crop.size, Image.Resampling.NEAREST),
            (column * panel_w + 8, panel_h + 28),
        )
        draw.text(
            (column * panel_w + 8, panel_h + 6),
            f"4x error vs {reference_name}",
            fill="white",
        )
    contact.save(output / "comparison.png")


def setup_view(source, arrays, document, context, profile):
    pose = views(document, context)["moving-023"]
    view = (
        pose[0] @ arrays["basis"].T - arrays["anchor"],
        pose[1] @ arrays["basis"].T,
        *pose[2:],
    )
    ground = float(arrays["source_bounds"][0, 2] - 1)
    low, high = source["positions"].min(0), source["positions"].max(0)
    corners = np.array(list(itertools.product(*zip(low, high, strict=True))))
    light = np.array(profile["direction"])
    shadows = corners - (corners[:, 2] - ground)[:, None] / light[2] * light
    box = image_roi({"positions": np.concatenate((corners, shadows))}, view)
    if box is None:
        raise ValueError("existing actual group must be visible")
    box = np.array(
        [np.maximum(box[0] - 1, [0, 0]), np.minimum(box[1] + 1, [view[3], view[4]])],
        int,
    )
    width, height = box[1] - box[0]
    origins, directions = image_rays(view, box, width, height, 2)
    directions /= np.linalg.norm(directions, axis=1)[:, None]
    return view, ground, box, origins, directions


def run(args):
    args.output.mkdir(parents=True, exist_ok=False)
    started = time.perf_counter()
    arrays = {}
    for name in ("control", "centre", "qef"):
        with np.load(args.product / f"{name}.npz", allow_pickle=False) as archive:
            arrays[name] = {k: archive[k] for k in archive.files}
        for key in (
            "basis",
            "anchor",
            "accepted_sources",
            "needs_sources",
            "face_normal_words",
            "face_patches",
            "patch_sources",
        ):
            if not np.array_equal(arrays[name][key], arrays["control"][key]):
                raise ValueError("matching geometry/normal/source products required")
    original, document, context, _ = load(args.input)
    original_control = factorized_positions(original)
    a = arrays["control"]
    source = transformed(original, a["basis"], a["anchor"])
    retained = retained_subset(source, a["needs_sources"])
    ids = np.unique(original["binding"][:, 1])
    if not np.array_equal(
        np.sort(np.r_[a["accepted_sources"], a["needs_sources"]]), ids
    ):
        raise ValueError("every original source must remain accounted for")
    sources = {"original": source} | {
        name: ready_source(value, retained) for name, value in arrays.items()
    }
    support = {"positions": np.concatenate([s["positions"] for s in sources.values()])}
    profile = {
        "kind": "sun",
        "direction": [0.45, -0.5, 0.7],
        "intensity": 3.0,
        "ambient": 0.1,
        "sky": [0.1] * 3,
    }
    view, ground, box, origins, directions = setup_view(
        support, a, document, context, profile
    )
    prepared = {name: prepare_source(s) for name, s in sources.items()}
    loaded = time.perf_counter()
    raw, render_reports = {}, {}
    for name, s in sources.items():
        raw[name], render_reports[name] = render(
            s, prepared[name], origins, directions, ground, profile
        )
    width, height = box[1] - box[0]
    report = {
        "producer": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "pose": "moving-023",
        "profile": {
            "width": view[3],
            "height": view[4],
            "samples_per_axis": 2,
            "light": profile,
        },
        "roi": box.tolist(),
        "rays": len(origins),
        "load_and_prepare_seconds": loaded - started,
        "renders": render_reports,
        "vs_original": {
            n: errors(raw["original"], raw[n], width, height) for n in arrays
        },
        "vs_control": {
            n: errors(raw["control"], raw[n], width, height) for n in ("centre", "qef")
        },
        "original_geometry_bytes": sum(x.nbytes for x in original_control.values()),
        "required_original_rest_geometry_bytes": sum(
            x.nbytes
            for x in factorized_positions(
                retained_subset(original, a["needs_sources"])
            ).values()
        ),
        "required_original_rest_triangles": len(retained["indices"]),
        "nominal_three_metre_projection_bound_px": displacement_bound(
            a["source_bounds"], view, 3
        ),
        "expanded_diagnostic_array_bytes": {
            n: sum(v.nbytes for v in s.values()) for n, s in sources.items()
        },
        "process_peak_rss_bytes": peak_rss_bytes(),
        "scope": "One actual ROI structural screen with uniform neutral GGX on all opponents, not native materials, PBR/World/device or continuous-motion admission. Source-only control separates source reconstruction from the clustering operator. All paths consume only their ready opaque mesh, explicit original remainder and common independent ground/sky. C/T/S and raw depths/normals retained; no Original depth oracle in candidates. Diagnostic corner expansion has dummy material coordinates; it is not a free native attribute-sharing layout. Output outside ROI is black. No phase/size/pose sweep.",
    }
    save_images(raw, view, box, args.output)
    np.savez_compressed(
        args.output / "raw.npz",
        roi=box,
        origins=origins,
        directions=directions,
        **{n + "_" + k: v for n, r in raw.items() for k, v in r.items()},
    )
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--product", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
