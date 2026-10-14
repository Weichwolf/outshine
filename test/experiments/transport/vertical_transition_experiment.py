"""Equal input/grade/curvature comparison with saved real axes and full ribbons."""

import argparse
import hashlib
import json
import resource
import subprocess
import time
from pathlib import Path

import numpy as np
from ground_refinement_experiment import cached_target
from outshine_ref.contracts import Mesh
from outshine_ref.transport import (
    plan_vertical_transition,
    sample_vertical_transition,
)
from surface_height_checks import normal_gradients
from surface_height_frames import RECIPES
from transport_building_clearance import street_kind

ROOT = Path(__file__).resolve().parents[3]
OUTPUT = ROOT / "build/experiments/transport/vertical-profiles"
INPUT = ROOT / "build/experiments/transport/inputs/Vienna-input.json"
FIXTURE = Path(__file__).resolve().parent / "fixtures/vertical-transition-cases.json"


def ribbon(start, direction, table, width, crossfall=0.015):
    normal = np.array([-direction[1], direction[0]])
    offsets = np.array([-width / 2, width / 2])
    xy = start + table[:, 0, None, None] * direction + offsets[:, None] * normal
    heights = table[:, 1, None] + offsets * crossfall
    vertices = np.c_[xy.reshape(-1, 2), heights.reshape(-1)]
    first = 2 * np.arange(len(table) - 1)
    faces = np.vstack(
        (np.c_[first, first + 2, first + 1], np.c_[first + 2, first + 3, first + 1])
    )
    return Mesh(vertices, faces)


def capture_sources():
    raw = json.loads(INPUT.read_text())
    target, receipt = cached_target()
    candidates = [
        r
        for r in raw["roads"]
        if len(r["coordinates"]) == 2
        and r["properties"].get("brunnel") not in ("bridge", "tunnel")
    ]
    candidates.sort(
        key=lambda r: (
            -np.linalg.norm(np.diff(r["coordinates"], axis=0)),
            r["source_id"],
        )
    )
    candidates = candidates[:256]
    points = np.asarray([r["coordinates"] for r in candidates])
    heights = target(points.reshape(-1, 2)).reshape(-1, 2)
    rows = []
    for road, xy, z in zip(candidates, points, heights):
        rows.append(
            {
                "source_id": road["source_id"],
                "properties": road["properties"],
                "xy": xy.tolist(),
                "target_heights_m": z.tolist(),
                "width_m": road["width_m"],
                "longitudinal_grade": RECIPES.get(
                    street_kind(road["properties"]), 0.08
                ),
            }
        )
    FIXTURE.write_text(
        json.dumps(
            {
                "source_sha256": hashlib.sha256(INPUT.read_bytes()).hexdigest(),
                "source_receipt": raw["receipt"],
                "dem": receipt,
                "cases": rows,
            },
            indent=2,
        )
        + "\n"
    )


def compare_sources():
    inputs = json.loads(FIXTURE.read_text())
    results, inspection = [], None
    for road in inputs["cases"]:
        xy, z = np.asarray(road["xy"]), np.asarray(road["target_heights_m"])
        direction = xy[1] - xy[0]
        length = float(np.linalg.norm(direction))
        direction /= length
        longitudinal = road["longitudinal_grade"]
        grade = longitudinal * 0.8
        note = {
            "source_id": road["source_id"],
            "properties": road["properties"],
            "xy": xy.tolist(),
            "target_heights_m": z.tolist(),
            "grade": grade,
            "methods": {},
        }
        for method in ("parabolic", "cubic"):
            began = time.perf_counter_ns()
            profile, report = plan_vertical_transition(
                *z, length, grade=grade, second_derivative=0.002, method=method
            )
            report["plan_ns"] = time.perf_counter_ns() - began
            if profile is not None:
                table, sampling = sample_vertical_transition(
                    profile, height_error_m=0.01
                )
                if table is None:
                    report.update(valid=False, reason=sampling["reason"])
                else:
                    mesh = ribbon(xy[0], direction, table, road["width_m"] or 4.0)
                    gradients = normal_gradients(
                        mesh.vertices[:, :2], mesh.triangles, mesh.vertices[:, 2]
                    )
                    frame = np.array(
                        [
                            direction / longitudinal,
                            [-direction[1] / 0.025, direction[0] / 0.025],
                        ]
                    )
                    normalized = np.linalg.norm(gradients @ frame.T, axis=1).max()
                    if normalized > 1.0000001:
                        raise ValueError(
                            "actual full ribbon face exceeds normalized grade budget"
                        )
                    query = np.linspace(0.0, length, 257)
                    error = float(
                        abs(
                            profile.evaluate(query)
                            - np.interp(query, table[:, 0], table[:, 1])
                        ).max()
                    )
                    if error > sampling["maximum_height_chord_bound_m"] + 1e-7:
                        raise ValueError(
                            "independent height samples exceed chord certificate"
                        )
                    endpoint_error = float(
                        abs(profile.evaluate([0.0, length]) - z).max()
                    )
                    if endpoint_error > 1e-7:
                        raise ValueError("source height contacts changed")
                    report.update(
                        sampling=sampling,
                        maximum_normalized_face_grade=float(normalized),
                        width_m=road["width_m"] or 4.0,
                    )
                    report.update(
                        sampled_chord_error_m=error,
                        endpoint_error_m=endpoint_error,
                        width_assumed=not bool(road["width_m"]),
                        endpoint_chord_grade_jump=float(
                            abs(
                                np.diff(table[[0, 1, -2, -1], 1])[[0, 2]]
                                / np.diff(table[[0, 1, -2, -1], 0])[[0, 2]]
                            ).max()
                        ),
                    )
                    if (
                        inspection is None
                        and abs(z[1] - z[0]) > 2.0
                        and method == "cubic"
                    ):
                        inspection = dict(
                            note,
                            length_m=length,
                            width_m=road["width_m"] or 4.0,
                            grade=grade,
                        )
            note["methods"][method] = report
        results.append(note)
    return {
        "source_sha256": inputs["source_sha256"],
        "source_receipt": inputs["source_receipt"],
        "dem": inputs["dem"],
        "fixture_sha256": hashlib.sha256(FIXTURE.read_bytes()).hexdigest(),
        "cases": results,
        "inspection": inspection,
        "selection": "256 longest straight, non-bridge/non-tunnel saved axes; same source XYZ targets, class grade/0.025 cross limit, actual crossfall0.015, second derivative0.002/m; explicit flat-end policy, not existing junction-gradient evidence",
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--capture-inputs",
        action="store_true",
        help="Explicitly replace saved source/DEM end targets; ordinary runs keep them fixed",
    )
    args = parser.parse_args()
    if args.capture_inputs:
        capture_sources()
    OUTPUT.mkdir(parents=True, exist_ok=True)
    report = compare_sources()
    report.update(
        commit=subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
        ).strip(),
        peak_rss_bytes=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * 1024,
    )
    (OUTPUT / "comparison.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                method: {
                    "valid": sum(
                        r["methods"][method]["valid"] for r in report["cases"]
                    ),
                    "p50_plan_us": float(
                        np.median(
                            [
                                r["methods"][method]["plan_ns"] / 1000
                                for r in report["cases"]
                            ]
                        )
                    ),
                }
                for method in ("parabolic", "cubic")
            }
        )
    )


if __name__ == "__main__":
    main()
