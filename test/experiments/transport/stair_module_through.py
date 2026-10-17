"""Whole traces from positive-area facets on both sides of a through profile."""

import numpy as np
import shapely


def through_sides(caps, line):
    centre = (line[0] + line[-1]) / 2
    axis = (line[-1] - line[0]) / np.linalg.norm(line[-1] - line[0])
    normal = np.array([-axis[1], axis[0]])
    reach = 1 + max(
        float(np.linalg.norm(vertices[:, :2] - centre, axis=1).max())
        for vertices, _faces, _domain in caps
    )
    result = []
    for side in (-1, 1):
        corners = np.array([[-1, 0], [1, 0], [1, side], [-1, side]])
        half = shapely.Polygon(centre + reach * (corners @ np.array([axis, normal])))
        group = []
        for vertices, faces, domain in caps:
            clipped = half if domain is None else half.intersection(domain)
            parts = shapely.intersection(shapely.polygons(vertices[faces, :2]), clipped)
            coordinates, parents = shapely.get_coordinates(parts, return_index=True)
            extent = np.zeros(len(faces))
            np.maximum.at(extent, parents, side * ((coordinates - centre) @ normal))
            # Line-only contact and sub-tolerance slivers do not prove a side.
            selected = (shapely.area(parts) > 0) & (extent > 1e-7)
            if selected.any():
                group.append((vertices, faces[selected], clipped))
        if not group:
            raise ValueError("through profile lacks positive-area facets on both sides")
        result.append(group)
    return result
