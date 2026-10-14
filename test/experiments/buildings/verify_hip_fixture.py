"""Read the tracked native fixture offline; never regenerate its expected plans."""

import argparse
import json
from pathlib import Path

import numpy as np
import shapely

from outshine_ref.buildings.modelling import prepare_hip


def verify(row, tolerance):
    source = shapely.Polygon(row['outer'], row['holes'])
    frame = row['frame']
    expected = row['expected']
    points = np.asarray(expected['positions_local_xy_m'])
    distances = np.asarray(expected['wavefront_distances_m'])
    assert points.shape == (len(distances), 2) and np.isfinite(points).all()
    assert distances.min() == 0 and distances.max() > 0
    origin = np.asarray(frame['origin_xy_m'])
    polygons, edges, planes = [], {}, {}
    for facet in expected['facets']:
        xy = points[facet['nodes']]
        polygon = shapely.Polygon(xy+origin)
        assert polygon.is_valid and polygon.area > 0
        gradient = np.asarray(facet['gradient'])
        heights = xy@gradient+facet['intercept_m']
        assert np.max(abs(heights-distances[facet['nodes']])) <= tolerance['wavefront_plane_tolerance_m']
        assert abs(np.linalg.norm(gradient)-1.) < 1e-10
        edges[tuple(facet['source_edge'])] = polygon
        planes[tuple(facet['source_edge'])] = (gradient, facet['intercept_m'])
        polygons.append(polygon)
    coverage = shapely.union_all(polygons)
    area_tolerance = max(1e-7, source.length*tolerance['position_tolerance_m'])
    assert coverage.symmetric_difference(source).area <= area_tolerance
    assert sum(p.area for p in polygons)-coverage.area <= 1e-7
    current = prepare_hip(source)
    assert len(current.facets) == len(edges)
    for facet in current.facets:
        xy = np.asarray(facet.polygon.exterior.coords)+current.origin
        polygon = shapely.Polygon(xy)
        assert polygon.symmetric_difference(edges[facet.boundary]).area <= area_tolerance
        gradient, intercept = planes[facet.boundary]
        stored = (xy-origin)@gradient+intercept
        fresh = (xy-current.origin)@facet.gradient+facet.intercept_m
        assert np.max(abs(stored-fresh)) <= tolerance['wavefront_plane_tolerance_m']
    # Check stored values independently of the fixture producer's factor method.
    for case in expected['cases']:
        factor = case['value']/distances.max() if case['mode'] == 'byHeight' else np.tan(np.deg2rad(case['value']))
        assert abs(factor-case['height_factor']) <= 1e-12*max(1., abs(factor))
        peak_error = abs(distances.max()*factor-current.distances_m.max()*current.factor(case['value'], case['mode']))
        assert peak_error <= tolerance['height_tolerance_m']


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fixture', type=Path, default=Path(__file__).parent/'reference/hip-plans.json')
    args = parser.parse_args()
    data = json.loads(args.fixture.read_text())
    assert data['schema'] == 'outshine.hip-plan-reference/1'
    assert len(data['records']) == 48
    ids = [row['id'] for row in data['records']]
    assert len(set(ids)) == 48
    for row in data['records']:
        try:
            verify(row, data['comparison'])
        except Exception as error:
            raise AssertionError(row['id']) from error
    print('48 offline fixtures: planes, coverage, source-edge ownership and both height modes verified')
