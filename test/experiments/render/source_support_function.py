"""Source-only closed support for field construction, not a runtime renderer.

The sign represents the union of original contour/height volumes. Magnitudes
are Lipschitz support bounds, not exact Euclidean distances. CSG joins can have
interior zeros; no surface normal or rendering contract follows from this field.
"""

from dataclasses import dataclass

import numpy as np
import shapely
from column_roof_patches import bottom_m, roof_patches, tangent_basis


@dataclass
class SourceSupport:
    source_ids: np.ndarray
    origins: np.ndarray
    inverses: np.ndarray
    inverse_norms: np.ndarray
    offsets: np.ndarray
    domains: tuple
    planes: np.ndarray
    bottoms: np.ndarray
    bounds: np.ndarray


def source_domains(surface):
    rows = []
    for shape in surface["shapes"]:
        bottom = bottom_m(surface, shape)
        for domain, plane in roof_patches(shape):
            if domain.area == 0:
                continue
            vertices = shapely.get_coordinates(domain)
            if not domain.is_valid or np.min(vertices @ plane[:2] + plane[2]) < bottom:
                raise ValueError("valid positive-height roof domain required")
            if not np.isfinite(np.r_[plane, bottom]).all():
                raise ValueError("finite source heights required")
            rows.append((domain, plane, bottom))
    if not rows:
        raise ValueError("nonempty original source forms required")
    return rows


def compile_support(parent, cells):
    records = [
        (int(i), surface)
        for _, ids, surfaces in parent.iter_forms(cells)
        for i, surface in zip(ids, surfaces, strict=True)
    ]
    basis = tangent_basis(parent.plans)
    known = [surface for _, surface in records if surface is not None]
    if not known:
        raise ValueError("known original source frames required")
    anchor = np.mean([surface["origin"] for surface in known], axis=0) @ basis.T
    source_ids, origins, inverses, norms, offsets = [], [], [], [], [0]
    domains, planes, bottoms, needs = [], [], [], []
    for identity, surface in records:
        try:
            if surface is None:
                raise ValueError("missing original source forms")
            rows = source_domains(surface)
            frame = np.array([surface[k] for k in ("east", "north", "up")]) @ basis.T
            origin = np.array(surface["origin"]) @ basis.T - anchor
            inverse = np.linalg.inv(frame)
            if not np.isfinite(inverse).all() or not np.isfinite(origin).all():
                raise ValueError("finite invertible original source frame required")
        except (
            ValueError,
            np.linalg.LinAlgError,
            shapely.errors.GEOSException,
        ) as error:
            needs.append({"source": identity, "reason": str(error)})
            continue
        source_ids.append(identity)
        origins.append(origin)
        inverses.append(inverse)
        norms.append(np.linalg.norm(inverse, ord=2) * (1 + 64 * np.finfo(float).eps))
        for domain, plane, bottom in rows:
            domains.append(domain)
            planes.append(plane)
            bottoms.append(bottom)
        offsets.append(len(domains))
    if not domains:
        raise ValueError("no supported original volumes")
    support = SourceSupport(
        np.array(source_ids, np.uint32),
        np.array(origins),
        np.array(inverses),
        np.array(norms),
        np.array(offsets, np.uint32),
        tuple(domains),
        np.array(planes),
        np.array(bottoms),
        np.array([domain.bounds for domain in domains]),
    )
    return support, basis, anchor, needs


def box_distance(points, bounds):
    q = np.abs(points - (bounds[:2] + bounds[2:]) / 2) - (bounds[2:] - bounds[:2]) / 2
    return np.linalg.norm(np.maximum(q, 0), axis=1) + np.minimum(q.max(1), 0)


def sample_support(support, points, *, prune=True):
    points = np.asarray(points, float)
    if (
        points.ndim != 2
        or points.shape[1] != 3
        or not np.isfinite(points).all()
        or len(points) * len(support.domains) > 2**24
    ):
        raise ValueError("bounded finite 3D point/domain packet required")
    result = np.full(len(points), np.inf)
    work = {
        "source_point_transforms": len(points) * len(support.source_ids),
        "domain_point_bound_tests": len(points) * len(support.domains),
        "exact_contour_distance_queries": 0,
    }
    for source in range(len(support.source_ids)):
        local = (points - support.origins[source]) @ support.inverses[source]
        scale = support.inverse_norms[source]
        begin, end = support.offsets[source : source + 2]
        for domain_id in range(int(begin), int(end)):
            plane = support.planes[domain_id]
            roof = (local[:, 2] - local[:, :2] @ plane[:2] - plane[2]) / np.linalg.norm(
                np.r_[plane[:2], 1]
            )
            vertical = np.maximum(support.bottoms[domain_id] - local[:, 2], roof)
            lower = (
                np.maximum(
                    vertical, box_distance(local[:, :2], support.bounds[domain_id])
                )
                / scale
            )
            ids = np.flatnonzero(lower < result) if prune else np.arange(len(points))
            if not len(ids):
                continue
            domain = support.domains[domain_id]
            queries = shapely.points(local[ids, :2])
            horizontal = shapely.distance(domain.boundary, queries)
            horizontal = np.where(
                shapely.covers(domain, queries), -horizontal, horizontal
            )
            value = np.maximum(vertical[ids], horizontal) / scale
            result[ids] = np.minimum(result[ids], value)
            work["exact_contour_distance_queries"] += len(ids)
    return result, work


def support_ranges(support, bounds):
    """Lipschitz ranges for source boxes, not opaque/empty World certificates.

    In exact arithmetic, a centre value plus/minus the box radius encloses the
    function. Named unsupported sources remain separate needs. Float64 ranges
    are an experimental bound; no directed-rounding interval proof is claimed.
    """
    bounds = np.asarray(bounds, float)
    if (
        bounds.ndim != 3
        or bounds.shape[1:] != (2, 3)
        or not np.isfinite(bounds).all()
        or np.any(bounds[:, 0] > bounds[:, 1])
    ):
        raise ValueError("finite ordered 3D support boxes required")
    centre = bounds[:, 0] / 2 + bounds[:, 1] / 2
    radius = np.linalg.norm(bounds[:, 1] / 2 - bounds[:, 0] / 2, axis=1)
    values, work = sample_support(support, centre)
    return np.column_stack((values - radius, values + radius)), work
