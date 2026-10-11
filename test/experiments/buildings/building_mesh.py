"""Closed prism geometry, window panes and exact sections of constructive plans."""

import numpy as np
import shapely

from building_layout import polygons, rectangle
from building_plan import CEILING_M, WALL_M, wall_bands


def solid(storage, geometry, low, high, material):
    vertices, faces, materials, parts = storage
    for polygon in polygons(geometry):
        polygon = shapely.orient_polygons(polygon)
        part = parts[-1]+1 if parts else 0
        for ring in [polygon.exterior]+list(polygon.interiors):
            for a, b in zip(ring.coords, list(ring.coords)[1:]):
                first = len(vertices)
                vertices.extend([[*a, low], [*b, low], [*b, high], [*a, high]])
                faces.extend([(first, first+1, first+2), (first, first+2, first+3)])
                materials.extend([material]*2)
                parts.extend([part]*2)
        for triangle in shapely.get_parts(shapely.constrained_delaunay_triangles(polygon)):
            xy = np.array(triangle.exterior.coords)[:3]
            a, b = xy[1]-xy[0], xy[2]-xy[0]
            if a[0]*b[1]-a[1]*b[0] < 0:
                xy = xy[::-1]
            for height, points in ((high, xy), (low, xy[::-1])):
                first = len(vertices)
                vertices.extend(np.c_[points, np.full(3, height)].tolist())
                faces.append((first, first+1, first+2))
                materials.append(material)
                parts.append(part)


def mesh(plan):
    storage = [], [], [], []
    solid(storage, plan['footprint'], plan['base_z_m'], plan['floor_z_m'], 0)
    solid(storage, plan['entrance']['landing'].difference(plan['footprint']), plan['base_z_m'], plan['floor_z_m'], 4)
    for polygon, low, high in wall_bands(plan):
        solid(storage, polygon, low, high, 1)
    solid(storage, plan['footprint'], plan['ceiling_z_m'], plan['ceiling_z_m']+CEILING_M, 2)
    panes(storage,plan)
    return dict(vertices=np.asarray(storage[0], dtype=np.float64), faces=np.asarray(storage[1], dtype=np.int32),
                materials=np.asarray(storage[2], dtype=np.uint8), parts=np.asarray(storage[3], dtype=np.int32))


def panes(storage, plan):
    for window in plan['windows']:
        tangent = np.array(window['tangent'])
        normal = np.array([tangent[1], -tangent[0]])
        centre = np.array(window['centre'])-normal*WALL_M/2
        pane = rectangle(centre, tangent, 1.20, .006, .006)
        solid(storage, pane, plan['floor_z_m']+window['sill_m'], plan['floor_z_m']+window['head_m'], 3)


def slice_triangles(product, normal, offset, materials=None):
    normal = np.asarray(normal)
    result = []
    for triangle, material in zip(product['vertices'][product['faces']], product['materials']):
        if materials is not None and material not in materials:
            continue
        distance = triangle@normal-offset
        if np.all(distance >= 0) or np.all(distance <= 0):
            continue
        points = []
        for index in range(3):
            next_index = (index+1)%3
            a, b = distance[index], distance[next_index]
            if (a <= 0 <= b or b <= 0 <= a) and a != b:
                points.append(triangle[index]+(triangle[next_index]-triangle[index])*(-a)/(b-a))
        unique = np.unique(np.round(points, 9), axis=0)
        if len(unique) == 2:
            result.append((unique, int(material)))
    return result
