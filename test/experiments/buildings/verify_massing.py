"""Volume, terraces, balconies and source-free replay of bounded building massing."""

import json
from pathlib import Path
import subprocess
import sys
import tempfile
from unittest.mock import patch

import numpy as np
import shapely

from codec import decode, encode
from demand import View, select
from facade_geometry import mesh
from facade_plan import plan
from massing import MIN_EDGE_M
from massing_reference import check_measurements, union
from offline_probe import probe
from preview import house_view
from samples import read_site
from verify import verify_closed_shell


def verify_exterior(product):
    verify_closed_shell(product)
    metrics = check_measurements(product, product['shell'])
    reference = union(product)
    check_measurements(product, reference)
    assert set(reference['materials']) == set(product['shell']['materials'])
    shell, layers = product['shell'], product['layers']
    for index, layer in enumerate(layers):
        polygons = [shapely.Polygon(t[:, :2]) for t in shell['vertices'][shell['faces'][shell['parts'] == -index-1]]]
        terrace = shapely.union_all(polygons)
        expected = layer['footprint'].difference(layers[index+1]['footprint']) if index+1 < len(layers) else layer['footprint']
        assert terrace.symmetric_difference(expected).area < 1e-8
        assert all(layer['footprint'].covers(triangle) for triangle in polygons)
    return dict(**metrics, direct_triangles=len(shell['faces']), union_triangles=len(reference['faces']))


def verify_features(product):
    geometry = product['decoration']
    checked = 0
    for index, feature in enumerate(product['features']):
        points = geometry['vertices'][geometry['faces'][geometry['parts'] == index]]
        normals = np.cross(points[:, 1]-points[:, 0], points[:, 2]-points[:, 0])
        assert np.isfinite(points).all() and np.all(np.linalg.norm(normals, axis=1) > 1e-10)
        if feature['kind'] in ('balcony', 'canopy'):
            # Every group of twelve triangles is one independently closed box.
            for box in points.reshape(-1, 12, 3, 3):
                local = box-box.mean(axis=(0, 1))
                box_normals = np.cross(local[:, 1]-local[:, 0], local[:, 2]-local[:, 0])
                assert np.einsum('ij,ij->i', local[:, 0], box_normals).sum()/6 > 0
                assert np.all(np.einsum('ij,ij->i', local.mean(1), box_normals) > 0)
            if feature['kind'] == 'balcony':
                facade = product['facades'][feature['facade']]
                repeat = facade['repeat']
                floor = repeat.height_m/repeat.rows
                row = round((feature['floor_m']-facade['bottom_m'])/floor-.04)
                door_base = facade['bottom_m']+(row+(1-repeat.opening_height)*.5)*floor
                assert abs(feature['floor_m']-door_base) < 1e-9
                assert feature['floor_m']+.95 < facade['top_m']
        elif feature['kind'] == 'roof-pv':
            polygon = shapely.from_wkt(feature['wkt'])
            assert product['layers'][-1]['footprint'].covers(polygon)
            assert abs(normals[:, 2].sum()*.5-polygon.area) < 1e-8 and np.all(normals[:, 2] > 0)
        else:
            facade = product['facades'][feature['facade']]
            assert np.all(normals @ facade['normal'] > 0)
            repeat = facade['repeat']
            floor = repeat.height_m/repeat.rows
            _, bottom, _, top = feature['rectangle']
            row = int(bottom/floor)
            assert top < (row+(1-repeat.opening_height)*.5)*floor
        checked += 1
    return checked


def verify_replay(product):
    encoded = encode(product)
    with patch('facade_plan.author', side_effect=AssertionError('hit authored building')), \
         patch('massing.stack', side_effect=AssertionError('hit rebuilt massing')), \
         patch('shapely.constrained_delaunay_triangles', side_effect=AssertionError('hit triangulated')):
        loaded = decode(encoded)
        assert encode(loaded) == encoded
        covered = set()
        for angle in np.linspace(0, 2*np.pi, 16, endpoint=False):
            centre = np.r_[product['polygon'].centroid.coords[0], (product['top_m']+product['bottom_m'])*.5]
            view = View(centre+[60*np.cos(angle), 60*np.sin(angle), 30.], centre)
            request = select(loaded, view)
            covered.update(f['edge'] for f in request['faces'])
            original, hit = mesh(product, request), mesh(loaded, request)
            for key in ('vertices', 'faces', 'uv', 'materials'):
                np.testing.assert_array_equal(original[key], hit[key])
        assert covered == set(range(len(product['facades'])))
        eye, target = house_view(loaded)
        view = View(eye, target, width=1280, height=720)
        projected = (np.r_[loaded['shell']['vertices'], loaded['decoration']['vertices']]-view.eye) @ view.basis.T
        pixels = projected[:, :2]*view.focal_px/projected[:, 2:]
        assert np.all(abs(pixels) < [view.width*.4, view.height*.4])
    return encoded


def verify_touching_courtyard():
    samples, _ = read_site('Vienna')
    sample = next(sample for sample in samples if sample['id'] == 'Vienna:1209980:25')
    polygon = sample['polygon']
    assert polygon.is_valid and len(polygon.interiors) == 1
    assert not polygon.exterior.intersection(polygon.interiors[0]).is_empty
    product = plan(sample, 'bauhaus')
    reference = union(product)
    metrics = check_measurements(product, product['shell'])
    check_measurements(product, reference)
    assert set(reference['materials']) == set(product['shell']['materials'])
    verify_replay(product)
    return dict(source_id=sample['id'], preserved_point_touching_courtyard=True, **metrics)


def verify_redundant_vertices():
    polygon = shapely.Polygon([(0, 0), (40, 0), (40, 40), (0, 40)],
                              [[(12, 12), (12, 28), (28, 28), (28, 12)]])
    rings = [np.repeat(np.asarray(ring.coords), 2, axis=0) for ring in [polygon.exterior, *polygon.interiors]]
    redundant = shapely.Polygon(rings[0], rings[1:])
    assert redundant.is_valid and redundant.equals(polygon)
    for style in ('bauhaus', 'artdeco', 'midcentury'):
        sample = dict(id='duplicate-courtyard', polygon=polygon, properties={'height': 18.})
        expected = plan(sample, style)
        actual = plan(dict(sample, polygon=redundant), style)
        assert encode(expected) == encode(actual)
        verify_exterior(actual)
    return dict(styles=3, identical_geometry_and_cached_bytes=True)


def verify_numerical_edges():
    sources = {'Vienna': {'Vienna:112890:37', 'Vienna:112890:123', 'Vienna:1141390:20', 'Vienna:2124964612:0'},
               'Tokyo': {'Tokyo:1147552190:4'}}
    checked, omitted = [], []
    for site, ids in sources.items():
        samples, receipt = read_site(site)
        selected = [sample for sample in samples if sample['id'] in ids]
        suppressed = [row for row in receipt['omitted'] if row['id'] in ids]
        assert len(selected)+len(suppressed) == len(ids)
        assert all(row['reason'] == 'provider_hide_3d' for row in suppressed)
        omitted.extend(row['id'] for row in suppressed)
        for sample in selected:
            product = plan(sample, 'bauhaus')
            assert product['polygon'].equals(sample['polygon'])
            for layer, offset in zip(product['layers'], product['massing']['offsets_m']):
                expected = sample['polygon'].buffer(-offset, join_style='mitre') if offset else sample['polygon']
                assert expected.hausdorff_distance(layer['footprint']) <= MIN_EDGE_M
                assert expected.symmetric_difference(layer['footprint']).area <= MIN_EDGE_M*expected.length
            check_measurements(product, product['shell'])
            check_measurements(product, union(product))
            encoded = encode(product)
            assert encode(decode(encoded)) == encoded
            checked.append(sample['id'])
    return dict(source_ids=checked, provider_omitted_sources=omitted, maximum_boundary_displacement_m=MIN_EDGE_M)


def verify():
    courtyard = shapely.Polygon([(0, 0), (40, 0), (40, 40), (0, 40)], [[(12, 12), (12, 28), (28, 28), (28, 12)]])
    dumbbell = shapely.union_all([shapely.box(0, 0, 12, 12), shapely.box(11, 5.4, 25, 6.6), shapely.box(24, 0, 36, 12)])
    shapes = [shapely.box(-10, -8, 10, 8), courtyard, dumbbell, shapely.box(0, 0, 40, 1.5)]
    rows, features, last = [], 0, None
    for index, polygon in enumerate(shapes):
        for style in ('bauhaus', 'artdeco', 'midcentury'):
            product = plan(dict(id=f'massing-{index}', polygon=polygon,
                                properties={'height': 18., 'min_height': -2., 'building:levels': 6}), style)
            assert product['layers'][0]['footprint'].equals(polygon)
            assert len(product['layers']) <= (2 if style == 'bauhaus' else 1 if style == 'midcentury' else 3)
            assert sum(layer['floors'] for layer in product['layers']) == 6
            if index == 2 and style != 'midcentury':
                assert product['layers'][-1]['footprint'].geom_type == 'MultiPolygon'
            if index == 3:
                assert len(product['layers']) == 1 and product['massing']['fallback_reason']
            metrics = verify_exterior(product)
            features += verify_features(product)
            encoded = verify_replay(product)
            rows.append(dict(shape=index, style=style, layers=len(product['layers']), **metrics))
            if index == 0 and style == 'bauhaus':
                last = encoded
    closed = plan(dict(id='utility', polygon=shapes[0], properties={'height': 24., 'building': 'silo'}))
    part = plan(dict(id='part', polygon=shapes[0], properties={'height': 18., 'building:part': 'yes'}))
    assert len(closed['layers']) == len(part['layers']) == 1
    assert not any(f['kind'] in ('balcony', 'facade-pv') for f in closed['features'])
    with tempfile.TemporaryDirectory(prefix='outshine-tests.outshine_buildings.') as directory:
        path = Path(directory)/'styled-plan.json'
        path.write_bytes(last)
        actual = json.loads(subprocess.check_output([sys.executable, str(Path(__file__).with_name('offline_probe.py')), str(path)],
                                                    text=True, timeout=30))
        assert actual == probe(last)
    records = [json.loads(last) for _ in range(4)]
    records[0]['product']['layers'][1]['bottom_m'] += 1
    records[1]['product']['surfaces'][0]['bounds'][0] += 1
    records[2]['product']['feature_bounds'][0][0] += 1
    records[3]['product']['opening_patterns'][0][1] = .1
    for record in records:
        try:
            decode(json.dumps(record))
        except ValueError:
            pass
        else:
            raise AssertionError('inconsistent styled plan accepted')
    return dict(cases=rows, validated_features=features, camera_views=16*len(rows),
                fresh_styled_offline_hit=True, invalid_records_rejected=len(records),
                touching_courtyard=verify_touching_courtyard(), redundant_vertices=verify_redundant_vertices(),
                numerical_edges=verify_numerical_edges())


if __name__ == '__main__':
    print(json.dumps(verify(), indent=2))
