"""Compare fixed original, material-mean and lighting-before-filtering controls."""

import argparse
import gzip
import hashlib
import json
import subprocess
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[3]
DATA = ROOT / "test/outshine/data/render/building-full-view"


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(client, directory, request, mode, timeout):
    directory.mkdir()
    (directory / "input.json").write_text(json.dumps(request, indent=2) + "\n")
    if mode == "--source-mesh":
        geometry = directory / "geometry"
        geometry.mkdir()
        (geometry / "0").symlink_to(
            directory.parent / "geometry", target_is_directory=True
        )
    command = [str(client), str(directory), mode]
    (directory / "command.json").write_text(json.dumps(command) + "\n")
    with (directory / "terminal.log").open("w") as log:
        result = subprocess.run(
            command, stdout=log, stderr=subprocess.STDOUT, timeout=timeout, check=False
        )
    (directory / "terminal.exit").write_text(str(result.returncode) + "\n")
    if result.returncode:
        raise ValueError(f"native control failed: {directory}")
    if mode == "--radiance-world":
        report = json.loads((directory / "radiance-result.json").read_text())
        if report.get("render_device_creations") != 1:
            raise ValueError(
                "one executing render sequence must share one render device"
            )


def image(path, pixels):
    value = np.fromfile(path, dtype="<f4")
    if value.size != pixels * 4 or not np.isfinite(value).all():
        raise ValueError(f"incomplete/nonfinite native image: {path}")
    if value[3::4].sum(dtype=np.float64) <= 0:
        raise ValueError(f"empty positive control: {path}")
    return value.reshape(pixels, 4).astype(np.float64)


def compare(original, candidate):
    mass = abs(original[:, :3]).sum()
    area = original[:, 3].sum()
    if mass <= 0 or area <= 0:
        raise ValueError("nonempty original radiance and coverage required")
    return {
        "rgb_relative_l1": float(abs(original[:, :3] - candidate[:, :3]).sum() / mass),
        "alpha_relative_l1": float(abs(original[:, 3] - candidate[:, 3]).sum() / area),
        "radiance_ratio": float(candidate[:, :3].sum() / original[:, :3].sum()),
        "coverage_ratio": float(candidate[:, 3].sum() / area),
    }


def compress(directory):
    records = []
    for path in sorted(directory.iterdir()):
        if path.suffix not in {".linear", ".depth", ".rgba", ".f32"} and not (
            path.suffix.startswith(".ss") and path.suffix[3:].isdigit()
        ):
            continue
        raw = path.read_bytes()
        checksum = hashlib.sha256(raw).hexdigest()
        packed = gzip.compress(raw, compresslevel=1, mtime=0)
        destination = Path(str(path) + ".gz")
        destination.write_bytes(packed)
        if (
            hashlib.sha256(gzip.decompress(destination.read_bytes())).hexdigest()
            != checksum
        ):
            raise ValueError(f"lossless image compression failed: {path}")
        records.append(
            {
                "file": path.name,
                "bytes": len(raw),
                "gzip_bytes": len(packed),
                "sha256": checksum,
            }
        )
        path.unlink()
    return records


def replay(args):
    if args.output.resolve().is_relative_to(ROOT):
        raise ValueError(
            "put private native outputs in system temp, outside the checkout"
        )
    original = json.loads(args.original_input.read_text())
    controls = json.loads((DATA / "source-world-controls.json").read_text())
    request = json.loads((DATA / "source-plane-request.json").read_text())
    if args.query:
        known = {q["name"] for q in controls["queries"]}
        if set(args.query) - known:
            raise ValueError("query must name one of the fixed original controls")
        controls["queries"] = [
            q for q in controls["queries"] if q["name"] in args.query
        ]
    for query in controls["queries"]:
        query["filter_axis_samples"] = args.filter_axis_samples
    args.output.mkdir(parents=True, exist_ok=False)
    source = {
        **controls,
        "source_request": request,
        "source_input": str(args.source_input.resolve()),
    }
    inputs = {
        str(p.resolve()): digest(p) for p in (args.source_input, args.original_input)
    }
    geometry = args.output / "geometry"
    geometry.mkdir()
    (geometry / "input.json").write_text(json.dumps(original, indent=2) + "\n")
    for part in range(len(original["parts"])):
        folder = args.original_input.resolve().parent / str(part)
        (geometry / str(part)).symlink_to(folder, target_is_directory=True)
        for name in ("positions", "normals", "uv", "uv1", "colours", "indices"):
            path = folder / (name + ".bin")
            inputs[str(path)] = digest(path)
    direct = {
        **controls,
        "least": original["least"],
        "most": original["most"],
        "catalogue": [[0, 0, 1]],
    }
    modes = [
        ("original", "--source-mesh", direct),
        ("mean", "--cached-world", source),
        ("radiance", "--radiance-world", source),
    ]
    if args.field_control:
        modes.append(
            (
                "original-field",
                "--radiance-world",
                {**source, "field_original": str(args.original_input.resolve())},
            )
        )
    for name, mode, declaration in modes:
        folder = args.output / name
        run(args.client.resolve(), folder, declaration, mode, args.timeout)
    suffix = f".ss{args.filter_axis_samples**2}"
    rows = []
    for query in controls["queries"]:
        name = query["name"]
        truth = image(
            args.output / "original" / (name + "-source-mesh" + suffix), 1280 * 720
        )
        for mode, ending in (
            ("mean", "-cached-world"),
            ("radiance", "-radiance-world"),
        ):
            actual = image(args.output / mode / (name + ending + suffix), 1280 * 720)
            rows.append({"query": name, "mode": mode, **compare(truth, actual)})
        if args.field_control:
            actual = image(
                args.output / "original-field" / (name + "-radiance-world" + suffix),
                1280 * 720,
            )
            rows.append(
                {"query": name, "mode": "original-field", **compare(truth, actual)}
            )
            ending = name + "-source-field.f32"
            fine = image(
                args.output / "original-field" / ending, request["pixels"] ** 2
            )
            captured = image(args.output / "radiance" / ending, request["pixels"] ** 2)
            rows.append(
                {"query": name, "mode": "source-field", **compare(fine, captured)}
            )
    artifacts = {name: compress(args.output / name) for name, _, _ in modes}
    receipt = {
        "client_sha256": digest(args.client),
        "inputs": inputs,
        "fixed_controls_sha256": digest(DATA / "source-world-controls.json"),
        "fixed_request_sha256": digest(DATA / "source-plane-request.json"),
        "filter_axis_samples": args.filter_axis_samples,
        "comparisons": rows,
        "artifacts": artifacts,
        "scope": "Finite component comparison; no oracle/pin update, World admission, motion/history or frame budget proof.",
    }
    (args.output / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
    print(
        f"radiance controls: {len(controls['queries'])} queries; receipt {args.output / 'receipt.json'}"
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--client", type=Path, required=True)
    parser.add_argument("--source-input", type=Path, required=True)
    parser.add_argument("--original-input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--query", action="append")
    parser.add_argument(
        "--filter-axis-samples", type=int, choices=range(1, 129), default=4
    )
    parser.add_argument("--field-control", action="store_true")
    parser.add_argument("--timeout", type=float, default=600)
    args = parser.parse_args()
    if not 1 <= args.filter_axis_samples <= 128:
        parser.error("filter samples require 1..128 per axis")
    if not np.isfinite(args.timeout) or args.timeout <= 0:
        parser.error("timeout must be finite and positive")
    replay(args)


if __name__ == "__main__":
    main()
