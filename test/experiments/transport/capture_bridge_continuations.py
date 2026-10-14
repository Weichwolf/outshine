"""Explicit offline capture of the complete compatible three-tile bridge field."""

import argparse
import hashlib
import json
from pathlib import Path

import shapely
from capture_bridge_approaches import definition
from shared_bridge_geometry import corridor

ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent
FIXTURE = HERE / "fixtures/bridge-continuations-vienna.json"
PIN = "fcc91443d76d69223c8173061cf1f2f69d78446a9cc26fd58d9482c3e4f68db4"
ROOT_PIN = "c6e99c52ded6a696c0fe4a8f31be48143abbc0a21ab331e666dffc491a162ff5"
APPROACH_PIN = "52ce1d0895a9bfd65a9a6ed4033ae6f65b08a9cf8294adf34161ccc492b346e4"


def read_inputs():
    root_path = HERE / "fixtures/shared-bridge-vienna.json"
    approach_path = HERE / "fixtures/bridge-approaches-vienna.json"
    for path, pin in ((root_path, ROOT_PIN), (approach_path, APPROACH_PIN)):
        if hashlib.sha256(path.read_bytes()).hexdigest() != pin:
            raise ValueError("previous bridge fixture changed")
    old = json.loads(approach_path.read_text())
    inputs, roads = [], []
    for entry in old["inputs"]:
        path = (
            ROOT
            / "build/experiments/transport/inputs"
            / (entry["site"] + "-input.json")
        )
        if (
            hashlib.sha256(path.read_bytes()).hexdigest()
            != entry["source_input_sha256"]
        ):
            raise ValueError("complete pinned source input changed")
        raw = json.loads(path.read_text())
        if raw["origin_lat_lon"] != [48.21, 16.37]:
            raise ValueError("one explicit common frame required")
        inputs.append(entry)
        roads.extend((raw, source) for source in raw["roads"])
    if len(roads) > 4096 or len({s["source_id"] for _, s in roads}) != len(roads):
        raise ValueError("bounded unique source window required")
    return json.loads(root_path.read_text()), old, inputs, roads


def connected_bridges(roads, shapes, root_ids, *, max_bridges=64):
    candidates = set()
    for i, (_, source) in enumerate(roads):
        props = source["properties"]
        if (
            props.get("subclass") == "subway"
            and props.get("brunnel") == "bridge"
            and type(props.get("layer")) is int
            and props["layer"] == 1
            and not props.get("indoor")
        ):
            candidates.add(i)
    chosen = {i for i, (_, s) in enumerate(roads) if s["source_id"] in root_ids}
    if not chosen or not chosen <= candidates or len(chosen) > max_bridges:
        raise ValueError("compatible bounded original bridge field required")
    for _ in range(max_bridges):
        domain = shapely.union_all([shapes[i] for i in sorted(chosen)])
        additions = {
            i
            for i in candidates - chosen
            if shapes[i].intersection(domain).area > 0
            or shapes[i].intersection(domain).length > 1e-7
        }
        if not additions:
            return chosen
        if len(chosen) + len(additions) > max_bridges:
            raise ValueError("bridge expansion source budget")
        chosen |= additions
    raise ValueError("bridge expansion iteration budget")


def source_role(source, known_missing):
    props = source["properties"]
    if props.get("indoor"):
        return "deferred", (
            "buildings_floor_surface_not_planned"
            if source["source_id"] in known_missing
            else "indoor_surface_requires_buildings_contact"
        )
    if props.get("brunnel") == "tunnel":
        return "unresolved", "tunnel_surface_requires_portal_recipe"
    if props.get("brunnel") == "bridge":
        return "unresolved", "overlapping_source_requires_chosen_surface"
    if props.get("subclass") == "steps":
        return "unresolved", "stairs_require_separate_physical_recipe"
    if props.get("subclass") == "subway":
        return "unresolved", "ground_subway_continuation_requires_profile"
    level = props.get("layer", 0)
    if type(level) is not int or level >= 1:
        return "unresolved", "overlapping_level_requires_chosen_surface"
    return "ground", ""


def capture():
    root, old, inputs, roads = read_inputs()
    shapes = [
        corridor({"xy": s["coordinates"], "width_m": s["width_m"] or 4})
        for _, s in roads
    ]
    root_ids = {s["source_id"] for s in root["sources"] if s["role"] == "deck"}
    bridges = connected_bridges(roads, shapes, root_ids)
    approach_ids = {c["ground"]["source_id"] for c in old["approaches"]}
    top = bridges | {
        i for i, (_, s) in enumerate(roads) if s["source_id"] in approach_ids
    }
    domain = shapely.union_all([shapes[i] for i in sorted(top)])
    conflicts = {
        i
        for i in range(len(roads))
        if i not in top and shapes[i].intersection(domain).area > 0
    }
    if len(top | conflicts) > 128:
        raise ValueError("complete field source budget")
    known_missing = {s["source_id"] for s in root["sources"] if s["role"] == "deferred"}
    sources = []
    for i in sorted(top | conflicts):
        raw, source = roads[i]
        role, reason = (
            ("deck" if i in bridges else "approach_ground", "")
            if i in top
            else source_role(source, known_missing)
        )
        if role in ("deck", "ground", "approach_ground"):
            row = definition(raw, source, role)
        else:
            row = {
                "source_id": source["source_id"],
                "xy": source["coordinates"],
                "width_m": source["width_m"] or 4,
                "width_assumed": True,
                "width_origin": "class_recipe" if source["width_m"] else "fallback_4m",
                "properties": source["properties"],
                "role": role,
            }
        sources.append(dict(row, omission_reason=reason))
    return {
        "sources": sources,
        "inputs": inputs,
        "old_root_fixture_sha256": ROOT_PIN,
        "old_approach_fixture_sha256": APPROACH_PIN,
        "origin_lat_lon": root["origin_lat_lon"],
        "scope": "Complete compatible connected bridge corridor field in three pinned tiles, all positive-area source conflicts. Geometry connectivity is not a traffic graph. Tile clipping is not a chosen floor contact; unknown indoor/tunnel/ground-subway/other-elevated surfaces remain unresolved. All original complete tile features remain in the inputs.",
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--write-fixture", action="store_true")
    args = parser.parse_args()
    result = capture()
    if args.write_fixture:
        FIXTURE.write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps(
            {
                "roles": {
                    role: sum(s["role"] == role for s in result["sources"])
                    for role in sorted({s["role"] for s in result["sources"]})
                },
                "written": args.write_fixture,
            }
        )
    )


if __name__ == "__main__":
    main()
