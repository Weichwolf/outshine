"""Use the existing shared solid primitive with preserved topology roots/owners."""

from types import SimpleNamespace

import numpy as np
from bridge_support_contacts import replay_contact
from foot_ramp_audit import shell_ownership
from foot_ramp_geometry import Ramp
from outshine_ref.buildings.modelling import PrismVolume, subtract_prisms
from outshine_ref.contracts import Mesh
from outshine_ref.transport.supports import SupportVolume
from retained_surface import surface_domain
from surface_solid_checks import audit_closed


def cut(product, shape, identifier):
    body = product.body
    surface = SimpleNamespace(
        mesh={
            "vertices": body.vertices,
            "faces": body.triangles,
            "materials": np.ones(len(body.triangles), np.uint16),
        },
        vertex_roots=np.arange(len(body.vertices), dtype=np.uint64),
    )
    prism = PrismVolume(
        identifier + "/ramp",
        shape,
        float(body.vertices[:, 2].min()) - 1,
        float(body.vertices[:, 2].max()) + 1,
    )
    value = subtract_prisms(identifier, surface, [prism], max_triangles=262144)
    if not len(value.mesh["faces"]):
        return None, {"whole_erasure": True}
    points = value.mesh["vertices"]
    _roots, first, inverse = np.unique(
        value.vertex_roots, return_index=True, return_inverse=True
    )
    if not np.array_equal(points, points[first[inverse]]):
        raise ValueError("ramp solid cut topology root joins distinct positions")
    result = Mesh(points[first], inverse[value.mesh["faces"]])
    p = result.vertices[result.triangles]
    normal = np.cross(p[:, 1] - p[:, 0], p[:, 2] - p[:, 0])
    normal /= np.linalg.norm(normal, axis=1)[:, None]
    top, bottom = normal[:, 2] > 0.5, normal[:, 2] < -0.5
    if np.any(abs(normal[~(top | bottom), 2]) > 1e-6):
        raise ValueError("ramp cut creates a nonvertical border wall")
    lower = Mesh(result.vertices, result.triangles[bottom, ::-1])
    upper = Mesh(result.vertices, result.triangles[top])
    return SupportVolume(result, lower, upper), {
        "whole_erasure": False,
        "source_ids": value.source_ids,
        "original_face_outputs": int(np.sum(value.source_indices == 0)),
        "cutter_face_outputs": int(np.sum(value.source_indices == 1)),
        "vertices": len(result.vertices),
        "faces": len(result.triangles),
    }


def audit(product, expected, reference, upper_reference):
    for cap in [product.lower_contact, product.upper_contact]:
        if surface_domain(cap).symmetric_difference(expected).area > 1e-8:
            raise ValueError("ramp cut actual whole border footprint differs")
    low = replay_contact(product.lower_contact, reference)
    high = replay_contact(product.upper_contact, upper_reference)
    shell_ownership(
        Ramp(product.body, product.lower_contact, product.upper_contact, expected)
    )
    volume = audit_closed(product.body)
    if volume <= 0 or abs(volume - expected.area * 0.12) > 1e-7:
        raise ValueError("ramp cut whole positive border volume failed")
    p = product.body.vertices[product.body.triangles]
    q = product.body.vertices.astype(np.float32).astype(float)[product.body.triangles]
    before = np.cross(p[:, 1] - p[:, 0], p[:, 2] - p[:, 0])
    after = np.cross(q[:, 1] - q[:, 0], q[:, 2] - q[:, 0])
    if np.any(np.einsum("ij,ij->i", before, after) <= 0):
        raise ValueError("ramp cut Float32 border degenerates or inverts")
    return {"lower_error_m": low, "upper_error_m": high, "volume_m3": volume}
