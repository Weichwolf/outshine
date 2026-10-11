"""Emit only demanded geometry and render controlled near/shell and city diagnostics."""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
import time

import numpy as np
import shapely

from codec import encode
from demand import View, select
from evidence import revision
from facade_geometry import mesh
from facade_plan import Repeat, coverage, plan
from samples import read_site


def weights(style, side=128):
    w, h = (.8, .58) if style == 'bauhaus' else (.5, .72)
    y, x = np.mgrid[:side, :side]
    points = (np.stack((x, y), axis=-1)+.5)/side
    outer = coverage(Repeat(1., 1., 1, 1, w, h), points, [1/side, 1/side])
    inner = coverage(Repeat(1., 1., 1, 1, w-.05, h-.05), points, [1/side, 1/side])
    rgba = np.stack((1-outer, outer-inner, inner, np.ones_like(inner)), axis=-1).astype(np.float32)
    return dict(size=side, rgba=rgba.ravel().tolist())


def configuration(products, view, detail, output, reference=False):
    objects, counts = [], dict(source_products=len(products), admitted_products=0, selected_faces=0,
                               vertices=0, triangles=0, openings=0, detail_fallbacks=[])
    began = time.perf_counter()
    for product in products:
        demand = select(product, view)
        if reference:
            for request in demand['faces']:
                request['detail'] = bool(product['facades'][request['edge']]['repeat'].count)
        geometry = mesh(product, demand, detail)
        if not len(geometry['faces']):
            continue
        counts['admitted_products'] += 1
        counts['selected_faces'] += len(demand['faces'])
        counts['vertices'] += len(geometry['vertices'])
        counts['triangles'] += len(geometry['faces'])
        counts['openings'] += geometry['detail_openings']
        counts['detail_fallbacks'].extend(dict(f, id=product['id']) for f in geometry['detail_fallbacks'])
        objects.append(dict(id=product['id'], style=product['style'], wall_linear=product['wall_linear'].tolist(),
                            mesh={k: geometry[k].tolist() for k in ('vertices', 'faces', 'uv', 'materials')}))
    counts['demand_and_emission_ms'] = (time.perf_counter()-began)*1000
    bounds = np.array([p['polygon'].bounds for p in products])
    span = max(50., float(np.max(bounds[:, 2:].max(axis=0)-bounds[:, :2].min(axis=0))))
    result = dict(profile=[view.width, view.height], eye=view.eye.tolist(), target=view.target.tolist(),
                  fov_deg=view.fov_deg, products=objects, output=str(output.resolve()),
                  ground_span_m=max(span*6, float(np.linalg.norm(view.eye-view.target))*3),
                  tiles={s: weights(s) for s in ('bauhaus', 'artdeco')})
    return result, counts


def run_render(config, path):
    path.write_text(json.dumps(config, separators=(',', ':'))+'\n')
    commit = subprocess.check_output(['git', 'rev-parse', '--short', 'HEAD'], text=True).strip()
    log = Path(tempfile.gettempdir())/f'outshine-buildings-{path.stem}-{commit}.log'
    began = time.perf_counter()
    with log.open('w') as handle:
        subprocess.run(['blender', '--background', '--factory-startup', '--python',
                        str(Path(__file__).with_name('render_blender.py')), '--', str(path)],
                       stdout=handle, stderr=subprocess.STDOUT, check=True, timeout=600)
    if not Path(config['output']).is_file():
        raise RuntimeError(f'No rendering; inspect {log}')
    renderer = [json.loads(line) for line in log.read_text().splitlines() if line.startswith('{"renderer":')]
    if not renderer:
        raise RuntimeError(f'Missing terminal renderer status; inspect {log}')
    return dict(render_seconds=time.perf_counter()-began, log=str(log), renderer=renderer[-1],
                image_sha256=hashlib.sha256(Path(config['output']).read_bytes()).hexdigest())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=Path('build/experiments/buildings/preview'))
    parser.add_argument('--style', choices=('bauhaus', 'artdeco'), default='bauhaus')
    parser.add_argument('--lod', choices=('detail', 'shell', 'reference'), default='detail')
    parser.add_argument('--distance', type=float, default=1.)
    parser.add_argument('--site', default='Vienna')
    parser.add_argument('--city', action='store_true')
    parser.add_argument('--prepare-only', action='store_true')
    args = parser.parse_args()
    if not np.isfinite(args.distance) or args.distance <= 0:
        parser.error('--distance must be positive and finite')
    args.output.mkdir(parents=True, exist_ok=True)
    samples, receipt = read_site(args.site)
    if args.city:
        chosen = [s for s in samples if s['polygon'].distance(shapely.Point(0, 0)) < 300]
        products = []
        receipt['planning_rejected'] = []
        for sample in chosen:
            try:
                products.append(plan(sample))
            except ValueError as error:
                receipt['planning_rejected'].append(dict(id=sample['id'], reason=str(error)))
        eye, target = np.array([-300., -380., 200.]), np.array([0., 0., 20.])
    else:
        chosen = [s for s in samples if 150 < s['polygon'].area < 500 and len(s['polygon'].exterior.coords) < 15
                  and not s['polygon'].interiors and 8 <= float(s['properties'].get('render_height', 9.)) <= 20]
        sample = min(chosen, key=lambda s: s['polygon'].distance(shapely.Point(0, 0)))
        products = [plan(sample, args.style)]
        centre = np.array(products[0]['polygon'].centroid.coords[0])
        span = max(products[0]['polygon'].bounds[2]-products[0]['polygon'].bounds[0],
                   products[0]['polygon'].bounds[3]-products[0]['polygon'].bounds[1])
        target = np.r_[centre, products[0]['top_m']*.45]
        facade = max(products[0]['facades'], key=lambda f: f['repeat'].width_m)
        tangent = np.r_[(facade['b']-facade['a'])/facade['repeat'].width_m, 0.]
        eye = target+(2.1*facade['normal']-1.7*tangent)*span+[0., 0., span]
        (args.output/(args.style+'-enriched-plan.json')).write_bytes(encode(products[0]))
    eye = target+(eye-target)*args.distance
    view = View(eye, target, width=1280, height=720, movement_m=1.)
    name = f'{args.site.lower()}-city' if args.city else args.style
    name += '-'+args.lod
    if args.distance != 1:
        name += '-distance'+str(args.distance)
    config, counts = configuration(products, view, args.lod != 'shell', args.output/(name+'.png'), args.lod == 'reference')
    counts.update(revision(), source_sha256=receipt['receipt']['sha256'], profile=[view.width, view.height],
                  mode=args.lod, distance=args.distance, demand='facing/frustum/one-metre movement reserve',
                  limitations=['no scene occlusion', 'shadow completeness unproven', 'flat-roof facade pilot',
                               'coverage-weighted BSDFs omit recess microshadows and offset reflections'])
    (args.output/(name+'-source.json')).write_text(json.dumps(receipt, indent=2)+'\n')
    config_path = args.output/(name+'-scene.json')
    config_path.write_text(json.dumps(config, separators=(',', ':'))+'\n')
    if not args.prepare_only:
        counts.update(run_render(config, config_path))
    (args.output/(name+'-report.json')).write_text(json.dumps(counts, indent=2)+'\n')
    print(json.dumps(counts), flush=True)


if __name__ == '__main__':
    main()
