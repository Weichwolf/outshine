"""Small, bounded solar and balcony plans; complete coarse geometry is cached on a miss."""

from dataclasses import replace

import numpy as np
import shapely

from shell import DTYPES, FIELDS


def describe(product):
    layers, features = product['layers'], []
    if product['massing']['mode'] == 'plain':
        return features
    top = layers[-1]
    inset = top['footprint'].buffer(-.7, join_style='mitre')
    if not inset.is_empty and inset.area > 8:
        x0, y0, x1, y1 = inset.bounds
        patch = inset.intersection(shapely.box(x0, y0, x0+(x1-x0)*.74, y1))
        features.append(dict(kind='roof-pv', height_m=top['top_m']+.025, wkt=patch.wkt))
    facades = [(i, f) for i, f in enumerate(product['facades']) if f['layer'] == 0 and f['repeat'].count
               and f['repeat'].width_m >= 6]
    if not facades:
        return features
    index, facade = max(facades, key=lambda pair: pair[1]['repeat'].width_m)
    repeat = facade['repeat']
    pw, ph = repeat.width_m/repeat.columns, repeat.height_m/repeat.rows
    if product['style'] == 'midcentury':
        features.append(dict(kind='canopy', facade=index))
    if (product['style'] != 'midcentury' and ph >= 2.4 and product['polygon'].area >= 50
            and not product['polygon'].buffer(-1.2, join_style='mitre').is_empty):
        column = min(repeat.columns-1, repeat.columns//2)
        for row in range(1, min(repeat.rows, 4)):
            floor = facade['bottom_m']+(row+.04)*ph
            features.append(dict(kind='balcony', facade=index, u_m=(column+.5)*pw,
                                 width_m=min(2.8, pw*.8), depth_m=1.2, floor_m=floor))
        if any(f['kind'] == 'balcony' for f in features):
            facade['repeat'] = replace(repeat, opening_height=.92)
    for other_index, other in sorted(facades, key=lambda pair: pair[1]['repeat'].width_m, reverse=True)[:2]:
        rhythm = other['repeat']
        floor = rhythm.height_m/rhythm.rows
        sill = (1-rhythm.opening_height)*floor*.5
        if sill < .35:
            continue
        for row in range(min(rhythm.rows, 3)):
            features.append(dict(kind='facade-pv', facade=other_index,
                                 rectangle=[.2, row*floor+.07, rhythm.width_m-.2, row*floor+sill-.04]))
    return features


def add_quad(storage, points, material, uv=None):
    vertices, faces, coordinates, materials, parts = storage
    first = len(vertices)
    vertices.extend(points)
    coordinates.extend([(0., 0.)]*4 if uv is None else uv)
    faces.extend([(first, first+1, first+2), (first, first+2, first+3)])
    materials.extend([material]*2)


def box(storage, facade, low, high, material):
    a, b = np.asarray(low), np.asarray(high)
    corners = np.array([[a[0], a[1], a[2]], [b[0], a[1], a[2]], [b[0], b[1], a[2]], [a[0], b[1], a[2]],
                        [a[0], a[1], b[2]], [b[0], a[1], b[2]], [b[0], b[1], b[2]], [a[0], b[1], b[2]]])
    tangent = (facade['b']-facade['a'])/facade['repeat'].width_m
    normal = facade['normal'][:2]
    world = np.c_[facade['a']+corners[:, :1]*tangent+corners[:, 1:2]*normal, corners[:, 2]]
    for indices in ((0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)):
        order = list(indices)[::-1]
        axes = np.flatnonzero(np.ptp(corners[order], axis=0) > 1e-10)
        uv = corners[order][:, axes]/[2., 1.]
        add_quad(storage, world[order], material, uv)


def prepare(product):
    storage, bounds = ([], [], [], [], []), []
    for index, feature in enumerate(product['features']):
        first_vertex, first_face = len(storage[0]), len(storage[1])
        if feature['kind'] == 'roof-pv':
            triangles = shapely.constrained_delaunay_triangles(shapely.from_wkt(feature['wkt']))
            for triangle in shapely.get_parts(triangles):
                points = np.asarray(triangle.exterior.coords)[:3]
                a, b = points[1]-points[0], points[2]-points[0]
                if a[0]*b[1]-a[1]*b[0] < 0:
                    points = points[::-1]
                start = len(storage[0])
                storage[0].extend(np.c_[points, np.full(3, feature['height_m'])])
                storage[1].append((start, start+1, start+2))
                storage[2].extend(points/[2., 1.])
                storage[3].append(5)
        else:
            facade = product['facades'][feature['facade']]
            if feature['kind'] == 'facade-pv':
                x0, y0, x1, y1 = feature['rectangle']
                tangent = (facade['b']-facade['a'])/facade['repeat'].width_m
                local = np.array([[x0, y0], [x1, y0], [x1, y1], [x0, y1]])
                points = np.c_[facade['a']+local[:, :1]*tangent, facade['bottom_m']+local[:, 1]]
                points += facade['normal']*.015
                add_quad(storage, points, 5, local/[2., 1.])
            elif feature['kind'] == 'canopy':
                box(storage, facade, [0., 0., facade['top_m']-.18],
                    [facade['repeat'].width_m, .75, facade['top_m']-.03], 4)
            else:
                centre, width, depth, floor = (feature[k] for k in ('u_m', 'width_m', 'depth_m', 'floor_m'))
                x0, x1 = centre-width*.5, centre+width*.5
                box(storage, facade, [x0, 0., floor-.12], [x1, depth, floor], 0)
                box(storage, facade, [x0, depth-.06, floor], [x1, depth, floor+.95], 5)
                for low, high in (([x0, 0., floor], [x0+.05, depth, floor+.95]),
                                  ([x1-.05, 0., floor], [x1, depth, floor+.95])):
                    box(storage, facade, low, high, 2)
        points = np.asarray(storage[0][first_vertex:])
        storage[4].extend([index]*(len(storage[1])-first_face))
        bounds.append(np.r_[points.min(axis=0), points.max(axis=0)].tolist())
    arrays = {key: np.asarray(value, dtype=dtype) for key, value, dtype in zip(FIELDS, storage, DTYPES)}
    arrays['vertices'] = arrays['vertices'].reshape(-1, 3)
    arrays['faces'] = arrays['faces'].reshape(-1, 3)
    arrays['uv'] = arrays['uv'].reshape(-1, 2)
    return arrays, bounds


def append_visible(storage, product, view):
    cached = product['decoration']
    for index, bounds in enumerate(product['feature_bounds']):
        low, high = np.asarray(bounds[:3]), np.asarray(bounds[3:])
        corners = np.array([[x, y, z] for x in (low[0], high[0]) for y in (low[1], high[1]) for z in (low[2], high[2])])
        projected = (corners-view.eye) @ view.basis.T
        distances = projected @ view.planes[:, :3].T+view.planes[:, 3]
        distances += view.movement_m*np.linalg.norm(view.planes[:, :3], axis=1)
        if np.any(distances.max(axis=0) < 0):
            continue
        selected = np.flatnonzero(cached['parts'] == index)
        points = cached['vertices'][cached['faces'][selected]]
        normals = np.cross(points[:, 1]-points[:, 0], points[:, 2]-points[:, 0])
        facing = np.einsum('ij,ij->i', view.eye-points.mean(axis=1), normals)
        selected = selected[facing+view.movement_m*np.linalg.norm(normals, axis=1) > 0]
        if not len(selected):
            continue
        vertices, faces, uv, materials = storage
        indices, inverse = np.unique(cached['faces'][selected], return_inverse=True)
        first = len(vertices)
        vertices.extend(cached['vertices'][indices])
        uv.extend(cached['uv'][indices])
        faces.extend(inverse.reshape(-1, 3)+first)
        materials.extend(cached['materials'][selected])
