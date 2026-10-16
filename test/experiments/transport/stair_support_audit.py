"""Whole saved pier bodies and actual Float32 contact envelopes; no construction."""

import numpy as np
import shapely
from bridge_support_contacts import ContactSurface
from outshine_ref.contracts import Mesh
from outshine_ref.geometry import vertical_clearance
from stair_network_geometry import vertex_links
from surface_height_checks import normal_gradients
from surface_solid_checks import audit_closed


def quantized(mesh):
    return Mesh(mesh.vertices.astype(np.float32).astype(float), mesh.triangles)


def domain(mesh):
    if not 0 < len(mesh.triangles) <= 4096:
        raise ValueError("pier contact face budget")
    xyz = mesh.vertices[mesh.triangles, :2]
    a, b = xyz[:, 1] - xyz[:, 0], xyz[:, 2] - xyz[:, 0]
    if np.any(a[:, 0] * b[:, 1] - a[:, 1] * b[:, 0] <= 0):
        raise ValueError("pier contact projection collapses or inverts")
    polygons = shapely.polygons(xyz)
    if not shapely.coverage_is_valid(polygons):
        raise ValueError("pier contact is not a conforming height coverage")
    shape = shapely.coverage_union_all(polygons)
    if shape.geom_type != "Polygon" or len(shape.interiors):
        raise ValueError("one whole pier footprint without holes required")
    return shape


def body_caps(product):
    body, low, high = product.body, product.lower_contact, product.upper_contact
    n, f = len(low.vertices), len(low.triangles)
    if not 0 < n <= 8192 or len(body.triangles) > 32768:
        raise ValueError("pier body size budget")
    if not (
        np.array_equal(low.vertices[:, :2], high.vertices[:, :2])
        and np.array_equal(low.triangles, high.triangles)
        and len(body.vertices) == 2 * n
        and np.array_equal(body.vertices[:n], high.vertices)
        and np.array_equal(body.vertices[n:], low.vertices)
        and np.array_equal(body.triangles[:f], high.triangles)
        and np.array_equal(body.triangles[f : 2 * f], low.triangles[:, ::-1] + n)
    ):
        raise ValueError("pier body differs from its whole contact caps")
    edges = low.triangles[:, [[0, 1], [1, 2], [2, 0]]].reshape(-1, 2)
    keys, counts = np.unique(np.sort(edges, axis=1), axis=0, return_counts=True)
    boundary = {tuple(edge) for edge in keys[counts == 1]}
    walls = {}
    for face in body.triangles[2 * f :]:
        edge = tuple(np.unique(face % n))
        if len(edge) != 2 or edge not in boundary:
            raise ValueError("pier has a nonvertical or interior wall")
        walls[edge] = walls.get(edge, 0) + 1
    if set(walls) != boundary or any(count != 2 for count in walls.values()):
        raise ValueError("pier lacks its exact boundary walls")
    links = vertex_links(body.vertices, body.triangles)
    records = []
    for mesh in (body, quantized(body)):
        lo = Mesh(mesh.vertices[n:], low.triangles)
        hi = Mesh(mesh.vertices[:n], high.triangles)
        domain(lo)
        domain(hi)
        heights = hi.vertices[:, 2] - lo.vertices[:, 2]
        if heights.min() < 0.1:
            raise ValueError("actual pier has insufficient whole height")
        xyz = mesh.vertices[mesh.triangles]
        normals = np.cross(xyz[:, 1] - xyz[:, 0], xyz[:, 2] - xyz[:, 0])
        if np.any(normals[2 * f :, 2] != 0):
            raise ValueError("actual pier wall projection is not vertical")
        if np.any(np.linalg.norm(normals, axis=1) == 0):
            raise ValueError("pier contains a collapsed face")
        volume = audit_closed(mesh)
        if volume <= 0:
            raise ValueError("pier lacks positive closed volume")
        records.append({"volume_m3": volume, "minimum_height_m": float(heights.min())})
    xyz = body.vertices[body.triangles]
    original = np.cross(xyz[:, 1] - xyz[:, 0], xyz[:, 2] - xyz[:, 0])
    if np.any(np.einsum("ij,ij->i", original, normals) <= 0):
        raise ValueError("actual Float32 pier face inverts")
    return dict(**links, double=records[0], float32=records[1])


def round_error(mesh):
    difference = quantized(mesh).vertices - mesh.vertices
    used = np.unique(mesh.triangles)
    return (
        float(np.linalg.norm(difference[used, :2], axis=1).max()),
        float(abs(difference[used, 2]).max()),
    )


def contact_error(first, second):
    values, checks = [], 0
    for low, high in ((first, second), (second, first)):
        note = vertical_clearance(low, high, max_faces=4096, max_pairs=65536, seconds=2)
        if not note["valid"]:
            raise ValueError("pier whole contact unresolved: " + note["reason"])
        values.append(abs(note["minimum_clearance_m"]))
        checks += note["checks"]
    return max(values), checks


class Reference(ContactSurface):
    def __init__(self, mesh):
        super().__init__(mesh)
        self.xy_rounding_m, _z = round_error(mesh)

    def check(self, cap):
        shape = domain(cap)
        dx, dz = round_error(cap)
        radius = self.xy_rounding_m + dx + 1e-7
        patch, _pieces, _count = self.patch(shape.buffer(radius))
        actual = quantized(cap)
        actual_shape = domain(actual)
        rounded = quantized(patch)
        cover = shapely.union_all(
            shapely.polygons(rounded.vertices[rounded.triangles, :2])
        )
        if not cover.covers(actual_shape):
            raise ValueError("actual pier cap exceeds the actual reference surface")
        error, checks = contact_error(cap, patch)
        if error > 1e-7:
            raise ValueError("pier whole Double contact differs from its reference")
        gradients = normal_gradients(
            patch.vertices[:, :2], patch.triangles, patch.vertices[:, 2]
        )
        slope = float(np.linalg.norm(gradients, axis=1).max())
        rx, rz = round_error(patch)
        bound = error + dz + rz + slope * (dx + rx)
        actual_error, actual_checks = contact_error(actual, rounded)
        if actual_error > bound + 1e-10:
            raise ValueError("actual pier contact exceeds its derived Float32 envelope")
        return {
            "double_contact_error_m": error,
            "float32_contact_error_m": actual_error,
            "derived_float32_contact_bound_m": bound,
            "reference_lipschitz": slope,
            "contact_xy_rounding_m": dx,
            "contact_z_rounding_m": dz,
            "reference_xy_rounding_m": rx,
            "reference_z_rounding_m": rz,
            "contact_pair_checks": checks + actual_checks,
        }
