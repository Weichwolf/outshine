"""Prepare complete coarse hulls once; select stored faces without triangulating on a hit."""

import numpy as np
import shapely

FIELDS = ('vertices', 'faces', 'uv', 'materials', 'parts')
DTYPES = (np.float64, np.int32, np.float32, np.uint8, np.int32)


def prepare(product):
    vertices, faces, uv, materials, parts = [], [], [], [], []
    top, bottom = product['top_m'], product['bottom_m']
    for index, facade in enumerate(product['facades']):
        a, b, repeat = facade['a'], facade['b'], facade['repeat']
        first = len(vertices)
        vertices.extend([[*a, bottom], [*b, bottom], [*b, top], [*a, top]])
        uv.extend([(0., 0.), (repeat.columns, 0.), (repeat.columns, repeat.rows), (0., repeat.rows)])
        faces.extend([(first, first+1, first+2), (first, first+2, first+3)])
        materials.extend([4 if repeat.count else 0]*2)
        parts.extend([index]*2)
    caps = shapely.constrained_delaunay_triangles(product['polygon'])
    for triangle in shapely.get_parts(caps):
        ring = np.array(triangle.exterior.coords)[:3]
        a, b = ring[1]-ring[0], ring[2]-ring[0]
        if a[0]*b[1]-a[1]*b[0] < 0:
            ring = ring[::-1]
        for height, points, part in ((top, ring, -1), (bottom, ring[::-1], -2)):
            first = len(vertices)
            vertices.extend(np.c_[points, np.full(3, height)])
            uv.extend([(0., 0.)]*3)
            faces.append((first, first+1, first+2))
            materials.append(3)
            parts.append(part)
    return {k: np.asarray(value, dtype=dtype) for k, value, dtype in
            zip(FIELDS, (vertices, faces, uv, materials, parts), DTYPES)}


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


def validate(cached, edges):
    if set(cached) != set(FIELDS):
        raise ValueError('incomplete prepared shell')
    for name, dtype in zip(FIELDS, DTYPES):
        cached[name] = np.asarray(cached[name], dtype=dtype)
    vertices, faces, uv, materials, parts = (cached[k] for k in FIELDS)
    if (vertices.ndim != 2 or vertices.shape[1] != 3 or not np.isfinite(vertices).all()
            or faces.ndim != 2 or faces.shape[1] != 3 or not len(faces)
            or uv.shape != (len(vertices), 2) or not np.isfinite(uv).all()
            or materials.shape != (len(faces),) or np.any(materials > 4)
            or parts.shape != (len(faces),) or faces.min() < 0 or faces.max() >= len(vertices)
            or np.any(parts < -2) or np.any(parts >= edges)):
        raise ValueError('invalid prepared shell buffers')
    counts = np.bincount(parts+2, minlength=edges+2)
    if counts[0] < 1 or counts[0] != counts[1] or np.any(counts[2:] != 2):
        raise ValueError('incomplete prepared shell coverage')
