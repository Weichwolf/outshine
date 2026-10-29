"""Actual captured palette with controlled positions and original-word demand."""

import argparse
import hashlib
import json
import subprocess
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw
from probe_terrain_material_fields import appearance
from terrain_material_capture import load
from terrain_material_litter import compose
from terrain_material_plane import hash_words, prepare

ROOT = Path(__file__).resolve().parents[3]


def inputs():
    folder = ROOT / "test/outshine/data/materials"
    manifest = json.loads((folder / "ground-litter.json").read_bytes())
    data = (folder / "ground-litter.palette").read_bytes()
    if (
        len(data) != manifest["bytes"]
        or hashlib.sha256(data).hexdigest() != manifest["sha256"]
    ):
        raise ValueError("unchanged captured native palette required")
    return np.frombuffer(data, dtype="<f4"), manifest


def optical_row(palette, manifest, row):
    count, layers = manifest["rows"], manifest["layer_offset_floats"]
    if not 0 <= row < count:
        raise ValueError("actual captured palette row required")
    start = 4 * (count + 2) + count + 1 + row * 8
    return {
        "mean": palette[4 + row * 4 : 8 + row * 4].astype(float),
        "recipe": palette[start : start + 4].astype(float),
        "contrast": float(palette[start + 4]),
        "coverage": float(palette[start + 7]),
        "difference": palette[layers + row * 8 : layers + row * 8 + 4].astype(float),
        "litter_recipe": palette[layers + row * 8 + 4 : layers + row * 8 + 8].astype(
            float
        ),
    }


def prepared_sources(domain_bounds_m, footprint, recipe, *, enabled, mask_needed=False):
    """Diagnostic request selection before bounded table work, not an asset manager."""
    grain, height, coarse, fine = recipe
    valid = enabled and coarse > 0 and fine > 0
    needed = (
        (valid and footprint < coarse * 0.8) or mask_needed,
        valid and footprint < fine * 0.8,
        valid and grain > 0 and height > 0 and footprint < grain * 0.8,
    )
    sources, records, fields = [], [], []
    for name, wavelength, required in zip(
        ("coarse", "fine", "grain"), (coarse, fine, grain), needed, strict=True
    ):
        if not required:
            sources.append(hash_words)
            records.append(
                {
                    "component": name,
                    "state": "inactive_before_preparation",
                    "field_bytes": 0,
                }
            )
            continue
        bounds = np.asarray(domain_bounds_m, dtype=float) / wavelength
        try:
            plane = prepare(bounds, halo=3 if name == "coarse" and mask_needed else 1)
        except ValueError as error:
            if "word budget" not in str(error):
                raise
            sources.append(hash_words)
            records.append(
                {
                    "component": name,
                    "state": "valid_analytic_budget_fallback",
                    "reason": str(error),
                    "field_bytes": 0,
                }
            )
            continue
        sources.append(plane.get)
        fields.append(plane)
        records.append(
            {
                "component": name,
                "state": "prepared_original_words",
                "field_bytes": plane.retained_bytes,
                "first_xy": plane.first_xy,
                "shape_yx": plane.words_yx.shape,
            }
        )
    return tuple(sources), records, tuple(fields)


def case(palette, manifest, row, extent_m, sign, *, width=256, height=192):
    values = optical_row(palette, manifest, row)
    steps = np.array([extent_m / width, extent_m / height])
    footprint = max(steps)
    coverage, coarse = values["coverage"], values["recipe"][2]
    mask_needed = 0 < coverage < 1 and coarse > 0 and footprint < 2 * coarse
    domain_bounds = [[1000.1, -200.2], [1000.1 + extent_m, -200.2 + extent_m]]
    base, base_records, base_fields = prepared_sources(
        domain_bounds,
        footprint,
        values["recipe"],
        enabled=coverage < 1,
        mask_needed=mask_needed,
    )
    litter, litter_records, litter_fields = prepared_sources(
        domain_bounds, footprint, values["litter_recipe"], enabled=coverage > 0
    )
    x, y = np.meshgrid(
        (np.arange(width) + 0.5) * extent_m / width,
        (np.arange(height) + 0.5) * extent_m / height,
    )
    at = np.column_stack((x.ravel() + 1000.1, y.ravel() - 200.2))
    dx, dy = np.tile([steps[0], 0], (len(at), 1)), np.tile([0, steps[1]], (len(at), 1))
    return {
        "values": values,
        "at": at,
        "dx": dx,
        "dy": dy,
        "base_sources": base,
        "litter_sources": litter,
        "mask_source": base[0] if mask_needed else hash_words,
        "records": {"base": base_records, "litter": litter_records},
        "fields": base_fields + litter_fields,
        "basis": np.array([[1, 0], [sign * 0.7, -sign * 0.3], [0, 1]], dtype=float),
        "shape": (height, width),
        "steps": steps,
    }


def query(data, *, prepared):
    sources = (
        {
            "base_words": data["base_sources"],
            "litter_words": data["litter_sources"],
            "mask_words": data["mask_source"],
        }
        if prepared
        else {}
    )
    return compose(data["at"], data["dx"], data["dy"], **data["values"], **sources)


def preview(surface, data):
    detail = np.column_stack((np.ones(len(surface)), surface[:, 3:])).reshape(
        *data["shape"], 4
    )
    optics = {"albedo": surface[:, :3].reshape(*data["shape"], 3), "roughness": 0}
    return appearance(detail, data["basis"], data["steps"], optics)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--palette-capture", type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    if args.palette_capture is None:
        palette, manifest = inputs()
    else:
        capture = load(args.palette_capture)
        palette = capture.palette
        manifest = {
            "rows": int(capture.palette_words[0]),
            "layer_offset_floats": int(capture.palette_words[3]),
            "sha256": capture.metadata["palette_sha256"],
            "native_producer_commit": capture.metadata["product_commit"],
            "capture_input_sha256": capture.input_sha256,
            "scope": "Actual captured palette only; positions, plane and footprints remain explicit controls.",
        }
    reports = []
    canvas = Image.new("RGB", (512, 2 * 216), "white")
    labels = ImageDraw.Draw(canvas)
    for index, (row, extent, sign) in enumerate(((0, 0.05, 1), (3, 8.0, -1))):
        data = case(palette, manifest, row, extent, sign)
        direct, lookup = query(data, prepared=False), query(data, prepared=True)
        np.testing.assert_array_equal(direct, lookup)
        arrays = {
            "positions_m": data["at"],
            "dx_m": data["dx"],
            "dy_m": data["dy"],
            "analytic_surface": direct,
            "prepared_surface": lookup,
        }
        for column, (name, response) in enumerate(
            (("analytic", direct), ("original-words", lookup))
        ):
            normals, radiance, image = preview(response, data)
            image.save(args.output / f"{index}-{name}.png")
            canvas.paste(image, (column * 256, index * 216 + 24))
            labels.text(
                (column * 256 + 4, index * 216 + 4),
                f"Captured row {row}; {extent:g}m; {name}",
                fill="black",
            )
            arrays[name + "_normals"], arrays[name + "_radiance"] = normals, radiance
        np.testing.assert_array_equal(
            arrays["analytic_radiance"], arrays["original-words_radiance"]
        )
        np.savez_compressed(args.output / f"{index}-controlled.npz", **arrays)
        reports.append(
            {
                "row": row,
                "extent_control_m": extent,
                "slope_sign": sign,
                "query_count": len(direct),
                "field_bytes": sum(f.retained_bytes for f in data["fields"]),
                "request_selection": data["records"],
                "all_six_surface_channels_and_radiance_exact": True,
            }
        )
    canvas.save(args.output / "controlled-composition.png")
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "captured_palette_provenance": manifest,
        "scope": "Actual unchanged versioned palette; controlled positions/phase, plane, derivatives and diagnostic GGX lighting. No actual Jura/Koerbersee fragments, DEM, full class/rock composition, GPU/hardware sampling, field IO/residency, NDF improvement or device/frame claim.",
        "rows": reports,
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(reports), flush=True)


if __name__ == "__main__":
    main()
