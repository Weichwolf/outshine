"""Conservative complete outer envelopes of actual emitted railing pieces."""

from itertools import combinations, pairwise

import numpy as np
import shapely
from indoor_mesh_checks import closed_mesh
from indoor_rail_instances import materialize


def band_projection(vertices, low, high):
    if vertices[:, 2].max() <= low or vertices[:, 2].min() >= high:
        return shapely.GeometryCollection()
    points = [v[:2] for v in vertices if low <= v[2] <= high]
    # All edges of this small convex hull are included. Extra diagonal samples
    # remain inside it; projecting their hull cannot enlarge the clipped solid.
    for a, b in combinations(vertices, 2):
        if a[2] == b[2]:
            continue
        for plane in (low, high):
            t = (plane - a[2]) / (b[2] - a[2])
            if 0 < t < 1:
                points.append((a + t * (b - a))[:2])
    return (
        shapely.MultiPoint(points).convex_hull
        if points
        else shapely.GeometryCollection()
    )


def sweep_envelopes(record):
    rings = record["mesh"]["vertices"].reshape(-1, 8, 3)[:, :4]
    pairs = list(pairwise(range(len(rings))))
    if np.array_equal(record["world_path"][0], record["world_path"][-1]):
        pairs.append((len(rings) - 1, 0))
    return [np.vstack((rings[a], rings[b])) for a, b in pairs]


class RailAudit:
    def __init__(self, reservations, *, max_tests=65536):
        self.reservations = tuple(reservations)
        self.tree = shapely.STRtree([r.geometry for r in reservations])
        self.tests = 0
        self.max_tests = max_tests

    def clear(self, vertices):
        footprint = shapely.MultiPoint(vertices[:, :2]).convex_hull
        for at in self.tree.query(footprint, predicate="intersects"):
            self.tests += 1
            if self.tests > self.max_tests:
                raise ValueError("rail_actual_envelope_budget")
            r = self.reservations[int(at)]
            overlap = band_projection(vertices, r.heights.lower_m, r.heights.upper_m)
            if (
                not overlap.is_empty
                and overlap.intersects(r.geometry)
                and not overlap.touches(r.geometry)
            ):
                raise ValueError(f"rail_material_blocks_traffic:{r.owner_id}")


def source_bands(floors, source_ids):
    programme = next(
        p for p in floors["programmes"] if tuple(p["source_ids"]) == source_ids
    )
    parts = [p for p in floors["adjusted_volumes"] if p.identifier in source_ids]
    base = programme["base_z_m"]
    heights = sorted({base + h for p in parts for h in (p.lower_m, p.upper_m)})
    return [
        (
            low,
            high,
            shapely.union_all(
                [
                    p.polygon
                    for p in parts
                    if base + p.lower_m <= low and base + p.upper_m >= high
                ]
            ),
        )
        for low, high in pairwise(heights)
    ]


def covered(vertices, bands):
    if vertices[:, 2].min() < bands[0][0] or vertices[:, 2].max() > bands[-1][1]:
        raise ValueError("rail_material_outside_source_height")
    for low, high, polygon in bands:
        section = band_projection(vertices, low, high)
        if not section.is_empty and not polygon.covers(section):
            raise ValueError("rail_material_outside_source_footprint")


def qualify_rails(result, batch, floors):
    if result["frame"] != batch["frame"] or any(
        f.frame != result["frame"] for f in floors["floors"]
    ):
        raise ValueError("rail_qualification_frame_mismatch")
    audit = RailAudit((*floors["reservations"], *batch["reservations"]))
    chosen = {f.identifier: f for f in floors["floors"]}
    owner_sources = {
        s["source_id"]: chosen[s["end_profiles"][0]["floor_id"]].source_ids
        for s in batch["stairs"]
    }
    owner_sources.update({f.identifier: f.source_ids for f in batch["floors"]})
    bands = {ids: source_bands(floors, ids) for ids in set(owner_sources.values())}
    envelopes, meshes, triangles = 0, 0, 0
    for record in (*result["handrails"], *result["guards"]):
        mesh = record["mesh"]
        closed_mesh(mesh)
        meshes += 1
        triangles += len(mesh["faces"])
        owner = record.get("source_id", record.get("floor_id"))
        for envelope in sweep_envelopes(record):
            audit.clear(envelope)
            covered(envelope, bands[owner_sources[owner]])
            envelopes += 1
    for support in result["supports"]:
        mesh = support.get("mesh")
        if mesh is None:
            mesh = materialize(
                result["templates"][support["kind"]], support["transform"]
            )
        closed_mesh(mesh)
        audit.clear(mesh["vertices"])
        covered(mesh["vertices"], bands[owner_sources[support["source_id"]]])
        envelopes += 1
        meshes += 1
        triangles += len(mesh["faces"])
    return {
        "closed_meshes": meshes,
        "actual_triangles": triangles,
        "outer_envelopes": envelopes,
        "traffic_candidates": audit.tests,
        "source_components": len(bands),
        "scope": "Complete filled convex outer envelopes of actual sweep segments and support bodies, clipped at every traffic/source height band; conservative for the hollow material. Supplied common-frame source/traffic context only; not statics, standards, rooms or World.",
    }
