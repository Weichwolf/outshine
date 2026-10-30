"""Whole original elevated rail inputs and unchanged held lower-world constraints."""

import json

import numpy as np
import shapely
from camera_height_inputs import digest
from elevated_rail_joints import apply as apply_joints
from elevated_rail_plane import lower_constraints
from outshine_ref.contracts import Mesh
from shared_bridge_geometry import layout
from stair_local_context import radius
from stair_module_inputs import FIXTURE, reference_profiles, source_context


def read_sources(recipe_path, cached_ids):
    recipe = json.loads(recipe_path.read_text())
    reference = FIXTURE.with_name(recipe["reference_fixture"])
    if (
        recipe["version"] != 1
        or digest(reference) != recipe["reference_fixture_sha256"]
    ):
        raise ValueError("Original elevated-source reference binding required")
    keys = recipe["source_ids"]
    if (
        not isinstance(keys, list)
        or not 1 <= len(keys) <= 8
        or len(set(keys)) != len(keys)
        or set(keys) & set(cached_ids)
    ):
        raise ValueError("Bounded unique whole new rail sources required")
    parameters = np.array(
        [
            recipe[k]
            for k in (
                "deck_thickness_m",
                "construction_reserve_m",
                "along_grade",
                "cross_grade",
            )
        ],
        float,
    )
    if (
        not np.isfinite(parameters).all()
        or np.any(parameters <= 0)
        or parameters[0] > 2
        or parameters[1] > 0.05
        or np.any(parameters[2:] > 0.06)
    ):
        raise ValueError("Bounded explicit procedural rail profile required")
    context, available, _sample, _layer = source_context(reference)
    if not set(keys) <= set(available):
        raise ValueError("Whole original rail source missing")
    profiles = reference_profiles()
    rows = []
    for key in keys:
        source = available[key]
        props = source["properties"]
        if (
            props.get("subclass") not in ("rail", "subway")
            or props.get("brunnel") != "bridge"
            or props.get("indoor")
            or type(props.get("layer")) is not int
            or not 1 <= props["layer"] <= 8
        ):
            raise ValueError("Explicit original elevated rail role required")
        profile = profiles[props["subclass"]]
        rows.append(
            {
                "source_id": key,
                "xy": np.array(source["xy_m"]),
                "width_m": profile["widthM"],
                "end_heights_m": [0, 0],
                "source_grid_m": 2**-11,
                "original_properties": props,
                "original_mvt_xy": source["mvt_xy"],
            }
        )
    apply_joints(rows)
    return recipe, context, available, rows


def sources(recipe_path, cached_ids):
    recipe, context, available, rows = read_sources(recipe_path, cached_ids)
    surface, domain, work = layout(rows)
    return recipe, context, available, rows, surface, domain, work


def add_surface(
    points, groups, notes, mesh, domain, required, clearance, name, kind, capacity
):
    groups.append((name, mesh, float(clearance)))
    for cast in (False, True):
        actual = (
            Mesh(mesh.vertices.astype("f4").astype(float), mesh.triangles)
            if cast
            else mesh
        )
        remaining = capacity - sum(len(p) for p in points)
        if remaining <= 0:
            raise ValueError("Whole lower-world point capacity exhausted")
        value, work = lower_constraints(
            actual, domain, required + clearance, points_most=remaining
        )
        points.append(value)
        notes.append(
            {
                "kind": kind,
                "lower": name,
                "float32": cast,
                "clearance_m": float(clearance),
                "cast_vertex_payload_bytes": mesh.vertices.size * 12 if cast else 0,
                **work,
            }
        )


def lower_world(view, assets, piers, available, domain, required, *, points_most=65536):
    if (
        type(points_most) is not int
        or not 1 <= points_most <= 65536
        or len(piers) > 128
    ):
        raise ValueError(
            "Bounded lower-world constraints and old support count required"
        )
    terrain = view.nearby(domain, radius(view), max_faces=8192)
    if terrain.mesh is None:
        raise ValueError("Held whole terrain query failed: " + terrain.reason)
    for cast in (False, True):
        xy = (
            terrain.mesh.vertices[:, :2].astype("f4").astype(float)
            if cast
            else terrain.mesh.vertices[:, :2]
        )
        shape = shapely.union_all(shapely.polygons(xy[terrain.mesh.triangles]))
        if domain.difference(shape).area > 1e-7:
            raise ValueError("Whole raised rail footprint lacks held terrain")
    points, groups, notes = [], [], []
    add_surface(
        points,
        groups,
        notes,
        terrain.mesh,
        domain,
        required,
        2.5,
        "terrain",
        "terrain",
        points_most,
    )
    profiles = reference_profiles()
    chosen = [
        (
            asset,
            asset.tree.query(domain, predicate="dwithin", distance=asset.xy_rounding_m),
        )
        for asset in assets
    ]
    chosen = [(asset, hits) for asset, hits in chosen if len(hits)]
    if (
        len(chosen) > 16
        or sum(len(a.source_ids) for a, _ in chosen) > 64
        or sum(len(a.data["body_faces"]) for a, _ in chosen) > 32768
    ):
        raise ValueError("Held local rail context asset/source/body capacity exhausted")
    notes.append(
        {
            "kind": "held_working_set",
            "asset_metadata_queries": len(assets),
            "selected_assets": len(chosen),
            "selected_source_ids": sum(len(a.source_ids) for a, _ in chosen),
            "selected_body_faces": sum(len(a.data["body_faces"]) for a, _ in chosen),
            "old_support_bounds_tested": len(piers),
        }
    )
    for asset, hits in chosen:
        owners = asset.data["owners"][hits]
        clearances = []
        for key in asset.source_ids:
            props = available[key]["properties"]
            kind = props.get("subclass")
            if not kind and props.get("class") == "minor":
                kind = "unclassified"
            if kind not in profiles or "clearanceM" not in profiles[kind]:
                raise ValueError("Actual old traffic profile unresolved: " + key)
            clearances.append(profiles[kind]["clearanceM"])
        per_face = np.max(np.where(owners, clearances, 0), axis=1)
        for clearance in np.unique(per_face):
            mesh = Mesh(
                asset.data["body_vertices"],
                asset.data["top_faces"][hits[per_face == clearance]],
            )
            add_surface(
                points,
                groups,
                notes,
                mesh,
                domain,
                required,
                clearance,
                asset.identity,
                "traffic",
                points_most,
            )
    for index, pier in enumerate(piers):
        both = np.vstack((pier.vertices, pier.vertices.astype("f4").astype(float)))
        shape = shapely.convex_hull(shapely.multipoints(both[:, :2]))
        if domain.intersection(shape).area <= 0:
            continue
        xy = shapely.get_coordinates(shape)[:-1]
        vertices = np.c_[xy, np.full(len(xy), both[:, 2].max())]
        faces = np.column_stack(
            (
                np.zeros(len(xy) - 2, int),
                np.arange(1, len(xy) - 1),
                np.arange(2, len(xy)),
            )
        )
        mesh = Mesh(vertices, faces)
        add_surface(
            points,
            groups,
            notes,
            mesh,
            domain,
            required,
            0.2,
            f"old-support-{index}-bounds",
            "old_pier_conservative_bounds",
            points_most,
        )
    return np.vstack(points), groups, notes
