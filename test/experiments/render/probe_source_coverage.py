"""Inspect source-only footprint aggregation before choosing a far renderer."""

import argparse
import hashlib
import json
import resource
import subprocess
import time
from pathlib import Path

import numpy as np
import shapely
from parent_surface_inputs import open_parent
from PIL import Image, ImageDraw
from source_footprint_groups import coverage_groups, read_footprints, union_inventory


def diagnostic(geometry, groups, report, output):
    fractions, low, pitch = (
        groups[key] for key in ("fractions", "low_xy_m", "pitch_m")
    )
    ny, nx = fractions.shape
    scale = min(2048 / nx, 2048 / ny)
    width, height = max(1, round(nx * scale)), max(1, round(ny * scale))
    x = low[0] + (np.arange(width) + 0.5) * nx * pitch / width
    y = low[1] + (np.arange(height) + 0.5) * ny * pitch / height
    raster = shapely.contains_xy(geometry, x[None, :], y[:, None])
    image = Image.new("RGB", (1280, 480), "#202020")
    draw = ImageDraw.Draw(image)
    masks = [raster.astype(float), (fractions > 0).astype(float), fractions]
    titles = [
        "Original floor footprints",
        "Opaque any-touch groups",
        "Plan-area averages (not ray opacity)",
    ]
    for column, (mask, title) in enumerate(zip(masks, titles, strict=True)):
        panel = Image.fromarray(np.round(np.flipud(mask) * 255).astype(np.uint8))
        scale = min(404 / nx, 350 / ny)
        size = max(1, round(nx * scale)), max(1, round(ny * scale))
        panel = panel.resize(size, Image.Resampling.NEAREST)
        image.paste(panel, (8 + column * 426, 35))
        draw.text((8 + column * 426, 12), title, fill="white")
    draw.text(
        (8, 410),
        f"{report['sources']} original sources; {report['union_components']} disjoint footprint components; {fractions.size} group cells",
        fill="white",
    )
    draw.text(
        (8, 433),
        f"Area: {geometry.area:.2f} m2; opaque groups: {report['opaque_any_touch_area_m2']:.2f} m2; filtered: {report['integrated_coverage_m2']:.2f} m2",
        fill="white",
    )
    draw.text(
        (8, 456),
        "Plan projection only: no height, skyline, shading, caster, readiness or native/world admission.",
        fill="white",
    )
    image.save(output)
    return {
        "reference_pixels": [width, height],
        "raster_coverage_integral_m2": float(raster.mean() * nx * ny * pitch**2),
        "geometric_coverage_integral_m2": float(geometry.area),
    }


def run(args):
    args.output.mkdir(parents=True, exist_ok=False)
    started = time.perf_counter()
    with open_parent(args.input) as parent:
        opened = time.perf_counter()
        geometries, registry, report = read_footprints(parent, set(args.cells))
        projected = time.perf_counter()
        geometry, inventory = union_inventory(geometries)
        united = time.perf_counter()
        groups, measures = coverage_groups(geometry, args.pitch_m)
        filtered = time.perf_counter()
        arrays = groups | registry
        np.savez_compressed(args.output / "groups.npz", **arrays)
        report.update(
            inventory,
            **measures,
            parent=parent.parent,
            cells=args.cells,
            source_archive_sha256=parent.archive_sha256,
            source_archive_bytes=args.input.stat().st_size,
            parent_source_records=len(parent.cells),
            selected_source_records=len(registry["source_ids"]),
            supported_source_records=int(registry["source_supported"].sum()),
            form_reads=parent.form_reads,
            form_bytes=parent.form_bytes,
            open_parent_seconds=opened - started,
            form_projection_seconds=projected - opened,
            union_seconds=united - projected,
            coverage_seconds=filtered - united,
            stored_array_bytes={key: value.nbytes for key, value in arrays.items()},
            builder_process_peak_rss_bytes=resource.getrusage(
                resource.RUSAGE_SELF
            ).ru_maxrss
            * 1024,
        )
    report["diagnostic"] = diagnostic(
        geometry, groups, report, args.output / "diagnostic.png"
    )
    report.update(
        producer=subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        input_sha256=hashlib.sha256(args.input.read_bytes()).hexdigest(),
        process_peak_rss_bytes=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * 1024,
        total_allocation_count=None,
        total_physical_copied_bytes=None,
        scope="Actual source bottom-plane footprint measures only; PSB1/BSC3, no native-view/triangle interpretation. Archive validation still reads the compressed source. No height/normal/material/visibility model, opaque replacement, World-empty certificate or device/world gain.",
    )
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {key: value for key, value in report.items() if key != "needs"}, indent=2
        )
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--cells", nargs="+", required=True, type=int)
    parser.add_argument("--pitch-m", type=float, default=64.0)
    parser.add_argument("--output", required=True, type=Path)
    run(parser.parse_args())
