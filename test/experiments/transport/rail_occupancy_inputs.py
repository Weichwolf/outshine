"""Bound saved raised bodies as separate height volumes, leaving Ground unchanged."""

import json
from dataclasses import dataclass
from pathlib import Path

import numpy as np
import shapely
from camera_height_inputs import digest
from elevated_rail_audit import inspect
from elevated_rail_inputs import read_sources
from elevated_rail_study import old_arrays
from outshine_ref.contracts import Mesh
from rail_supports_context import DEFAULT_FIXTURE
from shared_bridge_geometry import corridor
from stair_local_assets import road_asset
from stair_local_replay import load_chain
from stair_occupancy_candidate_sources import (
    bound_support_sources,
    from_state,
    road_source,
)
from stair_occupancy_candidates import CandidateVolumes
from wien_support_verify import unpack


@dataclass(frozen=True)
class RailProducts:
    state: tuple
    originals: dict
    rows: list
    deck: Mesh
    top_count: int
    top_faces: np.ndarray
    supports: list
    contacts: list
    owners: dict
    identity: str
    support_identity: str


def load(products, fixture=DEFAULT_FIXTURE):
    products = Path(products)
    report = json.loads((products / "report.json").read_text())
    proof = json.loads((products / "verification.json").read_text())
    selected, methods = report["selected"], report["methods"]
    if (
        proof["report_sha256"] != digest(products / "report.json")
        or proof["selected"] != selected
        or not proof["valid_whole_support_candidate"]
        or not proof["valid_retained_deck"]
        or proof["methods"][selected]["accepted"] != methods[selected]["accepted"]
        or report["admitted_source_ids"]
        or digest(fixture) != report["fixture_sha256"]
    ):
        raise ValueError("Exact independently read raised body candidate required")
    archive = products / (selected + ".npz")
    if digest(archive) != methods[selected]["mesh_sha256"]:
        raise ValueError("Whole saved raised supports changed")
    supports, contacts, _arrays = unpack(archive)
    owners = {
        r["product"]: r["source_id"]
        for r in methods[selected]["stations"]
        if "product" in r
    }
    deck_root = Path(report["deck"])
    if digest(deck_root / "report.json") != report["deck_report_sha256"]:
        raise ValueError("Whole preceding elevated deck binding changed")
    deck_report = json.loads((deck_root / "report.json").read_text())
    archive = deck_root / "candidates.npz"
    if (
        deck_report["selected_method"] != "flat"
        or not deck_report["methods"]["flat"]["accepted"]
        or digest(archive) != report["deck_candidates_sha256"]
        or deck_report["candidates_sha256"] != report["deck_candidates_sha256"]
        or deck_report["fixture_sha256"] != report["fixture_sha256"]
    ):
        raise ValueError("Same qualified flat elevated deck required")
    state = load_chain(
        Path(deck_report["resident"]),
        [Path(p["path"]) for p in deck_report["prior_modules"]],
    )
    if (
        state[4]["source_ids"] != report["old_source_ids"]
        or old_arrays(state[1]) != report["old_road_arrays"]
        or deck_report["old_road_arrays"] != report["old_road_arrays"]
        or state[4]["origin_lat_lon"] != report["origin_lat_lon"]
    ):
        raise ValueError("Whole unchanged preceding source/frame/body state required")
    recipe, context, originals, rows = read_sources(fixture, state[4]["source_ids"])
    keys = [r["source_id"] for r in rows]
    if (
        keys != report["rail_candidate_source_ids"]
        or keys != deck_report["source_ids"]
        or context["origin_lat_lon"] != report["origin_lat_lon"]
        or [r["original_properties"] for r in rows]
        != deck_report["original_properties"]
        or not set(owners.values()) <= set(keys)
        or len(supports) != methods[selected]["accepted"]
    ):
        raise ValueError("Original elevated source/role/support ownership changed")
    with np.load(archive, allow_pickle=False) as saved:
        deck = Mesh(saved["flat_vertices"], saved["flat_triangles"])
    n, m = deck_report["whole_layout"]["vertices"], deck_report["whole_layout"]["faces"]
    inspect(
        deck,
        deck.triangles[:m],
        rows,
        [],
        recipe["deck_thickness_m"],
        recipe["along_grade"],
        recipe["cross_grade"],
    )
    return RailProducts(
        state,
        originals,
        rows,
        deck,
        n,
        deck.triangles[:m],
        supports,
        contacts,
        owners,
        report["deck_candidates_sha256"],
        methods[selected]["mesh_sha256"],
    )


def deck_asset(products, *, cast=False):
    vertices = products.deck.vertices
    if cast:
        vertices = vertices.astype("f4").astype(float)
    # Ownership belongs to the original facet, preserved through quantization.
    pieces = shapely.polygons(products.deck.vertices[products.top_faces, :2])
    domains = [corridor(row) for row in products.rows]
    # A facet crossing a source seam conservatively retains all intersected owners.
    owners = np.array(
        [shapely.area(shapely.intersection(pieces, shape)) > 0 for shape in domains]
    ).T
    covered = shapely.union_all(domains)
    if not owners.any(axis=1).all() or np.any(
        shapely.area(shapely.difference(pieces, covered)) > 1e-6
    ):
        raise ValueError("Whole raised deck source-domain ownership required")
    return road_asset(
        products.identity + (":f32" if cast else ":f64"),
        [r["source_id"] for r in products.rows],
        products.rows,
        {
            "body_vertices": vertices,
            "body_faces": products.deck.triangles,
            "top_faces": products.top_faces,
            "bottom_faces": products.top_faces + products.top_count,
            "owners": owners,
            "ground": np.zeros(len(products.top_faces), bool),
        },
    )


def supplements(products, *, cast=False):
    def mesh(value):
        return Mesh(value.vertices.astype("f4").astype(float), value.triangles)

    supports, contacts = products.supports, products.contacts
    if cast:
        supports = [mesh(body) for body in supports]
        contacts = [(mesh(low), mesh(high)) for low, high in contacts]
    return (
        road_source(deck_asset(products, cast=cast), products.originals),
        *bound_support_sources(
            supports,
            contacts,
            products.owners,
            products.support_identity + (":f32" if cast else ":f64"),
        ),
    )


def catalog(products):
    old = from_state(products.state, products.originals)
    return CandidateVolumes(old.frame, (*old.sources, *supplements(products)))
