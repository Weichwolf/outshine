"""Closed heightfield products; coarse mode preserves the 2D material programme."""

import numpy as np
import shapely
from outshine_ref.buildings.facade_frames import front
from outshine_ref.buildings.modelling.heightfield import heightfield_envelope
from outshine_ref.buildings.rooflight_panels import triangle_surface
from recipe_hall_operators import sealed


def triangles(region):
    polygon = shapely.from_wkb(region.footprint_wkb)
    points = np.concatenate(
        [front(shapely.orient_polygons(p)) for p in shapely.get_parts(polygon)]
    )
    points[:, :, 2] = region.low_m
    if region.low_m != region.high_m:
        xy = points[:, :, :2].reshape(-1, 2)
        high = shapely.distance(
            shapely.points(xy), shapely.from_wkb(region.high_edge_wkb)
        )
        low = shapely.distance(
            shapely.points(xy), shapely.from_wkb(region.low_edge_wkb)
        )
        uncertainty = 64 * np.finfo(float).eps * max(1.0, abs(xy).max())
        if np.any(np.minimum(high, low) > uncertainty) or np.any(
            abs(high - low) <= uncertainty
        ):
            raise ValueError(
                "Slope triangulation introduced an unowned or ambiguous vertex"
            )
        points[:, :, 2] = np.where(high < low, region.high_m, region.low_m).reshape(
            -1, 3
        )
    return points


def upper(crown, *, coarse=False):
    if coarse:
        groups = {}
        for r in crown.regions:
            groups.setdefault(r.material, []).append(shapely.from_wkb(r.footprint_wkb))
        chunks = []
        for material, regions in groups.items():
            points = np.concatenate(
                [
                    front(shapely.orient_polygons(p))
                    for p in shapely.get_parts(shapely.union_all(regions))
                ]
            )
            points[:, :, 2] = crown.top_m
            chunks.append((points, material))
    else:
        chunks = [(triangles(r), r.material) for r in crown.regions]
    mesh = {}
    for points, material in chunks:
        count = points.size // 3
        piece = triangle_surface(
            {
                "vertices": points.reshape(-1, 3),
                "faces": np.arange(count, dtype=np.uint32).reshape(-1, 3),
            },
            material,
        )
        piece.pop("facade_extent")
        for key, values in piece.items():
            if key != "faces":
                mesh.setdefault(key, []).append(values)
    result = {key: np.concatenate(values) for key, values in mesh.items()}
    result["faces"] = np.arange(len(result["vertices"]), dtype=np.uint32).reshape(-1, 3)
    return result


def envelope(crown, *, coarse=False):
    result = heightfield_envelope(
        upper(crown, coarse=coarse), base_m=0.0, wall_material=1, base_material=0
    )
    source = shapely.from_wkb(crown.footprint_wkb)
    if source.symmetric_difference(shapely.from_wkb(result.footprint_wkb)).area > 1e-8:
        raise ValueError("Crown envelope changed the source footprint")
    return result


def product(crown, *, coarse=False):
    return sealed(envelope(crown, coarse=coarse).mesh)
