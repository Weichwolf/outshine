"""Inspect observed outer contacts without inferring free ends from missing joins."""

import argparse
import json
from pathlib import Path

FIXTURE = Path(__file__).with_name("fixtures") / "mvt_stair_boundary_neighbours.json"


def node_at(way, point):
    nodes = {
        p[3] for p in way["sampled_design_xy_height_node"] if p[:2] == point and p[3]
    }
    if len(nodes) != 1:
        raise ValueError("one recorded derived node required at the observed contact")
    return nodes.pop()


def inspect(case):
    ways = {w["native_way"]: w for w in case["ways"]}
    result = []
    for focus in case["focus_endpoints"]:
        way = ways[focus["native_way"]]
        point = way["source_xy_m"][focus["endpoint"]]
        node = node_at(way, point)
        neighbours = []
        for other in ways.values():
            if other is way:
                continue
            for at, xy in enumerate(other["source_xy_m"]):
                if xy != point:
                    continue
                if node_at(other, xy) != node:
                    raise ValueError(
                        "coincident source points lack a shared native node"
                    )
                if (way["derived_layer"], way["derived_bridge"]) != (
                    other["derived_layer"],
                    other["derived_bridge"],
                ):
                    raise ValueError("observed contact has conflicting derived levels")
                neighbours.append(
                    {
                        "native_way": other["native_way"],
                        "source_point_index": at,
                        "provider_feature_id": other["provider_feature_id"],
                        "role": "endpoint"
                        if at in (0, len(other["source_xy_m"]) - 1)
                        else "interior",
                    }
                )
        result.append(
            {
                **focus,
                "xy_m": point,
                "derived_node": node,
                "observed_neighbours": neighbours,
                "prepared_junction_count": len(way["prepared_junctions"]),
                "status": "observed_contact_requires_shared_profile"
                if neighbours
                else case["unmatched_status"],
            }
        )
    observed = [
        sorted(n["native_way"] for n in r["observed_neighbours"]) for r in result
    ]
    if observed != case["expected_observed_neighbours"]:
        raise ValueError("recorded neighbour inventory changed")
    return {
        "contacts": result,
        "scope": "Exact observed source coordinates and derived native nodes only; no full topology, geometric-nearness or fixed-height claim.",
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, default=FIXTURE)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    result = inspect(json.loads(args.input.read_text()))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(
        "four outer ends: three observed contacts, one unknown; no free-end certificate"
    )


if __name__ == "__main__":
    main()
