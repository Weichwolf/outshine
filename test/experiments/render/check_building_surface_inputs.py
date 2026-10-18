"""Check exported planes/normals against native samples and original source identities."""

import argparse
import json
import math
import sys
from collections import Counter
from pathlib import Path

EPS = sys.float_info.epsilon


def interval_contains(interval, value):
    low, high = interval["min"], interval["max"]
    return (
        low is None or value > low or (value == low and interval["min_closed"])
    ) and (high is None or value < high or (value == high and interval["max_closed"]))


def box_v(shape, point):
    east = point[0] - shape["centre"][0]
    north = point[1] - shape["centre"][1]
    return -east * shape["axis_u"][1] + north * shape["axis_u"][0]


def check_shape(shape, counts):
    counts["shapes"] += 1
    counts["holes"] += len(shape["holes"])
    if shape["roof_kind"] == 2:
        if shape["hip"]["available"]:
            assert shape["hip"]["maximum_distance_m"] > 0
            counts["hip_plans"] += 1
            counts["hip_facets"] += len(shape["hip"]["facets"])
        else:
            counts["unavailable_hip_plans"] += 1
            assert not shape["height_controls"]
    bands = shape["roof_bands"]
    if bands is None:
        return
    counts["roof_bands"] += len(bands)
    for control in shape["height_controls"]:
        value = box_v(shape, control["point"])
        selected = [
            band for band in bands if interval_contains(band["v_interval"], value)
        ]
        assert selected
        native = control["height_m"] + shape["roof_deck_m"]
        for band in selected:
            actual = band["height_at_v0_m"] + band["slope_v"] * value
            scale = max(
                1.0,
                abs(native),
                abs(actual),
                abs(band["height_at_v0_m"]),
                abs(band["slope_v"] * value),
            )
            assert abs(actual - native) <= 512 * EPS * scale
        counts["native_height_controls"] += 1


def local_point(surface, point):
    delta = [a - b for a, b in zip(point, surface["origin"], strict=True)]
    return [
        sum(a * b for a, b in zip(delta, surface[axis], strict=True))
        for axis in ("east", "north", "up")
    ]


def global_normal(surface, normal):
    return [
        sum(
            normal[axis] * surface[name][component]
            for axis, name in enumerate(("east", "north", "up"))
        )
        for component in range(3)
    ]


def check_hip_hit(surface, shape, ray, counts):
    hit = ray["hit"]
    point = local_point(surface, hit["point"])
    plan = shape["hip"]
    scale = shape["rise_m"] / plan["maximum_distance_m"]
    east, north = (point[i] - plan["origin"][i] for i in range(2))
    tolerance = 4096 * EPS * max(1.0, abs(hit["along"]), *map(abs, ray["origin"]))
    candidates = []
    for facet in plan["facets"]:
        distance = (
            east * facet["gradient"][0]
            + north * facet["gradient"][1]
            + facet["intercept_m"]
        )
        height = shape["roof_deck_m"] + scale * distance
        normal = global_normal(surface, facet["normal_enu"])
        if (
            abs(point[2] - height) <= tolerance
            and math.dist(normal, hit["normal"]) <= 256 * EPS
        ):
            candidates.append(facet)
    assert candidates
    counts["native_hip_plane_controls"] += 1
    counts["native_normal_controls"] += 1


def check_ray(surface, ray, counts):
    counts["native_rays"] += 1
    hit = ray["hit"]
    if hit is None:
        counts["native_misses"] += 1
        return
    counts["native_hits"] += 1
    assert ray["minimum"] <= hit["along"] <= ray["maximum"]
    assert all(math.isfinite(x) for x in hit["point"] + hit["normal"])
    assert abs(math.hypot(*hit["normal"]) - 1) <= 256 * EPS
    face = surface["faces"][hit["face"]]
    assert (face["part"], face["side"]) == (hit["part"], hit["side"])
    expected = face["normal_enu"]
    if hit["side"] == 1:
        shape = surface["shapes"][hit["part"]]
        bands = shape["roof_bands"]
        if bands is None:
            if shape["roof_kind"] == 2 and shape["hip"]["available"]:
                check_hip_hit(surface, shape, ray, counts)
                return
            counts["piecewise_or_curved_roof_hits"] += 1
            return
        value = box_v(shape, local_point(surface, hit["point"]))
        scale = max(1.0, abs(value), abs(hit["along"]), *map(abs, ray["origin"]))
        if any(
            end is not None and abs(end - value) <= 4096 * EPS * scale
            for band in bands
            for end in (band["v_interval"]["min"], band["v_interval"]["max"])
        ):
            counts["roof_normal_at_crease"] += 1
            return
        selected = [b for b in bands if interval_contains(b["v_interval"], value)]
        assert len(selected) == 1
        expected = selected[0]["normal_enu"]
    assert expected is not None
    assert math.dist(global_normal(surface, expected), hit["normal"]) <= 256 * EPS
    counts["native_normal_controls"] += 1


def verify(document, baseline):
    counts = Counter()
    assert document["results"] == []
    assert document["parent"] == baseline["parent"]
    assert len(document["sources"]) == len(baseline["sources"])
    for source, original in zip(document["sources"], baseline["sources"], strict=True):
        assert {k: v for k, v in source.items() if k != "surface"} == original
        counts["source_records"] += 1
        surface = source["surface"]
        assert surface["opaque_for_coverage"] is None
        assert surface["interior_topology"] is None
        expected = []
        for part, shape in enumerate(surface["shapes"]):
            check_shape(shape, counts)
            size = 2 + len(shape["ring"]) + sum(map(len, shape["holes"]))
            expected.extend((part, side) for side in range(size))
        assert [(f["part"], f["side"]) for f in surface["faces"]] == expected
        assert [f["index"] for f in surface["faces"]] == list(range(len(expected)))
        counts["face_records"] += len(expected)
        for ray in surface["rays"] or []:
            check_ray(surface, ray, counts)
    return dict(counts)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--baseline", type=Path, required=True)
    args = parser.parse_args()
    print(
        json.dumps(
            verify(
                json.loads(args.input.read_text()),
                json.loads(args.baseline.read_text()),
            )
        )
    )
