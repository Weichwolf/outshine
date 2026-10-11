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


def render_spatial(mesh_path, output, commit, footprint):
    script = Path(__file__).with_name('render_plan_blender.py')
    log_path = Path(tempfile.gettempdir())/f'outshine-buildings-plan-{mesh_path.stem}-{commit[:9]}.log'
    with tempfile.TemporaryDirectory(prefix='outshine-buildings-plan-render-') as temporary:
        directory = Path(temporary)
        image = directory/'spatial.png'
        batch = directory/'batch.json'
        batch.write_text(json.dumps([dict(mesh=str(mesh_path.resolve()), output=str(image), camera_tangent=frame(footprint)[0][:2])]))
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


def probe(sample, output, provenance, circulation=False):
    began = time.perf_counter()
    generator, verifier = build, verify
    if circulation:
        from building_circulation import build as generator
        from verify_circulation import verify as verifier
    plan = generator(sample)
    plan_ms = (time.perf_counter()-began)*1000
    product, checks, mesh_ms, spatial_path, spatial_metadata = None, None, None, None, None
    if plan['status'] == 'connected-first-storey':
        began = time.perf_counter()
        product = mesh(plan)
        mesh_ms = (time.perf_counter()-began)*1000
        checks = verifier(plan, product)
        mesh_path = output/(sample['label']+'-mesh.npz')
        np.savez_compressed(mesh_path, **product)
        spatial_path = output/(sample['label']+'-spatial.png')
        spatial_metadata = render_spatial(mesh_path, spatial_path, provenance['commit'], plan['footprint'])
    stem = ('circulation-' if circulation else 'plan-')+sample['label']
    image = output/(stem+'.png')
    figure(sample, plan, product, image, spatial_path)
    plan_data = json.dumps(plan, default=encode, ensure_ascii=False, indent=2)+'\n'
    (output/(sample['label']+'-plan.json')).write_text(plan_data)
    report = dict(**provenance, representation='functional-plan', source_id=sample['id'], label=sample['label'],
                  plan_status=plan['status'], source_sha256=sample.get('source_sha256'), renderer='Matplotlib 2D sections / Blender Workbench',
                  spatial_reference=spatial_metadata,
                  profile=[1600, 1000], image_sha256=hashlib.sha256(image.read_bytes()).hexdigest(),
                  rooms=len(plan['rooms']), doors=len(plan['doors']), openings=len(plan['windows']),
                  triangles=len(product['faces']) if product is not None else 0,
                  plan_json_bytes=len(plan_data.encode()), raw_mesh_bytes=sum(array.nbytes for array in product.values()) if product else 0,
                  generation_ms=dict(plan=plan_ms, mesh=mesh_ms), alternatives=plan['alternatives'], checks=checks,
                  stage='circulation' if circulation else 'partition',
                  circulation=plan.get('circulation'), circulation_status=plan.get('circulation_status'),
                  core_alternatives=plan.get('core_alternatives'), room_kinds=plan.get('room_kinds'),
                  limitations=plan['omissions'], source_height_interval_m=[plan['base_z_m'], plan['source_top_m']],
                  method=plan.get('method'), source_coverage='Ground-floor prototype; no completed building or recipe-family claim')
    (output/(stem+'-report.json')).write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n')
    print(json.dumps(dict(case=sample['label'], status=plan['status'], rooms=len(plan['rooms']), plan_ms=plan_ms)), flush=True)
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--cases', nargs='+')
    parser.add_argument('--circulation', action='store_true', help='compare common access with reserved cores')
    args = parser.parse_args()
    if args.output is None:
        args.output = Path('build/experiments/buildings')/('circulation-plans' if args.circulation else 'functional-plans')
    args.output.mkdir(parents=True, exist_ok=True)
    provenance = revision()
    rows = [probe(sample, args.output, provenance, args.circulation) for sample in cases()
            if args.cases is None or sample['label'] in args.cases]
    result = dict(**provenance, cases=rows, methods=['doglegs', 'orthogonal-grid'] if args.circulation else ['strips', 'bsp'],
                  max_rooms=24 if args.circulation else 12,
                  scope='Fixed-envelope first storey; actual wall apertures and mesh-derived reachability, no native frame measurement')
    (args.output/'layout-summary.json').write_text(json.dumps(result, ensure_ascii=False, indent=2)+'\n')


if __name__ == '__main__':
    main()
