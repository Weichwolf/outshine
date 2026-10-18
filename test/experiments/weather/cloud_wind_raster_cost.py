"""Compare actual legacy/shared sky submissions in fresh A/B/B/A processes."""

import argparse
import json
import statistics
import subprocess
import tempfile
from pathlib import Path

import numpy as np
from cloud_density_native import digest


def command_for(folder):
    receipt = json.loads((folder / "receipt.json").read_text())
    binary = Path(receipt["binary"])
    if digest(binary) != receipt["binary_sha256"]:
        raise ValueError("the actual native binary must match its capture receipt")
    return next(
        row["argv"].copy()
        for row in receipt["commands"]
        if row["argv"][0] == str(binary)
    )


def capture_run(folder, command, output, width, height):
    output.mkdir(parents=True, exist_ok=True)
    command[5:] = [
        str(output / "actual"),
        str(width),
        str(height),
        "all",
        "wind-sky",
        "timed",
    ]
    inputs = {name: digest(Path(name)) for name in command[:5]}
    snapshot = subprocess.run(
        ["ps", "-eo", "pid,ppid,ni,comm,args"],
        capture_output=True,
        text=True,
        check=True,
    )
    (output / "processes.txt").write_text(snapshot.stdout)
    result = subprocess.run(
        command, capture_output=True, text=True, timeout=180, check=False
    )
    (output / "stdout").write_text(result.stdout)
    (output / "stderr").write_text(result.stderr)
    rows = [
        json.loads(line)
        for line in result.stdout.splitlines()
        if line.startswith('{"case":')
    ]
    if result.returncode or result.stderr or len(rows) != 16:
        raise RuntimeError(f"the complete native timing run failed: {output}")
    if any(digest(Path(name)) != expected for name, expected in inputs.items()):
        raise AssertionError(
            "native binary and all four shader inputs must remain frozen"
        )
    for row in rows:
        values = row["submission_gpu_ns"]
        if len(values) != 2 or not all(np.isfinite(v) and v > 0 for v in values):
            raise AssertionError(
                "both actual frame times must exist; no unavailable or warm-up samples dropped"
            )
    return {
        "source_receipt": str(folder / "receipt.json"),
        "source_receipt_sha256": digest(folder / "receipt.json"),
        "argv": command,
        "exit_code": result.returncode,
        "input_sha256": inputs,
        "cases": rows,
        "output": str(output),
    }


def compare_images(legacy, current, width, height):
    rows = []
    for profile in (0, 1):
        for sign in (-1, 1):
            prefix = f"actual.wind-{profile}-{sign}"
            old = np.fromfile(legacy / f"{prefix}-0.images.bin", "<f4").reshape(
                5, -1, 4
            )
            new = np.fromfile(current / f"{prefix}-0.images.bin", "<f4").reshape(
                5, -1, 4
            )
            if old.shape != (5, width * height, 4) or new.shape != old.shape:
                raise AssertionError(
                    "both variants must retain the entire requested image"
                )
            np.testing.assert_array_equal(
                np.fromfile(legacy / f"{prefix}-0.rays.bin", "<u1"),
                np.fromfile(current / f"{prefix}-0.rays.bin", "<u1"),
            )
            for rate in (30, 60):
                before = np.fromfile(
                    legacy / f"{prefix}-{rate}.images.bin", "<f4"
                ).reshape(old.shape)
                after = np.fromfile(
                    current / f"{prefix}-{rate}.images.bin", "<f4"
                ).reshape(new.shape)
                stuck = int(
                    np.count_nonzero(np.any(before[1, :, :3] != old[1, :, :3], axis=1))
                )
                moved = int(
                    np.count_nonzero(np.any(after[1, :, :3] != new[1, :, :3], axis=1))
                )
                if stuck or not moved:
                    raise AssertionError(
                        "the original actual sticking case and corrected sky motion must both be present"
                    )
                rows.append(
                    {
                        "profile": profile,
                        "phase_sign": sign,
                        "rate_hz": rate,
                        "legacy_changed_rgb_pixels": stuck,
                        "shared_changed_rgb_pixels": moved,
                        "physical_rays_byte_equal": True,
                    }
                )
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--legacy", type=Path, required=True)
    parser.add_argument("--current", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--width", type=int, default=240)
    parser.add_argument("--height", type=int, default=135)
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(Path(tempfile.gettempdir()).resolve()):
        parser.error("private captures belong in system temp")
    if not 1 <= args.width <= 8192 or not 1 <= args.height <= 8192:
        parser.error("dimensions must match the existing bounded native probe contract")
    output.mkdir(parents=True, exist_ok=True)
    (output / "receipt.json").unlink(missing_ok=True)
    runs = []
    for index, folder in enumerate(
        (args.legacy, args.current, args.current, args.legacy)
    ):
        label = "legacy" if index in (0, 3) else "shared"
        run = capture_run(
            folder,
            command_for(folder),
            output / f"{index}-{label}",
            args.width,
            args.height,
        )
        run["variant"] = label
        runs.append(run)
        print(label, index, "complete32 native GPU samples", flush=True)
    images = compare_images(
        output / "0-legacy", output / "1-shared", args.width, args.height
    )
    timing = {}
    for label in ("legacy", "shared"):
        samples = [
            v
            for run in runs
            if run["variant"] == label
            for case in run["cases"]
            for v in case["submission_gpu_ns"]
        ]
        timing[label] = {
            "samples": len(samples),
            "min_ns": min(samples),
            "median_ns": statistics.median(samples),
            "max_ns": max(samples),
        }
    record = {
        "profile": [args.width, args.height],
        "order": "A/B/B/A, fresh processes, no sample omissions",
        "runs": runs,
        "timing": timing,
        "images": images,
        "scope": "Actual GPU submission including scene/sky pass and readback copies; no CPU IO/fence-as-GPU time, no whole-world/FPS/device acceptance. Legacy fails temporal quality, so timing is not a quality-parity speedup claim.",
    }
    (output / "receipt.json").write_text(json.dumps(record, indent=2) + "\n")
    print(json.dumps(timing), flush=True)


if __name__ == "__main__":
    main()
