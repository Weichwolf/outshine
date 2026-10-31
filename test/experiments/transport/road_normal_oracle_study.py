"""Check actual complete road plans against the independent Decimal normal oracle."""

import argparse
import hashlib
import json
from pathlib import Path

import numpy as np
from experiment_output import create_output
from road_normal_lod import sample
from road_normal_study import generate, read_row
from test_road_normal_cone import oracle_normal


def run(args):
    create_output(args.output)
    report = json.loads((args.plan / "report.json").read_text())
    if not report["accepted"] or report["method"] != "adaptive":
        raise ValueError("Accepted complete adaptive plan required")
    row, manifest = read_row(report["ordinal"])
    if report["archive_sha256"] != manifest["archive_sha256"]:
        raise ValueError("Matching original recipe archive required")
    with np.load(args.plan / "plan.npz", allow_pickle=False) as saved:
        plan = {key: saved[key] for key in saved.files}
    regenerated, curve = generate(
        row,
        "adaptive",
        report["position_limit_m"],
        np.deg2rad(report["normal_limit_degrees"]),
        report["max_queries"],
        report["max_vertices"],
        **(
            {"first_valid_split": True}
            if report.get("first_valid_split", False)
            else {}
        ),
    )
    for key in (
        "stations_m",
        "lateral_m",
        "positions_enh_m",
        "normals_enh",
        "indices",
        "cell_vertex_rows",
    ):
        if not np.array_equal(plan[key].view("u1"), regenerated[key].view("u1")):
            raise ValueError("Original plan no longer matches its recorded producer")
    stations = np.unique(
        np.r_[np.linspace(*curve.stations[[0, -1]], 17), curve.stations]
    )
    lateral = np.linspace(-curve.width, curve.width, 7)
    observed = sample(plan, stations, lateral)
    errors = []
    for i, station in enumerate(stations):
        span = int(
            np.clip(
                np.searchsorted(curve.stations, station, side="right") - 1,
                0,
                len(curve.lengths) - 1,
            )
        )
        for j, across in enumerate(lateral):
            expected = oracle_normal(curve, span, float(station), float(across))
            actual = observed[i, j]
            angle = float(
                np.arctan2(
                    np.linalg.norm(np.cross(actual, expected)), np.dot(actual, expected)
                )
            )
            errors.append(angle)
    maximum = float(max(errors, default=0))
    if maximum > np.deg2rad(report["normal_limit_degrees"]):
        raise ValueError("Independent Decimal normal exceeds declared angular limit")
    result = {
        "ordinal": report["ordinal"],
        "archive_sha256": report["archive_sha256"],
        "plan_sha256": hashlib.sha256(
            (args.plan / "plan.npz").read_bytes()
        ).hexdigest(),
        "samples": len(errors),
        "sampled_maximum_error_degrees": float(np.rad2deg(maximum)),
        "normal_limit_degrees": report["normal_limit_degrees"],
        "scope": "Independent 140-digit Decimal source differentiation at finitely many full-road stations/lateral positions, including right-sided source seams. The full continuous bound is separate; no native raster/PBR, profile, World or device claim.",
    }
    (args.output / "report.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--plan", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
