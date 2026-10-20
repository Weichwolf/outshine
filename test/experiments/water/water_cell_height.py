"""Existing physical fitter fed by routed, edge-aligned delivered cell products."""

import hashlib
import json

import numpy as np
import shapely
from compare_native_water_frame import course_controls
from outshine_ref.contracts import Mesh
from outshine_ref.geometry import SurfaceSampler
from outshine_ref.places.sources import tile_geography
from outshine_ref.water.frame import model_height_targets
from outshine_ref.water.geodetic import local_points
from river_membership import face_membership
from river_profiles import RiverProfile
from river_surface import surface_mesh
from water_cell_edges import edges


def project(points, zoom, anchor):
    latitude, longitude = tile_geography(points, 1, (zoom, 0, 0))
    return local_points(anchor, np.c_[longitude, latitude], 0)


def prepare(records, data, original_indices, *, spacing_m=None):
    """A bounded declared body/cohort, not a global hydraulic owner.

    Ring vertices already share published edge subdivisions. One ENU frame and
    soft source-height recipe serve contacts, height/bed consumers and render
    detail. The comparison driver, not this producer, may construct a fine fit.
    Native levels are unverified geodetic soft-model inputs, not measured ASL.
    """
    kinds = {r[1].info["kind"] for r in records}
    if len(kinds) != 1 or not kinds <= {"river", "lake"}:
        raise ValueError("one explicitly supported river/lake recipe required")
    tiles = sorted({r[0] for r in records})
    z, x, y = tiles[0]
    latitude, longitude = tile_geography([[0.5, 0.5]], 1, (z, x, y))
    anchor = np.array([longitude[0], latitude[0]])
    polygons = []
    for _, piece, _parent, _source in records:
        rings = [
            project(np.asarray(r.coords), z, anchor)[:, :2]
            for r in (piece.polygon.exterior, *piece.polygon.interiors)
        ]
        polygons.append(shapely.Polygon(rings[0], rings[1:]))
    flat, topology = surface_mesh(shapely.union_all(polygons), spacing_m)
    csr = face_membership(flat.vertices, flat.triangles, polygons)
    if np.any(csr.status):
        raise ValueError("normalized source-cell membership unresolved")
    kind = records[0][1].info["kind"]
    sampling = {"point_queries": len(flat.vertices), "course_reference_visits": 0}
    if kind == "river":
        selection = [
            i
            for i, course in enumerate(data["courses"])
            if data["inputs"][int(course["input"])]["kind"] == "river"
        ]
        courses, _ = course_controls(data, selection, anchor, np.ones(2))
        geographic = courses["course_original_lon_lat"]
        segments = local_points(anchor, geographic.reshape(-1, 2), 0).reshape(-1, 2, 3)
        profile = RiverProfile(segments[:, :, :2], courses["course_original_levels_m"])
        target, references = profile.sample(flat.vertices[:, :2], "blend-affine")
        sampling.update(
            course_reference_visits=int(references.sum()),
            declared_river_courses=len(selection),
            profile_segments=len(segments),
        )
        cap = 0.01
    else:
        indices = sorted({original_indices[r[2][:3] + r[2][-2:]] for r in records})
        target = np.full(
            len(flat.vertices), np.median(data["surfaces"][indices]["level_m"])
        )
        sampling["unique_original_surfaces"] = len(indices)
        cap = 0.005
    up = model_height_targets(anchor, flat.vertices[:, :2], target)
    mesh = Mesh(np.c_[flat.vertices[:, :2], up], flat.triangles)
    count = len(mesh.triangles)
    rows = np.r_[
        np.c_[np.arange(count), np.tile([1, 0, 0, -cap, cap], (count, 1))],
        np.c_[np.arange(count), np.tile([0, 1, 0, -cap, cap], (count, 1))],
    ]
    bounds = [float(up.min() - 20), float(up.max() + 20)]
    digest = hashlib.sha256()
    for tile, piece, parent, source in records:
        digest.update(
            json.dumps([tile, parent, source], sort_keys=True).encode()
            + piece.polygon.wkb
        )
    return (
        mesh,
        anchor,
        rows,
        bounds,
        csr,
        {
            "scope": "finite delivered normalized cohort with retained originals; not a world-wide body closure",
            "source_revision": digest.hexdigest(),
            "normalized_cells": tiles,
            "original_keys": sorted({r[2] for r in records}),
            "normalized_pieces": len(records),
            "source_height_sampling": sampling,
            "topology": topology,
            "laboratory_symmetric_grade_cap": cap,
            "model_height_semantics": "native numerical levels used as declared soft geodetic targets; no verified geoid/hydraulic datum",
        },
    )


def publish_contacts(
    mesh, anchor, version, records, knots, output, *, query_limit=4096
):
    """Publish the common functional owner at actual shared border vertices.

    Independent consumers import these same parent-facet contacts. This first
    product contains vertices, not a certified complete edge profile or fine-fit
    feasibility guarantee. Unknown/exterior points refuse, never snap to water.
    """
    if type(query_limit) is not int or not 1 <= query_limit <= 4096:
        raise ValueError("bounded shared border contact query budget required")
    consumers = {}
    for tile, piece, _parent, _source in records:
        coordinates = shapely.get_coordinates(piece.polygon)
        for key, axis, fixed, along in edges(tile):
            values = np.unique(coordinates[coordinates[:, axis] == fixed, along])
            for value in values:
                if value not in knots.get(key, ()):
                    raise ValueError("source border missing from published descriptor")
                xy = [0.0, 0.0]
                xy[axis], xy[along] = fixed, value
                consumers.setdefault(tuple(xy), set()).add(tile)
    selected = sorted(xy for xy, owners in consumers.items() if len(owners) > 1)
    if len(selected) > query_limit:
        raise ValueError("shared border contact query budget")
    xy = project(np.asarray(selected).reshape(-1, 2), records[0][0][0], anchor)[:, :2]
    sampler = SurfaceSampler(mesh, xy_tolerance_m=0, max_faces=16384)
    first = sampler.sample(xy, max_queries=query_limit)
    second = SurfaceSampler(mesh, xy_tolerance_m=0, max_faces=16384).sample(
        xy[::-1], max_queries=query_limit
    )
    if any(first.reasons) or any(second.reasons):
        return {
            "state": "unresolved_contacts",
            "queries": len(xy),
            "reasons": {r: first.reasons.count(r) for r in set(first.reasons) if r},
            "owner_version": version,
            "scope": "own-body border vertices unresolved; no partial contact product published",
        }
    np.testing.assert_array_equal(first.points_enu_m, second.points_enu_m[::-1])
    np.testing.assert_array_equal(first.face_indices, second.face_indices[::-1])
    context = hashlib.sha256(version.encode())
    for array in (anchor, mesh.vertices, mesh.triangles):
        context.update(np.ascontiguousarray(array).tobytes())
    payload = first.points_enu_m.nbytes + first.face_indices.nbytes
    np.savez_compressed(
        output,
        points=first.points_enu_m,
        parent_faces=first.face_indices,
        mercator_xy=np.asarray(selected),
        anchor=anchor,
        owner_version=np.asarray(version),
        owner_context=np.asarray(context.hexdigest()),
    )
    return {
        "state": "ready_vertex_contacts",
        "owner_version": version,
        "owner_context": context.hexdigest(),
        "shared_edge_vertices": len(xy),
        "distinct_parent_faces": len(np.unique(first.face_indices)),
        "independent_consumer_gap_m": 0.0,
        "points_and_parent_ids_bytes": payload,
        "height_queries_use_published_owner": True,
        "bed_consumer_integrated": False,
        "scope": "common vertex contacts before fine; complete edge profile and local fine-fit feasibility remain open",
    }
