"""Check coarse shadow closure and recessed volumes independently of camera-facing selection."""

import json
from pathlib import Path
from unittest.mock import patch

import numpy as np
import shapely

from codec import decode, encode
from demand import View, select
from evidence import revision
from facade_geometry import mesh
from facade_plan import cell_range, plan


def signed_volume(geometry):
    vertices = geometry['vertices']
    triangles = vertices[geometry['faces']]-vertices.mean(0)
    return float(np.einsum('ij,ij->i', triangles[:, 0], np.cross(triangles[:, 1], triangles[:, 2])).sum()/6)


def removed_recess_volume(product, demand):
    volume = 0.
    for request in demand['faces']:
        repeat = product['facades'][request['edge']]['repeat']
        if not request['detail'] or not repeat.count:
            continue
        x0, y0, x1, y1 = cell_range(repeat, request['region'])
        width, height = repeat.width_m/repeat.columns, repeat.height_m/repeat.rows
        outer = width*height*repeat.opening_width*repeat.opening_height
        inner = width*height*(repeat.opening_width-.05)*(repeat.opening_height-.05)
        volume += (x1-x0)*(y1-y0)*(outer*.04+inner*(repeat.depth_m-.04))
    return volume


def main():
    rectangle = [(0, 0), (20, 0), (20, 20), (0, 20)]
    polygons = [shapely.Polygon(rectangle), shapely.Polygon(rectangle, [[(5, 5), (5, 15), (15, 15), (15, 5)]]),
                shapely.Polygon([(0, 0), (20, 0), (20, 8), (8, 8), (8, 20), (0, 20)])]
    poses = [(np.array([-40., -50., 30.]), np.array([10., 10., 6.])),
             (np.array([40., 50., 30.]), np.array([10., 10., 6.])),
             (np.array([1., -3., 7.]), np.array([10., 10., 6.]))]
    rows = []
    for ordinal, polygon in enumerate(polygons):
        product = decode(encode(plan(dict(id=f'shadow-{ordinal}', polygon=polygon,
                                         properties={'render_height': 12., 'render_min_height': -2.}))))
        expected = polygon.area*14.
        for eye, target in poses:
            demand = select(product, View(eye, target))
            if not demand['faces'] and not demand['caps']:
                raise AssertionError('The control body must be admitted')
            with patch('shapely.constrained_delaunay_triangles', side_effect=AssertionError('cached hull rebuilt')):
                camera = mesh(product, demand, detail=False)
                shadow = mesh(product, demand, detail=False, include_shadow_hull=True)
            error = abs(signed_volume(shadow)-expected)
            if error > expected*1.e-10:
                raise AssertionError('The shadow hull loses source volume or orientation')
            rows.append(dict(footprint=ordinal, volume_m3=expected, volume_error_m3=error,
                             camera_triangles=len(camera['faces']), shadow_triangles=len(shadow['faces'])))
    product = plan(dict(id='recesses', polygon=shapely.box(0, 0, 20, 12),
                        properties={'building': 'apartments', 'render_height': 12.}))
    demand = select(product, View(np.array([-25., -35., 14.]), np.array([10., 6., 6.])))
    fine = mesh(product, demand, include_shadow_hull=True)
    coarse = mesh(product, demand, detail=False, include_shadow_hull=True)
    removed = removed_recess_volume(product, demand)
    error = abs(signed_volume(coarse)-signed_volume(fine)-removed)
    if error > 1.e-7 or fine['detail_fallbacks'] or not fine['detail_openings']:
        raise AssertionError('The complete shadow hull fills recesses or leaves a camera crop open')
    report = dict(**revision(), status='pass', closed_shadow_cases=rows,
                  recessed_volume_m3=removed, recessed_volume_error_m3=error,
                  detail_openings=fine['detail_openings'],
                  scope='Cached coarse hull of admitted bodies; off-camera casters and decorated shadow LOD remain open.')
    output = Path('build/experiments/buildings/shadow-validation.json')
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(dict(status='pass', closed_shadow_cases=len(rows), recess_volume_error_m3=error)))


if __name__ == '__main__':
    main()
