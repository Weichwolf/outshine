"""Direct shells and selective recessed facade geometry from the same repeat plan."""

import math

import numpy as np
import shapely

from facade_plan import cell_range, detail_count, opening_rectangles
from shell import append as append_shell
from features import append_visible


def quad(vertices, faces, uv, materials, points, coordinates, material):
    first = len(vertices)
    vertices.extend(points)
    uv.extend(coordinates)
    faces.extend(((first, first+1, first+2), (first, first+2, first+3)))
    materials.extend((material, material))


def wall_quad(storage, facade, bottom, rectangle, depth=0., material=0):
    x0, y0, x1, y1 = rectangle
    repeat = facade['repeat']
    points = np.array([[x0, y0, depth], [x1, y0, depth], [x1, y1, depth], [x0, y1, depth]])
    surface_quad(storage, facade, bottom, points, material)


def surface_quad(storage, facade, bottom, local, material):
    repeat = facade['repeat']
    tangent = (facade['b']-facade['a'])/repeat.width_m
    points = np.c_[facade['a']+local[:, :1]*tangent, bottom+local[:, 1]]
    points += local[:, 2:3]*facade['normal']
    uv = local[:, :2]/[repeat.width_m/repeat.columns, repeat.height_m/repeat.rows]
    quad(*storage, points, uv, material)


def opening(storage, facade, bottom, rectangle):
    x0, y0, x1, y1 = rectangle
    outline = np.array([[x0, y0], [x1, y0], [x1, y1], [x0, y1]])
    repeat = facade['repeat']
    fw, fh = .025*repeat.width_m/repeat.columns, .025*repeat.height_m/repeat.rows
    inner = outline+np.array([[fw, fh], [-fw, fh], [-fw, -fh], [fw, -fh]])
    depth = facade['repeat'].depth_m
    for loop, low, high in ((outline, 0., -.04), (inner, -.04, -depth)):
        for a, b in zip(loop, np.roll(loop, -1, axis=0)):
            points = np.array([[*a, low], [*b, low], [*b, high], [*a, high]])
            surface_quad(storage, facade, bottom, points, 2 if low < 0 else 0)
    for i in range(4):
        j = (i+1) % 4
        points = np.c_[np.array([outline[i], outline[j], inner[j], inner[i]]), np.full(4, -.04)]
        surface_quad(storage, facade, bottom, points, 2)
    wall_quad(storage, facade, bottom, [*inner[0], *inner[2]], -depth, 1)


def detailed_wall(storage, facade, bottom, region):
    repeat = facade['repeat']
    if not repeat.count:
        wall_quad(storage, facade, bottom, region)
        return
    pw, ph = repeat.width_m/repeat.columns, repeat.height_m/repeat.rows
    for rectangle in opening_rectangles(repeat, region):
        x0, y0, x1, y1 = rectangle
        column, row = int(math.floor((x0+x1)*.5/pw)), int(math.floor((y0+y1)*.5/ph))
        xs, ys = [column*pw, x0, x1, (column+1)*pw], [row*ph, y0, y1, (row+1)*ph]
        for y in range(3):
            for x in range(3):
                if x == y == 1:
                    continue
                wall_quad(storage, facade, bottom, [xs[x], ys[y], xs[x+1], ys[y+1]])
        opening(storage, facade, bottom, rectangle)


def append_shadow_hull(storage, product, requested, detailed):
    for index, facade in enumerate(product['facades']):
        if index not in requested:
            append_shell(storage, product, index)
        elif index in detailed:
            repeat = facade['repeat']
            x0, y0, x1, y1 = cell_range(repeat, detailed[index])
            pw, ph = repeat.width_m/repeat.columns, repeat.height_m/repeat.rows
            left, bottom, right, top = x0*pw, y0*ph, x1*pw, y1*ph
            rectangles = [(0., 0., left, repeat.height_m), (right, 0., repeat.width_m, repeat.height_m),
                          (left, 0., right, bottom), (left, top, right, repeat.height_m)]
            pattern = (repeat.opening_width, repeat.opening_height)
            material = 6+product['opening_patterns'].index(pattern)
            for rectangle in rectangles:
                if rectangle[2] > rectangle[0] and rectangle[3] > rectangle[1]:
                    wall_quad(storage, facade, facade['bottom_m'], rectangle, material=material)


def mesh(product, demand, detail=True, max_openings=50000, include_shadow_hull=False):
    if type(max_openings) is not int or max_openings < 0:
        raise ValueError('invalid detail budget')
    storage = ([], [], [], [])
    fallbacks, emitted, detailed = [], 0, {}
    for request in demand['faces']:
        index = request['edge']
        facade = product['facades'][index]
        repeat = facade['repeat']
        region = request['region']
        if detail and request['detail'] and repeat.count:
            count = detail_count(repeat, region)
            reason = None
            if emitted+count > max_openings:
                reason = 'detail_budget_exhausted'
            elif not recess_fits(product['layers'][facade['layer']]['footprint'], facade, region):
                reason = 'recess_crosses_footprint_boundary'
            if reason is None:
                detailed_wall(storage, facade, facade['bottom_m'], region)
                emitted += count
                if repeat.count:
                    detailed[index] = region
            else:
                append_shell(storage, product, index)
                fallbacks.append(dict(edge=index, reason=reason))
        else:
            append_shell(storage, product, index)
    admitted = bool(demand['faces'] or demand['caps'])
    if include_shadow_hull and admitted:
        append_shadow_hull(storage, product, {r['edge'] for r in demand['faces']}, detailed)
        for surface in product['surfaces']:
            append_shell(storage, product, surface['part'])
    else:
        for request in demand['caps']:
            append_shell(storage, product, request['part'], request['bounds'])
    append_visible(storage, product, demand['view'])
    vertices, faces, uv, materials = storage
    return dict(vertices=np.asarray(vertices, dtype=np.float64).reshape(-1, 3), faces=np.asarray(faces, dtype=np.int32).reshape(-1, 3),
                uv=np.asarray(uv, dtype=np.float32).reshape(-1, 2), materials=np.asarray(materials, dtype=np.uint8),
                detail_fallbacks=fallbacks, detail_openings=emitted)


def recess_fits(polygon, facade, region):
    repeat = facade['repeat']
    if not repeat.count:
        return True
    rectangles = opening_rectangles(repeat, [region[0], 0., region[2], repeat.height_m/repeat.rows])
    tangent = (facade['b']-facade['a'])/repeat.width_m
    for u0, _, u1, _ in rectangles:
        first, last = facade['a']+u0*tangent, facade['a']+u1*tangent
        strip = shapely.Polygon([first, last, last-facade['normal'][:2]*repeat.depth_m,
                                 first-facade['normal'][:2]*repeat.depth_m])
        if not shapely.covers(shapely.buffer(polygon, 1e-8), strip):
            return False
    return True
