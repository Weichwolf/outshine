"""Packed complete recipe domains, before optional triangulation or native PBR.

This is an offline experiment buffer, not a cache/GPU contract. Float64 controls,
holes, source/recipe-part/side identity and original recipes remain necessary.
Native triangulation/weld approximations and World admission stay separate.
"""

import numpy as np
import shapely
from source_material_patches import MaterialPatch
from source_surface_patches import SurfacePatch


def pack(groups, basis, *, dictionary=False):
    """Groups contain (actual prepared-source ordinal, surface, complete patches)."""
    if not 1 <= len(groups) <= 65536:
        raise ValueError("Bounded nonempty source groups required")
    source_ids, frames, info, local = [], [], [], []
    source_offsets, patch_offsets, polygon_offsets, ring_offsets = [0], [0], [0], [0]
    coordinates = []
    for identity, surface, patches in groups:
        if identity in source_ids or not patches:
            raise ValueError(
                "Unique source identity and complete nonempty group required"
            )
        source_index = len(source_ids)
        source_ids.append(identity)
        frames.append(
            np.array([surface[k] for k in ("origin", "east", "north", "up")]) @ basis.T
        )
        for patch in patches:
            if patch.geometry.source != identity:
                raise ValueError("Patch must belong to its actual source")
            domain = patch.geometry.domain
            if not domain.is_valid or domain.is_empty or domain.area <= 0:
                raise ValueError("Complete valid polygon domains required")
            info.append(
                [source_index, patch.part, patch.side, patch.gable, patch.orientation]
            )
            local.append(np.vstack((patch.local_origin, patch.local_axes)))
            for polygon in shapely.get_parts(domain):
                if polygon.geom_type != "Polygon":
                    raise ValueError("Only polygon domains are supported")
                for ring in (polygon.exterior, *polygon.interiors):
                    points = np.asarray(ring.coords, dtype="<f8")
                    if len(points) < 4 or not np.array_equal(points[0], points[-1]):
                        raise ValueError("Closed nonempty domain ring required")
                    coordinates.extend(points[:-1])
                    ring_offsets.append(len(coordinates))
                polygon_offsets.append(len(ring_offsets) - 1)
            patch_offsets.append(len(polygon_offsets) - 1)
        source_offsets.append(len(info))
    if len(coordinates) > 1048576 or len(info) > 65536:
        raise ValueError("Bounded domain controls required")
    arrays = {
        "source_ids": np.asarray(source_ids, dtype="<i4"),
        "source_frames": np.asarray(frames, dtype="<f8"),
        "source_patch_offsets": np.asarray(source_offsets, dtype="<u4"),
        "patch_info": np.asarray(info, dtype="<i4"),
        "local_frames": np.asarray(local, dtype="<f8"),
        "patch_polygon_offsets": np.asarray(patch_offsets, dtype="<u4"),
        "polygon_ring_offsets": np.asarray(polygon_offsets, dtype="<u4"),
        "ring_vertex_offsets": np.asarray(ring_offsets, dtype="<u4"),
        "coordinates": np.asarray(coordinates, dtype="<f8"),
    }
    if dictionary:
        words, inverse = np.unique(
            arrays["coordinates"].view("<u8"), axis=0, return_inverse=True
        )
        arrays["coordinates"] = words.view("<f8")
        arrays["vertex_ids"] = inverse.astype("<u4")
    return arrays


def domain_at(arrays, index):
    start, stop = arrays["patch_polygon_offsets"][index : index + 2]
    polygons = []
    for polygon in range(int(start), int(stop)):
        first, last = arrays["polygon_ring_offsets"][polygon : polygon + 2]
        rings = []
        for ring in range(int(first), int(last)):
            low, high = arrays["ring_vertex_offsets"][ring : ring + 2]
            vertices = (
                arrays["vertex_ids"][low:high]
                if "vertex_ids" in arrays
                else slice(low, high)
            )
            rings.append(arrays["coordinates"][vertices])
        polygons.append(shapely.Polygon(rings[0], rings[1:]))
    return polygons[0] if len(polygons) == 1 else shapely.MultiPolygon(polygons)


def restore_source(arrays, identity, surface):
    """Decode only this source's cached domains; do not run source generators.

    The caller supplies its retained decoded recipe for raw material evaluation.
    No triangulation, normal/PBR evaluation or other source's domain is needed.
    """
    matches = np.flatnonzero(arrays["source_ids"] == identity)
    if len(matches) != 1:
        raise ValueError("Source requires a complete packed group or whole fallback")
    source = int(matches[0])
    origin, *axes = arrays["source_frames"][source]
    frame = np.asarray(axes)
    low, high = arrays["source_patch_offsets"][source : source + 2]
    result = []
    for index in range(int(low), int(high)):
        _, part, side, gable, orientation = map(int, arrays["patch_info"][index])
        local_origin, *local_axes = arrays["local_frames"][index]
        local_axes = np.asarray(local_axes)
        world_axes = local_axes @ frame
        normal = orientation * np.cross(*world_axes)
        normal /= np.linalg.norm(normal)
        geometry = SurfacePatch(
            domain_at(arrays, index),
            origin + local_origin @ frame,
            world_axes,
            normal,
            identity,
        )
        result.append(
            MaterialPatch(
                geometry,
                surface["shapes"][part],
                part,
                side,
                bool(gable),
                local_origin,
                local_axes,
                surface,
                orientation,
            )
        )
    return result
