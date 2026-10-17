"""Keep rectangular facade hosts separate from the roof's blank upper wall band."""

import numpy as np
import shapely
from facade_parts import append
from outshine_ref.buildings.facade_frames import front
from outshine_ref.buildings.modelling.heightfield import heightfield_envelope
from outshine_ref.buildings.rooflight_panels import triangle_surface


def selected(mesh, mask):
    indices = mesh["faces"][mask].ravel()
    return {
        "vertices": mesh["vertices"][indices],
        "normals": mesh["normals"][indices],
        "uv": mesh["uv"][indices],
        "faces": np.arange(len(indices), dtype=np.uint32).reshape(-1, 3),
        "materials": mesh["materials"][mask],
        "facade_extent": np.zeros((len(indices), 3)),
    }


def body(crown):
    p = shapely.from_wkb(crown.footprint_wkb)
    points = front(p)
    points[:, :, 2] = crown.deck_m
    raw = triangle_surface(
        {
            "vertices": points.reshape(-1, 3),
            "faces": np.arange(points.size // 3, dtype=np.uint32).reshape(-1, 3),
        },
        9,
    )
    raw.pop("facade_extent")
    closed = heightfield_envelope(raw, base_m=0.0, wall_material=1, base_material=0)
    return selected(closed.mesh, closed.surface_roles != 0)


def skin(env, datum):
    top = selected(env.mesh, env.surface_roles == 0)
    points = env.mesh["vertices"]
    quads = []
    for a, b, _owner in env.boundary_edges:
        first, last = points[a], points[b]
        quads.append([[*first[:2], datum], [*last[:2], datum], last, first])
    quads = np.asarray(quads)[:, [[0, 1, 2], [0, 2, 3]]].reshape(-1, 3)
    sides = triangle_surface(
        {
            "vertices": quads,
            "faces": np.arange(len(quads), dtype=np.uint32).reshape(-1, 3),
        },
        1,
    )
    return append(top, [sides])


def bound(mesh, material):
    result = dict(mesh)
    result["materials"] = result["materials"].copy()
    result["materials"][result["materials"] == 1] = material
    return result
