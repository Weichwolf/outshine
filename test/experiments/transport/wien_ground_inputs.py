"""Pinned Vienna bridge/portal input and a complete tile soil domain with open portals."""

import json

import numpy as np
import shapely
from camera_height_inputs import digest, lumped_weights
from capture_bridge_continuations import FIXTURE as BRIDGE_FIXTURE
from capture_bridge_continuations import PIN as BRIDGE_PIN
from capture_height_targets import sample_targets
from capture_tunnel_portals import FIXTURE, PIN, base_surfaces
from ground_affine_bounds import joined, overlap_bounds, vertex_caps
from ground_surface import ground_layout
from retained_surface import surface_domain
from transport_place_inputs import GIRTH_M, mercator
from tunnel_portal_recipe import build


def tile_domain(fixture, ground):
    if digest(BRIDGE_FIXTURE) != BRIDGE_PIN:
        raise ValueError("pinned whole-tile source fixture changed")
    raw = json.loads(BRIDGE_FIXTURE.read_text())
    regions = []
    origin = fixture["origin_lat_lon"]
    for entry in raw["inputs"]:
        level, x, y = entry["receipt"]["tile"]
        span = GIRTH_M / (1 << level)
        corners = np.array(
            [
                [tx * span - GIRTH_M / 2, GIRTH_M / 2 - ty * span]
                for tx, ty in ((x, y + 1), (x + 1, y + 1), (x + 1, y), (x, y))
            ]
        )
        regions.append(
            shapely.Polygon(
                (corners - mercator(*origin)) * np.cos(np.radians(origin[0]))
            )
        )
    return shapely.union_all(
        [shapely.union_all(regions), surface_domain(ground).buffer(4)]
    )


def prepare(reference, dem_sources):
    if digest(FIXTURE) != PIN:
        raise ValueError("pinned portal fixture changed")
    fixture = json.loads(FIXTURE.read_text())
    previous, origin = base_surfaces(
        reference / "bridge-continuations/Vienna-continuation-whole-rebuild-solid.npz"
    )
    if origin != fixture["base"]:
        raise ValueError("previous chosen bridge/portal input changed")
    model, note = build(fixture, previous)
    if not note["valid"]:
        raise ValueError(str(note))
    domain = tile_domain(fixture, previous[1]).difference(model["open_trench"])
    layout, partition = ground_layout(
        previous[1].vertices[:, :2], previous[1].triangles, domain, max_faces=8192
    )
    xy, faces = layout["xy"], layout["faces"]
    targets, receipts = sample_targets(
        fixture["origin_lat_lon"], xy, dem_sources, 11, fetch=False
    )
    frames = np.broadcast_to(np.eye(2) / (2 / 3), (len(faces), 2, 2)).copy()
    frames[: layout["road_faces"]] = np.eye(2) / 0.025
    road_caps = vertex_caps(layout["road_vertex_map"], previous[1].vertices[:, 2])
    upper, upper_note = overlap_bounds(xy, faces, previous[0], offset_m=-0.37)
    roof, roof_note = overlap_bounds(xy, faces, model["roof"], offset_m=2.25, sign=-1)
    bounds = joined(road_caps, upper, roof)
    arrays = {
        "xy": xy,
        "faces": faces,
        "targets": targets,
        "frames": frames,
        "weights": lumped_weights(xy, faces),
        "road_count": layout["road_faces"],
        "old_vertex_map": layout["road_vertex_map"],
        "old_heights": previous[1].vertices[:, 2],
        "bound_columns": bounds[0],
        "bound_coefficients": bounds[1],
        "bound_limits": bounds[2],
    }
    meta = {
        "fixture_sha256": PIN,
        "bridge_fixture_sha256": BRIDGE_PIN,
        "previous": origin,
        "dem_receipts": receipts,
        "partition": partition,
        "upper_bounds": upper_note,
        "roof_bounds": roof_note,
        "old_recipe": note,
        "domain_geojson": shapely.to_geojson(domain),
    }
    return arrays, meta, fixture, previous, model
