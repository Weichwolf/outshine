"""Compare the isolated DDA/PBR prototype with unchanged native box instances."""

import argparse
import hashlib
import importlib.util
import json
import subprocess
import sys
import tempfile
from pathlib import Path

import numpy as np
from PIL import Image


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def control(native):
    directory = native / "test/experiments/render"
    sys.path.insert(0, str(directory))
    spec = importlib.util.spec_from_file_location(
        "archived_urban_control", directory / "native_urban_field.py"
    )
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def settling(directory, label):
    rows = np.loadtxt(directory / f"{label}.settling.times")
    assert rows.shape == (3, 3) and np.all(np.isfinite(rows))
    return {"gpu_ms": rows[:, 1].tolist(), "host_ms": rows[:, 2].tolist()}


def comparison(directory, name):
    paths = [directory / f"{name}-{mode}" for mode in ("boxes", "field")]
    images = [np.asarray(Image.open(p / "cold.png")) for p in paths]
    depths = [np.fromfile(p / "cold.depth", dtype="<f4") for p in paths]
    shadows = [np.fromfile(p / "cold.shadow", dtype="<f4") for p in paths]
    difference = np.abs(images[0].astype(int) - images[1].astype(int))
    occupied = [d > 0 for d in depths]
    shared = occupied[0] & occupied[1]
    delta = np.abs(depths[0][shared] - depths[1][shared])
    Image.fromarray(np.concatenate(images, axis=1)).save(directory / f"{name}.png")
    return {
        "name": name,
        "changed_rgb_pixels": int(np.any(difference, axis=-1).sum()),
        "max_rgb_delta": int(difference.max()),
        "coverage_mismatches": int((occupied[0] != occupied[1]).sum()),
        "depth_shared_quantiles": np.quantile(delta, [0.5, 0.95, 0.99, 1]).tolist()
        if delta.size
        else [],
        "shadow_changed_texels": int(np.count_nonzero(shadows[0] != shadows[1])),
        "shadow_coverage_mismatches": int(
            np.count_nonzero((shadows[0] > 0) != (shadows[1] > 0))
        ),
        "shadow_max_depth_delta": float(np.abs(shadows[0] - shadows[1]).max()),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--native", required=True, type=Path)
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--validation", action="store_true")
    args = parser.parse_args()
    native, source, output = (
        p.resolve() for p in (args.native, args.input, args.output)
    )
    if not output.is_relative_to(Path(tempfile.gettempdir()).resolve()):
        parser.error("--output must be in the system temporary directory")
    if not (native / "src/render/shaders/fieldTrace.glsl").is_file():
        parser.error("--native needs the isolated patch documented in box_field_pbr.md")
    base = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=native, text=True)
    if not base.startswith("08a51d354"):
        parser.error("the isolated comparison requires archived base 08a51d354")
    patch = Path(__file__).with_name("pbr_dda.patch")
    actual = subprocess.check_output(["git", "diff", "--unified=0"], cwd=native)
    if hashlib.sha256(actual).hexdigest() != sha(patch):
        parser.error("native source diff differs from pbr_dda.patch")
    with source.open() as stream:
        columns, rows, pitch, _ = map(float, stream.readline().split())
    side = max(columns, rows) * pitch
    output.mkdir(exist_ok=False)
    module = control(native)
    binary, build = module.build(native, output)
    key = sha(source)
    report = {
        "base": base.strip(),
        "patch_sha256": sha(patch),
        "build": build,
        "input_sha256": key,
        "columns": int(columns),
        "rows": int(rows),
        "scope": "flat complete field; fixed-light shadow reuse; not World admission",
        "cases": [],
        "comparisons": [],
    }
    for distance, radius in (("far", 2 * side), ("standard", 0.85 * side)):
        for height, altitude in (("low", 100.0), ("high", 0.1 * side)):
            name = f"{distance}-{height}"
            for mode in ("boxes", "field"):
                directory = output / f"{name}-{mode}"
                directory.mkdir()
                options = argparse.Namespace(
                    radius=radius,
                    altitude=altitude,
                    validation=args.validation,
                    representation=mode,
                )
                cold = module.run(
                    binary, native, directory, key, source, "cold", options
                )
                cold["settling"] = settling(directory, "cold")
                before = module.packages(directory / "assets.sqlite")
                warm = module.run(binary, native, directory, key, "-", "warm", options)
                warm["settling"] = settling(directory, "warm")
                assert before == module.packages(directory / "assets.sqlite")
                for suffix in ("png", "depth", "shadow", "shadow-frame"):
                    assert sha(directory / f"cold.{suffix}") == sha(
                        directory / f"warm.{suffix}"
                    )
                    (directory / f"warm.{suffix}").unlink()
                report["cases"].append(
                    {"name": name, "mode": mode, "cold": cold, "warm": warm}
                )
                print(name, mode, warm["gpu_ms"], flush=True)
            report["comparisons"].append(comparison(output, name))
            (output / "receipt.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
