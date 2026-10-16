"""Original complete scene and actual fine-ground/contact checks for the candidate."""

import numpy as np
import shapely
from bridge_support_contacts import ContactSurface, replay_contact
from camera_height_inputs import digest
from curb_inputs import protected_domain
from ground_surface_showcase import clipped
from outshine_ref.contracts import Mesh
from retained_surface import surface_domain
from tilted_rail_inputs import GROUND_PIN
from wien_support_verify import unpack

BASE_BODY = "4e4e26d902c5ba0aab66e3d49e4845ad98343c112e44ad9cb2b9a0b239f17273"
BASE_REPORT = "300f5c890768e50a80919c5897ae6c27dbfd7f99f1c02faf82df94beda1327dd"
FIXTURE_PIN = "50ba8c4c0b4963b64d539c69ceb23c2b9f28b371d24af1e4437235cc355b916c"


def ground_precision(patch, domain, problem):
    points = patch.vertices[patch.triangles]
    selected = shapely.intersects(
        shapely.polygons(points[:, :, :2]), domain.buffer(0.002)
    )
    native = np.unique(points[selected].reshape(-1, 3), axis=0)
    anchor, gradient = problem["reference"], problem["gradient"]
    error = native[:, 2] - anchor[2] - (native[:, :2] - anchor[:2]) @ gradient
    if abs(error).max() > 1e-7:
        raise ValueError(
            "original whole ground triangles do not share the selected affine field"
        )
    cast = native.astype(np.float32).astype(float)
    residual = cast[:, 2] - anchor[2] - (cast[:, :2] - anchor[:2]) @ gradient
    return {
        "float32_ground_field_error_m": float(abs(residual).max()),
        "float32_ground_xy_drift_m": float(
            np.linalg.norm(cast[:, :2] - native[:, :2], axis=1).max()
        ),
        "original_ground_vertices_in_precision_bound": len(native),
    }


def local_ground(network, domain, *, problem=None):
    if digest(network / "ground.npz") != GROUND_PIN:
        raise ValueError("original whole actual ground required")
    with np.load(network / "ground.npz") as saved:
        vertices, faces = saved["vertices"], saved["faces"]
    p = vertices[faces, :2]
    lo, hi = np.array(domain.bounds).reshape(2, 2)
    selected = np.all(p.max(axis=1) >= lo, axis=1) & np.all(p.min(axis=1) <= hi, axis=1)
    if not 1 <= selected.sum() <= 8192:
        raise ValueError("actual local ground preparation face budget")
    ids, inverse = np.unique(faces[selected], return_inverse=True)
    patch = Mesh(vertices[ids], inverse.reshape(-1, 3))
    actual, _slots, _weld = clipped(
        patch, np.zeros(len(patch.triangles)), domain, max_faces=8192
    )
    if surface_domain(actual).symmetric_difference(domain).area > 1e-7:
        raise ValueError(
            "actual original ground does not cover every proposed source footprint"
        )
    note = {
        "bbox_candidate_faces": int(selected.sum()),
        "clipped_faces": len(actual.triangles),
    }
    if problem is not None:
        note.update(ground_precision(patch, domain, problem))
    return actual, note


def inputs(args, fixture, problem, ribbon, precision):
    if digest(args.fixture) != FIXTURE_PIN:
        raise ValueError("pinned actual cycle bridge fixture required")
    if (
        digest(args.baseline / "after-body.npz") != BASE_BODY
        or digest(args.baseline / "comparison.json") != BASE_REPORT
    ):
        raise ValueError(
            "unchanged latest complete ramp/border/rail/bridge/terrain scene required"
        )
    fine, note = local_ground(args.network, problem["domain"], problem=problem)
    if precision["xy_drift_m"] + note["float32_ground_xy_drift_m"] > 0.002:
        raise ValueError("Float32 overlap neighbourhood exceeds its preparation margin")
    alignment = precision["relative_profile_lipschitz"] * (
        precision["xy_drift_m"] + note["float32_ground_xy_drift_m"]
    )
    bound = (
        fixture["recipe"]["nonmotorized_headroom_m"]
        + fixture["recipe"]["clearance_margin_m"]
        - precision["lower_field_error_m"]
        - note["float32_ground_field_error_m"]
        - alignment
    )
    if bound < fixture["recipe"]["nonmotorized_headroom_m"] - 1e-7:
        raise ValueError("whole Float32 underpass clearance bound violated")
    note.update(
        float32_minimum_clearance_bound_m=bound,
        float32_profile_alignment_allowance_m=alignment,
    )
    reference = Mesh(
        np.array(fixture["field_vertices"]), np.array(fixture["field_faces"])
    )
    note["actual_ground_whole_field_error_m"] = replay_contact(
        fine, ContactSurface(reference)
    )
    protected = protected_domain(args.supports, args.reference_root)
    if ribbon.shape.intersection(protected).area > 1e-8:
        raise ValueError(
            "new cycle bridge intersects an original protected bridge/portal/pier"
        )
    checked = 0
    for path in (args.baseline / "borders.npz", args.baseline / "ramps.npz"):
        _bodies, contacts, _arrays = unpack(path)
        for lower, _upper in contacts:
            checked += 1
            if ribbon.shape.intersection(surface_domain(lower)).area > 1e-8:
                raise ValueError(
                    "new cycle bridge needs an unplanned existing border/ramp edit"
                )
    with np.load(args.baseline / "after-body.npz") as saved:
        before = {k: saved[k] for k in saved.files}
    return (
        before,
        fine,
        dict(
            note,
            unchanged_existing_border_bodies=checked,
            actual_ground_sha256=GROUND_PIN,
            baseline_body_sha256=BASE_BODY,
            baseline_report_sha256=BASE_REPORT,
            fixture_sha256=FIXTURE_PIN,
        ),
    )
