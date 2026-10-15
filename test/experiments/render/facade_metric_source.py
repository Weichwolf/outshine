"""Check proposed metric coordinates against actual native building source hits."""

import argparse
import json
import re
import subprocess
import tempfile
from pathlib import Path

import numpy as np


def compare(rows):
    points = [r for r in rows if r["kind"] == "point"]
    meshes = [r for r in rows if r["kind"] == "mesh"]
    assert len(points) >= 100
    worst = 0.0
    for row in points:
        local, edge = np.array(row["local"]), np.array(row["edge"])
        reference = np.array(
            [np.linalg.norm(local[:2] - edge[:2]), local[2] - row["base_m"]]
        )
        worst = max(worst, float(np.max(np.abs(reference - row["stored_metres"]))))
    assert worst < 2e-4
    anchor_error = 0.0
    for case in range(4):
        a, b = [
            [r for r in points if r["case"] == case and r["anchor_m"] == anchor]
            for anchor in (0, 500)
        ]
        assert len(a) == len(b) and a
        assert [(r["part"], r["face"], r["gable"]) for r in a] == [
            (r["part"], r["face"], r["gable"]) for r in b
        ]
        error = np.array([r["stored_metres"] for r in a]) - [
            r["stored_metres"] for r in b
        ]
        anchor_error = max(anchor_error, float(np.max(np.abs(error))))
    assert anchor_error < 2e-4
    assert {(r["case"], r["anchor_m"], r["detail"]) for r in meshes} == {
        (case, anchor, detail)
        for case in range(4)
        for anchor in (0, 500)
        for detail in range(4)
    }
    gables = [r for r in points if r["gable"]]
    assert len(gables) >= 10 and all(r["uv"][0] % 256 == 0 for r in gables)
    assert any(r["metres"][0] > 1 for r in gables)
    return {
        "source_hits": len(points),
        "gable_hits_without_horizontal_bay": len(gables),
        "independent_metric_max_error_m": worst,
        "anchor_shift_m": 500,
        "anchor_max_metric_error_m": anchor_error,
        "actual_meshes": len(meshes),
        "projected_optional_metric_bytes": [
            r for r in meshes if r["detail"] == 0 and r["anchor_m"] == 0
        ],
        "scope": "Source-coordinate proposal and actual mesh counts; no UV1 emission, cache or stage integration",
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if output.is_relative_to(
        Path(__file__).resolve().parents[3]
    ) or not output.is_relative_to(Path(tempfile.gettempdir()).resolve()):
        parser.error("use an explicit system-temp output")
    output.mkdir(parents=True, exist_ok=True)
    (output / "comparison.json").unlink(missing_ok=True)
    command = ["nice", "-n", "10", str(args.binary.resolve())]
    run = subprocess.run(
        command, check=False, capture_output=True, text=True, timeout=90
    )
    (output / "source.log").write_text(run.stdout + run.stderr)
    assert run.returncode == 0, (run.returncode, run.stderr)
    checks = re.search(
        r"CHECKS (\d+) FAILURES 0 SKIPPED 0 UNPREPARED 0 PARTIAL 0", run.stdout
    )
    assert checks
    report = compare(
        [json.loads(line) for line in run.stdout.splitlines() if line.startswith("{")]
    )
    report.update(command=command, native_checks=int(checks[1]), exit=run.returncode)
    (output / "comparison.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
