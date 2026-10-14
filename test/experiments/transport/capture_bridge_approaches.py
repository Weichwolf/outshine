"""Capture real bridge/ground continuations from retained same-snapshot neighbors."""

import argparse
import hashlib
import json
from pathlib import Path

import numpy as np
import shapely
from capture_height_targets import sample_targets
from shared_bridge_checks import FIXTURE as ROOT_FIXTURE
from shared_bridge_geometry import corridor

ROOT = Path(__file__).resolve().parents[3]
FIXTURE = Path(__file__).with_name("fixtures") / "bridge-approaches-vienna.json"


def definition(raw, source, role):
    cache = (
        Path.home()
        / ".local/share/outshine/outshine/sources/transport-experiments/mapterhorn"
    )
    xy = np.asarray(source["coordinates"])
    heights, dem = sample_targets(raw["origin_lat_lon"], xy[[0, -1]], cache, 11, False)
    return {
        "source_id": source["source_id"],
        "xy": xy.tolist(),
        "properties": source["properties"],
        "width_m": source["width_m"] or 4.0,
        "width_assumed": True,
        "width_origin": "class_recipe" if source["width_m"] else "fallback_4m",
        "end_heights_m": heights.tolist(),
        "role": role,
        "dem": [
            {k: v for k, v in receipt.items() if k != "acquisition_and_decode_ms"}
            for receipt in dem
        ],
    }


def capture():
    root_data = json.loads(ROOT_FIXTURE.read_text())
    root_decks = [s for s in root_data["sources"] if s["role"] == "deck"]
    root_domain = shapely.union_all([corridor(s) for s in root_decks])
    root_ids = {s["source_id"] for s in root_data["sources"]}
    roads, inputs = [], []
    for name in ("Vienna-Donau", "Vienna-Donau-West", "Vienna-Donau-South"):
        path = ROOT / f"build/experiments/transport/inputs/{name}-input.json"
        raw = json.loads(path.read_text())
        if raw["origin_lat_lon"] != root_data["origin_lat_lon"]:
            raise ValueError("neighbor uses a different local frame")
        roads.extend((raw, r) for r in raw["roads"])
        inputs.append(
            {
                "site": name,
                "receipt": raw["receipt"],
                "source_input_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                "all_axis_ids": [r["source_id"] for r in raw["roads"]],
                "polygon_ids": [r["source_id"] for r in raw["deferred_sources"]],
            }
        )
    shapes = [
        corridor({"xy": r["coordinates"], "width_m": r["width_m"] or 4})
        for _, r in roads
    ]
    chosen, attempts = [], []
    for i, (raw, bridge) in enumerate(roads):
        if (
            bridge["source_id"] in root_ids
            or bridge["properties"].get("brunnel") != "bridge"
            or bridge["properties"].get("subclass") != "subway"
        ):
            continue
        if shapes[i].intersection(root_domain).area <= 0:
            continue
        for end, point in enumerate(np.asarray(bridge["coordinates"])[[0, -1]]):
            if root_domain.distance(shapely.Point(point)) < 10:
                continue
            matches = []
            for j, (_, ground) in enumerate(roads):
                props = ground["properties"]
                if (
                    props.get("subclass") != "subway"
                    or props.get("brunnel") in ("bridge", "tunnel")
                    or props.get("indoor")
                ):
                    continue
                distances = np.linalg.norm(
                    np.asarray(ground["coordinates"])[[0, -1]] - point, axis=1
                )
                for other in np.flatnonzero(distances <= 1e-6):
                    matches.append((j, int(other)))
            attempts.append(
                {
                    "bridge_source_id": bridge["source_id"],
                    "endpoint": end,
                    "matches": [roads[j][1]["source_id"] for j, _ in matches],
                    "reason": ""
                    if len(matches) == 1
                    else "ambiguous_or_missing_ground_continuation",
                }
            )
            if len(matches) == 1:
                j, other = matches[0]
                chosen.append(
                    {
                        "bridge": definition(raw, bridge, "approach_bridge"),
                        "ground": definition(
                            roads[j][0], roads[j][1], "approach_ground"
                        ),
                        "bridge_join_endpoint": end,
                        "ground_join_endpoint": other,
                    }
                )
    if not 1 <= len(chosen) <= 16:
        raise ValueError("bounded nonempty approach selection required")
    selected_ids = root_ids | {
        s["source_id"] for c in chosen for s in (c["bridge"], c["ground"])
    }
    approach_domain = shapely.union_all(
        [corridor(s) for c in chosen for s in (c["bridge"], c["ground"])]
    )
    conflicts = []
    for i, (raw, source) in enumerate(roads):
        if (
            source["source_id"] in selected_ids
            or shapes[i].intersection(approach_domain).area <= 0
        ):
            continue
        row = definition(raw, source, "unresolved")
        props = source["properties"]
        row["reason"] = (
            "indoor_surface_requires_buildings_contact"
            if props.get("indoor")
            else "tunnel_surface_requires_portal_recipe"
            if props.get("brunnel") == "tunnel"
            else "overlapping_source_requires_chosen_surface"
        )
        conflicts.append(row)
    if len(conflicts) > 64:
        raise ValueError("approach conflict source budget")
    document = {
        "approaches": chosen,
        "conflicts": conflicts,
        "attempts": attempts,
        "inputs": inputs,
        "root_fixture_sha256": hashlib.sha256(ROOT_FIXTURE.read_bytes()).hexdigest(),
        "scope": "Original neighbor buffers are retained. Positive root-deck overlap plus a unique exact subway endpoint chooses bridge/ground continuations. No identity or connection is inferred solely from a tile-local feature number. All other overlapping sources remain unresolved; south/east continuations are retained outside the selected approach plan.",
    }
    FIXTURE.write_text(json.dumps(document, indent=2) + "\n")
    print(
        json.dumps(
            {
                "fixture": str(FIXTURE),
                "approaches": len(chosen),
                "conflicts": len(conflicts),
                "sha256": hashlib.sha256(FIXTURE.read_bytes()).hexdigest(),
            }
        )
    )


if __name__ == "__main__":
    argparse.ArgumentParser(description=__doc__).parse_args()
    capture()
