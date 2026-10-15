"""Independent whole actual bed, metric section and volume replay on tilted rails."""

import numpy as np
import shapely
from outshine_ref.transport.rail_hardware import bake_rail_sleepers
from rail_hardware_checks import closed_volume


class GroundContact:
    def __init__(self, mesh, road):
        self.mesh, self.road = mesh, road
        self.shapes = shapely.polygons(mesh.vertices[mesh.triangles, :2])
        self.tree = shapely.STRtree(self.shapes)

    def verify(self, footprint, triangle, *, max_faces=8192):
        hits = self.tree.query(footprint, predicate="intersects")
        if not 0 < len(hits) <= max_faces:
            raise ValueError("tilted rail actual contact face budget or absence")
        intersections = shapely.intersection(self.shapes[hits], footprint)
        positive = shapely.area(intersections) > 0
        if not self.road[hits[positive]].all():
            raise ValueError("tilted hardware is not wholly on actual traffic faces")
        cover = shapely.union_all(intersections[positive])
        error = footprint.difference(cover).area
        p = np.asarray(triangle)
        gradient = np.linalg.solve(p[1:, :2] - p[0, :2], p[1:, 2] - p[0, 2])
        vertices = self.mesh.vertices[self.mesh.triangles[hits[positive]]]
        expected = p[0, 2] + (vertices[..., :2] - p[0, :2]) @ gradient
        maximum = float(np.max(abs(vertices[..., 2] - expected), initial=0))
        if error > 1e-7 or maximum > 1e-7:
            raise ValueError("tilted hardware does not match whole actual bed faces")
        return {
            "actual_face_candidates": len(hits),
            "bed_plane_error_m": maximum,
            "missing_actual_bed_m2": error,
        }


def metric_audit(product, gauge):
    rings = product.rails.vertices.reshape(2, -1, 12, 3)
    axis = (rings[:, :, 0] + rings[:, :, 1]).mean(axis=0) / 2
    delta = np.diff(axis, axis=0)
    lengths = np.linalg.norm(delta, axis=1)
    tangent = delta / lengths[:, None]
    normal = product.plane_frame[:, 2]
    lateral = np.cross(normal, tangent)
    gap = rings[1, :, 7] - rings[0, :, 6]
    gauge_error = max(
        float(abs(np.einsum("ij,ij->i", g, lateral) - gauge).max())
        for g in (gap[:-1], gap[1:])
    )
    levels = (
        np.array(
            [0, 0, 0.014, 0.035, 0.121, 0.136, 0.172, 0.172, 0.136, 0.121, 0.035, 0.014]
        )
        + 0.18
    )
    section_error = float(abs((rings - product.plane_origin) @ normal - levels).max())
    transforms = product.sleeper_transforms
    dimensions = np.linalg.norm(transforms[:, :, :3], axis=1)
    dimension_error = float(np.max(abs(dimensions - [2.6, 0.24, 0.18]), initial=0))
    bottom = np.concatenate(product.sleeper_bottoms)
    contact_error = float(abs((bottom - product.plane_origin) @ normal).max(initial=0))
    boxes = bake_rail_sleepers(transforms)
    above = (
        (np.vstack((product.rails.vertices, boxes.vertices)) - product.plane_origin)
        @ normal
        / normal[2]
    )
    if above.min() < -1e-9 or abs(above.max() - 0.352 / normal[2]) > 1e-9:
        raise ValueError(
            "actual tilted hardware envelope differs from its normal section"
        )
    volume_error = abs(closed_volume(boxes) - len(transforms) * 2.6 * 0.24 * 0.18)
    rail_volume_error = abs(
        closed_volume(product.rails) - 2 * 0.00892425 * lengths.sum()
    )
    if (
        max(gauge_error, dimension_error, contact_error, section_error) > 1e-9
        or max(volume_error, rail_volume_error) > 1e-7
    ):
        raise ValueError("actual tilted gauge, section, contact or volume failed")
    return {
        "gauge_error_m": gauge_error,
        "dimension_error_m": dimension_error,
        "section_height_error_m": section_error,
        "contact_error_m": contact_error,
        "sleeper_volume_error_m3": volume_error,
        "rail_volume_error_m3": rail_volume_error,
        "maximum_height_above_actual_bed_m": float(above.max()),
    }


def admit(products, rows):
    shapes, owners = [], []
    for index, product in enumerate(products):
        shapes.extend(product.footprints)
        owners.extend([index] * len(product.footprints))
    shapes, owners = np.array(shapes, object), np.array(owners)
    tree = shapely.STRtree(shapes)
    neighbours = [set() for _ in products]
    work = 0
    for start in range(0, len(shapes), 32):
        pairs = tree.query(shapes[start : start + 32], predicate="intersects")
        work += pairs.shape[1]
        if work > 1048576:
            raise ValueError("tilted hardware mutual projection pair budget")
        a, b = start + pairs[0], pairs[1]
        selected = (a < b) & (owners[a] != owners[b])
        a, b = a[selected], b[selected]
        overlap = shapely.area(shapely.intersection(shapes[a], shapes[b])) > 1e-9
        for first, second in zip(owners[a[overlap]], owners[b[overlap]], strict=True):
            neighbours[first].add(int(second))
            neighbours[second].add(int(first))
    accepted = set()
    for index, row in enumerate(rows):
        if accepted.intersection(neighbours[index]):
            row.update(valid=False, reason="tilted_hardware_projection_conflict")
        else:
            accepted.add(index)
    return [p for i, p in enumerate(products) if i in accepted], {
        "projection_pair_candidates": work,
        "rejected_optional_pieces": len(products) - len(accepted),
    }
