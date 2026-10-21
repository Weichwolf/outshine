import argparse
import json
import math
from pathlib import Path

import numpy as np
from shapely.geometry import Point, Polygon


def part_height(part, point):
    ring = np.asarray(part["ring_enu_m"])
    origin = ring[0, :2]
    design = np.column_stack((ring[:, :2] - origin, np.ones(len(ring))))
    plane, _, rank, _ = np.linalg.lstsq(design, ring[:, 2], rcond=None)
    if rank != 3 or np.max(abs(design @ plane - ring[:, 2])) > 1e-7:
        raise ValueError("The frozen physical support must be planar")
    polygon = Polygon(ring[:, :2], [[p[:2] for p in h] for h in part["holes_enu_m"]])
    if polygon.covers(Point(point)):
        return float(np.r_[np.asarray(point) - origin, 1] @ plane)
    return None


def reproduce(case):
    observations = []
    for sample in case["terrain_edge"]:
        point = sample["en"]
        blanket = []
        for contact in case["blanket_contacts"]:
            ring = list(zip(contact["ring"][::2], contact["ring"][1::2]))
            if Polygon(ring).covers(Point(point)):
                blanket.append(
                    contact["plateau"]
                    + sum(
                        (a - b) * slope
                        for a, b, slope in zip(point, contact["at"], contact["slope"])
                    )
                )
        actual = [
            height
            for building in case["buildings"]
            for part in building["parts"]
            if part["on_ground"] and (height := part_height(part, point)) is not None
        ]
        if len(blanket) != 1 or len(actual) != 1:
            raise ValueError(
                "This case requires exactly one physical owner at each point"
            )
        old = min(sample["was_up_m"], blanket[0])
        if not math.isclose(old, sample["up_m"], abs_tol=1e-7):
            raise ValueError("Blanket contact does not reproduce the captured terrain")
        observations.append(
            {
                "point_xy_m": point,
                "raw_height_m": sample["was_up_m"],
                "observed_height_m": sample["up_m"],
                "blanket_ceiling_m": blanket[0],
                "part_ceiling_m": actual[0],
                "cut_only_local_height_m": min(sample["was_up_m"], actual[0]),
            }
        )
    distance = math.dist(*(row["point_xy_m"] for row in observations))
    return {
        "status": "reproduced_frozen_counterexample_not_a_runtime_fix",
        "points": observations,
        "horizontal_edge_m": distance,
        "observed_height_drop_m": abs(
            observations[0]["observed_height_m"] - observations[1]["observed_height_m"]
        ),
        "local_contact_height_drop_m": abs(
            observations[0]["cut_only_local_height_m"]
            - observations[1]["cut_only_local_height_m"]
        ),
    }


def main():
    parser = argparse.ArgumentParser(
        description="Replay a stepped building ground defect"
    )
    parser.add_argument(
        "--case",
        type=Path,
        default=Path(__file__).parent / "fixtures/stepped_building_contacts.json",
    )
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    result = reproduce(json.loads(args.case.read_text()))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, separators=(",", ":")))


if __name__ == "__main__":
    main()
