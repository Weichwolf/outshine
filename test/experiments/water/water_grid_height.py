"""Regular physical-height cells independent of the exact water footprint mesh."""

import math
from collections import defaultdict
from dataclasses import dataclass

import numpy as np
import shapely
from compare_native_water_frame import course_controls
from native_water_observation import NO_INPUT
from outshine_ref.contracts import Mesh
from outshine_ref.geometry import SurfaceSampler, SurfaceSamples
from outshine_ref.water.frame import model_height_targets
from outshine_ref.water.geodetic import local_points
from river_profiles import RiverProfile
from water_cell_height import body_geometry


@dataclass(frozen=True)
class GridInput:
    mesh: Mesh
    mask: object
    anchor: np.ndarray
    rows: np.ndarray
    bounds: tuple
    source_polygons: tuple
    report: dict


def sparse_grid(polygons, spacing_m, *, candidate_limit=32768):
    """Keep full regular triangles; water remains a separate precise mask."""
    if (
        type(spacing_m) not in (int, float)
        or not math.isfinite(spacing_m)
        or spacing_m <= 0
        or type(candidate_limit) is not int
        or not 1 <= candidate_limit <= 32768
    ):
        raise ValueError("finite spacing and bounded grid candidates required")
    candidates, probes = set(), 0
    for polygon in polygons:
        a, b, c, d = polygon.bounds
        low = (math.floor(a / spacing_m), math.floor(b / spacing_m))
        high = (math.ceil(c / spacing_m), math.ceil(d / spacing_m))
        probes += (high[0] - low[0]) * (high[1] - low[1])
        if probes > candidate_limit:
            raise ValueError("source-bounds grid work budget exhausted")
        candidates.update(
            (x, y) for x in range(low[0], high[0]) for y in range(low[1], high[1])
        )
    triangles = []
    for x, y in sorted(candidates):
        a, b, c, d = (x, y), (x + 1, y), (x + 1, y + 1), (x, y + 1)
        triangles.extend(((a, b, c), (a, c, d)))
    coordinates = np.asarray(triangles, dtype=float) * spacing_m
    mask = shapely.union_all(polygons)
    if not len(coordinates) or shapely.get_num_coordinates(mask) > 4096:
        raise ValueError("bounded nonempty precise water mask required")
    active = shapely.area(shapely.intersection(shapely.polygons(coordinates), mask)) > 0
    retained = coordinates[active]
    xy, indices = np.unique(retained.reshape(-1, 2), axis=0, return_inverse=True)
    if not len(xy) or len(xy) > 8192 or len(retained) > 16384:
        raise ValueError("retained height grid budget exhausted")
    return (
        Mesh(np.c_[xy, np.zeros(len(xy))], indices.reshape(-1, 3)),
        mask,
        {
            "source_bbox_grid_cell_visits": probes,
            "unique_candidate_grid_cells": len(candidates),
            "candidate_triangles": len(coordinates),
            "retained_triangles": len(retained),
            "selection_area_m2": float(mask.area),
            "selection_coordinates": int(shapely.get_num_coordinates(mask)),
            "no_water_contour_mesh_constructed": True,
        },
    )


def body_profile(data, mask, anchor):
    selected, omitted = [], defaultdict(list)
    for index, course in enumerate(data["courses"]):
        if int(course["input"]) == NO_INPUT:
            omitted["missing_provenance"].append(index)
            continue
        info = data["inputs"][int(course["input"])]
        if info.get("layer") != "water_lines":
            omitted["wrong_course_layer"].append(index)
            continue
        if info["kind"] != "river":
            omitted["other_course_class"].append(index)
            continue
        if (
            info.get("bridge") is not False
            or info.get("tunnel") is not False
            or info.get("crossing")
        ):
            omitted["crossing_not_ground"].append(index)
            continue
        point = data["points_lat_lon"][
            course["first_point"] : course["first_point"] + course["point_count"], ::-1
        ]
        local = local_points(anchor, point, 0)[:, :2]
        if len(local) < 2 or np.all(local == local[0]):
            omitted["zero_length_course"].append(index)
            continue
        if shapely.LineString(local).intersection(mask).length <= 0:
            omitted["outside_water_body"].append(index)
            continue
        selected.append(index)
    if not selected:
        raise ValueError("no admitted source river profile")
    courses, report = course_controls(data, selected, anchor, np.ones(2))
    segments = local_points(
        anchor, courses["course_original_lon_lat"].reshape(-1, 2), 0
    ).reshape(-1, 2, 3)
    valid = np.any(segments[:, 0, :2] != segments[:, 1, :2], axis=1)
    profile = RiverProfile(
        segments[valid, :, :2], courses["course_original_levels_m"][valid]
    )
    placed = local_points(
        anchor,
        courses["course_original_lon_lat"].reshape(-1, 2),
        courses["course_original_levels_m"].ravel(),
    )
    return profile, {
        "selected_original_courses": selected,
        "other_courses_by_reason": {
            reason: len(ids) for reason, ids in omitted.items()
        },
        "omitted_original_courses_by_reason": dict(omitted),
        "omitted_zero_length_segment_addresses": courses["course_original_address"][
            ~valid
        ].tolist(),
        "profile": report,
        "source_endpoint_up_bounds_m": [
            float(placed[:, 2].min() - 20),
            float(placed[:, 2].max() + 20),
        ],
    }


def normal_extension(profile, xy):
    """Evaluate the existing profile at nearest source positions, including dry nodes.

    Equal nearest segments choose the lowest original index. Fixed-size query
    batches bound the temporary tie array; the original64-reference blend limit
    is unchanged. This defines soft targets, never hydrology or hard contacts.
    """
    xy = np.asarray(xy, dtype=float)
    if (
        xy.ndim != 2
        or xy.shape[1] != 2
        or not 0 < len(xy) <= 8192
        or not np.isfinite(xy).all()
    ):
        raise ValueError("finite bounded grid target points required")
    nearest = np.full(len(xy), len(profile.segments), dtype=int)
    visits = 0
    for first in range(0, len(xy), 32):
        pairs = profile.tree.query_nearest(shapely.points(xy[first : first + 32]))
        visits += len(pairs[1])
        if visits > 262144:
            raise ValueError("source-nearest tie work budget exhausted")
        np.minimum.at(nearest[first : first + 32], pairs[0], pairs[1])
    starts, directions = profile.segments[nearest, 0], profile.directions[nearest]
    along = np.clip(
        np.sum((xy - starts) * directions, axis=1) / profile.lengths_squared[nearest],
        0,
        1,
    )
    projected = starts + along[:, None] * directions
    target, references = profile.sample(projected, "blend-affine")
    if not np.isfinite(target).all():
        raise ValueError("source-profile targets unresolved within unchanged work caps")
    return target, {
        "grid_target_queries": len(xy),
        "nearest_tie_candidates": visits,
        "profile_reference_visits": int(references.sum()),
        "maximum_profile_references": int(references.max()),
    }


def prepare_grid(records, data, spacing_m, *, support_spacing_m=1024):
    if {r[1].info["kind"] for r in records} != {"river"}:
        raise ValueError("explicit river soft-target recipe required")
    records = sorted(records, key=lambda r: (r[0], r[1].feature, r[1].part))
    anchor, polygons = body_geometry(records)
    ratio = support_spacing_m / spacing_m if spacing_m > 0 else np.nan
    if (
        not np.isfinite(ratio)
        or ratio < 1
        or ratio != int(ratio)
        or int(ratio) & (int(ratio) - 1)
    ):
        raise ValueError("grid spacing must dyadically subdivide the fixed support")
    base, mask, base_topology = sparse_grid(polygons, support_spacing_m)
    support = shapely.union_all(shapely.polygons(base.vertices[base.triangles, :2]))
    if ratio == 1:
        flat, topology = base, dict(base_topology)
    else:
        flat, returned, topology = sparse_grid(
            list(shapely.get_parts(support)), spacing_m
        )
        if not returned.equals(support):
            raise ValueError("physical support changed during refinement")
    profile, sources = body_profile(data, mask, anchor)
    target, sampling = normal_extension(profile, flat.vertices[:, :2])
    up = model_height_targets(anchor, flat.vertices[:, :2], target)
    mesh = Mesh(np.c_[flat.vertices[:, :2], up], flat.triangles)
    count = len(mesh.triangles)
    rows = np.r_[
        np.c_[np.arange(count), np.tile([1, 0, 0, -0.01, 0.01], (count, 1))],
        np.c_[np.arange(count), np.tile([0, 1, 0, -0.01, 0.01], (count, 1))],
    ]
    return GridInput(
        mesh,
        mask,
        anchor,
        rows,
        tuple(sources["source_endpoint_up_bounds_m"]),
        tuple(polygons),
        {
            "spacing_m": spacing_m,
            "support_spacing_m": support_spacing_m,
            "physical_support_area_m2": float(support.area),
            "objective_domain": "same fixed coarse-cover triangles including dry halo; source mask remains separate",
            "base_support_topology": base_topology,
            "topology": topology,
            "sources": sources,
            "sampling": sampling,
            "original_keys": sorted({r[2] for r in records}),
            "model": "source-normal extension; native levels are soft numerical geodetic targets, not verified ASL/flow",
            "consumer_contract": "grid triangles are a physical height domain; render/height/bed consumers must additionally obey the precise water mask",
        },
    )


def sample_water(mesh, mask, xy, *, query_limit=4096):
    """The physical grid is never interpreted as water outside its source mask."""
    xy = np.asarray(xy, dtype=float)
    if (
        xy.ndim != 2
        or xy.shape[1] != 2
        or not np.isfinite(xy).all()
        or type(query_limit) is not int
        or not 1 <= query_limit <= 4096
        or len(xy) > query_limit
    ):
        raise ValueError("finite bounded water height queries required")
    eligible = np.flatnonzero(shapely.covers(mask, shapely.points(xy)))
    points = np.full((len(xy), 3), np.nan)
    normals = np.full((len(xy), 3), np.nan)
    faces = np.full(len(xy), -1, dtype=int)
    reasons = ["outside_water_mask"] * len(xy)
    if len(eligible):
        sampled = SurfaceSampler(mesh, xy_tolerance_m=0, max_faces=16384).sample(
            xy[eligible], max_queries=query_limit
        )
        points[eligible], normals[eligible], faces[eligible] = (
            sampled.points_enu_m,
            sampled.normals_enu,
            sampled.face_indices,
        )
        for local, original in enumerate(eligible):
            reasons[original] = sampled.reasons[local]
    return SurfaceSamples(
        points,
        normals,
        faces,
        tuple(reasons),
        {
            "queries": len(xy),
            "water_queries": len(eligible),
            "mask_rejections": len(xy) - len(eligible),
        },
    )
