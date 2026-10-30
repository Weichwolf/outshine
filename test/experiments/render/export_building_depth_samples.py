"""Export native original/sample comparisons without their empty full-frame borders."""

import argparse
import hashlib
import json
from pathlib import Path

import numpy as np


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def export(args):
    roots = {"original": args.original, "samples": args.samples}
    inputs = {
        name: json.loads((root / "input.json").read_text())
        for name, root in roots.items()
    }
    reference = inputs["original"]
    if reference["queries"] != inputs["samples"]["queries"]:
        raise ValueError("original and stored samples must use identical queries")
    if reference.get("stored_samples", False) or not inputs["samples"]["stored_samples"]:
        raise ValueError("comparison must retain an independent original geometry draw")
    if reference["atlas_sha256"] != inputs["samples"]["atlas_sha256"]:
        raise ValueError("comparison refers to different captures")
    reports = {
        name: json.loads((root / "mesh-result.json").read_text())
        for name, root in roots.items()
    }
    if any(
        report["mode"] != "source-mesh"
        or len(report["cases"]) != len(reference["queries"])
        for report in reports.values()
    ):
        raise ValueError("native reports do not match the requested comparison")
    runs = {
        name: json.loads((root / "qualified-command.json").read_text())
        for name, root in roots.items()
    }
    if any(run["exit"] != 0 for run in runs.values()):
        raise ValueError("both native runs must have completed successfully")
    signatures = {
        name: {key: run[key] for key in ["binary_sha256", "driver_sha256"]}
        for name, run in runs.items()
    }
    if signatures["original"] != signatures["samples"]:
        raise ValueError("both inputs must be drawn by the same native program")
    arrays, records = {}, []
    x0, y0, x1, y1 = args.crop
    if not (0 <= x0 < x1 <= 1280 and 0 <= y0 < y1 <= 720):
        raise ValueError("crop must lie inside the native raster")
    for index, query in enumerate(reference["queries"]):
        count = query["filter_axis_samples"] ** 2
        record = {"query": query, "buffers": {}, "draws": {}}
        for name, root in roots.items():
            draw = reports[name]["cases"][index]
            if draw["query"] != query["name"]:
                raise ValueError("native report/query order differs")
            record["draws"][name] = draw
            for suffix, channels in [
                ("linear", 4), ("depth", 1), ("normal", 4), (f"ss{count}", 4)
            ]:
                source = root / (query["name"] + "-source-mesh." + suffix)
                full = np.fromfile(source, dtype="<f4").reshape(720, 1280, channels)
                if not np.isfinite(full).all():
                    raise ValueError("nonfinite native readback")
                crop = full[y0:y1, x0:x1].copy()
                restored = np.empty_like(full)
                restored[:] = full[0, 0]
                restored[y0:y1, x0:x1] = crop
                if not np.array_equal(restored.view(np.uint32), full.view(np.uint32)):
                    raise ValueError("crop would remove native content")
                key = f"case{index}_{name}_{suffix}"
                arrays[key] = crop
                record["buffers"][key] = {
                    "clear": full[0, 0].tolist(), "full_sha256": digest(source)
                }
        records.append(record)
    args.output.mkdir(parents=True, exist_ok=True)
    output = args.output / "captures.npz"
    np.savez_compressed(output, **arrays)
    geometry = args.original / "geometry/0"
    metadata = {
        "version": 1,
        "native_commit": args.native_commit,
        "native_commit_scope": "product code/shaders; diagnostic driver bound separately by SHA256",
        "native_driver_sha256": signatures["original"]["driver_sha256"],
        "native_binary_sha256": signatures["original"]["binary_sha256"],
        "atlas_sha256": reference["atlas_sha256"],
        "catalogue": reference["catalogue"],
        "bounds": {key: reference[key] for key in ["least", "most"]},
        "original_geometry_files": {
            str(path.relative_to(geometry)): digest(path)
            for path in sorted(geometry.rglob("*"))
            if path.is_file()
        },
        "raster": [1280, 720],
        "crop": args.crop,
        "dtype": "little-endian Float32",
        "coordinates": "native source frame; upper-left pixel origin",
        "encoding": "premultiplied scene-linear RGBA; reverse-Z base depth; base normals",
        "lighting": {"key_lux": 100, "haze": 0, "exposure": 1},
        "scope": "Isolated source/sample draws and microcamera average; no World/pose/SSE/budget approval",
        "cases": records,
        "npz_sha256": digest(output),
        "npz_bytes": output.stat().st_size,
    }
    (args.output / "manifest.json").write_text(json.dumps(metadata, indent=2) + "\n")
    print(json.dumps({
        "cases": len(records), "arrays": len(arrays), "bytes": output.stat().st_size
    }))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--original", type=Path, required=True)
    parser.add_argument("--samples", type=Path, required=True)
    parser.add_argument("--native-commit", required=True)
    parser.add_argument("--crop", type=int, nargs=4, default=[576, 312, 704, 408])
    parser.add_argument("--output", type=Path, required=True)
    export(parser.parse_args())
