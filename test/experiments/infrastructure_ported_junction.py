#!/usr/bin/env python3
"""Shared junction ports and constrained strip profiles on captured native axes.

This local model preserves outer endpoint heights, binds short contained stubs to
the rigid core and validates the emitted indexed surface. Native integration is
separate; source heights inside the module remain fitting targets.
"""

import argparse
import hashlib
import json
from pathlib import Path
import time

import numpy as np
import shapely
from scipy.interpolate import CubicSpline
from shapely.geometry import LineString, Polygon

from geos_triangulation import triangles_of
from infrastructure_building_clearance import plot_geometry


def legs_of(context, lanes, node):
    legs, stubs = [], []
    for lane in lanes:
        source = context['designed'][lane]
        at = next(i for i, p in enumerate(source) if p['node'] == node)
        way = context['ways'][lane]
        for part in (source[at::-1], source[at:]):
            if len(part) < 2:
                continue
            points = np.array([[p['eastM'], p['northM'], p['gradeM']] for p in part])
            distance = np.r_[0., np.cumsum(np.linalg.norm(np.diff(points[:, :2], axis=0), axis=1))]
            if distance[-1] < .25:
                stubs.append(dict(lane=lane, length_m=float(distance[-1]), end=points[-1].tolist()))
                continue
            tangents = np.diff(points[:, :2], axis=0)[[0, -1]]
            tangents /= np.linalg.norm(tangents, axis=1)[:, None]
            curve = CubicSpline(distance, points[:, :2], bc_type=((1, tangents[0]), (1, tangents[1])))
            legs.append(dict(lane=lane, curve=curve, distance=distance, source=points,
                             width=way['halfWidthM'], maximum=way['maximum'] or .1))
    return legs, stubs


def section(curve, stations, width):
    xy, tangent = curve(stations), curve(stations, 1)
    tangent /= np.linalg.norm(tangent, axis=1)[:, None]
    normal = np.c_[-tangent[:, 1], tangent[:, 0]]
    return np.round(np.stack([xy + width * normal, xy - width * normal], axis=1) / .001) * .001


def surface_interval(xy, alpha, bias, reference, maximum):
    matrix = xy[:, 1:] - xy[:, :1]
    da = np.linalg.solve(matrix, (alpha[:, 1:] - alpha[:, :1])[..., None])[:, :, 0]
    db = np.linalg.solve(matrix, (bias[:, 1:] - bias[:, :1])[..., None])[:, :, 0]
    square = np.sum(da**2, axis=1)
    fixed = square < 1e-18
    if np.any(np.linalg.norm(db[fixed], axis=1) > maximum + 1e-8):
        raise ValueError('fixed surface gradient requires a different horizontal recipe')
    varying = ~fixed
    centre = -np.sum(da[varying] * db[varying], axis=1) / square[varying]
    perpendicular = db[varying] + centre[:, None] * da[varying]
    radius2 = maximum**2 - np.sum(perpendicular**2, axis=1)
    if np.any(radius2 < -1e-12):
        raise ValueError('surface gradient cannot fit this port recipe')
    radius = np.sqrt(np.maximum(0, radius2) / square[varying])
    return reference + np.max(centre - radius), reference + np.min(centre + radius)


def prepare(legs, reach, step):
    cores, approaches, lower, upper = [], [], -np.inf, np.inf
    for leg in legs:
        length = float(leg['distance'][-1])
        cut = min(reach, length * .5)
        samples = np.linspace(0, length, 1000)
        velocity, acceleration = leg['curve'](samples, 1), leg['curve'](samples, 2)
        curvature = abs(velocity[:, 0] * acceleration[:, 1] - velocity[:, 1] * acceleration[:, 0])
        curvature /= np.linalg.norm(velocity, axis=1)**3
        if np.max(curvature) * leg['width'] >= 1:
            raise ValueError('offset surface folds inside the horizontal curve')
        core_stations = np.linspace(0, cut, max(2, int(np.ceil(cut / step)) + 1))
        core_section = section(leg['curve'], core_stations, leg['width'])
        cores.append(Polygon(np.r_[core_section[:, 0], core_section[::-1, 1]]))
        stations = np.linspace(cut, length, max(2, int(np.ceil((length - cut) / step)) + 1))
        points = section(leg['curve'], stations, leg['width'])
        xy = np.concatenate([np.stack([points[:-1, 0], points[:-1, 1], points[1:, 0]], axis=1),
                             np.stack([points[:-1, 1], points[1:, 1], points[1:, 0]], axis=1)])
        t = (stations - cut) / (length - cut)
        smooth = t**3 * (10 + t * (-15 + 6 * t))
        weight = 1 - smooth
        alpha = np.concatenate([np.c_[weight[:-1], weight[:-1], weight[1:]],
                                np.c_[weight[:-1], weight[1:], weight[1:]]])
        reference = float(leg['source'][0, 2])
        end = float(leg['source'][-1, 2])
        bias = (end - reference) * (1 - alpha)
        lo, hi = surface_interval(xy, alpha, bias, reference, leg['maximum'])
        rise = leg['maximum'] * (length - cut) / 1.875
        lower, upper = max(lower, lo, end - rise), min(upper, hi, end + rise)
        leg.update(cut_m=cut, minimum_radius_m=float(1 / max(np.max(curvature), 1e-12)),
                   xy=xy, alpha=alpha, bias=bias, reference=reference, section=points,
                   stations=stations)
        approaches.append(Polygon(np.r_[points[:, 0], points[::-1, 1]]))
    core = shapely.union_all(cores)
    if not core.is_valid or core.geom_type != 'Polygon':
        raise ValueError('junction core is not a connected valid polygon')
    for i, approach in enumerate(approaches):
        if core.intersection(approach).area > 1e-7:
            raise ValueError('junction port is still inside another core band')
        for other in approaches[:i]:
            if approach.intersection(other).area > 1e-7:
                raise ValueError('approaches overlap beyond the shared core')
    return core, approaches, lower, upper


def emit(legs, core, height):
    xy = np.array([np.array(p.exterior.coords)[:3] for p in triangles_of(core)])
    faces = [np.concatenate([xy, np.full((*xy.shape[:2], 1), height)], axis=2)]
    for leg in legs:
        level = leg['reference'] + leg['alpha'] * (height - leg['reference']) + leg['bias']
        face = np.concatenate([leg['xy'], level[:, :, None]], axis=2)
        gradient = np.linalg.solve(face[:, 1:, :2] - face[:, :1, :2],
                                   (face[:, 1:, 2] - face[:, :1, 2])[..., None])[:, :, 0]
        leg['maximum_emitted_gradient'] = float(np.linalg.norm(gradient, axis=1).max())
        if leg['maximum_emitted_gradient'] > leg['maximum'] + 1e-8:
            raise ValueError('emitted approach violates its class gradient')
        faces.append(face)
    faces = np.concatenate(faces)
    winding = np.cross(faces[:, 1] - faces[:, 0], faces[:, 2] - faces[:, 0])[:, 2]
    faces[winding < 0] = faces[winding < 0][:, [0, 2, 1]]
    flat = faces.reshape(-1, 3)
    vertices, indices = np.unique(flat, axis=0, return_inverse=True)
    indices = indices.reshape(-1, 3)
    if len(np.unique(vertices[:, :2], axis=0)) != len(vertices):
        raise ValueError('shared horizontal vertices have different heights')
    edges = np.sort(np.concatenate([indices[:, [0, 1]], indices[:, [1, 2]],
                                   indices[:, [2, 0]]]), axis=1)
    edges, counts = np.unique(edges, axis=0, return_counts=True)
    if np.any(counts > 2):
        raise ValueError('ported surface is not manifold')
    return vertices, indices, edges, counts


def check_mesh(legs, core, approaches, vertices, indices, edges, counts):
    outline = shapely.union_all([core, *approaches])
    boundary = vertices[edges[counts == 1], :2].mean(axis=1)
    if np.any(shapely.distance(shapely.points(boundary), outline.boundary) > 1e-7):
        raise ValueError('ported mesh contains a crack or T-junction')
    projected = shapely.polygons(vertices[indices, :2])
    if (abs(float(shapely.area(projected).sum()) - outline.area) > 1e-7
            or shapely.union_all(projected).symmetric_difference(outline).area > 1e-7):
        raise ValueError('ported mesh contains overlapping or missing faces')
    faces = vertices[indices]
    if np.any(np.cross(faces[:, 1] - faces[:, 0], faces[:, 2] - faces[:, 0])[:, 2] <= 0):
        raise ValueError('ported mesh contains folded or degenerate faces')
    for leg in legs:
        expected = float(leg['source'][-1, 2])
        for endpoint in leg['section'][-1]:
            at = np.all(vertices[:, :2] == endpoint, axis=1)
            if np.count_nonzero(at) != 1 or abs(vertices[at][0, 2] - expected) > 1e-8:
                raise ValueError('ported mesh changed a fixed outer endpoint height')


def render(args, legs, core, vertices, indices, report):
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    from mpl_toolkits.mplot3d.art3d import Poly3DCollection
    fig = plt.figure(figsize=(12, 6), constrained_layout=True)
    top, view = fig.add_subplot(121), fig.add_subplot(122, projection='3d', proj_type='ortho')
    plot_geometry(top, [core], '#83a6a5')
    colours = ['#a8b79e', '#c4af90', '#92a6b5']
    for leg, colour in zip(legs, colours):
        xy = leg['source'][:, :2]
        top.plot(xy[:, 0], xy[:, 1], '--', color='#555555', linewidth=.8)
        band = Polygon(np.r_[leg['section'][:, 0], leg['section'][::-1, 1]])
        plot_geometry(top, [band], colour)
        port = leg['section'][0]
        top.plot(port[:, 0], port[:, 1], color='#167dba', linewidth=2)
    view.add_collection3d(Poly3DCollection(vertices[indices], facecolor='#a2b8a4',
                                          edgecolor='#3a5550', linewidth=.3))
    low, high = vertices.min(axis=0), vertices.max(axis=0)
    top.set(aspect='equal', xlabel='East [m]', ylabel='North [m]', title='Shared ports / source axes dashed')
    view.set(xlim=(low[0], high[0]), ylim=(low[1], high[1]), zlim=(low[2] - .1, high[2] + .1),
             xlabel='East [m]', ylabel='North [m]', zlabel='Height [m]', title='Indexed 3D surface / profiles')
    view.set_box_aspect((high[0] - low[0], high[1] - low[1], 7))
    view.view_init(elev=28, azim=-55)
    fig.suptitle('Zuerich native junction: core and approaches share exact transverse ports\n'
                 f"Mesh gradients and connectivity passed; {report['vertices']} vertices, {report['triangles']} triangles")
    path = args.output / '2_5d' / 'Zuerich-ported-junction.png'
    path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(path, dpi=120)
    plt.close(fig)
    return str(path)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('context', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--reach', type=float, default=6)
    parser.add_argument('--step', type=float, default=1)
    args = parser.parse_args()
    if not all(np.isfinite(v) and v > 0 for v in (args.reach, args.step)):
        parser.error('finite positive reach and step required')
    source = args.context.read_bytes()
    context = json.loads(source)
    legs, stubs = legs_of(context, (2384, 3932), 6329826849065819068)
    begin = time.perf_counter()
    core, approaches, low, high = prepare(legs, args.reach, args.step)
    target = float(np.mean([a['source'][0, 2] for a in legs]))
    if low > high:
        raise ValueError(f'ported surface has no feasible height interval: {low}, {high}')
    height = float(np.clip(target, low, high))
    vertices, indices, edges, counts = emit(legs, core, height)
    check_mesh(legs, core, approaches, vertices, indices, edges, counts)
    report = dict(status='passed', source_sha256=hashlib.sha256(source).hexdigest(),
                  scope='One real junction, three curved approaches; no native-world integration.',
                  source_target_m=target, height_m=height, displacement_m=abs(height - target),
                  lower_m=low, upper_m=high, reach_m=args.reach, step_m=args.step,
                  vertices=len(vertices), triangles=len(indices), contained_stubs=stubs,
                  build_and_verify_ms=(time.perf_counter() - begin) * 1000,
                  legs=[{k: a[k] for k in ('lane', 'width', 'maximum', 'cut_m',
                        'minimum_radius_m', 'maximum_emitted_gradient')} for a in legs])
    report['image'] = render(args, legs, core, vertices, indices, report)
    (args.output / 'ported-junction.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report), flush=True)


if __name__ == '__main__':
    main()
