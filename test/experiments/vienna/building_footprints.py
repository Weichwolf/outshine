"""Recover individual building footprints from grouped vector tile features."""

import shapely


def building_footprints(geometries):
    parts, owners = shapely.get_parts(geometries, return_index=True)
    invalid = ~shapely.is_valid(parts)
    if invalid.any():
        parts[invalid] = shapely.make_valid(parts[invalid])
        parts, parent = shapely.get_parts(parts, return_index=True)
        parts, descendant = shapely.get_parts(parts, return_index=True)
        owners = owners[parent[descendant]]
    polygon = (shapely.get_type_id(parts) == 3) & ~shapely.is_empty(parts)
    return parts[polygon], owners[polygon]
