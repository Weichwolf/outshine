"""Execute external Esri reference rules on pinned OSM footprints and compare controlled views."""

import argparse
import hashlib
import json
from pathlib import Path
import time

import numpy as np
import pyprt

from demand import View
from esri_catalog import EXAMPLES, EXAMPLES_COMMIT
from evidence import revision
from facade_plan import plan
from preview import configuration, house_view, run_render
from provider_contract import initial_shape
from samples import read_site


CASES = (
    ('vienna-compact', 'Vienna', 'Vienna:1511580:38'),
    ('vienna-courtyard', 'Vienna', 'Vienna:1565540:9'),
    ('tokyo-compact', 'Tokyo', 'Tokyo:1019257600:10'),
)


def geometry_digest(vertices, indices):
    positions = np.rint(vertices[indices]*1.e8).astype(np.int64)
    first = np.lexsort((positions[:, :, 2], positions[:, :, 1], positions[:, :, 0]), axis=1)[:, 0]
    offsets = (np.arange(3)[None, :]+first[:, None]) % 3
    canonical = positions[np.arange(len(indices))[:, None], offsets].reshape(-1, 9)
    order = np.lexsort(tuple(canonical[:, i] for i in range(8, -1, -1)))
    return hashlib.sha256(canonical[order].tobytes()).hexdigest()


def shape(sample):
    descriptor = initial_shape(sample)
    geometry = descriptor['geometry']
    vertices = np.asarray(geometry['vertices'])
    vertices[:, 1] += descriptor['attributes']['minHeight']
    holes, current = [], []
    for index in geometry['holes']:
        if index == 0xffffffff:
            holes.append(current)
            current = []
        else:
            current.append(index)
    return pyprt.InitialShape(vertices.ravel().tolist(), geometry['face_indices'], geometry['face_counts'], holes), descriptor


def generate(sample, rule='candler.rpk', overrides=None):
    initial, descriptor = shape(sample)
    parameters = descriptor['attributes']
    colour = '#'+''.join(f'{round(parameters[k]*255):02x}' for k in ('wallColorR', 'wallColorG', 'wallColorB'))
    attrs = dict(seed=descriptor['random_seed'], shapeName=descriptor['name'])
    if rule == 'candler.rpk':
        attrs.update(BuildingHeight=parameters['bodyHeight'], WallTexture='', ColorizeWall=colour)
    elif rule == 'extrusion_rule.rpk':
        attrs.update(minBuildingHeight=parameters['bodyHeight'], maxBuildingHeight=parameters['bodyHeight'], buildingColor=colour)
    attrs.update(overrides or {})
    began = time.perf_counter()
    generator = pyprt.ModelGenerator([initial])
    models = generator.generate_model([attrs], str(EXAMPLES/'data'/rule), 'com.esri.pyprt.PyEncoder',
                                      {'emitGeometry': True, 'emitReport': True, 'triangulate': True})
    elapsed = (time.perf_counter()-began)*1000
    if len(models) != 1:
        raise ValueError('Expected one generated reference model')
    model = models[0]
    vertices = np.asarray(model.get_vertices(), dtype=np.float64).reshape(-1, 3)
    indices = np.asarray(model.get_indices(), dtype=np.int64).reshape(-1, 3)
    if not len(vertices) or not np.isfinite(vertices).all() or any(n != 3 for n in model.get_faces()):
        raise ValueError('Reference did not yield finite triangulated geometry')
    if indices.min() < 0 or indices.max() >= len(vertices):
        raise ValueError('Invalid reference indices')
    triangles = vertices[indices]-vertices.mean(0)
    volume = abs(float(np.einsum('ij,ij->i', triangles[:, 0], np.cross(triangles[:, 1], triangles[:, 2])).sum()/6))
    low, high = vertices.min(0), vertices.max(0)
    record = dict(rule=rule, rule_sha256=hashlib.sha256((EXAMPLES/'data'/rule).read_bytes()).hexdigest(),
                  example_commit=EXAMPLES_COMMIT, prt_version=pyprt.get_api_version(), source_id=sample['id'],
                  source_sha256=sample['source_sha256'], source_properties=sample['properties'],
                  source_area_m2=sample['polygon'].area, source_holes=len(sample['polygon'].interiors),
                  provider_interval_m=[parameters['minHeight'], parameters['buildingHeight']],
                  input_attributes=attrs, evaluated_attributes=model.get_attributes(),
                  vertices=len(vertices), triangles=len(indices), bounds_prt_m=[low.tolist(), high.tolist()],
                  height_above_provider_m=float(high[1]-parameters['buildingHeight']),
                  surface_integral_m3=volume, surface_integral_scope='Volume only for a closed surface', geometry_generation_ms=elapsed,
                  geometry_sha256=geometry_digest(vertices, indices), canonical_triangle_quantum_m=1.e-8,
                  buffer_sha256=hashlib.sha256(vertices.tobytes()+indices.tobytes()).hexdigest(),
                  cga_messages=model.get_cga_errors(), cga_report=model.get_report())
    return generator, attrs, record


def export_obj(generator, attrs, rule, output):
    output.mkdir(parents=True, exist_ok=True)
    began = time.perf_counter()
    generator.generate_model([attrs], str(EXAMPLES/'data'/rule), 'com.esri.prt.codecs.OBJEncoder',
                             {'outputPath': str(output.resolve()), 'baseName': 'reference'})
    paths = sorted(output.glob('reference*.obj'))
    if len(paths) != 1 or not paths[0].stat().st_size:
        raise ValueError('Expected one exported reference OBJ')
    return paths[0], (time.perf_counter()-began)*1000


def views(product):
    eye, target = house_view(product)
    delta = eye-target
    orbit = np.array([-delta[1], delta[0], delta[2]])
    return dict(near=View(eye, target), orbit=View(target+orbit, target), far=View(target+delta*4, target))


def render_case(case, sample, record, obj, output, methods=('esri', 'own'), chosen_views=('near', 'orbit', 'far')):
    product = plan(sample)
    for name, view in views(product).items():
        if name not in chosen_views:
            continue
        for method in (m for m in methods if name == 'near' or m == 'esri'):
            stem = f'{method}-candler-{case}-{name}' if method == 'esri' else f'own-{case}-{name}'
            image_path = output/(stem+'.png')
            config, counts = configuration([product], view, True, image_path)
            if method == 'esri':
                low, high = np.asarray(record['bounds_prt_m'])
                bounds = [[low[0], -high[2], low[1]], [high[0], -low[2], high[1]]]
                config.update(products=[], tiles={}, solar_tile=None, reference_obj=str(obj.resolve()), reference_bounds_m=bounds)
                counts.update(vertices=record['vertices'], triangles=record['triangles'], openings=None,
                              floor_bands=None, planned_features=None, demand_and_emission_ms=None, selected_faces=None)
            rendered = run_render(config, output/(stem+'-scene.json'))
            report = dict(**revision(), **counts, **rendered, profile=config['profile'],
                          source_id=sample['id'], source_sha256=sample['source_sha256'],
                          comparison_case=case, view=name, mode='reference' if method == 'esri' else 'detail',
                          representation='esri-reference' if method == 'esri' else 'provider-context',
                          reference=record if method == 'esri' else None,
                          recipes=[] if method == 'esri' else [product['recipe']],
                          limitations=['Esri reference assets are external examples, not own generated architecture.',
                                       'Candler adds cornices and a roof above its height parameter; measured bounds are retained.',
                                       'PRT 3.3 reference execution; local SDK 3.4 documentation is inventoried separately.',
                                       'Eevee still rendering, no native frame-time measurement.'])
            (output/(stem+'-report.json')).write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n')
            print(json.dumps(dict(case=case, method=method, view=name, image=str(image_path),
                                  triangles=counts['triangles'], seconds=rendered['render_seconds'])), flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=Path('build/experiments/buildings/esri-oracle'))
    parser.add_argument('--cases', nargs='+', choices=[c[0] for c in CASES])
    parser.add_argument('--render', action='store_true')
    parser.add_argument('--methods', nargs='+', choices=('esri', 'own'), default=['esri', 'own'])
    parser.add_argument('--views', nargs='+', choices=('near', 'orbit', 'far'), default=['near', 'orbit', 'far'])
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    rows, sites = [], {}
    for case, site, identifier in CASES:
        if args.cases and case not in args.cases:
            continue
        if site not in sites:
            sites[site] = read_site(site, context=True)[0]
        sample = next(s for s in sites[site] if s['id'] == identifier)
        generator, attrs, record = generate(sample)
        obj, export_ms = export_obj(generator, attrs, record['rule'], args.output/case)
        record.update(case=case, export_ms=export_ms, obj=str(obj))
        rows.append(record)
        if args.render:
            render_case(case, sample, record, obj, args.output, args.methods, args.views)
    summary = dict(**revision(), external_reference_families=1, reference_variants=len(rows),
                   own_architecture_accepted=False, cases=rows,
                   scope='Candler reference examples on real provider footprints, not a reconstruction claim.')
    (args.output/'oracle-summary.json').write_text(json.dumps(summary, ensure_ascii=False, indent=2)+'\n')
    print(json.dumps(dict(reference_families=1, reference_variants=len(rows), output=str(args.output))), flush=True)


if __name__ == '__main__':
    main()
