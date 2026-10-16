"""Explicit offline capture of one real bridge, connected ground ways and local field."""

import argparse
import json
from pathlib import Path

import numpy as np
import shapely
from camera_height_inputs import digest
from shared_bridge_geometry import corridor
from tilted_rail_inputs import FIELD_PIN, GROUND_PIN
from transport_network import source_identity, source_roads, transport_widths
from wien_network_sources import INPUT_PIN

HERE = Path(__file__).resolve().parent
BRIDGE = "openfreemap:14/8939/5680:transportation:94031072:0"
APPROACHES = [
    "openfreemap:14/8939/5680:transportation:32205120:5",
    "openfreemap:14/8939/5680:transportation:32205120:6",
]


def normalized(raw):
    inputs = [
        {
            "line": shapely.LineString(r["coordinates"]),
            "width": r["width_m"],
            "properties": r["properties"],
        }
        for r in raw["roads"]
    ]
    recipes = json.loads((HERE / "transport_network_recipes.json").read_text())
    values, _note = source_roads(inputs, recipes, transport_widths())
    widths = {source_identity(r): r["width"] for r in values}
    return [
        dict(r, width_m=widths.get(source_identity(s)))
        for r, s in zip(raw["roads"], inputs, strict=True)
    ]


def neighbours(rows, chosen, usage):
    selected = {r["source_id"] for r in chosen}
    parts = [
        corridor({"xy": r["coordinates"], "width_m": r["width_m"]}) for r in chosen
    ]
    domain = shapely.union_all(parts)
    records = []
    for row in rows:
        if row["source_id"] in selected or usage[row["source_id"]]["status"] != "used":
            continue
        if row["width_m"] is None:
            raise ValueError("used physical source lacks an explicit normalized width")
        band = shapely.LineString(row["coordinates"]).buffer(
            row["width_m"] / 2, quad_segs=2, cap_style="round", join_style="bevel"
        )
        overlap = band.intersection(domain)
        if overlap.area <= 1e-9:
            continue
        properties = row["properties"]
        if properties.get("brunnel") in ("bridge", "tunnel") or properties.get(
            "layer", 0
        ):
            raise ValueError(
                "existing non-ground source needs its chosen physical surface"
            )
        bridge_overlap = band.intersection(parts[0])
        kind = "underpass" if bridge_overlap.area > 1e-9 else "ground_joint"
        if kind == "underpass" and properties.get("class") != "path":
            raise ValueError("nonmotorized underpass recipe does not cover this source")
        records.append(dict(row, role=kind))
    if len(records) > 64:
        raise ValueError("local bridge neighbour source budget")
    return domain, records


def actual_crossings(network, chosen, records):
    bridge = corridor({"xy": chosen[0]["coordinates"], "width_m": chosen[0]["width_m"]})
    with np.load(network / "ground.npz") as data:
        xy = data["vertices"][data["faces"][data["road"]], :2]
    lo, hi = np.array(bridge.bounds).reshape(2, 2)
    hits = np.all(xy.max(axis=1) >= lo, axis=1) & np.all(xy.min(axis=1) <= hi, axis=1)
    if hits.sum() > 8192:
        raise ValueError("actual traffic crossing capture face budget")
    clipped = shapely.intersection(shapely.polygons(xy[hits]), bridge)
    parts = shapely.get_parts(shapely.union_all(clipped[shapely.area(clipped) > 1e-10]))
    for row in records:
        if row["role"] != "underpass":
            continue
        band = shapely.LineString(row["coordinates"]).buffer(row["width_m"] / 2)
        selected = [p for p in parts if p.intersection(band).area > 1e-8]
        if len(selected) != 1:
            raise ValueError(
                "underpass needs one unique actual original traffic footprint"
            )
        row["actual_ground_traffic_wkb_hex"] = selected[0].wkb_hex


def capture(args):
    if args.output.exists():
        raise ValueError("explicit fresh fixture output required")
    if digest(args.capture) != INPUT_PIN:
        raise ValueError("pinned complete source capture required")
    for name, pin in (("field.npz", FIELD_PIN), ("ground.npz", GROUND_PIN)):
        if digest(args.network / name) != pin:
            raise ValueError("pinned original shared height field/ground required")
    raw = json.loads(args.capture.read_text())
    rows = normalized(raw)
    lookup = {r["source_id"]: r for r in rows}
    chosen = [
        dict(lookup[k], role="bridge" if k == BRIDGE else "approach")
        for k in [BRIDGE, *APPROACHES]
    ]
    report = json.loads((args.network / "comparison.json").read_text())
    usage = {r["source_id"]: r for r in report["source_usage"]}
    if usage[BRIDGE]["status"] != "not_used" or any(
        usage[k]["status"] != "used" for k in APPROACHES
    ):
        raise ValueError("expected old source decisions changed")
    domain, records = neighbours(rows, chosen, usage)
    actual_crossings(args.network, chosen, records)
    with np.load(args.network / "field.npz") as saved:
        xyz, faces = saved["vertices"], saved["faces"]
    selected = shapely.intersects(shapely.polygons(xyz[faces, :2]), domain)
    face_ids = np.flatnonzero(selected)
    vertex_ids, inverse = np.unique(faces[selected], return_inverse=True)
    if len(face_ids) > 128 or len(vertex_ids) > 256:
        raise ValueError("local original field capture budget")
    result = {
        "origin_lat_lon": raw["origin_lat_lon"],
        "planet": "20260927_080001_pt",
        "capture_sha256": INPUT_PIN,
        "field_sha256": FIELD_PIN,
        "ground_sha256": GROUND_PIN,
        "sources": chosen + records,
        "field_vertices": xyz[vertex_ids].tolist(),
        "field_faces": inverse.reshape(-1, 3).tolist(),
        "original_field_face_ids": face_ids.tolist(),
        "recipe": {
            "maximum_grade": 0.12,
            "deck_thickness_m": 0.35,
            "nonmotorized_headroom_m": 2.5,
        },
        "scope": "Actual source axes/properties and original affine field cells. Widths, bridge height, thickness, headroom and profile are procedural recipes. No measured deck elevation, public API, native integration or source-use admission.",
    }
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps(
            {
                "sources": len(result["sources"]),
                "field_faces": len(face_ids),
                "sha256": digest(args.output),
            }
        )
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--capture", type=Path, required=True)
    parser.add_argument("--network", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    capture(parser.parse_args())
