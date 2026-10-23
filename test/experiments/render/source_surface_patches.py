"""Planar original-form patches without native Shell triangulation/attributes."""

from dataclasses import dataclass
from itertools import pairwise

import numpy as np
import shapely
from column_roof_patches import bottom_m, roof_patches, tangent_basis


@dataclass
class SurfacePatch:
    domain: object
    origin: np.ndarray
    axes: np.ndarray
    normal: np.ndarray
    source: int


def wall_domains(shape, roofs, bottom):
    """Partition original contour edges by the supplied planar roof domains."""
    result = []
    for ring, hole in [(shape["ring"], False), *[(r, True) for r in shape["holes"]]]:
        ring = np.asarray(ring, float)
        following = np.roll(ring, -1, axis=0)
        signed = np.sum(ring[:, 0] * following[:, 1] - ring[:, 1] * following[:, 0])
        orientation = 1 if (signed >= 0) != hole else -1
        for a, b in zip(ring, np.roll(ring, -1, axis=0), strict=True):
            edge = b - a
            length = np.linalg.norm(edge)
            if length == 0:
                raise ValueError("zero-length original wall edge")
            direction = edge / length
            intervals = []
            tolerance = 64 * np.finfo(float).eps * max(1, np.max(np.abs(ring)))
            for domain, plane in roofs:
                for part in shapely.get_parts(domain):
                    for boundary in [part.exterior, *part.interiors]:
                        points = shapely.get_coordinates(boundary)
                        distance = (points[:, 0] - a[0]) * direction[1] - (
                            points[:, 1] - a[1]
                        ) * direction[0]
                        along = (points - a) @ direction
                        for at in range(len(points) - 1):
                            if (
                                max(abs(distance[at]), abs(distance[at + 1]))
                                > tolerance
                            ):
                                continue
                            first, last = sorted(along[at : at + 2])
                            first, last = max(first, 0), min(last, length)
                            if first < tolerance:
                                first = 0.0
                            if length - last < tolerance:
                                last = length
                            if last > first:
                                intervals.append((first, last, plane))
            cuts = np.unique(
                [0, length, *[v for low, high, _ in intervals for v in [low, high]]]
            )
            for low, high in pairwise(cuts):
                if high <= low:
                    continue
                middle = (low + high) / 2
                candidates = [
                    p
                    for first, last, p in intervals
                    if first - tolerance <= middle <= last + tolerance
                ]
                if not candidates:
                    raise ValueError("roof domains leave an unsupported wall interval")
                plane = candidates[0]
                endpoints = a + np.array([low, high])[:, None] * direction
                top = endpoints @ plane[:2] + plane[2]
                if np.min(top) < bottom:
                    raise ValueError("roof below original wall bottom")
                polygon = shapely.Polygon(
                    [(low, bottom), (high, bottom), (high, top[1]), (low, top[0])]
                )
                if polygon.area > 0:
                    result.append((polygon, a, direction, orientation))
    return result


def shape_patches(surface, shape, identity, basis, anchor):
    roofs = roof_patches(shape)
    axes = np.array([surface[k] for k in ["east", "north", "up"]]) @ basis.T
    origin = np.array(surface["origin"]) @ basis.T - anchor
    patches = []
    for domain, plane in roofs:
        directions = axes[:2] + plane[:2, None] * axes[2]
        normal = np.cross(*directions)
        normal /= np.linalg.norm(normal)
        patches.append(
            SurfacePatch(
                domain, origin + plane[2] * axes[2], directions, normal, identity
            )
        )
    for domain, a, direction, orientation in wall_domains(
        shape, roofs, bottom_m(surface, shape)
    ):
        directions = np.stack((direction @ axes[:2], axes[2]))
        normal = orientation * np.cross(*directions)
        normal /= np.linalg.norm(normal)
        patches.append(
            SurfacePatch(domain, origin + a @ axes[:2], directions, normal, identity)
        )
    return patches


def original_patches(parent, cells):
    basis = tangent_basis(parent.plans)
    records = [
        (int(i), s)
        for _, ids, surfaces in parent.iter_forms(cells)
        for i, s in zip(ids, surfaces, strict=True)
    ]
    supported = [s for _, s in records if s is not None]
    if not supported:
        raise ValueError("known source frames required")
    anchor = np.mean([s["origin"] for s in supported], axis=0) @ basis.T
    patches, needs, accepted = [], [], []
    for identity, surface in records:
        try:
            if surface is None or not surface["shapes"]:
                raise ValueError("missing source forms")
            parts = [
                p
                for shape in surface["shapes"]
                for p in shape_patches(surface, shape, identity, basis, anchor)
            ]
        except (
            ValueError,
            np.linalg.LinAlgError,
            shapely.errors.GEOSException,
        ) as error:
            needs.append({"source": identity, "reason": str(error)})
            continue
        patches.extend(parts)
        accepted.append(identity)
    return patches, basis, anchor, np.asarray(accepted, np.uint32), needs
