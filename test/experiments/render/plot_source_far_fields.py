"""Inspect unlit capture fields and original-native hull depth on identical poses.

The field sheet is not a native material rendering. Reprojection of the layered
candidate and full appearance/coverage acceptance remain separate work.
"""

import argparse
import json
import tempfile
from pathlib import Path

import numpy as np
from packed_normals import unpack
from PIL import Image, ImageDraw
from plot_surface_parent import depth, shade_depth
from source_far_assets import load_source


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--capture", type=Path, required=True)
    p.add_argument("--fine", type=Path, required=True)
    p.add_argument("--hull", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    a = p.parse_args()
    out = a.output.resolve()
    if not out.is_relative_to(Path(tempfile.gettempdir()).resolve()):
        raise ValueError("fresh system-temp output required")
    out.mkdir(parents=True, exist_ok=False)
    report = json.loads((a.capture / "report.json").read_text())
    palette = np.asarray(report["palette"])
    sheet = Image.new("RGB", (1536, 1056), "white")
    draw = ImageDraw.Draw(sheet)
    light_a = np.array([0.3, -0.4, 0.8])
    light_a /= np.linalg.norm(light_a)
    light_b = np.array([-0.6, 0.3, 0.5])
    light_b /= np.linalg.norm(light_b)
    for i, info in enumerate(report["views"]):
        with np.load(a.capture / f"view-{i}.npz", allow_pickle=False) as data:
            n = info["resolution"]
            valid = np.isfinite(data["depth"][: n * n])
            norm = unpack(data["normal"][: n * n])
            code = data["code"][: n * n].astype(np.uint32)
            false = (
                np.column_stack(
                    (
                        (code * 97 + 53) % 255,
                        (code * 31 + 119) % 255,
                        (code * 67 + 29) % 255,
                    )
                )
                / 255
            )
            colour = palette[data["colour"][: n * n], :3]
            images = [(norm + 1) * 0.5, false]
            images.extend(
                np.clip(colour * (0.2 + 0.8 * np.maximum(norm @ sun, 0))[:, None], 0, 1)
                for sun in (light_a, light_b)
            )
            for row, (title, image) in enumerate(
                zip(
                    (
                        "Native normal field",
                        "Material program ID (false colour)",
                        "Colour factor + live Lambert A",
                        "Colour factor + live Lambert B",
                    ),
                    images,
                    strict=True,
                )
            ):
                rgb = np.where(valid[:, None], np.clip(image, 0, 1), 0).reshape(n, n, 3)
                img = Image.fromarray((rgb * 255).astype(np.uint8)).resize((256, 240))
                sheet.paste(img, (256 * i, row * 264 + 24))
                draw.text(
                    (256 * i + 4, row * 264 + 5), f"View {i}: {title}", fill="black"
                )
    sheet.save(out / "fields.png")
    fine, hull = load_source(a.fine), load_source(a.hull)
    xyz = fine["positions"].astype(float)
    centre = (xyz.min(0) + xyz.max(0)) / 2
    native = json.loads((a.fine / "native.json").read_text())
    anchor = np.array(native["anchor_ecef"])
    up = anchor / np.linalg.norm(anchor)
    east = np.cross([0, 0, 1], up)
    east /= np.linalg.norm(east)
    north = np.cross(up, east)
    distance = 4 * np.ptp(xyz, axis=0).max()
    rows = []
    for yaw in (0, 90, 180, 270):
        angle = np.radians(yaw)
        eye = centre + distance * (
            np.cos(angle) * east + np.sin(angle) * north + 0.3 * up
        )
        forward = centre - eye
        forward /= np.linalg.norm(forward)
        right = np.cross(forward, up)
        right /= np.linalg.norm(right)
        basis = np.stack((right, np.cross(right, forward), forward))
        for move in (0, 32):
            view = (eye + move * right, basis, native["view"]["focal_px"], 1280, 720)
            before, after = (
                depth(s["positions"], s["indices"], view) for s in (fine, hull)
            )
            lost = (before > 0) & (after == 0)
            gained = (before == 0) & (after > 0)
            both = (before > 0) & (after > 0)
            dz = np.zeros_like(before)
            dz[both] = abs(0.1 / before[both] - 0.1 / after[both])
            diff = np.repeat(shade_depth(before)[:, :, None], 3, axis=2)
            diff[lost] = (255, 40, 40)
            diff[gained] = (40, 150, 255)
            diff[both & (dz > 1e-5)] = (255, 220, 30)
            images = [
                Image.fromarray(shade_depth(x)).convert("RGB") for x in (before, after)
            ] + [Image.fromarray(diff)]
            panel = Image.new("RGB", (1920, 390), "white")
            for i, (label, img) in enumerate(
                zip(
                    (
                        "Original Fine depth",
                        "Original native Shell depth",
                        "Red lost / blue gained / yellow depth",
                    ),
                    images,
                    strict=True,
                )
            ):
                panel.paste(img.resize((640, 360)), (640 * i, 30))
                ImageDraw.Draw(panel).text((640 * i + 8, 8), label, fill="black")
                img.save(out / f"{yaw}-{move}-{i}.png")
            panel.save(out / f"{yaw}-{move}-comparison.png")
            rows.append(
                {
                    "yaw": yaw,
                    "translation_right_m": move,
                    "eye": view[0].tolist(),
                    "focal_px": view[2],
                    "fine_coverage": int((before > 0).sum()),
                    "lost_samples": int(lost.sum()),
                    "gained_samples": int(gained.sum()),
                    "changed_depth_samples": int((both & (dz > 1e-5)).sum()),
                }
            )
    (out / "report.json").write_text(
        json.dumps(
            {
                "scope": "Isolated source diagnostic orbit, 1280x720 original focal, identical Fine/Shell poses. Field sheet is unlit attributes plus diagnostic relighting, not native appearance or a layered novel-view renderer.",
                "cases": rows,
            },
            indent=2,
        )
        + "\n"
    )


if __name__ == "__main__":
    main()
