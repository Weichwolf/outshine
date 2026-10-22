"""Native connection-plan comparison; no native height/body/scene admission."""

import argparse
import copy
import json
import subprocess
from pathlib import Path

import numpy as np
from mvt_t_connection_inputs import inputs, vienna_inputs
from mvt_t_connections import _checked, plan_connections
from stair_test_transform import rigid_coordinates

ROLES = {"unknown": 0, "connectable": 1, "terminal": 2, "fragment": 3}
REASONS = (
    "proposed_2d_pending_profiles",
    "endpoint_role_unknown",
    "endpoint_role_terminal",
    "endpoint_role_fragment",
    "incompatible_or_unknown_ground_access",
    "no_admissible_target",
    "ambiguous_targets",
    "candidate_budget_exhausted",
)


def native_plan(
    binary,
    rows,
    *,
    reach_m=0.5,
    min_crossing_sine=0.5,
    max_segments=200000,
    max_endpoints=4096,
    max_candidates=100000,
):
    rows = _checked(rows, max_segments, max_endpoints)
    text = [
        f"{len(rows)} {reach_m} {min_crossing_sine} {max_segments} {max_endpoints} {max_candidates}"
    ]
    for row in rows:
        kind = (
            2
            if row["kind"] == "steps"
            else int(row["kind"] in ("path", "footway", "pedestrian"))
        )
        fields = [
            kind,
            int(row["layer"] is not None),
            row["layer"] or 0,
            *[
                -1 if row[k] is None else int(row[k])
                for k in ("bridge", "tunnel", "foot")
            ],
            *[ROLES[r] for r in row["endpoint_roles"]],
            row["half_width_m"],
            int(row["position_error_bound_m"] is not None),
            row["position_error_bound_m"] or 0,
            len(row["xy_m"]),
        ]
        text.append(" ".join(map(str, fields)))
        text.extend(" ".join(map(str, point)) for point in row["xy_m"])
    result = subprocess.run(
        [str(binary)],
        input="\n".join(text) + "\n",
        text=True,
        capture_output=True,
        timeout=30,
        check=False,
    )
    if result.returncode != 0:
        raise ValueError(
            f"native connection preparation failed: {result.returncode}, {result.stdout}, {result.stderr}"
        )
    lines = iter(result.stdout.splitlines())
    header = next(lines).split()
    if len(header) != 13 or header[0] != "plan":
        raise ValueError("native plan header required")
    connections = []
    for _ in range(int(header[1])):
        (
            tag,
            branch,
            end,
            target,
            segment,
            fraction,
            station,
            east,
            north,
            advance,
            sine,
            known,
        ) = next(lines).split()
        if tag != "connection":
            raise ValueError("native connection record required")
        source, destination = (
            rows[int(branch)]["source_id"],
            rows[int(target)]["source_id"],
        )
        endpoint = 0 if int(end) == 0 else -1
        point = rows[int(branch)]["xy_m"][endpoint]
        connections.append(
            {
                "construction_id": [
                    "derived-t",
                    source,
                    endpoint,
                    destination,
                    int(segment),
                ],
                "source_id": source,
                "source_endpoint": endpoint,
                "source_original_xy_m": list(point),
                "target_source_id": destination,
                "target_segment": int(segment),
                "target_fraction": float(fraction),
                "target_station_m": float(station),
                "xy_m": [float(east), float(north)],
                "advance_m": float(advance),
                "crossing_sine": float(sine),
                "provider_position_bound_known": bool(int(known)),
                "position_basis": "coincident_original_coordinate"
                if float(advance) == 0
                else "bounded_forward_extension_recipe",
                "source_node_identity_claimed": False,
            }
        )
    dispositions = []
    for _ in range(int(header[2])):
        tag, source, end, reason = next(lines).split()
        if tag != "endpoint":
            raise ValueError("native endpoint record required")
        dispositions.append(
            {
                "source_id": rows[int(source)]["source_id"],
                "endpoint": 0 if int(end) == 0 else -1,
                "reason": REASONS[int(reason)],
            }
        )
    axes = {}
    for _ in rows:
        tag, source, count = next(lines).split()
        if tag != "axis":
            raise ValueError("native axis record required")
        axes[rows[int(source)]["source_id"]] = [
            list(map(float, next(lines).split())) for _ in range(int(count))
        ]
    if list(lines):
        raise ValueError("unexpected native output suffix")
    work = dict(
        zip(
            (
                "segments",
                "index_entries",
                "queries",
                "candidate_tests",
                "max_retained_query_indices",
                "budget_refusals",
                "index_build_container_bytes",
                "scratch_container_bytes",
            ),
            map(int, header[3:11]),
            strict=True,
        )
    )
    return {
        "status": "proposed_2d_not_height_or_native_admission",
        "connections": connections,
        "axes_xy_m": axes,
        "endpoint_dispositions": dispositions,
        "source_usage": {r["source_id"]: "original_axis_retained" for r in rows},
        "recipe": {"reach_m": reach_m, "min_crossing_sine": min_crossing_sine},
        "work": {
            **work,
            "plan_ms": float(header[11]),
            "all_axes_ms": float(header[12]),
        },
    }


def equal_quality(actual, expected):
    if isinstance(expected, dict):
        if set(actual) != set(expected):
            raise ValueError("different result fields")
        for key in expected:
            equal_quality(actual[key], expected[key])
    elif isinstance(expected, list):
        if len(actual) != len(expected):
            raise ValueError("different result lengths")
        for a, b in zip(actual, expected, strict=True):
            equal_quality(a, b)
    elif isinstance(expected, float):
        np.testing.assert_allclose(actual, expected, atol=1e-10, rtol=0)
    elif actual != expected:
        raise ValueError(f"different result: {actual!r} != {expected!r}")


def compare(binary, output):
    _case, original = inputs()
    cases = [("original", original), ("source-context", vienna_inputs())]
    for angle in (0.0, 0.37, 1.57, 3.14):
        transformed = copy.deepcopy(original)
        coordinates, _rotation = rigid_coordinates(angle, (130, -50))
        for row in transformed:
            row["xy_m"] = coordinates(row["xy_m"])
        cases.append((f"rigid-{angle}", transformed[::-1]))
    records = []
    for name, rows in cases:
        reference = plan_connections(rows, method="scan")
        native = native_plan(binary, rows)
        for key in (
            "status",
            "connections",
            "axes_xy_m",
            "endpoint_dispositions",
            "source_usage",
            "recipe",
        ):
            equal_quality(native[key], reference[key])
        records.append(
            {
                "case": name,
                "connections": len(native["connections"]),
                "native_work": native["work"],
                "reference_work": reference["work"],
            }
        )
    output.mkdir(parents=True, exist_ok=True)
    (output / "report.json").write_text(
        json.dumps(
            {
                "cases": records,
                "scope": "Native 2D proposals and original-axis preservation only; existing 1e-10 comparison tolerance for derived Float64 values. No world/native body admission. Container capacities are not total peak memory.",
            },
            indent=2,
        )
    )
    print(json.dumps(records))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    compare(args.binary.resolve(), args.output)


if __name__ == "__main__":
    main()
