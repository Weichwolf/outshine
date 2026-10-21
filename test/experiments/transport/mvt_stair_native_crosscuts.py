"""Inspect complete transverse cuts of retained native road triangles."""

import argparse
import json
import math
from itertools import pairwise
from pathlib import Path

FIXTURE = Path(__file__).with_name("fixtures") / "mvt_stair_native_crosscuts.json"


def cross_section(triangles, center, tangent, half_span_m=3.0):
    if len(center) != 2 or len(tangent) != 2 or not triangles:
        raise ValueError("nonempty geometry and two-dimensional cut required")
    if not all(math.isfinite(v) for v in [*center, *tangent, half_span_m]):
        raise ValueError("finite cut required")
    length = math.hypot(*tangent)
    if length == 0 or half_span_m <= 0:
        raise ValueError("nonzero direction and positive span required")
    along = [v / length for v in tangent]
    normal = [-along[1], along[0]]
    segments = []
    for triangle in triangles:
        xyz = triangle["enu_m"]
        if len(xyz) != 3 or any(
            len(p) != 3 or not all(math.isfinite(v) for v in p) for p in xyz
        ):
            raise ValueError("finite native triangle required")
        distance = [sum((p[i] - center[i]) * along[i] for i in range(2)) for p in xyz]
        points = []
        for i in range(3):
            j = (i + 1) % 3
            if distance[i] == 0:
                points.append(tuple(xyz[i]))
            if distance[i] * distance[j] < 0:
                fraction = distance[i] / (distance[i] - distance[j])
                points.append(
                    tuple(
                        xyz[i][k] + fraction * (xyz[j][k] - xyz[i][k]) for k in range(3)
                    )
                )
        points = sorted(set(points))
        if len(points) < 2:
            continue
        if len(points) != 2:
            raise ValueError("coplanar native triangle needs a different cut")
        a, b = sorted(
            [sum((p[i] - center[i]) * normal[i] for i in range(2)), p[2]]
            for p in points
        )
        if b[0] <= -half_span_m or a[0] >= half_span_m:
            continue
        if b[0] == a[0]:
            raise ValueError("vertical native section is not a single height profile")
        if a[0] <= -half_span_m or b[0] >= half_span_m:
            raise ValueError("cut span does not enclose the complete surface band")
        segments.append(
            {
                "page": triangle["page"],
                "triangle": triangle["triangle"],
                "offset_height_m": [a, b],
            }
        )
    if not segments:
        raise ValueError("cut has no retained native surface support")
    segments.sort(key=lambda s: s["offset_height_m"][0])
    seam_xy_m = seam_height_m = 0.0
    for previous, current in pairwise(segments):
        a = previous["offset_height_m"][1]
        b = current["offset_height_m"][0]
        seam_xy_m = max(seam_xy_m, abs(b[0] - a[0]))
        seam_height_m = max(seam_height_m, abs(b[1] - a[1]))
        if b[0] - a[0] > 1e-7:
            raise ValueError("native section has a coverage gap")
        if a[0] - b[0] > 1e-7:
            raise ValueError("native section has overlapping support")
        if abs(b[1] - a[1]) > 1e-7:
            raise ValueError("native section has a height discontinuity")
    first, last = segments[0]["offset_height_m"][0], segments[-1]["offset_height_m"][1]
    return {
        "segments": segments,
        "width_m": last[0] - first[0],
        "offset_bounds_m": [first[0], last[0]],
        "height_bounds_m": [
            min(p[1] for s in segments for p in s["offset_height_m"]),
            max(p[1] for s in segments for p in s["offset_height_m"]),
        ],
        "maximum_seam_offset_error_m": seam_xy_m,
        "maximum_seam_height_error_m": seam_height_m,
        "scope": "Numerical inspection of all retained crossing faces; raw segments retained, no flattening or owner selection.",
    }


def inspect(case):
    return [
        {
            "native_way": cut["native_way"],
            "source_station_m": cut["source_station_m"],
            **cross_section(
                cut["triangles"],
                cut["center_xy_m"],
                cut["tangent_xy"],
                cut["half_span_m"],
            ),
        }
        for cut in case["cuts"]
    ]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, default=FIXTURE)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    result = inspect(json.loads(args.input.read_text()))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(
        f"{len(result)} complete native transverse profiles; no replacement admission"
    )


if __name__ == "__main__":
    main()
