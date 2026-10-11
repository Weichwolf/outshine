#!/usr/bin/env python3
"""Bounded rigid cores and smooth approaches on captured native Zurich road axes.

The local solver minimizes maximum height displacement under fixed endpoint heights,
class gradients and complete footprint clearance. It does not solve the full network,
horizontal curvature, portals, neighboring tile ports or collision delivery.
"""

import argparse
import hashlib
import json
from pathlib import Path
import time

import numpy as np
import shapely
from shapely.geometry import LineString, Point
from shapely.ops import substring
from scipy.optimize import linprog

from geos_triangulation import triangles_of
from infrastructure_building_clearance import plot_geometry


def band(context, lane):
    way = context['ways'][lane]
    points = np.array([[p['eastM'], p['northM'], p['gradeM']]
                       for p in context['designed'][lane]])
    distance = np.r_[0, np.cumsum(np.linalg.norm(np.diff(points[:, :2], axis=0), axis=1))]
    unique = np.r_[True, np.diff(distance) > 1e-9]
    points, distance = points[unique], distance[unique]
    line = LineString(points[:, :2])
    return dict(lane=lane, layer=way['layer'], bridge=way['bridge'],
                tier=(way['layer'], way['bridge']), points=points, distance=distance,
                line=line, half_width=way['halfWidthM'],
                maximum=way['maximum'] or .1, clearance=way['clearanceM'],
                footprint=shapely.set_precision(line.buffer(
                    way['halfWidthM'], resolution=2, cap_style=2), .001),
                core=[float(distance[-1]), 0.], overlaps=[])


def plan(axes, margin):
    constraints, pairs = [], []
    for a in range(len(axes)):
        for b in range(a + 1, len(axes)):
            overlap = axes[a]['footprint'].intersection(axes[b]['footprint'])
            if overlap.is_empty or overlap.area < 1e-9:
                continue
            for axis in (axes[a], axes[b]):
                distances = [axis['line'].project(Point(xy))
                             for xy in shapely.get_coordinates(overlap)]
                axis['core'][0] = min(axis['core'][0], max(0, min(distances) - margin))
                axis['core'][1] = max(axis['core'][1], min(axis['distance'][-1], max(distances) + margin))
                axis['overlaps'].append(overlap)
            pairs.append((a, b))
    tiers = sorted({axis['tier'] for axis in axes})
    lower = np.full(len(tiers), -np.inf)
    upper = np.full(len(tiers), np.inf)
    targets = [[] for _ in tiers]
    for axis in axes:
        index = tiers.index(axis['tier'])
        axis['owner'] = index
        first, last = axis['core']
        if first > last:
            raise ValueError('axis without a footprint conflict')
        core = substring(axis['line'], first, last).buffer(axis['half_width'], resolution=2, cap_style=2)
        core = shapely.set_precision(core, .001)
        axis['core_footprint'] = shapely.union_all([core, *axis['overlaps']]).intersection(axis['footprint'])
        surface_lower, surface_upper = prepare_surface(axis)
        runs = np.array([first, axis['distance'][-1] - last])
        rises = axis['maximum'] * runs / 1.875
        ends = axis['points'][[0, -1], 2]
        lower[index] = max(lower[index], float(np.max(ends - rises)), surface_lower)
        upper[index] = min(upper[index], float(np.min(ends + rises)), surface_upper)
        targets[index].append(float(np.interp((first + last) * .5,
                                             axis['distance'], axis['points'][:, 2])))
    for a, b in pairs:
        low, high = sorted((axes[a], axes[b]), key=lambda x: x['owner'])
        if low['owner'] != high['owner']:
            constraints.append((low['owner'], high['owner'], low['clearance'] + .25))
    return np.array([np.mean(t) for t in targets]), lower, upper, sorted(set(constraints)), pairs


def solve(target, lower, upper, gaps):
    """Propagated envelopes L,T give h=max(L,T-e). Feasibility requires
    L<=upper and e>=max(0,T-upper,L-target,(T-target)/2); the bound is attained.
    Edges must follow the DAG order. This is not a cyclic network solver.
    """
    floor, goal = lower.copy(), target.copy()
    previous = -1
    for low, high, gap in gaps:
        if not previous <= low < high < len(target):
            raise ValueError('clearance edges must follow topological order')
        previous = low
        floor[high] = max(floor[high], floor[low] + gap)
        goal[high] = max(goal[high], goal[low] + gap)
    if np.any(floor > upper):
        return None
    error = max(0., float(np.max(goal - upper)), float(np.max(floor - target)),
                .5 * float(np.max(goal - target)))
    return np.maximum(floor, goal - error), error


def solve_batch(target, lower, upper, gaps):
    floor, goal = lower.copy(), target.copy()
    for low, high, gap in gaps:
        np.maximum(floor[:, high], floor[:, low] + gap, out=floor[:, high])
        np.maximum(goal[:, high], goal[:, low] + gap, out=goal[:, high])
    valid = np.all(floor <= upper, axis=1)
    error = np.maximum.reduce([np.zeros(len(target)), np.max(goal - upper, axis=1),
                               np.max(floor - target, axis=1), .5 * np.max(goal - target, axis=1)])
    return np.maximum(floor, goal - error[:, None]), error, valid


def reference(target, lower, upper, gaps):
    count = len(target)
    matrix, bounds = [], []
    for owner, goal in enumerate(target):
        row = np.zeros(count + 1)
        row[owner], row[-1] = 1, -1
        matrix.extend([row.copy(), -row.copy()])
        matrix[-1][-1] = -1
        bounds.extend([goal, -goal])
    for low, high, gap in gaps:
        row = np.zeros(count + 1)
        row[low], row[high] = 1, -1
        matrix.append(row)
        bounds.append(-gap)
    objective = np.r_[np.zeros(count), 1.]
    return linprog(objective, A_ub=matrix, b_ub=bounds,
                   bounds=[*zip(lower, upper), (0, None)], method='highs')


def height_at(axis, height, stations):
    first, last = axis['core']
    length = axis['distance'][-1]
    start, end = axis['points'][[0, -1], 2]
    def smooth(t):
        t = np.clip(t, 0, 1)
        return t**3 * (10 + t * (-15 + 6 * t))
    values = np.full_like(stations, height)
    if first > 0:
        before = stations < first
        values[before] = start + (height - start) * smooth(stations[before] / first)
    if last < length:
        after = stations > last
        values[after] = height + (end - height) * smooth((stations[after] - last) / (length - last))
    return values


def check_profiles(axes, heights):
    worst = 0.
    for axis in axes:
        first, last = axis['core']
        length = axis['distance'][-1]
        starts = axis['points'][[0, -1], 2]
        height = heights[axis['owner']]
        slopes = [1.875 * abs(height - starts[0]) / first if first > 1e-9 else 0.,
                  1.875 * abs(height - starts[1]) / (length - last) if length - last > 1e-9 else 0.]
        maximum = max(slopes)
        if maximum > axis['maximum'] + 1e-8:
            raise ValueError('profile exceeds class gradient')
        actual = height_at(axis, height, np.array([0., length]))
        if np.max(abs(actual - starts)) > 1e-8:
            raise ValueError('profile changed a fixed endpoint height')
        worst = max(worst, maximum)
    return worst


def prepare_surface(axis):
    rigid = triangles_of(axis['core_footprint'])
    approach = axis['footprint'].difference(axis['core_footprint'])
    triangles = np.r_[rigid, triangles_of(approach)] if not approach.is_empty else rigid
    outline = shapely.set_precision(shapely.union_all(triangles), .001)
    if outline.symmetric_difference(axis['footprint']).area > 1e-7:
        raise ValueError('triangles do not cover the recipe footprint')
    coordinates, weights, biases = [], [], []
    reference_height = float(axis['points'][0, 2])
    for index, triangle in enumerate(triangles):
        xy = np.asarray(triangle.exterior.coords)[:3]
        first, second = xy[1] - xy[0], xy[2] - xy[0]
        if first[0] * second[1] - first[1] * second[0] < 0:
            xy = xy[[0, 2, 1]]
        stations = np.array([axis['line'].project(Point(p)) for p in xy])
        base = height_at(axis, reference_height, stations)
        weight = height_at(axis, reference_height + 1, stations) - base
        bias = base - reference_height
        on_core = shapely.covers(axis['core_footprint'], shapely.points(xy))
        weight[on_core], bias[on_core] = 1., 0.
        if index < len(rigid):
            weight[:], bias[:] = 1., 0.
        coordinates.append(xy)
        weights.append(weight)
        biases.append(bias)
    xy, weights, biases = map(np.asarray, (coordinates, weights, biases))
    matrices = xy[:, 1:, :] - xy[:, :1, :]
    slope_weight = np.linalg.solve(matrices, (weights[:, 1:] - weights[:, :1])[..., None])[:, :, 0]
    slope_bias = np.linalg.solve(matrices, (biases[:, 1:] - biases[:, :1])[..., None])[:, :, 0]
    squared = np.sum(slope_weight**2, axis=1)
    fixed = squared < 1e-18
    if np.any(np.linalg.norm(slope_bias[fixed], axis=1) > axis['maximum'] + 1e-8):
        raise ValueError('fixed surface gradient needs a different horizontal recipe')
    variable = ~fixed
    centres = -np.sum(slope_weight[variable] * slope_bias[variable], axis=1) / squared[variable]
    perpendicular = slope_bias[variable] + centres[:, None] * slope_weight[variable]
    radius_squared = axis['maximum']**2 - np.sum(perpendicular**2, axis=1)
    if np.any(radius_squared < -1e-12):
        raise ValueError('surface gradient cannot fit this recipe')
    radii = np.sqrt(np.maximum(0, radius_squared) / squared[variable])
    lower = reference_height + np.max(centres - radii) if len(centres) else -np.inf
    upper = reference_height + np.min(centres + radii) if len(centres) else np.inf
    axis['surface'] = (xy, weights, biases, reference_height)
    return lower, upper


def surface_faces(axis, height, datum):
    xy, weights, biases, reference = axis['surface']
    levels = reference + weights * (height - reference) + biases - datum
    floor = np.concatenate([xy, levels[:, :, None]], axis=2)
    sides = []
    if axis['bridge']:
        sides.extend([np.asarray(triangle)[::-1] - [0, 0, .25] for triangle in floor])
        for a, b in zip(axis['footprint'].exterior.coords, list(axis['footprint'].exterior.coords)[1:]):
            stations = np.array([axis['line'].project(Point(p)) for p in (a, b)])
            if np.all(stations < 1e-7) or np.all(stations > axis['distance'][-1] - 1e-7):
                continue
            levels = height_at(axis, height, stations) - datum
            a, b = np.r_[a, levels[0]], np.r_[b, levels[1]]
            sides.append([a - [0, 0, .25], b - [0, 0, .25],
                          b + [0, 0, 1.1], a + [0, 0, 1.1]])
    return floor, sides


def check_surfaces(axes, heights):
    worst = 0.
    for axis in axes:
        faces, _ = surface_faces(axis, heights[axis['owner']], 0)
        triangles = np.asarray(faces)
        normals = np.cross(triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0])
        if np.any(normals[:, 2] < 1e-10):
            raise ValueError('degenerate or folded road surface')
        grades = np.linalg.norm(normals[:, :2], axis=1) / abs(normals[:, 2])
        if np.max(grades) > axis['maximum'] + 1e-8:
            raise ValueError('meshed surface exceeds class gradient')
        worst = max(worst, float(np.max(grades)))
    return worst


def check_clearances(axes, heights, pairs):
    floors = [np.asarray(surface_faces(axis, heights[axis['owner']], 0)[0]) for axis in axes]
    plans = [np.asarray([shapely.Polygon(tri[:, :2]) for tri in made], dtype=object) for made in floors]
    checked = 0
    for a, b in pairs:
        low, high = sorted((a, b), key=lambda i: axes[i]['owner'])
        overlaps = shapely.STRtree(plans[high]).query(plans[low], predicate='intersects')
        for first, second in overlaps.T:
            xy = shapely.get_coordinates(plans[low][first].intersection(plans[high][second]))
            if not len(xy):
                continue
            values = []
            for triangle in (floors[low][first], floors[high][second]):
                gradient = np.linalg.solve(triangle[1:, :2] - triangle[0, :2],
                                           triangle[1:, 2] - triangle[0, 2])
                values.append((xy - triangle[0, :2]) @ gradient + triangle[0, 2])
            gap = values[1] - values[0]
            if axes[low]['owner'] == axes[high]['owner']:
                if np.max(abs(gap)) > 1e-8:
                    raise ValueError('same-level meshes disagree inside their shared core')
            elif np.min(gap) < axes[low]['clearance'] + .25 - 1e-8:
                raise ValueError('mesh clearance violated inside the footprint overlap')
            checked += 1
    return checked


def verify():
    rng = np.random.default_rng(2169)
    feasible = infeasible = 0
    for count in (1, 2, 3):
        for _ in range(60):
            target = rng.uniform(-10, 10, count)
            lower = rng.uniform(-15, 5, count)
            upper = lower + rng.uniform(0, 20, count)
            gaps = [(i, j, float(rng.uniform(1, 7))) for i in range(count)
                    for j in range(i + 1, count) if rng.random() < .7]
            actual = solve(target, lower, upper, gaps)
            expected = reference(target, lower, upper, gaps)
            if actual is None:
                assert expected.status == 2
                infeasible += 1
                continue
            heights, error = actual
            assert expected.success and abs(error - expected.fun) < 1e-7
            assert np.all(heights >= lower - 1e-8) and np.all(heights <= upper + 1e-8)
            assert all(heights[b] - heights[a] >= gap - 1e-8 for a, b, gap in gaps)
            feasible += 1
            batched, errors, valid = solve_batch(target[None, :], lower[None, :], upper[None, :], gaps)
            assert valid[0] and abs(errors[0] - error) < 1e-10
            assert np.max(abs(batched[0] - heights)) < 1e-10
    assert feasible and infeasible
    return dict(feasible=feasible, infeasible=infeasible, lp_comparisons=feasible + infeasible,
                algorithm='Topological lower/target envelopes; exact minimax displacement; O(V+E).')


def benchmark(output):
    rng = np.random.default_rng(2281)
    rows = []
    for nodes in (300, 3000, 30000):
        for levels in (1, 2, 3):
            target = rng.uniform(-5, 5, (nodes // levels, levels))
            lower, upper = np.full_like(target, -20), np.full_like(target, 20)
            gaps = [(i, i + 1, 6.) for i in range(levels - 1)]
            durations = []
            for _ in range(7):
                begin = time.perf_counter()
                heights, error, valid = solve_batch(target, lower, upper, gaps)
                durations.append((time.perf_counter() - begin) * 1000)
            assert np.all(valid) and np.all(heights <= upper + 1e-8)
            assert all(np.all(heights[:, b] - heights[:, a] >= gap - 1e-8) for a, b, gap in gaps)
            for index in (0, len(target) // 2, len(target) - 1):
                scalar = solve(target[index], lower[index], upper[index], gaps)
                assert scalar is not None and abs(scalar[1] - error[index]) < 1e-8
            rows.append(dict(nodes=nodes, levels=levels, independent_components=len(target),
                             edges=len(gaps) * len(target), input_array_bytes=3 * target.nbytes,
                             p50_ms=float(np.median(durations)), maximum_ms=max(durations)))
    result = dict(scope='Equal total pose-node counts, independent local DAGs; solver only. '
                  'No full-network, geometry-preparation, GPU or A18 proof.', measurements=rows)
    (output / 'recipe-solver-cost.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result), flush=True)


def render(args, name, axes, heights, report):
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    from matplotlib.lines import Line2D
    from mpl_toolkits.mplot3d.art3d import Poly3DCollection
    colours = ['#a9b3a1', '#b9a893', '#7c999e']
    fig = plt.figure(figsize=(13, 6), constrained_layout=True)
    top = fig.add_subplot(121)
    view = fig.add_subplot(122, projection='3d', proj_type='ortho')
    crossings = [a['footprint'].intersection(b['footprint']) for i, a in enumerate(axes)
                 for b in axes[i + 1:] if a['footprint'].intersects(b['footprint'])]
    centre = shapely.get_coordinates(shapely.union_all(crossings)).mean(axis=0)
    datum = float(min(a['points'][:, 2].min() for a in axes))
    for axis in axes:
        colour = '#167dba' if axis['bridge'] else colours[axis['owner']]
        plot_geometry(top, [axis['footprint']], colour)
        floor, sides = surface_faces(axis, heights[axis['owner']], datum)
        view.add_collection3d(Poly3DCollection(floor, facecolor=colour, edgecolor='none'))
        if sides:
            view.add_collection3d(Poly3DCollection(sides, facecolor='#6f7d7e', edgecolor='none'))
    top.set(xlim=(centre[0] - 60, centre[0] + 60), ylim=(centre[1] - 60, centre[1] + 60),
            aspect='equal', xlabel='East [m]', ylabel='North [m]', title='2D footprint / levels')
    if any(a['bridge'] for a in axes):
        top.legend(handles=[Line2D([], [], color='#167dba', label='Bridge')], loc='upper right')
    view.set(xlim=(centre[0] - 90, centre[0] + 90), ylim=(centre[1] - 90, centre[1] + 90),
             zlim=(0, 30), xlabel='East [m]', ylabel='North [m]', zlabel=f'Height above {datum:.1f} m',
             title='Rigid cores / smooth approaches / U sides')
    view.set_box_aspect((180, 180, 45))
    view.view_init(elev=28, azim=-55)
    fig.suptitle(f"{name}: {report['levels']} local levels; endpoints, gradient and clearance passed\n"
                 'Local profiles; horizontal curvature and native network integration remain open')
    path = args.output / '2_5d' / f'{name}.png'
    path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(path, dpi=120)
    plt.close(fig)
    return str(path)


def joined_junction_plan(args, name, axes, context):
    if len({a['tier'] for a in axes}) != 1:
        raise ValueError('junction floors cannot merge different physical levels')
    node_sets = [{p['node'] for p in context['designed'][a['lane']] if p.get('node')}
                 for a in axes]
    shared_nodes = set.intersection(*node_sets)
    if not shared_nodes:
        raise ValueError('this connected-junction recipe requires a shared native port')
    began = time.perf_counter()
    footprints = [shapely.set_precision(a['footprint'], 0) for a in axes]
    core = shapely.union_all([shapely.set_precision(a['core_footprint'], 0) for a in axes])
    whole = shapely.union_all(footprints)
    boundaries = shapely.union_all([core.boundary, *[p.boundary for p in footprints]])
    cells = shapely.get_parts(shapely.polygonize(shapely.get_parts(boundaries)))
    cells = cells[shapely.covers(whole, shapely.point_on_surface(cells))]
    canonical, owners = [], []
    for cell in cells:
        point = cell.representative_point()
        owner = None if core.covers(point) else next(
            (i for i, footprint in enumerate(footprints) if footprint.covers(point)), -1)
        if owner == -1:
            raise ValueError('approach surface requires a physical owner')
        rings = [np.round(np.asarray(r.coords) / .001) * .001
                 for r in [cell.exterior, *cell.interiors]]
        cell = shapely.Polygon(rings[0], rings[1:])
        if cell.area > 1e-10:
            if not cell.is_valid:
                raise ValueError('shared grid vertices require a different junction recipe')
            canonical.append(cell)
            owners.append(owner)
    cells = np.asarray(canonical, dtype=object)
    core = shapely.union_all([cell for cell, owner in zip(cells, owners) if owner is None])
    joined_outline = shapely.union_all(cells)
    error = joined_outline.symmetric_difference(whole).area
    if error > (whole.length + core.length) * .001 * 2**.5:
        raise ValueError('junction arrangement exceeds its coordinate quantization error')
    whole = joined_outline
    triangles = np.concatenate([triangles_of(cell) for cell in cells])
    faces = np.asarray([np.asarray(p.exterior.coords)[:3] for p in triangles])
    vertices, indices = np.unique(faces.reshape(-1, 2), axis=0, return_inverse=True)
    indices = indices.reshape(-1, 3)
    edges = np.sort(np.concatenate([indices[:, [0, 1]], indices[:, [1, 2]],
                                   indices[:, [2, 0]]]), axis=1)
    edges, counts = np.unique(edges, axis=0, return_counts=True)
    if np.any(counts > 2):
        raise ValueError('junction floor has non-manifold edges')
    boundary = vertices[edges[counts == 1]].mean(axis=1)
    if np.any(shapely.distance(shapely.points(boundary), whole.boundary) > 1e-7):
        raise ValueError('junction floor has a crack or a T-junction')
    polygons = shapely.polygons(faces)
    merged = shapely.union_all(polygons)
    if (merged.symmetric_difference(whole).area > 1e-7
            or abs(float(shapely.area(polygons).sum()) - whole.area) > 1e-7):
        raise ValueError('junction floor has overlapping or missing triangles')
    elapsed = (time.perf_counter() - began) * 1000
    import matplotlib.pyplot as plt
    fig, views = plt.subplots(1, 2, figsize=(10, 5), constrained_layout=True)
    for axis in axes:
        plot_geometry(views[0], [axis['footprint']], '#adb8a8')
    plot_geometry(views[1], [core], '#83a6a5')
    for axis in axes:
        plot_geometry(views[1], [axis['footprint'].difference(core)], '#adb8a8')
    views[1].triplot(vertices[:, 0], vertices[:, 1], indices, color='#344f50', linewidth=.55)
    centre = core.centroid.coords[0]
    for ax, title in zip(views, ('Independent buffered bands / overlap', 'One core / trimmed approaches / shared vertices')):
        ax.set(xlim=(centre[0] - 30, centre[0] + 30), ylim=(centre[1] - 30, centre[1] + 30),
               aspect='equal', title=title, xlabel='East [m]', ylabel='North [m]')
    fig.suptitle(f'{name}: native shared port; unique indexed floor, no interior cracks')
    path = args.output / '2d' / f'{name}-joined.png'
    path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(path, dpi=120)
    plt.close(fig)
    return dict(image=str(path), shared_native_nodes=sorted(shared_nodes), vertices=len(vertices),
                triangles=len(indices), shared_edges=int(np.count_nonzero(counts == 2)),
                common_arrangement_cells=len(cells),
                canonical_vertex_grid_m=.001, outline_symmetric_difference_m2=error,
                removed_overlap_m2=sum(a['footprint'].area for a in axes) - whole.area,
                build_and_check_ms=elapsed, scope='One connected 2D native junction; '
                'joined 3D gradients and full-network integration open.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('context', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--margin', type=float, default=4)
    parser.add_argument('--benchmark', action='store_true')
    args = parser.parse_args()
    if not np.isfinite(args.margin) or args.margin < 0:
        parser.error('a finite nonnegative core margin is required')
    verification = verify()
    data = args.context.read_bytes()
    context = json.loads(data)
    cases = {'Zuerich-ground-junction': [2384, 3932],
             'Zuerich-rail-footbridge': [4134, 4345],
             'Zuerich-three-levels': [53, 4134, 4345]}
    reports = []
    for name, lanes in cases.items():
        axes = [band(context, lane) for lane in lanes]
        began = time.perf_counter()
        target, lower, upper, gaps, pairs = plan(axes, args.margin)
        plan_ms = (time.perf_counter() - began) * 1000
        began = time.perf_counter()
        answer = solve(target, lower, upper, gaps)
        elapsed = (time.perf_counter() - began) * 1000
        oracle = reference(target, lower, upper, gaps)
        report = dict(name=name, lanes=lanes, levels=len(target), solver_ms=elapsed, plan_ms=plan_ms,
                      target_heights_m=target.tolist(), lower_m=lower.tolist(), upper_m=upper.tolist(),
                      clearances=gaps, source=str(args.context),
                      source_sha256=hashlib.sha256(data).hexdigest(), margin_m=args.margin,
                      footprint_grid_m=.001,
                      surface_triangles=sum(len(a['surface'][0]) for a in axes),
                      surface_array_bytes=sum(sum(x.nbytes for x in a['surface'][:3]) for a in axes),
                      verification=verification)
        if answer is None:
            if oracle.status != 2:
                raise ValueError('bounded solver falsely rejected a feasible local plan')
            report.update(status='infeasible', action='Needs more approach space or a different rigid recipe.')
        else:
            heights, error = answer
            if not oracle.success or abs(error - oracle.fun) > 1e-7:
                raise ValueError('bounded solver differs from the LP optimum')
            for low, high, gap in gaps:
                if heights[high] - heights[low] < gap - 1e-8:
                    raise ValueError('full footprint clearance violated')
            worst = check_profiles(axes, heights)
            surface_grade = check_surfaces(axes, heights)
            overlap_checks = check_clearances(axes, heights, pairs)
            report.update(status='feasible', heights_m=heights.tolist(), displacement_m=error,
                          maximum_gradient=worst, maximum_surface_gradient=surface_grade,
                          triangle_overlap_checks=overlap_checks,
                          lp_displacement_error_m=abs(error - oracle.fun))
            report['image'] = render(args, name, axes, heights, report)
            if name == 'Zuerich-ground-junction':
                report['joined_2d_plan'] = joined_junction_plan(args, name, axes, context)
        reports.append(report)
        print(json.dumps(report), flush=True)
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / 'recipes.json').write_text(json.dumps(reports, indent=2) + '\n')
    if args.benchmark:
        benchmark(args.output)


if __name__ == '__main__':
    main()
