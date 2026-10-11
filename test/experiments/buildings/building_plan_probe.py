"""Compare bounded room partitions and inspect camera-independent first-storey geometry."""

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import time

import numpy as np
import shapely

from building_mesh import mesh
from building_layout import frame
from building_plan import build
from building_plan_cases import cases
from building_plan_views import figure
from evidence import revision
from verify_layout import verify
from PIL import Image


def encode(value):
    if isinstance(value, shapely.Geometry):
        return dict(wkb_hex=shapely.to_wkb(value, hex=True))
    if isinstance(value, np.ndarray):
        return value.tolist()
    if isinstance(value, np.generic):
        return value.item()
    raise TypeError(type(value).__name__)


def render_spatial(mesh_path, output, commit, footprint, view='storey', camera_tangent=None):
    script = Path(__file__).with_name('render_plan_blender.py')
    log_path = Path(tempfile.gettempdir())/f'outshine-buildings-plan-{output.stem}-{commit[:9]}.log'
    with tempfile.TemporaryDirectory(prefix='outshine-buildings-plan-render-') as temporary:
        directory = Path(temporary)
        image = directory/'spatial.png'
        batch = directory/'batch.json'
        batch.write_text(json.dumps([dict(mesh=str(mesh_path.resolve()), output=str(image), view=view,
                                         camera_tangent=frame(footprint)[0][:2] if camera_tangent is None else camera_tangent)]))
        command = ['blender', '--background', '--factory-startup', '--threads', '2', '--python-exit-code', '1',
                   '--python', str(script.resolve()), '--', '--batch', str(batch)]
        with log_path.open('w') as log:
            subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, timeout=60, check=True)
        metadata = json.loads(image.with_suffix('.json').read_text())
        if (metadata['mesh_sha256'] != hashlib.sha256(mesh_path.read_bytes()).hexdigest()
                or metadata['image_sha256'] != hashlib.sha256(image.read_bytes()).hexdigest()):
            raise ValueError('Fresh spatial render does not match its source mesh')
        with Image.open(image) as picture:
            if list(picture.size) != metadata['profile']:
                raise ValueError('Spatial render profile mismatch')
            picture.verify()
        shutil.copy2(image, output)
        shutil.copy2(image.with_suffix('.json'), output.with_suffix('.json'))
    return metadata


def probe(sample, output, provenance, circulation=False, vertical=False):
    began = time.perf_counter()
    generator, verifier, mesher, draw = build, verify, mesh, figure
    if vertical:
        from building_vertical import build as generator, mesh as mesher
        from building_vertical_views import figure as draw
        from verify_vertical import verify as verifier
    elif circulation:
        from building_circulation import build as generator
        from verify_circulation import verify as verifier
    plan = generator(sample)
    plan_ms = (time.perf_counter()-began)*1000
    product, checks, mesh_ms, spatial_path, spatial_metadata, core_path, core_metadata = (None,)*7
    usable = plan.get('vertical_status') == 'connected-storeys' if vertical else plan['status'] == 'connected-first-storey'
    if usable:
        began = time.perf_counter()
        product = mesher(plan)
        mesh_ms = (time.perf_counter()-began)*1000
        checks = verifier(plan, product)
        mesh_path = output/(sample['label']+'-mesh.npz')
        np.savez_compressed(mesh_path, **product)
        spatial_path = output/(sample['label']+'-spatial.png')
        spatial_metadata = render_spatial(mesh_path, spatial_path, provenance['commit'], plan['footprint'])
        if vertical and 'stair' in plan:
            core_path = output/(sample['label']+'-stair-spatial.png')
            core_metadata = render_spatial(mesh_path,core_path,provenance['commit'],plan['footprint'],
                                           view='core',camera_tangent=plan['stair']['across'])
    stem = ('vertical-' if vertical else 'circulation-' if circulation else 'plan-')+sample['label']
    image = output/(stem+'.png')
    if vertical:
        draw(sample,plan,product,image,spatial_path,core_path)
    else:
        draw(sample,plan,product,image,spatial_path)
    plan_data = json.dumps(plan, default=encode, ensure_ascii=False, indent=2)+'\n'
    (output/(sample['label']+'-plan.json')).write_text(plan_data)
    report = dict(**provenance, representation='functional-plan', source_id=sample['id'], label=sample['label'],
                  plan_status=plan.get('vertical_status') if vertical else plan['status'], source_sha256=sample.get('source_sha256'), renderer='Matplotlib 2D sections / Blender Workbench',
                  spatial_reference=spatial_metadata, core_reference=core_metadata,
                  profile=[1600, 1000], image_sha256=hashlib.sha256(image.read_bytes()).hexdigest(),
                  rooms=checks['spaces'] if vertical and checks else len(plan['rooms']),
                  doors=sum(len(row) for row in plan['storey_portals']) if vertical and usable else len(plan['doors']),
                  openings=len(np.unique(product['parts'][product['materials']==3])) if product is not None else 0,
                  triangles=len(product['faces']) if product is not None else 0,
                  plan_json_bytes=len(plan_data.encode()), raw_mesh_bytes=sum(array.nbytes for array in product.values()) if product else 0,
                  generation_ms=dict(plan=plan_ms, mesh=mesh_ms), alternatives=plan['alternatives'], checks=checks,
                  stage='vertical' if vertical else 'circulation' if circulation else 'partition',
                  storeys=plan['storeys'], stair_connections=max(0,plan['storeys']-1) if vertical and usable else 0,
                  vertical_status=plan.get('vertical_status'),
                  circulation=plan.get('circulation'), circulation_status=plan.get('circulation_status'),
                  core_alternatives=plan.get('core_alternatives'), room_kinds=plan.get('room_kinds'),
                  limitations=plan['omissions'], source_height_interval_m=[plan['base_z_m'], plan['source_top_m']],
                  method=plan.get('method'), source_coverage='Source envelope retained; constructive plan omitted with a recorded reason' if not usable else
                  'All source storeys with generic repeated spaces and constructed stairs; no completed architectural programme' if vertical
                  else 'Ground-floor prototype; no completed building or recipe-family claim')
    (output/(stem+'-report.json')).write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n')
    print(json.dumps(dict(case=sample['label'], status=report['plan_status'], rooms=report['rooms'], windows=report['openings'], plan_ms=plan_ms)), flush=True)
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--cases', nargs='+')
    parser.add_argument('--circulation', action='store_true', help='compare common access with reserved cores')
    parser.add_argument('--vertical', action='store_true', help='construct all source storeys with real stairs and slab openings')
    args = parser.parse_args()
    if args.output is None:
        args.output = Path('build/experiments/buildings')/('vertical-plans' if args.vertical else 'circulation-plans' if args.circulation else 'functional-plans')
    args.output.mkdir(parents=True, exist_ok=True)
    provenance = revision()
    rows = [probe(sample, args.output, provenance, args.circulation,args.vertical) for sample in cases()
            if args.cases is None or sample['label'] in args.cases]
    result = dict(**provenance, cases=rows, methods=['doglegs', 'orthogonal-grid'] if args.circulation or args.vertical else ['strips', 'bsp'],
                  max_rooms=24 if args.circulation or args.vertical else 12, max_storeys=64 if args.vertical else 1,
                  scope='Fixed source envelope; sampled actual mesh contacts and access, no native frame measurement')
    (args.output/'layout-summary.json').write_text(json.dumps(result, ensure_ascii=False, indent=2)+'\n')


if __name__ == '__main__':
    main()
