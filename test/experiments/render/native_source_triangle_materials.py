"""Compare selected original triangle material samples with the whole original."""

import argparse
import hashlib
import json
import subprocess
from pathlib import Path
from zipfile import ZipFile

import numpy as np
import triangle_columns as columns
from compare_native_world_fields import capture_region, pixels
from experiment_output import create_output
from export_native_field_opponents import execute, read_samples
from export_native_world_field import queries
from native_world_inputs import POSES, ROOT, clip_from_original, load
from source_triangle_materials import write_source

DATA = ROOT / "test/outshine/data/render/building-world-opponents/ss2"


def bound_selection(directory, source):
    metadata = json.loads((directory / "native-visible-triangles.json").read_text())
    original = (
        ROOT / "test/outshine/data/render/building-depth-samples/original-geometry"
    )
    bindings = {
        "world_manifest_sha256": directory / "manifest.json",
        "source_geometry_manifest_sha256": original / "manifest.json",
        "samples_sha256": directory / "native-visible-triangles.npz",
    }
    for name, path in bindings.items():
        if hashlib.sha256(path.read_bytes()).hexdigest() != metadata[name]:
            raise ValueError("Native selection source/query binding changed")
    with ZipFile(bindings["samples_sha256"]) as archive:
        if (
            metadata["version"] != 1
            or sum(entry.file_size for entry in archive.infolist()) > 8 * 1024**2
        ):
            raise ValueError("Bounded version-one native sample input required")
    with np.load(bindings["samples_sha256"], allow_pickle=False) as archive:
        if set(archive.files) != set(metadata["arrays"]):
            raise ValueError("Native selection inventory changed")
        for key, record in metadata["arrays"].items():
            value = archive[key]
            if (
                list(value.shape) != record["shape"]
                or value.dtype.str != record["dtype"]
                or hashlib.sha256(value.tobytes()).hexdigest() != record["sha256"]
            ):
                raise ValueError("Native selection array binding changed")
        selected = archive["selection/original_triangle_ordinals"].copy()
    if (
        len(selected) != metadata["selected_triangles"]
        or len(source["indices"]) != metadata["original_triangles"]
    ):
        raise ValueError("Native selection inventory changed")
    return selected, metadata


def geometric_selection(source, manifest, cases):
    poses = [pose for pose in POSES if pose != "repeat"]
    field = columns.prepare(source, clip_from_original(cases["capture", poses[0]]), 64)
    selected, work = [], []
    for pose in poses:
        world = clip_from_original(cases["original", pose])
        points = pixels(capture_region(field.capture, world, manifest["raster"]))
        _, faces, complete, costs = columns.query(
            field, world, points, manifest["raster"]
        )
        if not complete.all():
            raise ValueError("Unresolved geometric query retains the original group")
        selected.extend(faces[faces >= 0].tolist())
        work.append({"pose": pose, "work": costs})
    return np.unique(selected), work


def compare(directory, request):
    results, saved = [], {}
    for item in request:
        name, camera = item["name"], item["capture"]
        whole, whole_z = read_samples(directory / "whole-native", name, camera)
        selected, selected_z = read_samples(directory / "selected-native", name, camera)
        different = np.any(whole != selected, axis=2)
        results.append(
            {
                "query": name,
                "rgba_pixels_different": int(different.sum()),
                "rgba_max_difference": float(np.abs(whole - selected).max()),
                "depth_pixels_different": int(np.count_nonzero(whole_z != selected_z)),
                "whole_covered": int(np.count_nonzero(whole_z)),
                "selected_covered": int(np.count_nonzero(selected_z)),
            }
        )
        yy, xx = np.nonzero((whole_z > 0) | (selected_z > 0))
        saved[name + "/pixel_xy"] = np.c_[xx, yy].astype("<i4")
        for key, value in (
            ("whole_rgba", whole),
            ("selected_rgba", selected),
            ("whole_depth", whole_z),
            ("selected_depth", selected_z),
        ):
            saved[name + "/" + key] = value[yy, xx]
    np.savez_compressed(directory / "samples.npz", **saved)
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--input", type=Path, default=DATA)
    parser.add_argument(
        "--selection", choices=("native-reference", "geometric"), required=True
    )
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    manifest, _, cases, source = load(args.input, attributes=True)
    selected, binding = bound_selection(args.input, source)
    if args.selection == "geometric":
        selected, binding = geometric_selection(source, manifest, cases)
    create_output(args.output)
    write_source(source, np.arange(len(source["indices"])), args.output / "whole")
    write_source(source, selected, args.output / "selected")
    request = queries(
        {pose: cases["original", pose]["capture"] for pose in POSES if pose != "repeat"}
    )
    native_reports = {}
    binary_hash = hashlib.sha256(args.binary.read_bytes()).hexdigest()
    for mode in ("whole", "selected"):
        native_reports[mode] = execute(
            args.binary.resolve(),
            args.output / (mode + "-native"),
            {"directory": str((args.output / mode).resolve()), "queries": request},
        )
        if hashlib.sha256(args.binary.read_bytes()).hexdigest() != binary_hash:
            raise ValueError("Freeze the native program throughout the comparison")
        for unused in (args.output / (mode + "-native")).glob("*-integral.f32"):
            unused.unlink()
    result = compare(args.output, request)
    qualified = all(
        case["rgba_pixels_different"] == case["depth_pixels_different"] == 0
        for case in result
    )
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "selection": args.selection,
        "binary_sha256": binary_hash,
        "source_sha256": {
            path.name: hashlib.sha256(path.read_bytes()).hexdigest()
            for path in (
                Path(__file__),
                Path(__file__).with_name("source_triangle_materials.py"),
            )
        },
        "selection_binding": binding,
        "original_triangles": len(source["indices"]),
        "selected_triangles": len(selected),
        "native_reports": native_reports,
        "cases": result,
        "finite_native_material_comparison_equal": qualified,
        "scope": "Original native vertex/material processing; no compact field, free visibility "
        "oracle, runtime World admission or performance/UMA acceptance",
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({"equal": qualified, "cases": result}))
    return 0 if qualified else 1


if __name__ == "__main__":
    raise SystemExit(main())
