"""Fixed actual Vienna graph/ground inputs and conservative affine-cell hardware gaps."""

import json

import numpy as np
import shapely
from camera_height_inputs import digest
from outshine_ref.contracts import Mesh
from rail_graph_inputs import rail_chains, read_window, safe_pieces

PREFIX = "results/ViennaNetwork/components/"
PINS = {
    "inputs/ViennaNetwork-input.json": "6d39b0165d18e8c3b466b38207d4291d564bcbd9a710628984bcacdb449f2a79",
    PREFIX
    + "graph.npz": "7b56c5f15046bcd9ebc37524a84f7ac713380165a183d03fc9a9275493db0785",
    PREFIX
    + "source-ids.json": "1af5997a16aed95cf38576dbe41344a27056133e1b1c3470c994ca4fc4a13231",
    PREFIX
    + "source-usage.json": "0a17287132cca6e9bdb18923e01b91ab041317802eae433c7010abb150262bbc",
}
FIELD_PIN = "20699c72258c3aae6e4a872d151241bc9d3cf8ce340a112e09e4847476206cac"
GROUND_PIN = "16cd7e8b774136dc50e70e9549d376630b4f7bb88e221f4b46527c30367627e5"


def inputs(root):
    report = json.loads((root / "comparison.json").read_text())
    if (
        report["dirty"]
        or digest(root / "ground.npz") != GROUND_PIN
        or digest(root / "field.npz") != FIELD_PIN
    ):
        raise ValueError("clean pinned actual ground and affine field required")
    # The domain is also independently checked against actual emitted contacts.
    pins = dict(PINS, **{PREFIX + "domain.wkb": digest(root / (PREFIX + "domain.wkb"))})
    source, raw, graphs, bed, bindings = read_window(root, "ViennaNetwork", files=pins)
    with np.load(root / "ground.npz") as d:
        ground = Mesh(d["vertices"], d["faces"])
        road = d["road"]
    with np.load(root / "field.npz") as d:
        field = Mesh(d["vertices"], d["faces"])
    chains, branches = [], []
    for graph in graphs:
        if graph["rail_graph"]:
            values, points = rail_chains(graph)
            chains.extend(values)
            branches.extend(points)
    for chain in chains:
        axis = shapely.LineString(chain["xy"])
        originals = shapely.union_all(
            [shapely.LineString(raw[s]["coordinates"]) for s in chain["sources"]]
        )
        if not originals.buffer(0.251).covers(axis):
            raise ValueError("saved noded rail graph differs from original sources")
    pieces, ledger, work = safe_pieces(
        chains,
        np.array(branches).reshape(-1, 2),
        [g for g in graphs if not g["rail_graph"]],
        1.35,
    )
    return (
        pieces,
        ledger,
        work,
        raw,
        ground,
        road,
        field,
        bed,
        dict(
            bindings,
            files=pins,
            ground_sha256=GROUND_PIN,
            field_sha256=FIELD_PIN,
            origin_lat_lon=source["origin_lat_lon"],
        ),
    )


def affine_pieces(pieces, field, *, reach=1.35):
    triangles = shapely.polygons(field.vertices[field.triangles, :2])
    tree = shapely.STRtree(triangles)
    output, ledger = [], []
    for index, piece in enumerate(pieces):
        line = shapely.LineString(piece["xy"])
        hits = tree.query(line, predicate="intersects")
        if len(hits) > 2048:
            raise ValueError("rail affine cell candidate budget")
        kept = []
        for face in sorted(hits):
            inside = triangles[face].buffer(-reach)
            if inside.is_empty:
                continue
            parts = shapely.get_parts(line.intersection(inside))
            for part in parts:
                if part.geom_type != "LineString" or part.length < 0.24:
                    continue
                if len(output) >= 2048:
                    raise ValueError("rail affine piece budget")
                output.append(
                    dict(
                        piece,
                        input_piece=index,
                        plane_face=int(face),
                        xy=shapely.get_coordinates(part),
                    )
                )
                kept.append(part)
        remaining = float(shapely.length(kept).sum())
        ledger.append(
            {
                "piece": index,
                "sources": piece["sources"],
                "original_length_m": line.length,
                "candidate_length_m": remaining,
                "excluded_length_m": max(0.0, line.length - remaining),
                "reason": "affine_crease_margin_or_short_span"
                if remaining < line.length - 1e-7
                else "",
            }
        )
    return output, ledger
