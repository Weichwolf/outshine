"""One bounded canonical pair: own complete crossing facets before local fits.

The caller supplies the fixed source/recipe-owned pair topology, not a camera mesh.
This does not choose world-wide corner/junction or lake/coast datum ownership.
"""

import hashlib
from dataclasses import dataclass

import numpy as np
from outshine_ref.contracts import Mesh
from outshine_ref.water import fit_physical_surface
from outshine_ref.water._fit import grade_envelopes
from outshine_ref.water.frame import normal_caps, tangent_directions


@dataclass(frozen=True)
class Subdomain:
    mesh: Mesh
    vertex_ids: np.ndarray
    face_ids: np.ndarray
    rows: np.ndarray
    primary: np.ndarray


@dataclass(frozen=True)
class ContactBand:
    domain: Subdomain
    mesh: Mesh
    version: str
    context_key: str
    report: dict


def subset(mesh, rows, faces):
    faces = np.array(faces, dtype=int, copy=True)
    ids, local = np.unique(mesh.triangles[faces], return_inverse=True)
    row_ids = np.flatnonzero(np.isin(rows[:, 0].astype(int), faces))
    selected = rows[row_ids].copy()
    selected[:, 0] = np.searchsorted(faces, selected[:, 0].astype(int))
    # This experiment has exactly two explicit primary rows for every root face.
    nf = len(mesh.triangles)
    primary = np.c_[
        np.searchsorted(row_ids, faces), np.searchsorted(row_ids, nf + faces)
    ]
    if not np.array_equal(row_ids[primary], np.c_[faces, nf + faces]):
        raise ValueError("two canonical primary rows per parent required")
    for array in (ids, faces, selected, primary):
        array.setflags(write=False)
    return Subdomain(
        Mesh(mesh.vertices[ids], local.reshape(-1, 3)), ids, faces, selected, primary
    )


def crossing_faces(mesh, plane_x, half_width, *, face_limit=4096, vertex_limit=4096):
    if not np.isfinite([plane_x, half_width]).all() or half_width < 0:
        raise ValueError("finite canonical plane and nonnegative band width required")
    if len(mesh.vertices) > 8192 or len(mesh.triangles) > 16384:
        raise ValueError("canonical pair topology budget")
    x = mesh.vertices[mesh.triangles, 0]
    faces = np.flatnonzero(
        (x.min(axis=1) <= plane_x + half_width)
        & (x.max(axis=1) >= plane_x - half_width)
    )
    if not len(faces):
        raise ValueError("no contact facets at the declared interface")
    if len(faces) > face_limit or len(np.unique(mesh.triangles[faces])) > vertex_limit:
        raise ValueError("canonical contact band budget")
    return faces


def topology_key(mesh):
    digest = hashlib.sha256()
    digest.update(np.asarray(mesh.vertices[:, :2], dtype="<f8").tobytes())
    digest.update(np.asarray(mesh.triangles, dtype="<u4").tobytes())
    return digest.hexdigest()


def pair_context(mesh, anchor, rows, bounds, plane_x):
    digest = hashlib.sha256(topology_key(mesh).encode())
    for array in (anchor, rows, bounds, [plane_x]):
        digest.update(np.asarray(array, dtype="<f8").tobytes())
    return digest.hexdigest()


def solve_band(mesh, anchor, rows, bounds, *, plane_x, half_width, recipe_revision):
    if not isinstance(recipe_revision, str) or not recipe_revision:
        raise ValueError("source/datum/recipe-bound revision required")
    faces = crossing_faces(mesh, plane_x, half_width)
    domain = subset(mesh, rows, faces)
    fit = fit_physical_surface(
        domain.mesh,
        anchor,
        domain.rows,
        up_bounds_m=bounds,
        primary_rows=domain.primary,
    )
    if fit.mesh is None:
        return None, dict(fit.report)
    root_key = pair_context(mesh, anchor, rows, bounds, plane_x)
    digest = hashlib.sha256((recipe_revision + root_key).encode())
    for array in (
        np.asarray(anchor),
        np.asarray(bounds),
        np.asarray([plane_x, half_width]),
        domain.mesh.vertices,
        domain.rows,
        domain.face_ids,
        fit.mesh.vertices,
    ):
        digest.update(np.ascontiguousarray(array).tobytes())
    report = dict(
        fit.report,
        canonical_pair_faces=len(mesh.triangles),
        shared_faces=len(faces),
        shared_vertices=len(domain.vertex_ids),
        actual_shared_x_range_m=[
            float(domain.mesh.vertices[:, 0].min()),
            float(domain.mesh.vertices[:, 0].max()),
        ],
        shared_geometry_index_payload_bytes=sum(
            a.nbytes
            for a in (
                domain.vertex_ids,
                domain.face_ids,
                fit.mesh.vertices,
                fit.mesh.triangles,
            )
        ),
    )
    return ContactBand(domain, fit.mesh, digest.hexdigest(), root_key, report), report


def physical_audit(mesh, anchor, rows, bounds):
    caps = normal_caps(anchor, mesh.vertices[:, :2], mesh.triangles, bounds)
    triangles = mesh.vertices[mesh.triangles]
    normal = np.cross(
        triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
    )
    normal /= normal[:, 2, None]
    faces = rows[:, 0].astype(int)
    tangent = tangent_directions(caps["up"][faces], rows[:, 1:4])
    envelope = grade_envelopes(normal, caps, faces, tangent)
    error = max(
        float((rows[:, 4] - envelope[:, 0]).max()),
        float((envelope[:, 1] - rows[:, 5]).max()),
        0.0,
    )
    box_error = max(
        float(bounds[0] - mesh.vertices[:, 2].min()),
        float(mesh.vertices[:, 2].max() - bounds[1]),
        0.0,
    )
    return {
        "whole_face_envelope_violation": error,
        "height_box_violation_m": box_error,
        "admitted": max(error, box_error) <= 1e-9,
    }


def solve_region(mesh, anchor, rows, bounds, band, *, plane_x, side, prior_offset_m=0):
    if side not in (0, 1) or not np.isfinite(prior_offset_m):
        raise ValueError("explicit side and finite soft-prior offset required")
    if pair_context(mesh, anchor, rows, bounds, plane_x) != band.context_key:
        raise ValueError(
            "contact product belongs to a different canonical pair context"
        )
    centres = mesh.vertices[mesh.triangles, 0].mean(axis=1)
    own = np.flatnonzero(centres < plane_x if side == 0 else centres >= plane_x)
    domain = subset(mesh, rows, np.union1d(own, band.domain.face_ids))
    local_ids = np.searchsorted(domain.vertex_ids, band.domain.vertex_ids)
    if not np.array_equal(domain.vertex_ids[local_ids], band.domain.vertex_ids):
        raise ValueError("contact topology is not shared by this region")
    priors = domain.mesh.vertices.copy()
    priors[:, 2] += prior_offset_m
    fixed = dict(
        zip(local_ids.tolist(), band.mesh.vertices[:, 2].tolist(), strict=True)
    )
    fit = fit_physical_surface(
        Mesh(priors, domain.mesh.triangles),
        anchor,
        domain.rows,
        up_bounds_m=bounds,
        primary_rows=domain.primary,
        fixed=fixed,
    )
    if fit.mesh is None:
        return None, domain, dict(fit.report)
    # Publish the owner's exact bits; recheck the composed physical field afterwards.
    vertices = fit.mesh.vertices.copy()
    displacement = float(abs(vertices[local_ids, 2] - band.mesh.vertices[:, 2]).max())
    vertices[local_ids] = band.mesh.vertices
    composed = Mesh(vertices, domain.mesh.triangles)
    check = physical_audit(composed, anchor, domain.rows, bounds)
    report = dict(
        fit.report,
        composed_audit=check,
        owner_bit_replacement_max_m=displacement,
        root_face_ids=domain.face_ids.tolist(),
        fixed_vertices=len(fixed),
    )
    if not check["admitted"]:
        return None, domain, dict(report, reason="composed_contact_field_not_admitted")
    return composed, domain, report


def compose(mesh, anchor, rows, bounds, band, regions, *, plane_x):
    if pair_context(mesh, anchor, rows, bounds, plane_x) != band.context_key:
        raise ValueError(
            "contact product belongs to a different canonical pair context"
        )
    centres = mesh.vertices[mesh.triangles, 0].mean(axis=1)
    vertices = mesh.vertices.copy()
    for side, (regional, domain) in enumerate(regions):
        selected = np.flatnonzero(
            centres < plane_x if side == 0 else centres >= plane_x
        )
        ids = np.unique(mesh.triangles[selected])
        local = np.searchsorted(domain.vertex_ids, ids)
        if not np.array_equal(domain.vertex_ids[local], ids):
            raise ValueError("regional product lacks owned faces")
        vertices[ids] = regional.vertices[local]
    vertices[band.domain.vertex_ids] = band.mesh.vertices
    result = Mesh(vertices, mesh.triangles)
    check = physical_audit(result, anchor, rows, bounds)
    if not check["admitted"]:
        raise ValueError("composed canonical pair physical admission failed")
    return result, check


@dataclass(frozen=True)
class LevelMoment:
    area_m2: float
    residual_integral_m3: float
    shift_lower_m: float
    shift_upper_m: float
    faces: int


def level_moment(target, field, faces, bounds):
    """Five scalars per disjoint face owner; duplicated contact faces count once."""
    import math

    faces = np.asarray(faces, dtype=int)
    if not len(faces):
        return LevelMoment(0, 0, -math.inf, math.inf, 0)
    if topology_key(target) != topology_key(field):
        raise ValueError("same canonical target/field topology required")
    triangle = target.vertices[target.triangles[faces], :2]
    a, b = triangle[:, 1] - triangle[:, 0], triangle[:, 2] - triangle[:, 0]
    area = abs(a[:, 0] * b[:, 1] - a[:, 1] * b[:, 0]) / 2
    residual = target.vertices[:, 2] - field.vertices[:, 2]
    integrated = area * residual[target.triangles[faces]].mean(axis=1)
    ids = np.unique(field.triangles[faces])
    return LevelMoment(
        math.fsum(area.tolist()),
        math.fsum(integrated.tolist()),
        float(bounds[0] - field.vertices[ids, 2].min()),
        float(bounds[1] - field.vertices[ids, 2].max()),
        len(faces),
    )


def common_level_shift(moments, *, free_level):
    """Exact constant-mode minimum of the summed P1 mass objective within bounds.

    The shared product may shift only if it owns the additive level freedom.
    External hard datum/point contacts prohibit shifting; no pin is relaxed.
    Normal caps/directions and the canonical XY/prism remain fixed throughout.
    """
    import math

    if type(free_level) is not bool or not 1 <= len(moments) <= 3:
        raise ValueError(
            "explicit free-level policy and bounded canonical pair required"
        )
    values = [moments[k] for k in sorted(moments)]
    area = math.fsum(m.area_m2 for m in values)
    low = max(m.shift_lower_m for m in values)
    high = min(m.shift_upper_m for m in values)
    if not area > 0 or low > high:
        raise ValueError("no valid shared level interval")
    proposed = math.fsum(m.residual_integral_m3 for m in values) / area
    chosen = float(np.clip(proposed, low, high)) if free_level else 0.0
    if not low <= chosen <= high:
        raise ValueError("pinned level outside shared height bounds")
    return chosen, {
        "source_faces_counted_once": sum(m.faces for m in values),
        "moment_payload_bytes": len(values) * 5 * 8,
        "unconstrained_constant_shift_m": proposed,
        "shared_shift_interval_m": [low, high],
        "free_level": free_level,
    }
