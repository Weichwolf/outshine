"""Measure paired offline Places with the actual enabled/disabled material shaders."""

import argparse
import hashlib
import json
import subprocess
import tempfile
import time
from pathlib import Path

from facade_coating_raster import ROOT


def capture(binary, runtime, output, place, side, width, height):
    override = json.dumps({"render": {"widthPx": width, "heightPx": height}})
    command = [
        str(binary),
        "shots",
        "--scenario-overrides",
        override,
        "--rows",
        "--stats",
        "--offline",
        "--preload-seconds",
        "10",
        place,
    ]
    began = time.monotonic()
    load = Path("/proc/loadavg").read_text().strip()
    result = subprocess.run(
        command, cwd=runtime, text=True, capture_output=True, check=False
    )
    text = result.stdout + result.stderr
    (output / f"{place}-{side}.log").write_text(text)
    rows = [line.split("\t") for line in text.splitlines() if line.startswith("ROW\t")]
    record = {
        "place": place,
        "side": side,
        "command": command,
        "cwd": str(runtime),
        "exit": result.returncode,
        "seconds": time.monotonic() - began,
        "load": load,
        "rows": rows,
    }
    assert len(rows) <= 1
    if len(rows) == 1:
        row = rows[0]
        record["timing_thresholds_met"] = (
            result.returncode == 0
            and row[3] == "1"
            and int(row[7]) == 60
            and int(row[8]) == 0
            and row[13] == "1"
            and int(row[14]) == 0
            and float(row[6]) <= 1000 / 60
            and float(row[17]) < 10000
            and float(row[18]) == 360
        )
    else:
        record["timing_thresholds_met"] = False
    print(record, flush=True)
    return record


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--scene", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--width", type=int, default=1920)
    parser.add_argument("--height", type=int, default=1080)
    parser.add_argument("places", nargs="+")
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(
        Path(tempfile.gettempdir()).resolve()
    ) or output.is_relative_to(ROOT):
        parser.error("use an explicit system-temp output outside the checkout")
    output.mkdir(parents=True, exist_ok=True)
    report = {
        "binary_sha256": hashlib.sha256(args.binary.read_bytes()).hexdigest(),
        "width": args.width,
        "height": args.height,
        "target_fps": 60,
        "records": [],
        "scope": "Paired declared public client/ABI; timing thresholds alone do not prove image parity",
        "complete_warm_load": "unmeasured: ROW omits setup StandingMs; LoadingMs covers preload only",
        "equal_quality_acceptance": "not asserted by this timing consumer",
    }
    for place in args.places:
        for side in ("before", "after"):
            report["records"].append(
                capture(
                    args.binary.resolve(),
                    args.scene.resolve() / side,
                    output,
                    place,
                    side,
                    args.width,
                    args.height,
                )
            )
            (output / "captures.json").write_text(json.dumps(report, indent=2) + "\n")
    assert all(row["timing_thresholds_met"] for row in report["records"]), (
        "timing thresholds remain red"
    )


if __name__ == "__main__":
    main()
