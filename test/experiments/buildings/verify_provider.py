"""Check source-faithful volumes, PRT axes/holes, omission policy and offline replay."""

import argparse
import json
from pathlib import Path
import resource
import sys
import time
from unittest.mock import patch

import numpy as np
import shapely

from codec import decode, encode
from demand import View, select
from evidence import revision
from facade_geometry import mesh
from facade_plan import plan
from massing_reference import check_measurements
from provider_contract import attributes, hidden, initial_shape
from samples import read_site


def check_shape(sample):
    shape = initial_shape(sample)
    geometry = shape['geometry']
    points = np.array(geometry['vertices'])
    assert np.all(points[:, 1] == 0)
    rings, start = [], 0
    for count in geometry['face_counts']:
        indices = geometry['face_indices'][start:start+count]
        rings.append(points[indices][:, [0, 2]]*[1., -1.])
        start += count
    holes, at = {}, 0
    while at < len(geometry['holes']):
        outer = geometry['holes'][at]
        at += 1
        inner = []
        while geometry['holes'][at] != 0xffffffff:
            inner.append(geometry['holes'][at]); at += 1
        holes[outer] = inner
        at += 1
    inner_indices = {index for values in holes.values() for index in values}
    reconstructed = shapely.union_all([shapely.Polygon(ring, [rings[h] for h in holes.get(i, [])])
                                       for i, ring in enumerate(rings) if i not in inner_indices])
    assert reconstructed.equals(sample['polygon'])
    for i, ring in enumerate(rings):
        xyz = points[np.array(geometry['face_indices'])[sum(geometry['face_counts'][:i]):sum(geometry['face_counts'][:i+1])]]
        normal = np.cross(xyz, np.roll(xyz, -1, axis=0)).sum(axis=0)
        assert normal[1] < 0 if i in inner_indices else normal[1] > 0
    assert -2**31 <= shape['random_seed'] < 2**31
    return shape


def verify_analytic():
    courtyard = shapely.Polygon([(0, 0), (20, 0), (20, 20), (0, 20)], [[(4, 4), (4, 8), (8, 8), (8, 4)]])
    shapes = [shapely.box(0, 0, 10, 7), courtyard,
              shapely.MultiPolygon([courtyard, shapely.box(30, 0, 35, 5)])]
    for index, polygon in enumerate(shapes):
        sample = dict(id=f'prt-{index}', polygon=polygon,
                      properties={'height':12., 'render_height':7., 'min_height':-2., 'colour':'8080ff', 'future':'retained'})
        shape = check_shape(sample)
        assert shape['attributes']['buildingHeight'] == 12. and shape['attributes']['bodyHeight'] == 14.
        np.testing.assert_array_equal([shape['attributes'][k] for k in ('wallColorR','wallColorG','wallColorB')], [128/255,128/255,1.])
        assert shape['source_properties']['future'] == 'retained'
    sample = dict(id='default', polygon=shapes[0], properties={})
    product = plan(sample)
    assert product['style'] == 'osm' and product['massing']['mode'] == 'plain' and not product['features']
    assert all(f['repeat'].count == 0 for f in product['facades'])
    np.testing.assert_array_equal(product['wall_linear'], [1., 1., 1.])
    assert product['provenance']['height'] == 'procedural'
    for value in (True, 1, 'true'):
        try:
            with patch('facade_plan.prepare', side_effect=AssertionError('hidden input built geometry')):
                plan(dict(sample, properties={'hide_3d':value}))
        except ValueError as error:
            assert str(error) == 'provider_hide_3d'
        else:
            raise AssertionError('hidden input was rendered')
    for value in (False, 0, 'false'):
        assert not hidden({'hide_3d':value})
    for properties in ({'hide_3d':'maybe'}, {'render_height':True}, {'render_height':float('nan')},
                       {'render_height':3., 'render_min_height':5.}, {'colour':'#ffffff00'}):
        try:
            attributes(properties)
        except ValueError:
            continue
        raise AssertionError(f'invalid source accepted: {properties}')
    return dict(geometry_cases=3, axes_and_holes=True, source_height_precedence=True, colour_srgb=True,
                white_missing_colour=True, omission_before_mesh=True, invalid_records_rejected=5)


def verify_sources():
    rows = []
    for site in ('Vienna', 'Tokyo', 'CentralPark'):
        samples, receipt = read_site(site)
        accepted, rejected, replayed, generation_ms, payload_bytes = 0, [], 0, 0., 0
        for sample in samples:
            try:
                began = time.perf_counter()
                product = plan(sample)
                generation_ms += (time.perf_counter()-began)*1000
            except ValueError as error:
                rejected.append(dict(id=sample['id'], reason=str(error)))
                continue
            assert len(product['layers']) == 1 and product['layers'][0]['footprint'].equals(sample['polygon'])
            assert product['top_m'] == sample['properties']['render_height']
            assert product['bottom_m'] == sample['properties']['render_min_height']
            assert product['provenance']['height'] == 'provider-derived' and not product['features']
            assert not any(f['repeat'].count for f in product['facades'])
            check_measurements(product, product['shell'])
            payload_bytes += sum(product['shell'][key].nbytes for key in ('vertices','faces','uv','materials'))
            if replayed < 16:
                check_shape(sample)
                encoded = encode(product)
                with patch('facade_plan.plan', side_effect=AssertionError('cache rebuilt plan')), \
                     patch('shapely.constrained_delaunay_triangles', side_effect=AssertionError('cache triangulated')):
                    loaded = decode(encoded)
                    assert encode(loaded) == encoded
                    for angle in np.linspace(0, 2*np.pi, 8, endpoint=False):
                        centre = np.r_[sample['polygon'].centroid.coords[0], (product['top_m']+product['bottom_m'])*.5]
                        view = View(centre+[100*np.cos(angle), 100*np.sin(angle), 40.], centre)
                        a, b = mesh(product, select(product, view)), mesh(loaded, select(loaded, view))
                        for key in ('vertices','faces','uv','materials'):
                            np.testing.assert_array_equal(a[key], b[key])
                replayed += 1
            accepted += 1
        assert receipt['source_polygons'] == len(samples)+len(receipt['rejected'])+len(receipt['omitted'])
        assert all(row['reason'] == 'provider_hide_3d' for row in receipt['omitted'])
        rows.append(dict(site=site, accepted=accepted, omitted=receipt['omitted'], geometry_rejected=receipt['rejected'],
                         planning_rejected=rejected, cached_examples=replayed, source_sha256=receipt['receipt']['sha256'],
                         generation_ms=generation_ms, shell_payload_bytes=payload_bytes))
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=Path('build/experiments/buildings/provider-validation.json'))
    args = parser.parse_args()
    result = dict(**revision(), analytic=verify_analytic(), sites=verify_sources())
    result['whole_process_peak_rss_bytes'] = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss*(1 if sys.platform == 'darwin' else 1024)
    result['measurement_scope'] = 'One validation pass; plan and complete hull generation only; source decode and checks outside timer. RSS covers whole process; no frame/GPU/device measurement.'
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps(dict(output=str(args.output), analytic=result['analytic'],
                          sites=[dict(site=r['site'],accepted=r['accepted'],omitted=len(r['omitted']),
                                      rejected=len(r['planning_rejected']),cached=r['cached_examples']) for r in result['sites']])))


if __name__ == '__main__':
    main()
