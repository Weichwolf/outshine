"""Select whole source contours at first-hit samples, before triangulation.

Sampling can miss subpixel faces. This experiment supplies no visibility or SSE
guarantee and must not replace a parent product on that basis.
"""

from dataclasses import dataclass

import numpy as np
import shapely
from shapely.affinity import affine_transform


@dataclass
class ContourProjection:
    tree: object
    identities: np.ndarray
    normals: np.ndarray
    offsets: np.ndarray
    denominators: np.ndarray
    centre: np.ndarray
    screen: np.ndarray
    collapsed: np.ndarray


def project_contours(patches, centre, toward, right, up):
    centre, toward, right, up = np.asarray([centre, toward, right, up], float)
    frame = np.stack((right, up, toward))
    if (
        not np.isfinite(np.r_[centre, frame.ravel()]).all()
        or not np.allclose(frame @ frame.T, np.eye(3), rtol=0, atol=1e-12)
        or not 0 < len(patches) <= 16384
    ):
        raise ValueError("finite orthonormal view and bounded contours required")
    screen = frame[:2]
    polygons, identities, normals, offsets, denominators, collapsed = (
        [],
        [],
        [],
        [],
        [],
        [],
    )
    for identity, patch in enumerate(patches):
        denominator = float(patch.normal @ toward)
        if denominator <= 1e-12:
            continue
        transform = patch.axes @ screen.T
        shift = (patch.origin - centre) @ screen.T
        polygon = affine_transform(
            patch.domain,
            [*transform[:, 0], *transform[:, 1], *shift],
        )
        if polygon.is_empty or polygon.area == 0:
            collapsed.append(identity)
            continue
        if not polygon.is_valid or polygon.area < 0:
            raise ValueError(
                f"invalid projected contour {identity}: "
                f"{shapely.is_valid_reason(polygon)}, area={polygon.area}, "
                f"normal/view={denominator}"
            )
        polygons.append(polygon)
        identities.append(identity)
        normals.append(patch.normal)
        offsets.append(float((patch.origin - centre) @ patch.normal))
        denominators.append(denominator)
    return ContourProjection(
        shapely.STRtree(polygons),
        np.asarray(identities, np.int32),
        np.asarray(normals, float).reshape(-1, 3),
        np.asarray(offsets, float),
        np.asarray(denominators, float),
        centre,
        screen,
        np.asarray(collapsed, np.int32),
    )


def select_contours(projection, pixels, half_extent, *, maximum_pairs=2000000):
    if (
        type(pixels) is not int
        or not 1 <= pixels <= 256
        or not np.isfinite(half_extent)
        or half_extent <= 0
        or type(maximum_pairs) is not int
        or maximum_pairs < 0
    ):
        raise ValueError("bounded raster, positive extent and pair budget required")
    samples = (np.arange(pixels) + 0.5) * (2 * half_extent / pixels) - half_extent
    owners = np.full((pixels, pixels), -1, np.int32)
    tested = 0
    for y, vertical in enumerate(-samples):
        points = np.column_stack((samples, np.full(pixels, vertical)))
        rays, contours = projection.tree.query(
            shapely.points(points), predicate="covered_by"
        )
        tested += len(contours)
        if tested > maximum_pairs:
            raise ValueError("contour/ray pair budget exhausted")
        if not len(contours):
            continue
        positions = points[rays] @ projection.screen
        depth = (
            projection.offsets[contours]
            - np.einsum("ij,ij->i", positions, projection.normals[contours])
        ) / projection.denominators[contours]
        nearest = np.full(pixels, -np.inf)
        np.maximum.at(nearest, rays, depth)
        hit = depth == nearest[rays]
        first = np.full(pixels, np.iinfo(np.int32).max, np.int32)
        np.minimum.at(first, rays[hit], projection.identities[contours[hit]])
        owners[y] = np.where(first == np.iinfo(np.int32).max, -1, first)
    return np.unique(owners[owners >= 0]), owners, {"ray_contour_pairs": tested}
