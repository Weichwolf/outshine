"""Analytic opaque pixel fragments using GEOS polygons and reciprocal-depth planes."""

import numpy as np
import shapely


def positive_part(polygon, plane, cell):
    """Clip a fragment to a linear reciprocal-depth comparison inside one pixel."""
    xy = shapely.get_coordinates(polygon)
    values = xy @ plane[:2] + plane[2]
    if values.min(initial=np.inf) >= 0:
        return polygon
    if values.max(initial=-np.inf) <= 0:
        return shapely.Polygon()
    a, b, c = plane
    if abs(a) > abs(b):
        y = cell[1] + 0.5
        point = np.array([-(b * y + c) / a, y])
    else:
        x = cell[0] + 0.5
        point = np.array([x, -(a * x + c) / b])
    tangent = np.array([-b, a]) / np.hypot(a, b)
    normal = np.array([a, b]) / np.hypot(a, b)
    p, q = point - 4 * tangent, point + 4 * tangent
    return shapely.intersection(
        polygon, shapely.Polygon([p, q, q + 8 * normal, p + 8 * normal])
    )


def visible_fragments(candidates, planes, cell, work):
    layers = []
    centre = np.r_[np.array(cell) + 0.5, 1]
    ordered = sorted(candidates, key=lambda item: (-planes[item[0]] @ centre, item[0]))
    for face, original in ordered:
        remaining = original
        if layers:
            touched = np.flatnonzero(
                shapely.intersects([p for _, p in layers], original)
            )
        else:
            touched = []
        for index in touched:
            other, previous = layers[index]
            if remaining.area == 0:
                break
            work["fragment_pair_tests"] += 1
            overlap = shapely.intersection(remaining, previous)
            if overlap.area == 0:
                continue
            difference = planes[face] - planes[other]
            # Coincident planes retain the original face-ID order. The threshold
            # concerns numerical plane comparison, not a world-depth tolerance.
            scale = (abs(planes[face]) + abs(planes[other])) @ np.r_[
                np.maximum(abs(np.array(cell)), abs(np.array(cell) + 1)), 1
            ]
            tolerance = (
                256 * np.finfo(float).eps * max(float(scale), np.finfo(float).tiny)
            )
            values = shapely.get_coordinates(overlap) @ difference[:2] + difference[2]
            if np.max(abs(values), initial=0) <= tolerance:
                new_front, old_front = face < other, face >= other
                work["coincident_comparisons"] += 1
            else:
                new_front, old_front = values.min() >= 0, values.max() <= 0
            if old_front:
                remaining = shapely.difference(remaining, overlap)
            elif new_front:
                previous = shapely.difference(previous, overlap)
            else:
                nearer = positive_part(overlap, difference, cell)
                remaining = shapely.difference(
                    remaining, shapely.difference(overlap, nearer)
                )
                previous = shapely.difference(previous, nearer)
            layers[index] = (other, previous)
        layers = [(other, p) for other, p in layers if not p.is_empty]
        if remaining.area > 0:
            layers.append((face, remaining))
    area = sum(p.area for _, p in layers)
    if area > 1 + 1e-8:
        raise ValueError("analytic opaque fragments exceed pixel area")
    work["visible_fragments"] += len(layers)
    return layers


class AreaGrid:
    def __init__(self, source, eye, basis, start, step, shape):
        self.shape = tuple(map(int, shape))
        width, height = self.shape
        if (
            width <= 0
            or height <= 0
            or width * height > 262144
            or np.linalg.det(basis) * np.prod(step) <= 0
        ):
            raise ValueError("bounded front-wound analytic pixel grid required")
        points = (source["positions"].astype(float) - eye) @ basis.T
        if points[:, 2].min() <= 0.2:
            raise ValueError("analytic grid crosses source near plane")
        self.xy = (points[:, :2] / points[:, 2, None] - start) / step
        triangle = self.xy[source["indices"]]
        edges = triangle[:, 1:] - triangle[:, :1]
        determinant = edges[:, 0, 0] * edges[:, 1, 1] - edges[:, 0, 1] * edges[:, 1, 0]
        faces = np.flatnonzero(determinant < -1e-12)
        reciprocal = 1 / points[source["indices"], 2]
        gradient = np.linalg.solve(
            edges[faces], (reciprocal[faces, 1:] - reciprocal[faces, :1])[..., None]
        )[..., 0]
        self.planes = np.zeros((len(source["indices"]), 3))
        self.planes[faces] = np.column_stack(
            (
                gradient,
                reciprocal[faces, 0] - np.sum(gradient * triangle[faces, 0], axis=1),
            )
        )
        self.rows = [[] for _ in range(width * height)]
        self.work = {
            "input_triangles": len(source["indices"]),
            "projected_front_triangles": len(faces),
            "triangle_cell_clips": 0,
            "fragment_pair_tests": 0,
            "visible_fragments": 0,
            "coincident_comparisons": 0,
        }
        for face in faces:
            p = shapely.Polygon(triangle[face])
            low = np.maximum(np.floor(triangle[face].min(0)).astype(int), 0)
            high = np.minimum(
                np.ceil(triangle[face].max(0)).astype(int), [width, height]
            )
            if np.any(high <= low):
                continue
            y, x = np.meshgrid(
                np.arange(low[1], high[1]), np.arange(low[0], high[0]), indexing="ij"
            )
            x, y = x.ravel(), y.ravel()
            clipped = shapely.intersection(p, shapely.box(x, y, x + 1, y + 1))
            self.work["triangle_cell_clips"] += len(x)
            for xx, yy, piece in zip(x, y, clipped, strict=True):
                if piece.area > 0:
                    self.rows[yy * width + xx].append((int(face), piece))

    def fragments(self):
        width, _ = self.shape
        for row, candidates in enumerate(self.rows):
            if candidates:
                yield (
                    row,
                    visible_fragments(
                        candidates, self.planes, (row % width, row // width), self.work
                    ),
                )


def integration_points(polygon, *, refined=False):
    """Degree-two triangle rule; optional one-level subdivision checks BRDF curvature."""
    triangles = shapely.constrained_delaunay_triangles(polygon)
    coords = np.array(
        [np.asarray(t.exterior.coords)[:3] for t in triangles.geoms if t.area > 0]
    )
    if not len(coords):
        return np.empty((0, 2)), np.empty(0)
    if refined:
        a, b, c = coords.transpose(1, 0, 2)
        ab, bc, ca = (a + b) / 2, (b + c) / 2, (c + a) / 2
        coords = np.concatenate(
            (
                np.stack((a, ab, ca), axis=1),
                np.stack((ab, b, bc), axis=1),
                np.stack((ca, bc, c), axis=1),
                np.stack((ab, bc, ca), axis=1),
            )
        )
    e, f = coords[:, 1] - coords[:, 0], coords[:, 2] - coords[:, 0]
    area = abs(e[:, 0] * f[:, 1] - e[:, 1] * f[:, 0]) / 2
    barycentric = np.array(
        [[2 / 3, 1 / 6, 1 / 6], [1 / 6, 2 / 3, 1 / 6], [1 / 6, 1 / 6, 2 / 3]]
    )
    points = np.einsum("qv,tvd->tqd", barycentric, coords).reshape(-1, 2)
    return points, np.repeat(area / 3, 3)
