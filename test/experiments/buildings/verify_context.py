"""Compare exact indexed/exhaustive joins and test host scope, topology and offline assets."""

import argparse
from collections import Counter
import json
from pathlib import Path
import resource
import sys
import time
from unittest.mock import patch

import numpy as np
import shapely

from codec import decode, encode
from context import attach
from context_catalog import LAYER_ROLES, POI_ROLES, catalog
from demand import View, select
from evidence import revision
from facade_geometry import mesh
from facade_plan import plan
from provider_contract import initial_shape
from recipes import choose
from samples import INVENTORY, read_site


def feature(geometry, properties, identifier):
    return dict(id=identifier, geometry=json.loads(shapely.to_geojson(geometry)), properties=properties)


def analytic():
    courtyard = shapely.Polygon([(0, 0), (20, 0), (20, 20), (0, 20)],
                               [[(5, 5), (5, 10), (10, 10), (10, 5)]])
    samples = [dict(id='house', polygon=courtyard, properties=dict(render_height=12, render_min_height=0)),
               dict(id='neighbour', polygon=shapely.box(20, 0, 30, 20), properties=dict(render_height=12, render_min_height=0)),
               dict(id='floating-part', polygon=shapely.box(0, 0, 5, 5), properties=dict(render_height=15, render_min_height=12))]
    data = {
        'landuse': [feature(shapely.box(-50, -50, 50, 50), {'class':'residential'}, 1),
                    feature(shapely.box(0, 0, 20, 20), {'class':'commercial'}, 2)],
        'poi': [feature(shapely.Point(3, 3), {'class':'shop', 'subclass':'bakery', 'level':0, 'indoor':1, 'name:de':'Backerei'}, 3),
                feature(shapely.Point(7, 7), {'class':'school'}, 4),
                feature(shapely.Point(20, 3), {'class':'office'}, 5),
                feature(shapely.Point(35, 3), {'class':'hospital'}, 6),
                feature(shapely.Point(3, 3), {'class':'shop', 'level':4}, 7)],
        'housenumber': [feature(shapely.Point(2, 18), {'housenumber':'17A'}, 8)],
        'transportation': [feature(shapely.LineString([(0, -5), (20, -5)]), {'class':'minor', 'surface':'asphalt', 'name':'Road'}, 9),
                           feature(shapely.LineString([(0, -4), (20, -4)]), {'class':'minor', 'brunnel':'bridge', 'layer':1}, 10)],
        'boundary': [feature(shapely.LineString([(0, 30), (30, 30)]), {'admin_level':4, 'name':'Region'}, 11)],
    }
    layers = {key:dict(features=rows) for key, rows in data.items()}
    report = attach(samples, layers, lambda xy, _:xy, 'analytic')
    context = samples[0]['context']
    assert context['site_use']['value'] == 'commercial'
    bakery = next(r for r in context['evidence'] if r['properties'].get('subclass') == 'bakery')
    assert bakery['scope'] == 'partial-use' and bakery['level'] == 0 and bakery['properties']['name:de'] == 'Backerei'
    reasons = {r['id'].split(':')[1]:r['reason'] for r in report['unused']}
    assert reasons['4'] == 'no containing host' and reasons['5'] == 'ambiguous host' and reasons['6'] == 'no containing host'
    assert reasons['7'] == 'ambiguous host'
    roads = [r for r in context['evidence'] if r['layer'] == 'transportation']
    assert next(r for r in roads if r['properties'].get('brunnel') == 'bridge')['usable_frontage'] is False
    assert next(r for r in roads if r['properties'].get('surface') == 'asphalt')['usable_frontage'] is True
    product = plan(samples[0])
    assert product['recipe']['family'] == 'commercial' and product['source_properties'].get('building') is None
    assert product['recipe']['origin'] == 'context-heuristic' and product['polygon'].equals(courtyard)
    assert any(f['repeat'].count for f in product['facades']) and not product['features']
    assert any(f['frontage_sources'] for f in product['facades'])
    shape = initial_shape(samples[0])
    assert shape['attributes']['houseNumbers'] == ['17A'] and shape['attributes']['localUseLevels'] == ['0']
    assert 'streetWidth' not in shape['attributes']
    supplied = dict(samples[0], properties={'building':'residential', 'render_height':12., 'colour':'8080ff'})
    chosen = plan(supplied)
    assert chosen['recipe']['family'] == 'residential' and chosen['recipe']['origin'] == 'supplied'
    np.testing.assert_allclose(chosen['wall_linear'], [(128/255/1.055+.055/1.055)**2.4]*2+[1.])
    conflict = dict(layers, landuse=dict(features=[feature(shapely.box(-1, -1, 31, 21), {'class':'commercial'}, 20),
                                                 feature(shapely.box(-1, -1, 31, 21), {'class':'industrial'}, 21)]))
    attach(samples, conflict, lambda xy, _:xy, 'analytic')
    assert samples[0]['context']['site_use']['value'] == 'mixed'
    assert choose({}, samples[0]['context'], courtyard, 12., 0.)['family'] == 'mixed-use'
    touching = shapely.MultiPolygon([shapely.box(0, 0, 10, 20), shapely.box(10, 0, 20, 20)])
    fragmented = dict(layers, landuse=dict(features=[feature(touching, {'class':'residential'}, 30)]))
    fragmented_report = attach(samples, fragmented, lambda xy, _:xy, 'analytic')
    assert not fragmented_report['rejected'] and samples[0]['context']['site_use']['value'] == 'residential'
    slender = choose({}, samples[0]['context'], courtyard, 200., 0.)
    assert slender['family'] == 'residential'
    return dict(courtyard_exclusion=True, shared_boundary_ambiguous=True, outside_not_nearest=True,
                elevated_parts_and_level_scope=True, nested_sites=True, conflicting_sites=True,
                explicit_tags_and_colours_win=True, bridged_road_not_ground_frontage=True,
                touching_aggregated_parcels=True, use_precedes_slenderness=True)


def check_cache(product):
    encoded = encode(product)
    with patch('samples.read_site', side_effect=AssertionError('cache read provider')), \
         patch('context.attach', side_effect=AssertionError('cache repeated join')), \
         patch('facade_plan.author', side_effect=AssertionError('cache rebuilt recipe')), \
         patch('shapely.constrained_delaunay_triangles', side_effect=AssertionError('cache triangulated shell')):
        cached = decode(encoded)
        assert encode(cached) == encoded and cached['context'] == product['context'] and cached['recipe'] == product['recipe']
        centre = np.r_[product['polygon'].centroid.coords[0], (product['top_m']+product['bottom_m'])/2]
        for angle in np.arange(8)*np.pi/4:
            view = View(centre+np.array([np.cos(angle), np.sin(angle), .35])*200, centre)
            first, second = mesh(product, select(product, view)), mesh(cached, select(cached, view))
            for key in ('vertices', 'faces', 'uv', 'materials'):
                np.testing.assert_array_equal(first[key], second[key])
    return len(encoded)


def real_sites():
    results = []
    for site in ('Vienna', 'Tokyo', 'CentralPark'):
        raw, source = read_site(site)
        indexed_samples, indexed = read_site(site, context=True)
        families, scoped, cached, bytes_total, cached_families = Counter(), 0, 0, 0, set()
        for sample in indexed_samples:
            context = sample['context']
            properties = sample['properties']
            if properties['render_height'] <= properties['render_min_height']:
                continue
            recipe = choose(properties, context, sample['polygon'], properties['render_height'], properties['render_min_height'])
            families[recipe['family']] += 1
            scoped += sum(r.get('scope') == 'partial-use' for r in context['evidence'])
            if recipe['family'] != 'unknown' and (cached < 16 or recipe['family'] not in cached_families):
                product = plan(sample)
                assert product['polygon'].equals(sample['polygon']) and product['top_m'] == properties['render_height']
                bytes_total += check_cache(product); cached += 1; cached_families.add(recipe['family'])
        selected = raw[:64]
        from samples import SOURCES, source_bytes
        import mapbox_vector_tile
        import math
        from samples import GIRTH_M
        layers = mapbox_vector_tile.decode(source_bytes(source['receipt']), default_options={'y_coord_down':True})
        latitude, longitude = source['origin_lat_lon']
        z, tx, ty = source['receipt']['tile']
        span, scale = GIRTH_M/(1 << z), math.cos(math.radians(latitude))
        anchor = np.array([longitude/360*GIRTH_M, math.asinh(math.tan(math.radians(latitude)))*GIRTH_M/(2*math.pi)])
        def project(xy, size):
            return ((xy/size+[tx, ty])*[span, -span]+[-GIRTH_M/2, GIRTH_M/2]-anchor)*scale
        began = time.perf_counter()
        fast = attach(selected, layers, project, source['receipt']['sha256'])
        reference = [s['context'] for s in selected]
        fast_ms = (time.perf_counter()-began)*1000
        slow = attach(selected, layers, project, source['receipt']['sha256'], indexed=False)
        assert reference == [s['context'] for s in selected]
        assert fast['unused'] == slow['unused'] and fast['rejected'] == slow['rejected']
        results.append(dict(site=site, buildings=len(indexed_samples), families=dict(families), partial_poi_uses=scoped,
                            context_preparation_ms=indexed['context']['preparation_ms'], context_features=indexed['context']['projected_context_features'],
                            cached_plans=cached, cached_pose_comparisons=cached*8, cache_payload_bytes=bytes_total,
                            identical_reference_buildings=len(selected), indexed_ms=fast_ms, exhaustive_ms=slow['preparation_ms'],
                            source_sha256=source['receipt']['sha256'], layers=indexed['context']['layers'],
                            unused_context_counts=dict(Counter(r['reason'] for r in indexed['context']['unused'])),
                            rejected_context=indexed['context']['rejected']))
        print(json.dumps({k:results[-1][k] for k in ('site','families','partial_poi_uses','context_preparation_ms','indexed_ms','exhaustive_ms')}), flush=True)
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=Path('build/experiments/buildings/context-validation.json'))
    args = parser.parse_args()
    inventory = json.loads(INVENTORY.read_text())
    assert set(inventory['layers']) <= LAYER_ROLES.keys()
    assert set(inventory['layers']['poi']['values']['class']) <= POI_ROLES.keys()
    result = dict(**revision(), analytic=analytic(), sites=real_sites(), poi_classes=len(inventory['layers']['poi']['values']['class']),
                  poi_subclasses=len(inventory['layers']['poi']['values']['subclass']), layer_roles=len(LAYER_ROLES),
                  process_peak_rss_bytes=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss*(1 if sys.platform == 'darwin' else 1024),
                  scope='Python preprocessing and local/cache invariants; no native frame/GPU/A18 proof')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2)+'\n')
    print(json.dumps(dict(output=str(args.output), success=True)), flush=True)


if __name__ == '__main__':
    main()
