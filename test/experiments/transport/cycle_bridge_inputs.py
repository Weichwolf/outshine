"""Original complete scene and actual fine-ground/contact checks for the candidate."""

import numpy as np
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
FIXTURE_PIN = "7eaebd31422444de98cbeff3af181259f0a7d198521ebd31ea14e6a79ee78385"


def local_ground(network, domain):
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
    return actual, {
        "bbox_candidate_faces": int(selected.sum()),
        "clipped_faces": len(actual.triangles),
    }


def inputs(args, fixture, problem, ribbon):
    if digest(args.fixture) != FIXTURE_PIN:
        raise ValueError("pinned actual cycle bridge fixture required")
    if (
        digest(args.baseline / "after-body.npz") != BASE_BODY
        or digest(args.baseline / "comparison.json") != BASE_REPORT
    ):
        raise ValueError(
            "unchanged latest complete ramp/border/rail/bridge/terrain scene required"
        )
    fine, note = local_ground(args.network, problem["domain"])
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
