"""Same-pose Fine/Shell/layered diagnostic lighting, coverage and query work.

Stored colours are factors, not native material evaluation. Missing/extra samples
are reported before fallback; all unvalidated layered products keep Fine fallback.
"""

import argparse
import hashlib
import json
import resource
import tempfile
import time
from pathlib import Path

import numpy as np
from far_voxel_preflight import voxel_preflight
from packed_normals import unpack
from PIL import Image, ImageDraw
from source_far_assets import load_source
from source_far_reprojection import (
    field_bounds,
    intersect_field,
    intersect_source,
    view_rays,
)


def surface_fields(source, faces):
    valid = faces >= 0
    normal = np.zeros((len(faces), 3))
    colour = np.zeros((len(faces), 3))
    code = np.zeros(len(faces), np.int16)
    normal[valid] = unpack(source["normal"][faces[valid]])
    colour[valid] = source["palette"][source["colour_index"][faces[valid]], :3]
    code[valid] = source["code"][faces[valid]]
    return {"normal": normal, "colour": colour, "code": code}


def relight(fields, sun, depth):
    sun = np.array(sun, float)
    sun /= np.linalg.norm(sun)
    light = 0.2 + 0.8 * np.maximum(fields["normal"] @ sun, 0)
    rgb = np.clip(fields["colour"] * light[:, None], 0, 1)
    return np.where(np.isfinite(depth)[:, None], rgb, 0)


def compare(before, after, a, b):
    both = np.isfinite(before) & np.isfinite(after)
    angle = np.degrees(
        np.arccos(np.clip((a["normal"][both] * b["normal"][both]).sum(1), -1, 1))
    )
    return {
        "coverage": int(np.isfinite(after).sum()),
        "lost_samples": int((np.isfinite(before) & ~np.isfinite(after)).sum()),
        "gained_samples": int((~np.isfinite(before) & np.isfinite(after)).sum()),
        "depth_over_1cm_samples": int((abs(before[both] - after[both]) > 0.01).sum()),
        "material_program_changed_samples": int(
            (a["code"][both] != b["code"][both]).sum()
        ),
        "normal_over_1_degree_samples": int((angle > 1).sum()),
        "max_depth_difference_m": float(abs(before[both] - after[both]).max(initial=0)),
    }


def query_fields(capture, origins, directions, palette):
    t, slots, work = intersect_field(capture, origins, directions)
    fields = {
        "normal": np.zeros((len(t), 3)),
        "colour": np.zeros((len(t), 3)),
        "code": np.zeros(len(t), np.int16),
    }
    valid = slots >= 0
    fields["normal"][valid] = unpack(capture["normal"][slots[valid]])
    fields["colour"][valid] = palette[capture["colour"][slots[valid]], :3]
    fields["code"][valid] = capture["code"][slots[valid]]
    return t, fields, work


def save_panel(path, depths, fields, pixel_ids, view):
    panel = Image.new("RGB", (480 * len(depths), 1140), "white")
    titles = ("Fine", "Native Shell", "Layered closest axis", "Layered compatible axes")
    for col, (title, t, f) in enumerate(
        zip(titles[: len(depths)], depths, fields, strict=True)
    ):
        for row, sun in enumerate(([0.3, -0.4, 0.8], [-0.6, 0.3, 0.5])):
            image = np.zeros((view[4], view[3], 3), np.uint8)
            image[pixel_ids] = (255 * relight(f, sun, t)).astype(np.uint8)
            panel.paste(
                Image.fromarray(image).resize((480, 270)), (480 * col, row * 380 + 30)
            )
        image = np.zeros((view[4], view[3], 3), np.uint8)
        both = np.isfinite(t) & np.isfinite(depths[0])
        colours = np.zeros((len(t), 3), np.uint8)
        colours[both] = (80, 80, 80)
        colours[np.isfinite(depths[0]) & ~np.isfinite(t)] = (255, 40, 40)
        colours[~np.isfinite(depths[0]) & np.isfinite(t)] = (40, 150, 255)
        colours[
            both & (abs(np.where(both, t, 0) - np.where(both, depths[0], 0)) > 0.01)
        ] = (255, 220, 30)
        image[pixel_ids] = colours
        panel.paste(Image.fromarray(image).resize((480, 270)), (480 * col, 790))
        draw = ImageDraw.Draw(panel)
        draw.text((480 * col + 5, 8), title + ": diagnostic live light A", fill="black")
        draw.text(
            (480 * col + 5, 388), title + ": diagnostic live light B", fill="black"
        )
        draw.text(
            (480 * col + 5, 768),
            "Red lost / blue gained / yellow depth > 1cm",
            fill="black",
        )
    panel.save(path)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ("fine", "hull", "output"):
        p.add_argument("--" + name, type=Path, required=True)
    p.add_argument("--capture", type=Path)
    p.add_argument("--distance-extents", type=float, nargs="+", default=[4, 8])
    a = p.parse_args()
    if not all(np.isfinite(x) and 1 <= x <= 1024 for x in a.distance_extents):
        raise ValueError("bounded orbit distances required")
    out = a.output.resolve()
    if not out.is_relative_to(Path(tempfile.gettempdir()).resolve()):
        raise ValueError("fresh system-temp output required")
    out.mkdir(parents=True, exist_ok=False)
    fine, hull = load_source(a.fine), load_source(a.hull)
    capture_report = (
        json.loads((a.capture / "report.json").read_text()) if a.capture else None
    )
    if capture_report and capture_report["source"] != fine["metadata"]:
        raise ValueError("capture belongs to another source")
    for name in ("subset.index", "subset.forms"):
        if (a.fine / name).read_bytes() != (a.hull / name).read_bytes():
            raise ValueError("hull changes source")
    captures = []
    bounds = [fine["positions"], hull["positions"]]
    for i in range(len(capture_report["views"]) if capture_report else 0):
        with np.load(a.capture / f"view-{i}.npz", allow_pickle=False) as f:
            data = dict(f)
        captures.append(data)
        low, high, _, _ = field_bounds(data)
        corners = np.array(
            [
                [x, y, z]
                for x in (low[0], high[0])
                for y in (low[1], high[1])
                for z in (low[2], high[2])
            ]
        )
        bounds.append(data["centre"] + corners @ data["basis"])
    bounds = np.concatenate(bounds)
    xyz = fine["positions"].astype(float)
    centre = (xyz.min(0) + xyz.max(0)) / 2
    native = json.loads((a.fine / "native.json").read_text())
    up = np.array(native["anchor_ecef"])
    up /= np.linalg.norm(up)
    east = np.cross([0, 0, 1], up)
    east /= np.linalg.norm(east)
    north = np.cross(up, east)
    records = []
    for yaw in (0, 44, 46, 90, 180, 270):
        angle = np.radians(yaw)
        for scale in a.distance_extents:
            eye = centre + scale * np.ptp(xyz, axis=0).max() * (
                np.cos(angle) * east + np.sin(angle) * north + 0.3 * up
            )
            forward = centre - eye
            forward /= np.linalg.norm(forward)
            right = np.cross(forward, up)
            right /= np.linalg.norm(right)
            basis = np.stack((right, np.cross(right, forward), forward))
            for move in (0, 32):
                view = (
                    eye + move * right,
                    basis,
                    native["view"]["focal_px"],
                    1280,
                    720,
                )
                origins, directions, pixels = view_rays(view, bounds)
                start = time.perf_counter()
                depths, fields = [], []
                for source in (fine, hull):
                    t, faces = intersect_source(source, origins, directions)
                    depths.append(t)
                    fields.append(surface_fields(source, faces))
                order = sorted(
                    range(len(captures)),
                    key=lambda i: float(captures[i]["basis"][2] @ forward),
                    reverse=True,
                )
                order = [i for i in order if captures[i]["basis"][2] @ forward > 0.01]
                work = []
                best = np.full(len(origins), np.inf)
                merged = None
                for j, i in enumerate(order):
                    t, f, w = query_fields(
                        captures[i], origins, directions, fine["palette"]
                    )
                    work.append({"capture": i, **w})
                    if j == 0:
                        depths.append(t.copy())
                        fields.append({k: v.copy() for k, v in f.items()})
                        merged = {k: np.zeros_like(v) for k, v in f.items()}
                    replace = t < best
                    best[replace] = t[replace]
                    for k in merged:
                        merged[k][replace] = f[k][replace]
                if order:
                    depths.append(best)
                    fields.append(merged)
                row = {
                    "yaw": yaw,
                    "distance_extent_multiple": scale,
                    "translation_right_m": move,
                    "eye": view[0].tolist(),
                    "basis": basis.tolist(),
                    "focal_px": view[2],
                    "width": 1280,
                    "height": 720,
                    "query_rays": len(origins),
                    "captures": order,
                    "fine_coverage": int(np.isfinite(depths[0]).sum()),
                    "work": work,
                    "seconds_including_native_reference": time.perf_counter() - start,
                    "comparisons": {
                        name: compare(depths[0], depths[i], fields[0], fields[i])
                        for i, name in enumerate(
                            ("fine", "shell", "closest_axis", "compatible_axes")[
                                : len(depths)
                            ]
                        )
                        if i
                    },
                }
                hit = np.isfinite(depths[0])
                row["voxel_preflight"] = voxel_preflight(
                    xyz,
                    view[0],
                    origins[hit] + depths[0][hit, None] * directions[hit],
                    extent_multiple=scale,
                    focal_px=view[2],
                    width=1280,
                    height=720,
                )
                tag = f"{yaw}-{scale}-{move}"
                save_panel(out / (tag + ".png"), depths, fields, pixels, view)
                records.append(row)
                print(json.dumps(row), flush=True)
    report = {
        "cases": records,
        "source": fine["metadata"],
        "native_meshes": {
            name: {
                "triangles": len(source["indices"]),
                "stored_vertices": len(source["positions"]),
                "index_references": int(source["indices"].size),
                "raw_used_bytes": source["native_bytes"],
            }
            for name, source in (("fine", fine), ("shell", hull))
        },
        "input_sha256": {
            "fine_mesh_receipt": hashlib.sha256(
                (a.fine / "mesh/receipt.json").read_bytes()
            ).hexdigest(),
            "hull_mesh_receipt": hashlib.sha256(
                (a.hull / "mesh/receipt.json").read_bytes()
            ).hexdigest(),
            "capture_report": hashlib.sha256(
                (a.capture / "report.json").read_bytes()
            ).hexdigest()
            if a.capture
            else None,
            **{
                f"capture_view_{i}": hashlib.sha256(
                    (a.capture / f"view-{i}.npz").read_bytes()
                ).hexdigest()
                for i in range(len(captures))
            },
        },
        "peak_rss_kib": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,
        "code_sha256": {
            name: hashlib.sha256(
                Path(__file__).with_name(name).read_bytes()
            ).hexdigest()
            for name in (
                Path(__file__).name,
                "source_far_reprojection.py",
                "source_far_assets.py",
                "far_voxel_preflight.py",
            )
        },
        "scope": "Same 1280x720/original-focal poses. Analytic front-facing native triangles versus stored sample-and-hold tangent-cell fields; lighting changes at query time. Colour factors plus Lambert, not native PBR/UV evaluation. No native frame or worldwide acceptance.",
        "fallback": "All layered proposals remain unvalidated; retain original Fine. Metrics precede fallback; no layered opaque occluder admission or synthetic SurfaceError.",
        "open": [
            "native material/UV evaluation",
            "continuous near/far and axis switching",
            "geographic/source controls",
            "native CPU/GPU/memory costs",
        ],
    }
    (out / "report.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
