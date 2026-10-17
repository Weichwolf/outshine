"""Direct closed physical relief and shared covered-back draw buffers."""

import numpy as np
import shapely
from outshine_ref.buildings.facade_frames import front, sides
from outshine_ref.buildings.modelling import Shape
from outshine_ref.buildings.modelling.faces import Face
from outshine_ref.buildings.rooflight_panels import triangle_surface
from recipe_hall_operators import sealed


def direct(plan):
    fronts, walls, backs = [], [], []
    for polygon in shapely.get_parts(plan.region):
        polygon = shapely.orient_polygons(polygon)
        cap = front(polygon)
        cap[:, :, 2] = plan.depth_m
        edge, _uv = sides(polygon, plan.depth_m)
        back = cap[:, ::-1].copy()
        back[:, :, 2] = 0
        fronts.append(cap)
        walls.append(edge)
        backs.append(back)
    outside = np.concatenate([*fronts, *walls])
    points = np.concatenate([outside, *backs]).reshape(-1, 3)
    raw = {
        "vertices": points,
        "faces": np.arange(len(points), dtype=np.uint32).reshape(-1, 3),
    }
    full = triangle_surface(raw, 10)
    if len(full["faces"]) != plan.triangles:
        raise ValueError("Relief topology disagrees with its preflight capacity")
    return full, len(outside)


def generic(plan):
    parts = []
    for polygon in shapely.get_parts(plan.region):
        polygon = shapely.orient_polygons(polygon)
        vertices, rings = [], []
        for ring in (polygon.exterior, *polygon.interiors):
            points = np.asarray(ring.coords)[:-1]
            rings.append(tuple(range(len(vertices), len(vertices) + len(points))))
            vertices.extend(np.c_[points, np.zeros(len(points))])
        raw = (
            Shape.from_faces(np.asarray(vertices), [Face(rings[0], tuple(rings[1:]))])
            .extrude(plan.depth_m)
            .emit()
        )
        parts.append(raw["vertices"][raw["faces"]])
    points = np.concatenate(parts).reshape(-1, 3)
    return triangle_surface(
        {
            "vertices": points,
            "faces": np.arange(len(points), dtype=np.uint32).reshape(-1, 3),
        },
        10,
    )


def placed(raw, plan):
    t = np.asarray(plan.tangent)
    basis = np.array([[*t, 0], [0, 0, 1], [t[1], -t[0], 0]])
    return sealed(
        raw
        | {
            "vertices": raw["vertices"] @ basis + [*plan.origin, 0],
            "normals": raw["normals"] @ basis,
        }
    )


def prefix(mesh, count):
    return sealed(
        {
            k: v[: count if k in ("faces", "materials") else count * 3]
            for k, v in mesh.items()
        }
    )


def cap_prefix(part):
    count = len(part.full["faces"]) - len(part.outer["faces"])
    if not 0 < count < len(part.outer["faces"]):
        raise ValueError("Prepared relief requires matching front/back cap topology")
    return prefix(part.full, count)


def products(plan):
    raw, outer_triangles = direct(plan)
    full = placed(raw, plan)
    return full, prefix(full, outer_triangles)
