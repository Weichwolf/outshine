import argparse
import json
import math
from itertools import pairwise
from pathlib import Path

from shapely.geometry import Polygon


def projected(point, start, end):
    direction = [b - a for a, b in zip(start, end)]
    fraction = sum((p - a) * d for p, a, d in zip(point, start, direction)) / sum(
        d * d for d in direction
    )
    return fraction, [a + fraction * d for a, d in zip(start, direction)]


def height_at(triangle, point):
    a, b, c = triangle
    x, y = point
    denominator = (b[1] - c[1]) * (a[0] - c[0]) + (c[0] - b[0]) * (a[1] - c[1])
    u = ((b[1] - c[1]) * (x - c[0]) + (c[0] - b[0]) * (y - c[1])) / denominator
    v = ((c[1] - a[1]) * (x - c[0]) + (a[0] - c[0]) * (y - c[1])) / denominator
    return u * a[2] + v * b[2] + (1 - u - v) * c[2]


def reproduce(case):
    ways = {way["native_way"]: way for way in case["ways"]}
    focus = case["focus"]
    stair = ways[focus["endpoint_way"]]
    path = ways[focus["target_way"]]
    endpoint = stair["source_xy_m"][focus["endpoint"]]
    segment = path["source_xy_m"][focus["target_segment"] :][:2]
    fraction, projection = projected(endpoint, *segment)
    gap = math.dist(endpoint, projection)
    first, second = [face["enu_m"] for face in case["recorded_overlapping_faces"]]
    overlap = Polygon([p[:2] for p in first]).intersection(
        Polygon([p[:2] for p in second])
    )
    maximum = max(
        abs(height_at(first, p) - height_at(second, p)) for p in overlap.exterior.coords
    )
    stations = stair["prepared_stations_xyz_m"]
    length = sum(math.dist(a[:2], b[:2]) for a, b in pairwise(stations))
    desired = stair["sampled_design_xyz_m"][-1][2]
    available_rise = stair["max_gradient"] * length
    return {
        "status": "reproduced_frozen_counterexample_not_a_solver_result",
        "same_derived_level": stair["derived_layer"] == path["derived_layer"],
        "both_derived_ground": not stair["derived_bridge"]
        and not path["derived_bridge"],
        "endpoint_gap_m": gap,
        "target_segment_fraction": fraction,
        "projected_endpoint_xy_m": projection,
        "endpoint_inside_target_ribbon": 0 < fraction < 1
        and gap < path["half_width_m"],
        "endpoint_is_target_source_vertex": any(
            math.dist(endpoint, p) < 1e-8 for p in path["source_xy_m"]
        ),
        "stair_end_has_prepared_grade": stair["prepared_edges"][0]["has_grade"][-1],
        "frozen_face_overlap_m2": overlap.area,
        "frozen_max_height_difference_m": maximum,
        "remaining_stair_length_m": length,
        "available_stair_rise_m": available_rise,
        "original_upper_height_prior_m": desired,
        "prepared_upper_height_m": stations[-1][2],
        "upper_prior_rise_deficit_m": desired - stations[0][2] - available_rise,
    }


def main():
    parser = argparse.ArgumentParser(
        description="Replay a frozen native MVT stair/path defect."
    )
    parser.add_argument(
        "--case",
        type=Path,
        default=Path(__file__).parent / "fixtures/mvt_stair_t_junction.json",
    )
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    result = reproduce(json.loads(args.case.read_text()))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, separators=(",", ":")))


if __name__ == "__main__":
    main()
