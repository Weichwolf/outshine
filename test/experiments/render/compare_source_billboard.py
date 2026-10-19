"""Compare source-complete Fine/Shell groups and two-triangle image proposals.

All methods use identical 2x2 sample rays in a 1280x720 camera and the same
colour-factor/Lambert diagnostic. This is not native PBR or GPU acceptance.
"""

import argparse
import hashlib
import json
import resource
import tempfile
import time
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw
from source_billboard import (
    average_samples,
    bake_card,
    card_samples,
    image_rays,
    join_sources,
    projected_bounds,
    reference_samples,
)
from source_far_assets import load_source
from source_far_reprojection import prepare_source
from source_surface_fields import relight


def camera(centre, east, north, up, extent, focal, distance, yaw, move):
    angle = np.radians(yaw)
    eye = centre + distance * extent * (
        np.cos(angle) * east + np.sin(angle) * north + 0.3 * up
    )
    forward = centre - eye
    forward /= np.linalg.norm(forward)
    right = np.cross(forward, up)
    right /= np.linalg.norm(right)
    basis = np.stack((right, np.cross(right, forward), forward))
    return eye + move * right, basis, focal, 1280, 720


def difference(reference, image):
    a, b = reference[:, :, 3], image[:, :, 3]
    union = (a > 0) | (b > 0)
    delta = image[:, :, :3] - reference[:, :, :3]
    opaque = (a >= 1 - 1e-6) & (b >= 1 - 1e-6)
    return {
        "reference_alpha_sum_px": float(a.sum()),
        "candidate_alpha_sum_px": float(b.sum()),
        "alpha_l1_px": float(abs(b - a).sum()),
        "lost_half_coverage_px": int(((a >= 0.5) & (b < 0.5)).sum()),
        "gained_half_coverage_px": int(((a < 0.5) & (b >= 0.5)).sum()),
        "premultiplied_rgb_union_rmse": float(np.sqrt(np.mean(delta[union] ** 2)))
        if union.any()
        else None,
        "premultiplied_rgb_roi_rmse": float(np.sqrt(np.mean(delta**2))),
        "common_opaque_pixels": int(opaque.sum()),
        "common_opaque_rgb_rmse": float(np.sqrt(np.mean(delta[opaque] ** 2)))
        if opaque.any()
        else None,
    }


def depth_difference(reference, candidate, covered):
    common = np.isfinite(reference) & np.isfinite(candidate) & covered
    delta = abs(candidate[common] - reference[common])
    return {
        "common_samples": int(common.sum()),
        "p95_depth_difference_m": float(np.percentile(delta, 95))
        if len(delta)
        else None,
        "max_depth_difference_m": float(delta.max()) if len(delta) else None,
    }


def panel(path, images, title):
    result = Image.new("RGB", (len(images[0]) * 384, 2 * 250), "white")
    draw = ImageDraw.Draw(result)
    for row, light in enumerate(images):
        for col, (name, pixels) in enumerate(light.items()):
            # Identical sky composite; metrics above use premultiplied RGB.
            rgb = pixels[:, :, :3] + (1 - pixels[:, :, 3:4]) * [0.45, 0.65, 0.85]
            im = Image.fromarray(np.uint8(np.clip(rgb, 0, 1) * 255 + 0.5))
            im.thumbnail((376, 210), Image.Resampling.NEAREST)
            # Enlarge small distant objects explicitly; original pixel data saved.
            factor = min(376 / im.width, 210 / im.height)
            im = im.resize(
                (int(im.width * factor), int(im.height * factor)),
                Image.Resampling.NEAREST,
            )
            result.paste(im, (col * 384 + 4, row * 250 + 36))
            draw.text(
                (col * 384 + 4, row * 250 + 4),
                f"{name}, light {row}: {title}",
                fill="black",
            )
    result.save(path)


def render_view(fine, shell, prepared, card, view, suns):
    boxes = [
        projected_bounds(s["positions"], view) for s in (fine, shell, card["mesh"])
    ]
    lo = np.maximum(np.floor(np.min(boxes, axis=(0, 1))).astype(int), 0)
    hi = np.minimum(np.ceil(np.max(boxes, axis=(0, 1))).astype(int), view[3:])
    width, height = hi - lo
    if width <= 0 or height <= 0:
        raise ValueError("empty comparison is not evidence")
    origins, directions = image_rays(view, np.array([lo, hi]), width, height)
    images, seconds = [], {}
    geometry = []
    for name, source, index in zip(
        ("Fine", "Shell"), (fine, shell), prepared, strict=True
    ):
        start = time.perf_counter()
        geometry.append(reference_samples(source, index, origins, directions))
        seconds[name + "_reference_query_s"] = time.perf_counter() - start
    for i, sun in enumerate(suns):
        light = {}
        for name, (depth, fields) in zip(("Fine", "Shell"), geometry, strict=True):
            values = np.column_stack((relight(fields, sun, depth), np.isfinite(depth)))
            light[name] = average_samples(values, width, height, 2)
        start = time.perf_counter()
        card_depth, alpha, matte, live = card_samples(card, origins, directions, sun)
        seconds[f"card_query_both_methods_light_{i}_s"] = time.perf_counter() - start
        for name, rgb in (("Matte", matte), ("Live card", live)):
            light[name] = average_samples(
                np.column_stack((rgb, alpha)), width, height, 2
            )
        images.append(light)
    return images, {
        "roi_xyxy": [lo.tolist(), hi.tolist()],
        "sample_rays": len(origins),
        "depth": {
            "Shell": depth_difference(
                geometry[0][0], geometry[1][0], np.isfinite(geometry[1][0])
            ),
            "card": depth_difference(geometry[0][0], card_depth, alpha >= 0.5),
        },
        "timing": seconds,
        "lights": [
            {name: difference(light["Fine"], image) for name, image in light.items()}
            for light in images
        ],
    }


def load_group(fine_paths, shell_paths):
    if len(fine_paths) != len(shell_paths) or not fine_paths:
        raise ValueError("paired source-complete inputs required")
    fine, shell, anchor, records = [], [], None, []
    for a, b in zip(fine_paths, shell_paths, strict=True):
        native = [json.loads((p / "native.json").read_text()) for p in (a, b)]
        for n in native:
            if anchor is not None and n["anchor_ecef"] != anchor:
                raise ValueError("group coordinates require identical native anchors")
            anchor = n["anchor_ecef"]
        for name in ("subset.index", "subset.forms"):
            if (a / name).read_bytes() != (b / name).read_bytes():
                raise ValueError("Shell changes original source bytes")
        fine.append(load_source(a))
        shell.append(load_source(b))
        for key in ("original_parent", "original_source_ordinal", "original_source_id"):
            if fine[-1]["metadata"][key] != shell[-1]["metadata"][key]:
                raise ValueError("Shell changes original source membership")
        for index, expected in enumerate((0, 1)):
            if any(n["details"] != [expected] for n in native[index]["results"]):
                raise ValueError(
                    "native detail is not the requested Fine/Shell control"
                )
        identity = [
            fine[-1]["metadata"][k]
            for k in ("original_parent", "original_source_ordinal")
        ]
        records.append(
            {
                "identity": identity,
                "fine": str(a.resolve()),
                "shell": str(b.resolve()),
                "sha256": {
                    str(p / name): hashlib.sha256((p / name).read_bytes()).hexdigest()
                    for p in (a, b)
                    for name in (
                        "input.json",
                        "native.json",
                        "mesh/receipt.json",
                        "subset.index",
                        "subset.forms",
                    )
                },
            }
        )
    if len({tuple(r["identity"]) for r in records}) != len(records):
        raise ValueError("duplicate original source membership")
    return join_sources(fine), join_sources(shell), native[0], records


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fine", type=Path, nargs="+", required=True)
    parser.add_argument("--shell", type=Path, nargs="+", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    out = args.output.resolve()
    if not out.is_relative_to(Path(tempfile.gettempdir()).resolve()):
        raise ValueError("fresh system-temp output required")
    fine, shell, native, inputs = load_group(args.fine, args.shell)
    out.mkdir(parents=True, exist_ok=False)
    up = np.array(native["anchor_ecef"])
    up /= np.linalg.norm(up)
    east = np.cross([0, 0, 1], up)
    east /= np.linalg.norm(east)
    north = np.cross(up, east)
    xyz = fine["positions"].astype(float)
    centre, extent = (xyz.min(0) + xyz.max(0)) / 2, np.ptp(xyz, axis=0).max()
    focal = native["view"]["focal_px"]
    suns = [0.3 * east - 0.4 * north + 0.8 * up, -0.6 * east + 0.3 * north + 0.5 * up]
    start = time.perf_counter()
    prepared = [prepare_source(s) for s in (fine, shell)]
    prepare_s = time.perf_counter() - start
    capture = camera(centre, east, north, up, extent, focal, 8, 0, 0)
    start = time.perf_counter()
    card = bake_card(fine, prepared[0], capture, suns[0])
    bake_s = time.perf_counter() - start
    for name in ("rgba", "matte"):
        Image.fromarray(card[name]).save(out / f"{name}.png")
    np.savez(
        out / "card.npz",
        **card["mesh"],
        uv=card["uv"],
        normal_moment=card["normal_moment"],
    )
    records = []
    cases = (
        [(8, 0, x) for x in (0, 32, 64)]
        + [(8, y, 32) for y in (5, 15, 45, 180)]
        + [(32, 0, x) for x in (0, 64)]
    )
    for distance, yaw, move in cases:
        view = camera(centre, east, north, up, extent, focal, distance, yaw, move)
        images, record = render_view(fine, shell, prepared, card, view, suns)
        name = f"view-{distance}-{yaw}-{move}"
        panel(out / f"{name}.png", images, name)
        np.savez_compressed(
            out / f"{name}.npz",
            **{
                f"{i}-{k}": v
                for i, light in enumerate(images)
                for k, v in light.items()
            },
        )
        record.update(
            distance_extents=distance,
            yaw_deg=yaw,
            translation_m=move,
            eye=view[0].tolist(),
            basis=view[1].tolist(),
        )
        records.append(record)
        print(json.dumps({"finished": name, "rays": record["sample_rays"]}), flush=True)
    mesh_bytes = sum(v.nbytes for v in card["mesh"].values()) + card["uv"].nbytes
    report = {
        "scope": "Two original source-complete controls, not full-world/native PBR/GPU admission. Same 1280x720 camera; four samples/pixel; unvalidated cards retain Fine fallback.",
        "inputs": inputs,
        "members": fine["members"],
        "source_extent_m": extent,
        "focal_px": focal,
        "suns": [s.tolist() for s in suns],
        "reference": "Trimesh triangle R-tree, all IDs before front cull; deterministic face-index depth tie. No Embree location deduplication.",
        "code_sha256": {
            p.name: hashlib.sha256(p.read_bytes()).hexdigest()
            for p in (
                Path(__file__).with_name(n)
                for n in (
                    Path(__file__).name,
                    "source_billboard.py",
                    "source_far_reprojection.py",
                    "source_far_assets.py",
                    "source_surface_fields.py",
                    "packed_normals.py",
                )
            )
        },
        "cost": {
            "fine_triangles": len(fine["indices"]),
            "fine_native_bytes": fine["native_bytes"],
            "shell_triangles": len(shell["indices"]),
            "shell_native_bytes": shell["native_bytes"],
            "card_triangles": 2,
            "card_mesh_uv_bytes": mesh_bytes,
            "texture_width_height": list(card["rgba"].shape[1::-1]),
            "matte_payload_bytes": card["matte"].nbytes + mesh_bytes,
            "live_payload_bytes": card["rgba"].nbytes
            + card["normal_moment"].nbytes
            + mesh_bytes,
            "bake_source_rays": card["source_rays"],
            "python_full_texture_array_allocations_per_combined_query_lower_bound": 9,
            "python_full_texture_array_bytes_per_combined_query_lower_bound": 280
            * card["rgba"].shape[0]
            * card["rgba"].shape[1],
            "prepare_reference_s": prepare_s,
            "bake_s": bake_s,
            "peak_process_rss_linux_kib": resource.getrusage(
                resource.RUSAGE_SELF
            ).ru_maxrss,
            "work_scope": "Two triangle card; four bilinear taps per covered sample/field, 4B matte or 12B base+normal payload per texel. Counts exclude mips, descriptors, membership, fallback, depth, cache/layout/allocator cost. Python/R-tree timings include library overhead and do not predict GPU speed. Each light separately queries card and computes BOTH methods. Allocations/copies/library-internal work not fully instrumented; RSS is process high water.",
        },
        "views": records,
    }
    (out / "report.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
