"""Compile coarse urban domains to shared categorical city fields before any mesh."""

import numpy as np
import shapely
from coarse_city_domains import domains
from urban_patches import UrbanPatches
from urban_recipe import (
    DEFAULT_FOOTPRINT,
    DEFAULT_PROJECTED_PITCH_M,
    HEIGHT_CUMULATIVE,
    HEIGHTS_M,
    VACANCY_PERCENT,
)


def hash_cells(x, y, salt):
    """Defined wrapping uint64 recipe; neither Python hash nor camera participates."""
    with np.errstate(over="ignore"):
        bits = x.astype(np.uint64) * np.uint64(0x9E3779B97F4A7C15)
        bits ^= y.astype(np.uint64) * np.uint64(0xD1B54A32D192ED03)
        bits ^= np.uint64(salt)
        bits = (bits ^ (bits >> 30)) * np.uint64(0xBF58476D1CE4E5B9)
        bits = (bits ^ (bits >> 27)) * np.uint64(0x94D049BB133111EB)
    return bits ^ (bits >> 31)


def memberships(chunks, side, pitch, data):
    if not len(chunks) or not data["urban"]:
        return np.zeros(len(chunks) + 1, np.uint32), np.empty(0, np.uint32)
    low = chunks * side * pitch
    high = low + side * pitch
    boxes = shapely.box(low[:, 0], low[:, 1], high[:, 0], high[:, 1])
    pairs = shapely.STRtree(data["urban"]).query(boxes, predicate="intersects")
    pairs = pairs[:, np.lexsort((pairs[1], pairs[0]))]
    counts = np.bincount(pairs[0], minlength=len(chunks))
    offsets = np.concatenate(([0], np.cumsum(counts))).astype(np.uint32)
    sources = np.array(data["urban_rows"], np.uint32)[pairs[1]]
    return offsets, sources


def covered_lots(data, ix, iy, pitch, footprint, side):
    vacancy = hash_cells(ix, iy, 1105) % 100 >= VACANCY_PERCENT
    blocks, inverse = np.unique(
        np.column_stack((ix // side, iy // side)), axis=0, return_inverse=True
    )
    low = blocks * side * pitch
    high = low + side * pitch
    margin = (
        64 * np.finfo(float).eps * np.maximum(np.maximum(np.abs(low), np.abs(high)), 1)
    )
    broad = shapely.box(
        low[:, 0] - margin[:, 0],
        low[:, 1] - margin[:, 1],
        high[:, 0] + margin[:, 0],
        high[:, 1] + margin[:, 1],
    )
    shapely.prepare(data["allowed"])
    safe = shapely.covers(data["allowed"], broad)
    whole = safe[inverse] & vacancy
    accepted_broad = int(whole.sum())
    tested = 0
    half = pitch * footprint / 2
    for begin in range(0, len(ix), 4096):
        ids = np.arange(begin, min(begin + 4096, len(ix)))
        ids = ids[vacancy[ids] & ~whole[ids]]
        centre = (np.column_stack((ix[ids], iy[ids])) + 0.5) * pitch
        boxes = shapely.box(
            centre[:, 0] - half,
            centre[:, 1] - half,
            centre[:, 0] + half,
            centre[:, 1] + half,
        )
        whole[ids] = shapely.covers(data["allowed"], boxes)
        tested += len(ids)
    return whole, {
        "patch_domain_tests": len(blocks),
        "footprint_geometries_tested": tested,
        "lots_accepted_without_footprint_geometry": accepted_broad,
    }


def bake(
    document,
    *,
    projected_pitch_m=DEFAULT_PROJECTED_PITCH_M,
    side=8,
    footprint=DEFAULT_FOOTPRINT,
    max_cells=1048576,
):
    if (
        not np.isfinite([projected_pitch_m, footprint]).all()
        or not 8 <= projected_pitch_m <= 512
    ):
        raise ValueError("bounded projected pitch required")
    if type(side) is not int or not 1 <= side <= 32 or not 0 < footprint < 1:
        raise ValueError("bounded chunk side and inferred footprint fraction required")
    if type(max_cells) is not int or not 1 <= max_cells <= 4194304:
        raise ValueError("explicit bounded cell work budget required")
    data = domains(document)
    left, bottom, right, top = data["tile_bounds"]
    # Unique ownership by centre, while buffered source geometry remains available.
    lower = np.ceil(np.array([left, bottom]) / projected_pitch_m - 0.5).astype(np.int64)
    upper = np.ceil(np.array([right, top]) / projected_pitch_m - 0.5).astype(np.int64)
    if data["allowed"].is_empty:
        upper = lower.copy()
    count = int(np.prod(upper - lower))
    if count > max_cells:
        raise ValueError("cell work budget exceeded before generating cell arrays")
    ix, iy = np.meshgrid(np.arange(lower[0], upper[0]), np.arange(lower[1], upper[1]))
    ix, iy = ix.ravel(), iy.ravel()
    whole, work = covered_lots(data, ix, iy, projected_pitch_m, footprint, side)
    xi, yi = ix[whole], iy[whole]
    chunk_ids, inverse = np.unique(
        np.column_stack((xi // side, yi // side)), axis=0, return_inverse=True
    )
    if len(chunk_ids) > 65536:
        raise ValueError("ready patch count budget exceeded")
    tiers = np.zeros((len(chunk_ids), side, side), np.uint8)
    random = (hash_cells(xi, yi, 708) >> 11).astype(float) * 2.0**-53
    tiers[inverse, yi % side, xi % side] = (
        np.searchsorted(HEIGHT_CUMULATIVE, random) + 1
    )
    frame = np.array([*data["origin_scale"], projected_pitch_m, footprint])
    heights = np.array(HEIGHTS_M, float)
    low = (chunk_ids * side * projected_pitch_m - frame[:2]) * frame[2]
    bounds = np.zeros((len(chunk_ids), 2, 3), np.float32)
    if len(chunk_ids):
        bounds[:, 0, :2] = low
        bounds[:, 1, :2] = low + side * projected_pitch_m * frame[2]
        bounds[:, 1, 2] = heights[tiers].max(axis=(1, 2))
        bounds[:, 0] = np.nextafter(bounds[:, 0], np.float32(-np.inf))
        bounds[:, 1] = np.nextafter(bounds[:, 1], np.float32(np.inf))
    offsets, sources = memberships(chunk_ids, side, projected_pitch_m, data)
    product = UrbanPatches(
        chunk_ids.astype(np.int32), tiers, heights, frame, bounds, offsets, sources
    )
    metadata = {
        "format_revision": "urban-patch-v1",
        "recipe_revision": "inferred-lattice-v1",
        "source": document["source"],
        "supporting_sources": [
            parent["source"] for parent in document.get("parents", [])
        ],
        "tile": document["tile"],
        "building_layer_present": document["building_layer_present"],
        "status": "inferred_partial_urban"
        if len(chunk_ids)
        else "urban_evidence_without_lots"
        if data["urban"]
        else "no_urban_evidence_not_world_empty",
        "source_records": data["records"],
        "urban_source_rows": data["urban_rows"],
        "urban_area_projected_m2": data["urban_area_projected_m2"],
        "allowed_area_projected_m2": data["allowed_area_projected_m2"],
        "owner_area_projected_m2": data["owner_area_projected_m2"],
        "unknown_urban_evidence_area_projected_m2": max(
            0.0, data["owner_area_projected_m2"] - data["urban_area_projected_m2"]
        ),
        "cells_considered": count,
        **work,
        "occupied_lots": len(xi),
        "patches": len(chunk_ids),
        "whole_lot_setback": "Boundary-crossing lots are not generated; urban evidence remains in provenance, not declared empty.",
        "height_density_origin": "Globally seeded procedural prior, not measured height/density. Flat metric control, no DEM/curvature/functional building contract.",
        "grid_metric": "Canonical Web-Mercator grid. Physical pitch varies with latitude; frame scale is the tile-centre cosine. No worldwide metric/neighbor/LOD admission.",
    }
    return product, metadata
