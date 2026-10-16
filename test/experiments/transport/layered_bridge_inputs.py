"""Actual native facet ownership, retained bodies and precision neighbourhoods."""

import json

import numpy as np
import shapely
from camera_height_inputs import digest
from curb_inputs import protected_domain
from curb_verify import actual
from layered_bridge_audit import field_error
from outshine_ref.contracts import Mesh
from outshine_ref.geometry import vertical_clearance
from retained_surface import surface_domain
from tilted_rail_inputs import GROUND_PIN

FIXTURE_PIN = "71f76677dee5df5ddd6480f4af0ef5cb48fbcf178e5e8c9e4abf52c4fa4e0fd1"
BASE_BODY = "dc1500a495e1135da8dc5df5ea9f6f4896c31b6d1161b069890311d7dc1dfe24"
BASE_USAGE = "01e2b7ff38d6b93c932cbfde7cf48a93a29b390c98bc0da8b354405436d851f2"


def base_inputs(args):
    if (
        digest(args.fixture) != FIXTURE_PIN
        or digest(args.baseline / "after-body.npz") != BASE_BODY
    ):
        raise ValueError(
            "pinned actual layered fixture and latest complete scene required"
        )
    if digest(args.baseline / "source-usage.json") != BASE_USAGE:
        raise ValueError("latest complete original source ledger required")
    if digest(args.network / "ground.npz") != GROUND_PIN:
        raise ValueError("pinned original fine ground required")
    with np.load(args.baseline / "after-body.npz") as d:
        before = {k: d[k] for k in d.files}
    with np.load(args.network / "ground.npz") as d:
        ground = Mesh(d["vertices"], d["faces"])
    borders = []
    fixture = json.loads(args.fixture.read_text())
    for record in fixture["actual_borders"]["inputs"]:
        path = args.borders / record["file"]
        if digest(path) != record["sha256"]:
            raise ValueError("old raised-body input changed")
        products, _arrays = actual(path)
        borders.extend(products)
    return before, ground, borders


def ground_proof(ground, problem, field, physical):
    region = problem["domain"].buffer(0.002)
    if region.difference(surface_domain(field)).area > 1e-7:
        raise ValueError("actual field lacks the Float32 overlap neighbourhood")
    xyz = ground.vertices[ground.triangles]
    a, b = np.array(region.bounds).reshape(2, 2)
    hit = np.all(xyz[:, :, :2].max(axis=1) >= a, axis=1) & np.all(
        xyz[:, :, :2].min(axis=1) <= b, axis=1
    )
    if hit.sum() > 8192:
        raise ValueError("whole original ground preparation budget")
    ids, inverse = np.unique(ground.triangles[hit], return_inverse=True)
    patch = Mesh(ground.vertices[ids], inverse.reshape(-1, 3))
    shapes = shapely.polygons(patch.vertices[patch.triangles, :2])
    if (
        not shapely.coverage_is_valid(shapes)
        or region.difference(shapely.union_all(shapes)).area > 1e-7
    ):
        raise ValueError(
            "original native ground does not cover the whole candidate once"
        )
    zero = np.array([[problem["stations"][0] - 1, 0], [problem["stations"][-1] + 1, 0]])
    error = field_error(patch, zero, problem, field, region=region)
    cast = patch.vertices.astype(np.float32).astype(float)
    drift = float(np.linalg.norm(cast[:, :2] - patch.vertices[:, :2], axis=1).max())
    z_error = float(abs(cast[:, 2] - patch.vertices[:, 2]).max())
    triangles = field.vertices[field.triangles]
    gradient = np.linalg.solve(
        triangles[:, 1:, :2] - triangles[:, 0, None, :2],
        (triangles[:, 1:, 2] - triangles[:, 0, None, 2])[:, :, None],
    )[:, :, 0]
    lipschitz = float(np.linalg.norm(gradient, axis=1).max())
    ground_error = error + z_error + lipschitz * drift
    total_xy = drift + physical["float32_xy_drift_m"]
    if total_xy > 0.002:
        raise ValueError("quantized overlap exceeds the verified neighbourhood")
    return patch, {
        "native_candidate_faces": int(hit.sum()),
        "whole_field_error_m": error,
        "float32_ground_field_bound_m": ground_error,
        "float32_ground_xy_drift_m": drift,
        "ground_lipschitz": lipschitz,
        "total_overlap_xy_drift_m": total_xy,
    }


def retained_proof(args, candidate, ground, borders, field):
    p, _h, ribbon, note = candidate
    patch, proof = ground_proof(ground, p, field, note["physical"])
    protected = protected_domain(args.supports, args.reference_root)
    with np.load(args.baseline / "bridge.npz") as d:
        last = Mesh(d["upper_vertices"], d["cap_faces"])
    protected = shapely.union_all([protected, surface_domain(last).buffer(0.2)])
    if ribbon.shape.intersection(protected).area > 1e-8:
        raise ValueError(
            "candidate needs an unplanned retained bridge/portal/pier change"
        )
    checks = 0
    for product in borders:
        shape = surface_domain(product.upper_contact)
        if ribbon.shape.intersection(shape).area <= 1e-8:
            continue
        checks += 1
        separation = vertical_clearance(
            product.upper_contact, ribbon.lower, seconds=2, max_faces=8192
        )
        minimum = 4.5 if p["profile"] == "motorway" else 0
        if (
            not separation["valid"]
            or separation["minimum_clearance_m"] < minimum - 1e-7
        ):
            raise ValueError("candidate structure intersects a retained border/ramp")
    r = p["recipe"]
    slopes = abs(
        np.diff(np.array(note["geometry"]["table"])[:, 1])
        / np.diff(np.array(note["geometry"]["table"])[:, 0])
    )
    alignment = float(slopes.max() * proof["total_overlap_xy_drift_m"])
    expected = r["soil_gap_m"] + r["clearance_margin_m"]
    bound = (
        expected
        - note["physical"]["float32_lower_field_error_m"]
        - proof["float32_ground_field_bound_m"]
        - alignment
    )
    step = (
        note["physical"]["float32_upper_field_error_m"]
        + proof["float32_ground_field_bound_m"]
        + alignment
    )
    if bound < r["soil_gap_m"] - 1e-7 or step > 0.001:
        raise ValueError("actual Float32 ground separation/contact bound violated")
    return patch, dict(
        proof,
        old_raised_body_checks=checks,
        float32_minimum_soil_clearance_bound_m=bound,
        float32_ground_contact_step_bound_m=step,
    )
