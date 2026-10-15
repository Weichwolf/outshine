"""Whole finite contact prisms between explicitly chosen piecewise affine surfaces."""

import numpy as np
import shapely
from outshine_ref.contracts import Mesh
from outshine_ref.geometry import vertical_clearance
from outshine_ref.transport.supports import SupportVolumeBuilder
from retained_surface import surface_domain


class ContactSurface:
    def __init__(self, mesh, *, maximum=262144):
        if not 0 < len(mesh.triangles) <= maximum:
            raise ValueError("support contact surface face budget")
        self.mesh = mesh
        self.shapes = shapely.polygons(mesh.vertices[mesh.triangles, :2])
        self.tree = shapely.STRtree(self.shapes)

    def patch(self, footprint):
        hits = self.tree.query(footprint, predicate="intersects")
        if not 0 < len(hits) <= 2048:
            raise ValueError("support contact patch absent or face budget")
        pieces = shapely.intersection(self.shapes[hits], footprint)
        pieces = pieces[shapely.area(pieces) > 0]
        covered = shapely.union_all(pieces)
        if footprint.difference(covered).area > 1e-8:
            raise ValueError("support footprint outside selected contact")
        used, inverse = np.unique(self.mesh.triangles[hits], return_inverse=True)
        patch = Mesh(self.mesh.vertices[used], inverse.reshape(-1, 3))
        return patch, pieces, len(hits)


def contact_prism(footprint, lower, upper, *, minimum_height=2.0):
    """Small experiment compatibility adapter; construction lives in the wheel."""
    product, note = SupportVolumeBuilder(lower.mesh, upper.mesh).build(
        footprint, minimum_height_m=minimum_height
    )
    if product is None:
        raise ValueError(note["reason"])
    return product.body, (product.lower_contact, product.upper_contact), note


def replay_contact(actual, reference):
    patch, _pieces, _count = reference.patch(surface_domain(actual))
    forward = vertical_clearance(actual, patch, max_faces=4096, seconds=2)
    backward = vertical_clearance(patch, actual, max_faces=4096, seconds=2)
    if not all(
        row["valid"] and abs(row["minimum_clearance_m"]) <= 1e-7
        for row in (forward, backward)
    ):
        raise ValueError("support whole contact differs from selected surface")
    return max(abs(r["minimum_clearance_m"]) for r in (forward, backward))
