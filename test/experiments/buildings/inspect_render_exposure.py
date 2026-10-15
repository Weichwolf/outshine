"""Expose stored linear control radiance for inspection without changing its physical render."""

import argparse
import hashlib
import json
import math
from pathlib import Path

import mitsuba as mi
import numpy as np
from evidence import revision


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def inspect(report_path, exposure):
    if not math.isfinite(exposure) or not 0 < exposure <= 1e6:
        raise ValueError("Inspection exposure requires a finite multiplier in (0, 1e6]")
    original = json.loads(report_path.read_text())
    stem = report_path.name.removesuffix("-report.json")
    if stem == report_path.name or original.get("experiment_tree_dirty"):
        raise ValueError("Inspection requires a clean actual render report")
    source = report_path.with_name(stem + "-denoised.pfm")
    image = report_path.with_name(stem + ".png")
    if digest(image) != original["image_sha256"]:
        raise ValueError("Original image and render report disagree")
    source_hash = digest(source)
    mi.set_variant("llvm_ad_rgb")
    bitmap = mi.Bitmap(str(source))
    pixels = np.asarray(bitmap).copy()
    width, height = original["profile"]
    if pixels.shape != (height, width, 3) or not np.isfinite(pixels).all():
        raise ValueError("Inspection needs complete finite linear RGB radiance")
    if (
        exposure > 1
        and np.max(np.abs(pixels)) > float(np.finfo(pixels.dtype).max) / exposure
    ):
        raise ValueError("Inspection exposure would overflow its radiance values")
    output = report_path.with_name(stem + "-inspection.png")
    mi.util.write_bitmap(str(output), pixels * exposure, write_async=False)
    if digest(source) != source_hash:
        raise ValueError("Inspection changed its original radiance")
    record = revision() | {
        "status": "derived-inspection",
        "view": original["view"],
        "radiance_render_commit": original["commit"],
        "source_report": str(report_path.resolve()),
        "source_report_sha256": digest(report_path),
        "source_linear_radiance": str(source.resolve()),
        "source_linear_sha256": source_hash,
        "original_image_sha256": original["image_sha256"],
        "profile": original["profile"],
        "exposure_multiplier": exposure,
        "image_sha256": digest(output),
        "scope": "Display inspection only: positive linear exposure followed by the existing Mitsuba PNG/sRGB transfer, matching the native display's linear multiplier. No new tracing, auto-exposure, geometry, material or lighting change. Original fixed-exposure day/night controls remain unchanged; this derivative is not a paired energy comparison or native image proof.",
    }
    output.with_suffix(".json").write_text(json.dumps(record, indent=2) + "\n")
    print(
        json.dumps(
            {
                "output": str(output),
                "status": record["status"],
                "exposure_multiplier": exposure,
            }
        )
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("report", type=Path)
    parser.add_argument("--exposure", type=float, default=8.0)
    args = parser.parse_args()
    inspect(args.report, args.exposure)
