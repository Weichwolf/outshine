"""RGB8 whole-frame/affected-region errors and enlarged original-pixel diagnostics."""

import argparse
import hashlib
import json
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw


def compare(first, second):
    a = np.array(Image.open(first).convert("RGB"))
    b = np.array(Image.open(second).convert("RGB"))
    if a.shape != b.shape:
        raise ValueError("Different image profiles")
    error = np.abs(a.astype(np.int16) - b)
    maximum = error.max(2)
    ys, xs = np.nonzero(maximum)
    box = (
        [int(xs.min()), int(ys.min()), int(xs.max()) + 1, int(ys.max()) + 1]
        if len(xs)
        else None
    )
    crop = error[box[1] : box[3], box[0] : box[2]] if box else error[:0]
    result = {
        "changed_pixels": int((maximum > 0).sum()),
        "over_one_pixels": int((maximum > 1).sum()),
        "maximum_rgb8_error": int(error.max()),
        "mean_rgb8_error_whole_frame": float(error.mean()),
        "changed_region": box,
        "mean_rgb8_error_changed_region": float(crop.mean()) if crop.size else 0,
        "original_sha256": hashlib.sha256(first.read_bytes()).hexdigest(),
        "candidate_sha256": hashlib.sha256(second.read_bytes()).hexdigest(),
    }
    return result, (a, b, error)


def panel(groups, output, title):
    sheet = Image.new("RGB", (1260, 165 * len(groups) + 35), "#303030")
    draw = ImageDraw.Draw(sheet)
    draw.text(
        (10, 8),
        title + " | Original Shell / Profile / absolute RGB8 error x8",
        fill="white",
    )
    reports = []
    for row, (label, first, second) in enumerate(groups):
        result, (a, b, error) = compare(first, second)
        reports.append({"case": label, **result})
        if result["changed_region"]:
            x0, y0, x1, y1 = result["changed_region"]
            x0, y0, x1, y1 = (
                max(x0 - 5, 0),
                max(y0 - 5, 0),
                min(x1 + 5, a.shape[1]),
                min(y1 + 5, a.shape[0]),
            )
        else:
            x0, y0, x1, y1 = 0, 0, a.shape[1], a.shape[0]
        scale = min(8, 400 / (x1 - x0), 112 / (y1 - y0))
        size = (max(1, int((x1 - x0) * scale)), max(1, int((y1 - y0) * scale)))
        for column, values in enumerate(
            [a, b, np.minimum(error * 8, 255).astype(np.uint8)]
        ):
            image = (
                Image.fromarray(values)
                .crop((x0, y0, x1, y1))
                .resize(size, Image.Resampling.NEAREST)
            )
            sheet.paste(image, (10 + column * 420, 70 + row * 165))
        draw.text(
            (10, 40 + row * 165),
            f"{label}: max={result['maximum_rgb8_error']}, changed={result['changed_pixels']}, over1={result['over_one_pixels']}, crop scale={scale:.2f}",
            fill="white",
        )
    sheet.save(output)
    return reports


def run(args):
    args.output.mkdir(parents=True, exist_ok=False)
    matrix = json.loads((args.input / "render-status.json").read_text())
    lookup = {
        (r["pose"], r["light"], r["strategy"]): Path(r["directory"]) / "scene.png"
        for r in matrix
    }
    rows = []
    for light in ["day", "back", "night"]:
        groups = [
            (pose, lookup[(pose, light, "original")], lookup[(pose, light, "profile")])
            for pose in ["static-020", "moving-020", "moving-021", "near-control"]
        ]
        rows.extend(
            {"light": light, **r}
            for r in panel(groups, args.output / f"{light}.png", light)
        )
    if (args.input / "transition-render-status.json").exists():
        matrix = json.loads((args.input / "transition-render-status.json").read_text())
        lookup = {
            (r["pose"], r["strategy"]): Path(r["directory"]) / "scene.png"
            for r in matrix
        }
        groups = [
            (pose, lookup[(pose, "original")], lookup[(pose, "profile")])
            for pose in ["before", "after"]
        ]
        groups += [
            (
                "reference motion",
                lookup[("before", "original")],
                lookup[("after", "original")],
            ),
            (
                "profile motion",
                lookup[("before", "profile")],
                lookup[("after", "profile")],
            ),
        ]
        rows.extend(
            {"light": "transition-day", **r}
            for r in panel(
                groups,
                args.output / "transition.png",
                "Genuine 0.25m LOD-switch bracket",
            )
        )
    report = {
        "comparisons": rows,
        "scope": "RGB8 output errors, full frames plus bounding rectangle of changed pixels. Enlarged crops preserve original pixels; error display scaled x8. Neutral opaque PBR, not native material/temporal/whole-world admission.",
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    for row in rows:
        print(
            row["light"],
            row["case"],
            "max",
            row["maximum_rgb8_error"],
            "changed",
            row["changed_pixels"],
            "over1",
            row["over_one_pixels"],
        )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
