"""View the actual certified/fallback primitive partitions as depth-only diagnostics."""

import argparse
import hashlib
import json
import tempfile
from collections import defaultdict
from pathlib import Path

import numpy as np
from current_visibility import camera
from facing_inputs import buildings
from PIL import Image, ImageDraw
from plot_surface_parent import depth, join, shade_depth


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--comparison", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    out = args.output.resolve()
    if not out.is_relative_to(Path(tempfile.gettempdir()).resolve()):
        raise ValueError("fresh system-temp output required")
    report = json.loads((args.comparison / "report.json").read_text())
    if (
        hashlib.sha256((args.inputs / "manifest.json").read_bytes()).hexdigest()
        != report["input_manifest_sha256"]
    ):
        raise ValueError("wrong original inputs")
    out.mkdir(parents=True, exist_ok=False)
    entries = {m["key"]: m for m in report["models"]}
    groups = defaultdict(lambda: {"original": [], "parent": []})
    for model in buildings(args.inputs):
        entry = entries[model["key"]]
        if entry["original_triangles"] != len(model["indices"]):
            raise ValueError(
                "partial cluster pilot cannot represent full original streams"
            )
        group = groups[model["group"]]
        group["scene"] = model["scene"]
        group["original"].append((model["vertices"], model["indices"]))
        with np.load(args.comparison / entry["file"], allow_pickle=False) as data:
            tri = data["triangles"]
        if tri.shape != (entry["selected_triangles"], 3, 3):
            raise ValueError("wrong parent geometry")
        group["parent"].append(
            (tri.reshape(-1, 3), np.arange(tri.size // 3).reshape(-1, 3))
        )
    rows = []
    for name, group in groups.items():
        original, parent = join(group["original"]), join(group["parent"])
        for yaw in (0, 90, 180, 270):
            view = camera(group["scene"], original[0], 1280, 720, np.radians(yaw))
            for motion in (0, 32):
                eye, basis, focal, width, height = view
                moved = (eye + motion * basis[0], basis, focal, width, height)
                before, after = (
                    depth(*geometry, moved) for geometry in (original, parent)
                )
                lost, gained = (before > 0) & (after == 0), (before == 0) & (after > 0)
                both = (before > 0) & (after > 0)
                delta = np.zeros_like(before)
                delta[both] = abs(0.1 / before[both] - 0.1 / after[both])
                image = np.repeat(shade_depth(before)[:, :, None], 3, axis=2)
                image[lost] = (255, 40, 40)
                image[gained] = (40, 150, 255)
                image[both & (delta > 1e-5)] = (255, 220, 30)
                pictures = [
                    Image.fromarray(shade_depth(x)).convert("RGB")
                    for x in (before, after)
                ] + [Image.fromarray(image)]
                tag = f"{name[:12]}-{yaw}-{motion}"
                panel = Image.new("RGB", (1920, 390), "white")
                for i, (label, picture) in enumerate(
                    zip(
                        (
                            "Original native geometry",
                            "Certified geometry + original fallback",
                            "Red lost / blue gained / yellow depth",
                        ),
                        pictures,
                        strict=True,
                    )
                ):
                    panel.paste(picture.resize((640, 360)), (640 * i, 30))
                    ImageDraw.Draw(panel).text((640 * i + 8, 8), label, fill="black")
                    picture.save(out / f"{tag}-{i}.png")
                panel.save(out / f"{tag}-comparison.png")
                row = {
                    "cohort": name,
                    "yaw": yaw,
                    "motion_right_m": motion,
                    "covered_pixels": int((before > 0).sum()),
                    "lost_pixels": int(lost.sum()),
                    "gained_pixels": int(gained.sum()),
                    "changed_depth_pixels": int((both & (delta > 1e-5)).sum()),
                    "max_depth_change_m": float(delta.max()),
                    "original_triangles": len(original[1]),
                    "parent_triangles": len(parent[1]),
                    "original_depth_sha256": hashlib.sha256(
                        before.tobytes()
                    ).hexdigest(),
                    "parent_depth_sha256": hashlib.sha256(after.tobytes()).hexdigest(),
                }
                rows.append(row)
                print(json.dumps(row), flush=True)
    (out / "report.json").write_text(
        json.dumps(
            {
                "scope": "One-sided pixel-centre depth only; no native attributes, materials, world or device gate. Same full cached streams, original eye, 32m translation and 1280x720/focal profile.",
                "cases": rows,
            },
            indent=2,
        )
        + "\n"
    )


if __name__ == "__main__":
    main()
