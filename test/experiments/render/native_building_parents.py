"""Measure existing source-parent geometry, GGX mixtures and paid composition."""

import argparse
import hashlib
import json
import math
import os
import statistics
import subprocess
import sys
import tempfile
import time
from pathlib import Path

from native_building_parent_inputs import prepare
from native_building_parent_reference import compare

ROOT = Path(__file__).resolve().parents[3]
DATA = ROOT / "test/outshine/data/render"


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(args):
    output = args.output.resolve()
    if not output.is_relative_to(
        Path(tempfile.gettempdir()).resolve()
    ) or output.is_relative_to(ROOT):
        raise ValueError("use a fresh explicit system-temp output outside the checkout")
    output.mkdir(parents=True, exist_ok=False)
    started = time.perf_counter()
    inputs = prepare(args.parents, args.fits, args.source, args.controls, output)
    prepared_ms = (time.perf_counter() - started) * 1000
    records, costs = [], []
    for row in inputs["queries"]:
        folder = output / row["query"]
        command = [str(args.binary), str(folder), str(args.shaders), str(folder)]
        env = os.environ.copy()
        if args.audit:
            command.append("--audit")
            if sys.platform == "darwin":
                env["MTL_DEBUG_LAYER"] = "1"
        record = {"command": command, "terminal": None}
        records.append(record)
        try:
            with (folder / "run.log").open("w") as log:
                result = subprocess.run(
                    command,
                    cwd=ROOT,
                    env=env,
                    stdout=log,
                    stderr=subprocess.STDOUT,
                    timeout=120,
                    check=False,
                )
            record["terminal"] = result.returncode
        except subprocess.TimeoutExpired:
            record["terminal"] = "timeout"
            raise
        finally:
            (output / "commands.json").write_text(json.dumps(records, indent=2))
        if result.returncode:
            raise RuntimeError(
                f"native probe exited {result.returncode}: {folder / 'run.log'}"
            )
        native = json.loads((folder / "result.json").read_text())
        local = {}
        for case in native["cases"]:
            samples = case["samples"]
            if len(samples) != 24 or any(
                not math.isfinite(sample["gpu_ns"]) or sample["gpu_ns"] <= 0
                for sample in samples
            ):
                raise ValueError("the complete 24-round GPU window is required")
            local[case["name"]] = {
                "gpu_median_ms": statistics.median(
                    sample["gpu_ns"] for sample in samples
                )
                / 1e6,
                "encode_median_ms": statistics.median(
                    sample["encode_ns"] for sample in samples
                )
                / 1e6,
                "layers": case["layers"],
                "buffer_bytes": case["buffer_bytes"],
            }
        costs.append(
            {"query": row["query"], "device": native["device"], "cases": local}
        )
    references = compare(
        output,
        args.parents,
        args.fits,
        args.source,
        args.controls,
        DATA / "material-filter/energy-tables.npz",
    )
    report = {
        "checkout_commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
        ).strip(),
        "checkout_dirty": bool(
            subprocess.check_output(
                ["git", "status", "--porcelain"], cwd=ROOT, text=True
            )
        ),
        "binary_sha256": digest(args.binary),
        "shader_sha256": {
            path.name: digest(path) for path in sorted(args.shaders.glob("*.spv"))
        },
        "audit": args.audit,
        "scope": "Bounded point-sampled source-parent crops at unchanged presentation pixel spacing. Paid first-hit atlas, GGX and C/T composition; no full-frame/LOD/world/device-budget admission.",
        "prepare_ms": prepared_ms,
        "inputs": inputs,
        "commands": records,
        "costs": costs,
        "reference": references,
    }
    (output / "report.json").write_text(json.dumps(report, indent=2))
    print(json.dumps(costs, indent=2))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("parents", "fits", "binary", "shaders", "output"):
        parser.add_argument("--" + name, required=True, type=Path)
    parser.add_argument(
        "--source",
        type=Path,
        default=DATA / "building-full-view/original-inputs.tar.gz",
    )
    parser.add_argument(
        "--controls",
        type=Path,
        default=DATA / "building-full-view/native-cells/manifest.json",
    )
    parser.add_argument("--audit", action="store_true")
    args = parser.parse_args()
    for name in (
        "parents",
        "fits",
        "source",
        "controls",
        "binary",
        "shaders",
        "output",
    ):
        setattr(args, name, getattr(args, name).resolve())
    run(args)


if __name__ == "__main__":
    main()
