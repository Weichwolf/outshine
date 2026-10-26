"""One bounded interface field is shared by independently generated local meshes.

The preferred path reuses the admitted height field and its spatial data. Extra
local QPs remain a measured alternative. Pair products are not canonical cell
owners: combining multiple interfaces/corners still needs a common owner.
"""

import hashlib
import json
from dataclasses import dataclass

import numpy as np
import shapely
from outshine_ref.contracts import Mesh
from outshine_ref.water import fit_physical_surface
from water_contact_band import Subdomain, physical_audit, subset
from water_edge_profiles import (
    masked_profile_owner,
    prepare_profile_owner,
    restrict_profile,
)
from water_field_difference import surface
from water_shared_borders import adjacent_cells


@dataclass(frozen=True)
class InterfaceHeight:
    edge: tuple
    cells: tuple
    priors: Mesh
    witness: Mesh
    anchor: np.ndarray
    rows: np.ndarray
    bounds: tuple
    mask: object
    polylines: tuple
    whole: object
    common: object
    band: object
    profile: object
    source_version: str
    context: str
    version: str
    report: dict


@dataclass(frozen=True)
class InterfaceRegion:
    field: object
    domain: object
    profile: object
    interface_version: str
    report: dict


def input_context(
    edge, cells, priors, anchor, rows, bounds, mask, polylines, source_version
):
    digest = hashlib.sha256(
        json.dumps(
            ["water-interface-height-v1", edge, cells, source_version],
            separators=(",", ":"),
        ).encode()
    )
    for array in (
        priors.vertices,
        priors.triangles,
        anchor,
        rows,
        np.asarray(bounds),
        *polylines,
    ):
        value = np.ascontiguousarray(array)
        digest.update(json.dumps([value.shape, value.dtype.str]).encode())
        digest.update(value.tobytes())
    digest.update(mask.wkb)
    return digest.hexdigest()


def solved_version(context, mesh):
    digest = hashlib.sha256(context.encode())
    digest.update(mesh.vertices.tobytes())
    return digest.hexdigest()


def validate_mask(mask):
    if (
        not isinstance(mask, shapely.Geometry)
        or mask.geom_type not in ("Polygon", "MultiPolygon")
        or mask.is_empty
        or not mask.is_valid
        or shapely.get_num_coordinates(mask) > 32768
        or not np.isfinite(shapely.get_coordinates(mask)).all()
    ):
        raise ValueError("bounded finite valid water mask required")


def contact_faces(mesh, mask, polylines, *, face_limit):
    if not polylines or sum(len(line) for line in polylines) > 8192:
        raise ValueError("bounded nonempty interface polylines required")
    for line in polylines:
        if (
            line.ndim != 2
            or line.shape[1] != 2
            or len(line) < 2
            or not np.isfinite(line).all()
        ):
            raise ValueError("finite interface polyline coordinates required")
        if np.any(np.all(np.diff(line, axis=0) == 0, axis=1)):
            raise ValueError("nonzero declared interface intervals required")
    curve = shapely.union_all([shapely.LineString(line) for line in polylines])
    if curve.length <= 0 or not mask.covers(curve):
        raise ValueError("whole interface must lie in the precise water mask")
    _points, _gradient, polygons, _area = surface(mesh)
    candidates = np.sort(shapely.STRtree(polygons).query(curve))
    lengths = shapely.length(shapely.intersection(polygons[candidates], curve))
    faces = candidates[lengths > 0]
    if not len(faces) or len(faces) > face_limit:
        raise ValueError("shared-facet count exceeds the interface budget")
    return faces, polygons, len(candidates)


def prepare_interface(
    priors,
    anchor,
    rows,
    bounds,
    mask,
    polylines,
    *,
    edge,
    cells,
    source_version,
    band_face_limit=4096,
):
    """Solve the bounded coarse pair once, then restrict its admissible field.

    The restricted full solution witnesses local feasibility with fixed band
    vertices. A separately optimized band alone need not admit an extension.
    The existing fitter/caps are used; failure never publishes a partial owner.
    """
    if type(band_face_limit) is not int or not 1 <= band_face_limit <= 4096:
        raise ValueError("bounded shared-facet work required")
    if len(edge) != 4 or edge[1] not in (0, 1) or tuple(cells) != adjacent_cells(edge):
        raise ValueError("canonical interface edge and its two cells required")
    validate_mask(mask)
    anchor, rows = np.asarray(anchor, dtype=float), np.asarray(rows, dtype=float)
    count = len(priors.triangles)
    if (
        anchor.shape != (2,)
        or not np.isfinite(anchor).all()
        or rows.shape != (2 * count, 6)
        or not np.isfinite(rows).all()
        or not isinstance(source_version, str)
        or not source_version
    ):
        raise ValueError(
            "explicit finite frame, two rows per root face and source version required"
        )
    primary = np.c_[np.arange(count), count + np.arange(count)]
    if not np.array_equal(rows[primary, 0], np.tile(np.arange(count)[:, None], (1, 2))):
        raise ValueError("two canonical primary rows per root face required")
    lines = tuple(np.array(line, dtype=float, copy=True) for line in polylines)
    faces, polygons, candidates = contact_faces(
        priors, mask, lines, face_limit=band_face_limit
    )
    missing = mask.difference(shapely.union_all(polygons)).area
    if missing > max(1e-8, mask.area * 1e-12):
        raise ValueError("coarse pair does not cover the complete water mask")
    fit = fit_physical_surface(
        priors, anchor, rows, up_bounds_m=bounds, primary_rows=primary
    )
    if fit.mesh is None:
        return None, dict(fit.report)
    audit = physical_audit(fit.mesh, anchor, rows, bounds)
    if not audit["admitted"]:
        return None, dict(
            fit.report, reason="coarse_pair_witness_not_admitted", physical_audit=audit
        )
    domain = subset(fit.mesh, rows, faces)
    frame, constraints = anchor.copy(), rows.copy()
    for array in (frame, constraints, *lines):
        array.setflags(write=False)
    context = input_context(
        edge, cells, priors, frame, constraints, bounds, mask, lines, source_version
    )
    version = solved_version(context, fit.mesh)
    common_owner = prepare_profile_owner(
        fit.mesh, anchor=frame, mask=mask, owner_version=version
    )
    profile = restrict_profile(common_owner, lines)
    vertex_ids = np.arange(len(fit.mesh.vertices), dtype=np.uint32)
    face_ids = np.arange(count)
    for array in (vertex_ids, face_ids, primary):
        array.setflags(write=False)
    whole = Subdomain(fit.mesh, vertex_ids, face_ids, constraints, primary)
    report = dict(
        fit.report,
        physical_audit=audit,
        coarse_faces=count,
        shared_faces=len(faces),
        shared_vertices=len(domain.vertex_ids),
        coarse_geometry_bytes=fit.mesh.vertices.nbytes + fit.mesh.triangles.nbytes,
        interface_candidates=candidates,
        context=context,
        version=version,
        geometry_coverage_residual_m2=missing,
        shared_numeric_bytes=domain.mesh.vertices.nbytes
        + domain.mesh.triangles.nbytes
        + domain.face_ids.nbytes
        + domain.vertex_ids.nbytes
        + profile.report["numeric_payload_bytes"],
        scope="bounded coarse witness before local objectives; no other-interface/corner, global hydrology or native admission",
    )
    return InterfaceHeight(
        tuple(edge),
        tuple(cells),
        priors,
        fit.mesh,
        frame,
        constraints,
        tuple(bounds),
        mask,
        lines,
        whole,
        common_owner,
        domain,
        profile,
        source_version,
        context,
        version,
        report,
    ), report


def validate_interface(owner, expected_source_version):
    if owner.source_version != expected_source_version:
        raise ValueError("stale interface source version")
    if not np.array_equal(
        owner.witness.triangles, owner.priors.triangles
    ) or not np.array_equal(
        owner.witness.vertices[:, :2], owner.priors.vertices[:, :2]
    ):
        raise ValueError("coarse witness topology/frame coordinates differ")
    context = input_context(
        owner.edge,
        owner.cells,
        owner.priors,
        owner.anchor,
        owner.rows,
        owner.bounds,
        owner.mask,
        owner.polylines,
        owner.source_version,
    )
    if (
        context != owner.context
        or solved_version(context, owner.witness) != owner.version
    ):
        raise ValueError("interface frame/constraints/witness context mismatch")
    band = owner.band
    if not np.array_equal(
        band.mesh.vertices, owner.witness.vertices[band.vertex_ids]
    ) or not np.array_equal(
        band.vertex_ids[band.mesh.triangles], owner.witness.triangles[band.face_ids]
    ):
        raise ValueError("shared facets differ from the coarse witness")


def interface_region_domain(owner, mask, *, face_limit=4096):
    candidates = np.sort(owner.common.tree.query(mask))
    areas = shapely.area(shapely.intersection(owner.common.polygons[candidates], mask))
    own = candidates[areas > 0]
    faces = np.union1d(own, owner.band.face_ids)
    if not len(own) or len(faces) > face_limit:
        raise ValueError("regional coarse-facet budget exhausted")
    return subset(owner.priors, owner.rows, faces), len(candidates), len(own)


def fit_interface_region(
    owner, mask, *, cell, expected_source_version, prior_offset_m=0, face_limit=4096
):
    """An independent local objective inherits the same complete shared facets.

    Local profile context remains local. Its payload is compared with the common
    interface only after the complete shared field is verified; no context guard
    is bypassed. Publication copies already fixed owner variables, then audits.
    """
    validate_interface(owner, expected_source_version)
    validate_mask(mask)
    if (
        cell not in owner.cells
        or not np.isfinite(prior_offset_m)
        or type(face_limit) is not int
        or not 1 <= face_limit <= 16384
    ):
        raise ValueError(
            "declared interface cell, finite prior and bounded regional faces required"
        )
    if mask.difference(owner.mask).area > max(1e-8, mask.area * 1e-12):
        raise ValueError("regional mask extends beyond the pair water footprint")
    domain, candidates, own = interface_region_domain(
        owner, mask, face_limit=face_limit
    )
    local_ids = np.searchsorted(domain.vertex_ids, owner.band.vertex_ids)
    witness = Mesh(owner.witness.vertices[domain.vertex_ids], domain.mesh.triangles)
    witness_audit = physical_audit(witness, owner.anchor, domain.rows, owner.bounds)
    if not witness_audit["admitted"]:
        return None, {
            "reason": "restricted_feasibility_witness_not_admitted",
            "audit": witness_audit,
        }
    target = domain.mesh.vertices.copy()
    target[:, 2] += prior_offset_m
    fixed = dict(
        zip(local_ids.tolist(), owner.band.mesh.vertices[:, 2].tolist(), strict=True)
    )
    fit = fit_physical_surface(
        Mesh(target, domain.mesh.triangles),
        owner.anchor,
        domain.rows,
        up_bounds_m=owner.bounds,
        primary_rows=domain.primary,
        fixed=fixed,
    )
    if fit.mesh is None:
        return None, dict(fit.report)
    vertices = fit.mesh.vertices.copy()
    residual = float(abs(vertices[local_ids] - owner.band.mesh.vertices).max())
    vertices[local_ids] = owner.band.mesh.vertices
    mesh = Mesh(vertices, fit.mesh.triangles)
    audit = physical_audit(mesh, owner.anchor, domain.rows, owner.bounds)
    if not audit["admitted"]:
        return None, dict(
            fit.report,
            reason="published_local_field_not_admitted",
            physical_audit=audit,
        )
    version = owner.version + ":" + str(cell) + ":" + float(prior_offset_m).hex()
    field = prepare_profile_owner(
        mesh, anchor=owner.anchor, mask=mask, owner_version=version
    )
    profile = restrict_profile(field, owner.polylines)
    if not np.array_equal(profile.points, owner.profile.points) or not np.array_equal(
        profile.segments, owner.profile.segments
    ):
        return None, dict(
            fit.report, reason="local_profile_does_not_reproduce_interface"
        )
    report = dict(
        fit.report,
        physical_audit=audit,
        feasibility_witness_audit=witness_audit,
        root_face_ids=domain.face_ids.tolist(),
        fixed_vertices=len(fixed),
        free_vertices=len(domain.vertex_ids) - len(fixed),
        owner_variable_publication_residual_m=residual,
        shared_interface_version=owner.version,
        local_context=field.context,
        canonical_profile_context=owner.profile.context,
        local_profile_context=profile.context,
        profile_payload_matches_interface=True,
        own_facet_candidates=candidates,
        own_water_facets=own,
        prior_offset_m=prior_offset_m,
    )
    return InterfaceRegion(field, domain, profile, owner.version, report), report


def restrict_interface_region(owner, mask, *, cell, expected_source_version):
    """Reuse the admitted common field; local detail never starts another QP.

    The coarse mesh is shared immutable data. Only the precise eligibility mask
    and local profile context differ. Expensive local fit variants remain an
    explicitly separate experiment; their refusal cannot poison this product.
    """
    validate_interface(owner, expected_source_version)
    validate_mask(mask)
    if cell not in owner.cells:
        raise ValueError("declared interface cell required")
    if mask.difference(owner.mask).area > max(1e-8, mask.area * 1e-12):
        raise ValueError("regional mask extends beyond the pair water footprint")
    domain = owner.whole
    audit = physical_audit(owner.witness, owner.anchor, owner.rows, owner.bounds)
    if not audit["admitted"]:
        return None, {"reason": "common_field_not_admitted", "physical_audit": audit}
    field = masked_profile_owner(
        owner.common,
        mask=mask,
        owner_version=owner.version + ":masked-cell:" + str(cell),
    )
    profile = restrict_profile(field, owner.polylines)
    if not np.array_equal(profile.points, owner.profile.points) or not np.array_equal(
        profile.segments, owner.profile.segments
    ):
        return None, {"reason": "masked_common_profile_differs"}
    report = {
        "method": "shared_coarse_field_with_precise_local_mask",
        "physical_audit": audit,
        "local_physical_solves": 0,
        "shared_coarse_faces": len(owner.witness.triangles),
        "shared_geometry_bytes": owner.report["coarse_geometry_bytes"],
        "coarse_mesh_object_shared": field.mesh is owner.witness,
        "coarse_sampler_object_shared": field.sampler is owner.common.sampler,
        "coarse_spatial_index_shared": field.tree is owner.common.tree,
        "shared_interface_version": owner.version,
        "local_context": field.context,
        "canonical_profile_context": owner.profile.context,
        "local_profile_context": profile.context,
        "profile_payload_matches_interface": True,
        "qualified_interface": owner.edge,
        "canonical_cell_owner_proved": False,
    }
    return InterfaceRegion(field, domain, profile, owner.version, report), report
