"""Actual 1280x720 depth diagnostics; original materials/shaders are not simulated."""

import argparse
import hashlib
import json
import tempfile
from pathlib import Path

import numpy as np
from clustered_surface_parent import cluster_parent
from current_visibility import camera, raster
from facing_images import clip_near
from facing_inputs import buildings
from PIL import Image, ImageDraw


def depth(xyz, ids, view):
    eye, basis, focal, width, height = view
    triangles = clip_near(((xyz.astype(float) - eye) @ basis.T)[ids])
    if not len(triangles):
        return np.zeros((height, width))
    p = triangles.reshape(-1, 3)
    projected = np.column_stack(
        (
            width / 2 + focal * p[:, 0] / p[:, 2],
            height / 2 - focal * p[:, 1] / p[:, 2],
            0.1 / p[:, 2],
        )
    )
    return raster(
        projected,
        np.maximum(p[:, 2], np.nextafter(0.1, np.inf)),
        np.arange(len(p)).reshape(-1, 3),
        width,
        height,
    )


def join(parts):
    vertices, triangles, offset = [], [], 0
    for xyz, ids in parts:
        vertices.append(xyz)
        triangles.append(ids.astype(np.int64) + offset)
        offset += len(xyz)
    return np.concatenate(vertices), np.concatenate(triangles)


def shade_depth(depth_image):
    return np.where(
        depth_image > 0,
        64 + 191 * np.sqrt(np.clip(depth_image * 50, 0, 1)),
        0,
    ).astype(np.uint8)


def screen_correspondence(pairs, view):
    eye, basis, focal, width, height = view
    maximum, checked = 0.0, 0
    for original, mapped in pairs:
        p, q = ((x.reshape(-1, 3) - eye) @ basis.T for x in (original, mapped))
        positive = (p[:, 2] > 0.1) & (q[:, 2] > 0.1)
        p, q = p[positive], q[positive]
        p = focal * p[:, :2] / p[:, 2, None]
        q = focal * q[:, :2] / q[:, 2, None]
        visible = (abs(p) <= [width / 2, height / 2]).all(axis=1) | (
            abs(q) <= [width / 2, height / 2]
        ).all(axis=1)
        delta = np.linalg.norm(p[visible] - q[visible], axis=1)
        maximum = max(maximum, float(delta.max(initial=0)))
        checked += len(delta)
    if maximum > 1 + 1e-10:
        raise AssertionError("real correspondence exceeds one-pixel bound")
    return {
        "sampled_corner_pairs_in_view": checked,
        "max_sampled_pixel_displacement": maximum,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--comparison", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--cell-m", type=float, default=0.5)
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(Path(tempfile.gettempdir()).resolve()):
        raise ValueError("system temporary output required")
    output.mkdir(parents=True, exist_ok=False)
    metadata = json.loads((args.comparison / "report.json").read_text())
    if (
        hashlib.sha256((args.inputs / "manifest.json").read_bytes()).hexdigest()
        != metadata["input_manifest_sha256"]
    ):
        raise ValueError("comparison belongs to different original inputs")
    for filename, expected in metadata["code_sha256"].items():
        if (
            hashlib.sha256(Path(__file__).with_name(filename).read_bytes()).hexdigest()
            != expected
        ):
            raise ValueError("comparison code changed: " + filename)
    rows = [
        json.loads(line)
        for line in (args.comparison / "rows.jsonl").read_text().splitlines()
    ]
    selection = {row["key"]: row for row in rows if row["cell_m"] == args.cell_m}
    models = {}
    for model in buildings(args.inputs):
        data = models.setdefault(
            model["group"],
            {"original": [], "selected": [], "scene": model["scene"], "pairs": []},
        )
        xyz, ids = model["vertices"], model["indices"]
        data["original"].append((xyz, ids))
        if selection[model["key"]]["geometry_bound_passes"]:
            parent = cluster_parent(xyz, ids, cell_m=args.cell_m)
            data["selected"].append((parent.positions, parent.triangles))
            original = xyz[ids].astype(float)
            mapped = xyz[parent.corner_map].astype(float)
            mapped[parent.mode == 0] = original[parent.mode == 0]
            data["pairs"].append((original, mapped))
        else:
            data["selected"].append((xyz, ids))
    reports = []
    for group, data in models.items():
        original, selected = join(data["original"]), join(data["selected"])
        for yaw in (0.0, 90.0, 180.0, 270.0):
            view = camera(data["scene"], original[0], 1280, 720, np.radians(yaw))
            for motion in (0.0, 32.0):
                eye, basis, focal, width, height = view
                moved = (eye + motion * basis[0], basis, focal, width, height)
                before, after = (
                    depth(*geometry, moved) for geometry in (original, selected)
                )
                lost, gained = (before > 0) & (after == 0), (before == 0) & (after > 0)
                both = (before > 0) & (after > 0)
                dz = np.zeros_like(before)
                dz[both] = abs(0.1 / before[both] - 0.1 / after[both])
                shade = shade_depth(before)
                change = np.repeat(shade[:, :, None], 3, axis=2)
                change[lost] = (255, 40, 40)
                change[gained] = (40, 150, 255)
                change[both & (dz > 1e-5)] = (255, 220, 30)
                images = [
                    Image.fromarray(shade_depth(x)).convert("RGB")
                    for x in (before, after)
                ] + [Image.fromarray(change)]
                tag = f"{group[:12]}-{yaw:g}-{motion:g}"
                for name, img in zip(
                    ("original", "parent", "difference"), images, strict=True
                ):
                    img.save(output / f"{tag}-{name}.png")
                panel = Image.new("RGB", (1920, 390), "white")
                for i, (title, img) in enumerate(
                    zip(
                        (
                            "Original native cached geometry",
                            "Conditional geometry parent",
                            "Red lost / blue gained / yellow depth",
                        ),
                        images,
                        strict=True,
                    )
                ):
                    panel.paste(img.resize((640, 360)), (i * 640, 30))
                    ImageDraw.Draw(panel).text((i * 640 + 8, 8), title, fill="black")
                panel.save(output / f"{tag}-comparison.png")
                report = {
                    "cohort": group,
                    "yaw_deg": yaw,
                    "motion_right_m": motion,
                    "cell_m": args.cell_m,
                    "original_triangles": len(original[1]),
                    "selected_triangles": len(selected[1]),
                    "original_covered_pixels": int((before > 0).sum()),
                    "lost_pixels": int(lost.sum()),
                    "gained_pixels": int(gained.sum()),
                    "changed_depth_pixels": int((both & (dz > 1e-5)).sum()),
                    "max_common_depth_change_m": float(dz.max()),
                    "original_depth_sha256": hashlib.sha256(
                        before.tobytes()
                    ).hexdigest(),
                    "parent_depth_sha256": hashlib.sha256(after.tobytes()).hexdigest(),
                    "eye": eye.tolist(),
                    "actual_eye": moved[0].tolist(),
                    "focal_px": focal,
                    "independent_screen_correspondence": screen_correspondence(
                        data["pairs"], moved
                    ),
                }
                reports.append(report)
                print(json.dumps(report), flush=True)
    (output / "report.json").write_text(
        json.dumps(
            {
                "scope": "Pixel-centre one-sided depth only, incomplete world, no materials or runtime acceptance. Whole observer-box certificate reused for 32m translation.",
                "cases": reports,
            },
            indent=2,
        )
        + "\n"
    )


if __name__ == "__main__":
    main()
