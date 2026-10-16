"""Capture the complete real motorway/path candidate without choosing bridge heights."""

import argparse
import json
from pathlib import Path

import numpy as np
import shapely
from camera_height_inputs import digest
from capture_cycle_bridge import normalized
from curb_verify import actual
from cycle_bridge_inputs import BASE_REPORT as RAMP_REPORT_PIN
from shared_bridge_geometry import corridor
from tilted_rail_inputs import FIELD_PIN, GROUND_PIN
from wien_network_sources import INPUT_PIN

MOTORWAY = (
    "277582192:0",
    "1539647672:0",
    "1967640672:0",
    "277582492:0",
    "1536006832:0",
    "12820304712:0",
    "1967640942:0",
    "1536007142:0",
    "1967640962:0",
    "277582482:0",
    "277582342:0",
    "1980254822:0",
    "1980254812:0",
    "1539662812:0",
    "1539662792:0",
)
PATH = ("651921932:0", "447801010:0", "447801010:2")
PREFIX = "openfreemap:14/8939/5681:transportation:"
LEDGER_PIN = "01e2b7ff38d6b93c932cbfde7cf48a93a29b390c98bc0da8b354405436d851f2"


def selected_sources(raw, usage):
    lookup = {r["source_id"]: r for r in normalized(raw)}
    rows = []
    for profile, names in (("motorway", MOTORWAY), ("path", PATH)):
        for name in names:
            row = lookup[PREFIX + name]
            bridge = row["properties"].get("brunnel") == "bridge"
            expected = "not_used" if bridge else "used"
            if usage[row["source_id"]]["status"] != expected:
                raise ValueError("candidate source old decision changed")
            if bridge and row["properties"].get("layer") != (
                2 if profile == "motorway" else 1
            ):
                raise ValueError("candidate bridge order changed")
            rows.append(
                dict(row, profile=profile, role="deck" if bridge else "approach")
            )
    return rows, lookup


def nearby_sources(lookup, selected, domain, usage):
    taken = {r["source_id"] for r in selected}
    result = []
    for identity, row in lookup.items():
        if identity in taken or row["width_m"] is None:
            continue
        band = shapely.LineString(row["coordinates"]).buffer(
            row["width_m"] / 2, quad_segs=2, cap_style="round", join_style="bevel"
        )
        if band.intersection(domain).area <= 1e-8:
            continue
        result.append(dict(row, old_decision=usage[identity]))
    if len(result) > 128:
        raise ValueError("layered candidate context source budget")
    return result


def physical_traffic(network, selected):
    domain = shapely.union_all(
        [
            corridor({"xy": r["coordinates"], "width_m": r["width_m"]})
            for r in selected
            if r["profile"] == "motorway" and r["role"] == "deck"
        ]
    )
    with np.load(network / "ground.npz") as data:
        xyz, faces = data["vertices"], data["faces"][data["road"]]
    xy = xyz[faces, :2]
    lo, hi = np.array(domain.bounds).reshape(2, 2)
    hits = np.all(xy.max(axis=1) >= lo, axis=1) & np.all(xy.min(axis=1) <= hi, axis=1)
    if hits.sum() > 8192:
        raise ValueError("actual lower traffic preparation budget")
    pieces = shapely.intersection(shapely.polygons(xy[hits]), domain)
    traffic = shapely.union_all(pieces[shapely.area(pieces) > 1e-10])
    if shapely.get_num_coordinates(traffic) > 16384:
        raise ValueError("actual lower traffic footprint coordinate budget")
    return {
        "wkb_hex": traffic.wkb_hex,
        "candidate_faces": int(hits.sum()),
        "scope": "Actual old road-top footprint under explicit motorway decks, including conservative old approach caps. Lower chosen source profiles replace their former active height; no new lower traffic inferred from this mask.",
    }


def physical_borders(root, domain):
    if digest(root / "comparison.json") != RAMP_REPORT_PIN:
        raise ValueError("actual original border/ramp report required")
    report = json.loads((root / "comparison.json").read_text())
    records, shapes, heights = [], [], []
    for filename, key in (
        ("borders.npz", "borders_sha256"),
        ("ramps.npz", "ramps_sha256"),
    ):
        path = root / filename
        if digest(path) != report[key]:
            raise ValueError("original raised-body export changed")
        products, _data = actual(path)
        records.append({"file": filename, "sha256": report[key]})
        a, b = np.array(domain.bounds).reshape(2, 2)
        for product in products:
            xy = product.lower_contact.vertices[:, :2]
            if not (np.all(xy.max(axis=0) >= a) and np.all(xy.min(axis=0) <= b)):
                continue
            shape = shapely.union_all(
                shapely.polygons(xy[product.lower_contact.triangles])
            ).intersection(domain)
            if shape.area > 1e-8:
                shapes.append(shape)
                heights.append(
                    float(
                        np.max(
                            product.upper_contact.vertices[:, 2]
                            - product.lower_contact.vertices[:, 2]
                        )
                    )
                )
    return {
        "wkb_hex": shapely.union_all(shapes).wkb_hex,
        "owner_bodies": len(shapes),
        "maximum_height_m": max(heights, default=0),
        "inputs": records,
        "scope": "Exact old raised-body footprints clipped to this candidate. No heights or new reservation inferred.",
    }


def capture(args):
    if args.output.exists() or digest(args.capture) != INPUT_PIN:
        raise ValueError("fresh output and pinned complete original capture required")
    for name, pin in (("field.npz", FIELD_PIN), ("ground.npz", GROUND_PIN)):
        if digest(args.network / name) != pin:
            raise ValueError("pinned actual affine field/fine ground required")
    if digest(args.scene / "source-usage.json") != LEDGER_PIN:
        raise ValueError("latest complete source ledger required")
    raw = json.loads(args.capture.read_text())
    usage = {
        r["source_id"]: r
        for r in json.loads((args.scene / "source-usage.json").read_text())
    }
    selected, lookup = selected_sources(raw, usage)
    domains = [
        corridor({"xy": r["coordinates"], "width_m": r["width_m"]}) for r in selected
    ]
    domain = shapely.union_all(domains)
    context = nearby_sources(lookup, selected, domain, usage)
    with np.load(args.network / "field.npz") as data:
        vertices, faces = data["vertices"], data["faces"]
    hit = np.flatnonzero(
        shapely.intersects(shapely.polygons(vertices[faces, :2]), domain)
    )
    ids, inverse = np.unique(faces[hit], return_inverse=True)
    if len(hit) > 128 or len(ids) > 256:
        raise ValueError("layered candidate affine field budget")
    result = {
        "origin_lat_lon": raw["origin_lat_lon"],
        "planet": "20260927_080001_pt",
        "capture_sha256": INPUT_PIN,
        "field_sha256": FIELD_PIN,
        "ground_sha256": GROUND_PIN,
        "previous_source_ledger_sha256": LEDGER_PIN,
        "sources": selected,
        "context": context,
        "old_decisions": [usage[r["source_id"]] for r in selected],
        "field_vertices": vertices[ids].tolist(),
        "field_faces": inverse.reshape(-1, 3).tolist(),
        "original_field_face_ids": hit.tolist(),
        "actual_traffic": physical_traffic(args.network, selected),
        "actual_borders": physical_borders(args.borders, domain),
        "scope": "Actual candidate axes/attributes, neighbouring source context and old physical road/ground inputs only. No solved heights, logical turn invention, new source admission, runtime or consumer release.",
    }
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps(
            {
                "sha256": digest(args.output),
                "selected": len(selected),
                "context": len(context),
                "field_cells": len(hit),
            }
        ),
        flush=True,
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("capture", "network", "scene", "borders", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    capture(parser.parse_args())
