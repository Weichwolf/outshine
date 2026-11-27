"""Assemble closed slabs and edge guards for mapped bank-to-bank bridges."""

from collections import defaultdict
from itertools import pairwise

import numpy as np
import shapely
from outshine_ref.transport.water_span import solve_water_span


def slab(meshes, material, polygon, height, thickness):
    triangles = shapely.get_parts(shapely.constrained_delaunay_triangles(polygon))
    xy = np.array(
        [p.exterior.coords[:3] for p in shapely.orient_polygons(triangles)]
    ).reshape(-1, 2)
    top = np.column_stack((xy, height(xy)))
    faces = np.arange(len(xy)).reshape(-1, 3)
    meshes.add(material, top, faces)
    meshes.add(material, top - [0, 0, thickness], faces[:, ::-1])
    for ring in [polygon.exterior, *polygon.interiors]:
        xy = np.asarray(ring.coords)
        top = np.column_stack((xy, height(xy)))
        count = len(xy)
        a = np.arange(count - 1)
        sides = np.column_stack((a, a + count, a + 1, a + 1, a + count, a + count + 1))
        meshes.add(
            material, np.vstack((top, top - [0, 0, thickness])), sides.reshape(-1, 3)
        )


def edge_guards(meshes, polygon, span):
    for ring in [polygon.exterior, *polygon.interiors]:
        for a, b in pairwise(np.asarray(ring.coords)):
            delta = b - a
            length = np.linalg.norm(delta)
            if length < 0.16 or abs(delta @ span.direction) < 0.72 * length:
                continue
            inset = np.array([-delta[1], delta[0]]) * (0.08 / length)
            # Move the rail just inside the walking edge.
            start, end = a + inset, b + inset
            for rise in (0.55, 1.05):
                bar = shapely.LineString([start, end]).buffer(0.032, cap_style="flat")
                slab(
                    meshes,
                    "bridge metal",
                    bar,
                    lambda xy, h=rise: span.height(xy) + h,
                    0.065,
                )
            divisions = max(1, int(np.ceil(length / 1.8)))
            for x, y in np.linspace(start, end, divisions + 1):
                post = shapely.box(x - 0.036, y - 0.036, x + 0.036, y + 0.036)
                slab(
                    meshes,
                    "bridge metal",
                    post,
                    lambda xy: span.height(xy) + 1.05,
                    1.05,
                )


def build_water_bridges(meshes, source, extent, water):
    outlines, routes = defaultdict(list), defaultdict(list)
    for name, geom, prop in source.features:
        if name != "transportation" or prop.get("brunnel") != "bridge":
            continue
        level = prop.get("layer", 1)
        if prop["class"] == "bridge" and geom.geom_type.endswith("Polygon"):
            outlines[level].append(shapely.make_valid(geom))
        elif geom.geom_type.endswith("LineString"):
            routes[level].append(geom)
    solved = {}
    for level, pieces in outlines.items():
        drawn = []
        for footprint in shapely.get_parts(shapely.union_all(pieces)):
            if footprint.geom_type != "Polygon" or not extent.covers(footprint):
                continue
            span = solve_water_span(footprint, routes[level], water, source.height)
            if span is None:
                continue
            footprint = shapely.orient_polygons(footprint)
            slab(
                meshes,
                "bridge deck",
                footprint,
                span.height,
                min(1.4, span.length / 40),
            )
            edge_guards(meshes, footprint, span)
            drawn.append(footprint)
        if drawn:
            solved[level] = shapely.union_all(drawn)
    return solved
