"""Prepare complete coarse hulls once; select stored faces without triangulating on a hit."""

import numpy as np
import shapely

FIELDS = ('vertices', 'faces', 'uv', 'materials', 'parts')
DTYPES = (np.float64, np.int32, np.float32, np.uint8, np.int32)


def prepare(product):
    vertices, faces, uv, materials, parts = [], [], [], [], []
    for index, facade in enumerate(product['facades']):
        a, b, repeat = facade['a'], facade['b'], facade['repeat']
        top, bottom = facade['top_m'], facade['bottom_m']
        first = len(vertices)
        vertices.extend([[*a, bottom], [*b, bottom], [*b, top], [*a, top]])
        uv.extend([(0., 0.), (repeat.columns, 0.), (repeat.columns, repeat.rows), (0., repeat.rows)])
        faces.extend([(first, first+1, first+2), (first, first+2, first+3)])
        pattern = (repeat.opening_width, repeat.opening_height)
        materials.extend([6+product['opening_patterns'].index(pattern) if repeat.count else 0]*2)
        parts.extend([index]*2)
    layers, surfaces = product['layers'], []
    horizontal = [(layer['top_m'], True, layer['footprint'].difference(layers[i+1]['footprint'])
                   if i+1 < len(layers) else layer['footprint']) for i, layer in enumerate(layers)]
    horizontal.append((layers[0]['bottom_m'], False, layers[0]['footprint']))
    for index, (height, upward, polygon) in enumerate(horizontal):
        part = -index-1
        surfaces.append(dict(part=part, height_m=height, upward=upward, bounds=tuple(polygon.bounds)))
        caps = shapely.constrained_delaunay_triangles(polygon)
        for triangle in shapely.get_parts(caps):
            points = np.array(triangle.exterior.coords)[:3]
            a, b = points[1]-points[0], points[2]-points[0]
            if (a[0]*b[1]-a[1]*b[0] > 0) != upward:
                points = points[::-1]
            first = len(vertices)
            vertices.extend(np.c_[points, np.full(3, height)])
            uv.extend([(0., 0.)]*3)
            faces.append((first, first+1, first+2))
            materials.append(3)
            parts.append(part)
    arrays = {k: np.asarray(value, dtype=dtype) for k, value, dtype in
              zip(FIELDS, (vertices, faces, uv, materials, parts), DTYPES)}
    _, first, inverse = np.unique(np.c_[arrays['vertices'], arrays['uv']], axis=0, return_index=True, return_inverse=True)
    arrays['vertices'] = arrays['vertices'][first]
    arrays['uv'] = arrays['uv'][first]
    arrays['faces'] = inverse[arrays['faces']].astype(np.int32)
    return arrays, surfaces


def append(storage, product, part, bounds=None):
    cached = product['shell']
    selected = np.flatnonzero(cached['parts'] == part)
    if bounds is not None:
        points = cached['vertices'][cached['faces'][selected], :2]
        lower, upper = np.array(bounds[:2]), np.array(bounds[2:])
        selected = selected[np.all(points.max(axis=1) >= lower, axis=1)
                            & np.all(points.min(axis=1) <= upper, axis=1)]
    if not len(selected):
        return
    vertices, faces, uv, materials = storage
    indices, inverse = np.unique(cached['faces'][selected], return_inverse=True)
    first = len(vertices)
    vertices.extend(cached['vertices'][indices])
    uv.extend(cached['uv'][indices])
    faces.extend(inverse.reshape(-1, 3)+first)
    materials.extend(cached['materials'][selected])


def validate(cached, edges, surfaces, patterns):
    if set(cached) != set(FIELDS):
        raise ValueError('incomplete prepared shell')
    for name, dtype in zip(FIELDS, DTYPES):
        cached[name] = np.asarray(cached[name], dtype=dtype)
    vertices, faces, uv, materials, parts = (cached[k] for k in FIELDS)
    if (vertices.ndim != 2 or vertices.shape[1] != 3 or not np.isfinite(vertices).all()
            or faces.ndim != 2 or faces.shape[1] != 3 or not len(faces)
            or uv.shape != (len(vertices), 2) or not np.isfinite(uv).all()
            or materials.shape != (len(faces),) or np.any(materials > 5+patterns)
            or parts.shape != (len(faces),) or faces.min() < 0 or faces.max() >= len(vertices)
            or np.any(parts < -len(surfaces)) or np.any(parts >= edges)):
        raise ValueError('invalid prepared shell buffers')
    counts = np.bincount(parts+len(surfaces), minlength=edges+len(surfaces))
    if np.any(counts[:len(surfaces)] < 1) or np.any(counts[len(surfaces):] != 2):
        raise ValueError('incomplete prepared shell coverage')
    for surface in surfaces:
        points = vertices[faces[parts == surface['part']]]
        flat = points.reshape(-1, 3)
        bounds = np.r_[flat[:, :2].min(0), flat[:, :2].max(0)]
        normals = np.cross(points[:, 1]-points[:, 0], points[:, 2]-points[:, 0])
        if (not np.all(points[:, :, 2] == surface['height_m'])
                or not np.allclose(bounds, surface['bounds'], atol=1e-10, rtol=0)
                or np.any(normals[:, 2]*(1 if surface['upward'] else -1) <= 0)):
            raise ValueError('prepared horizontal surface has wrong bounds, height or winding')
