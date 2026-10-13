"""Verify actual opening-component solids, aperture clearances and equivalent construction costs."""

import argparse
import hashlib
import json
from pathlib import Path
import time

import numpy as np
import shapely

from evidence import revision
from opening_details import expand,slots
from opening_modules import CASING_M,FRAME_M,FRAME_MATERIAL,SILL_MATERIAL,template,place
from verify_layout import closed_parts


def decoded(data):
    return shapely.from_wkb(data['wkb_hex']) if set(data)=={'wkb_hex'} else data


def projected(product, material):
    triangles = product['vertices'][product['faces'][product['materials']==material]]
    shapes = [shapely.Polygon(face[:,[0,2]]) for face in triangles]
    return shapely.union_all([polygon for polygon in shapes if polygon.area>1e-10])


def verify_template(kind,width,height,method):
    product = template(kind,width,height,method)
    actual = closed_parts(product)
    if kind=='window':
        inner = width*height-(width-2*FRAME_M)*(height-2*FRAME_M)
        outer = (width+2*CASING_M)*(height+2*CASING_M)-width*height
        expected = inner*.075+outer*.045+(width+2*CASING_M+.05)*.16*.032
        profile = projected(product,FRAME_MATERIAL)
        clear = shapely.box(-width/2+FRAME_M,FRAME_M,width/2-FRAME_M,height-FRAME_M)
        if profile.intersection(clear).area>1e-8 or abs(profile.area-outer-inner)>1e-8:
            raise AssertionError('Window component altered the stated clear aperture or rim coverage')
        sill = projected(product,SILL_MATERIAL)
        if sill.bounds[3]>-CASING_M+1e-8:
            raise AssertionError('Sill intersects the bottom of the casing')
    else:
        expected = (2*CASING_M*height+(width+2*CASING_M)*CASING_M)*.045
        portal = shapely.box(-width/2,0.,width/2,height)
        if projected(product,FRAME_MATERIAL).intersection(portal).area>1e-8:
            raise AssertionError('Entrance frame narrowed or sealed the original door')
    if abs(actual-expected)>1e-8:
        raise AssertionError('Opening solids disagree with independently derived profile volume')
    for angle in (0.,.31,1.2,3.14):
        moved = place(product,[113.2,-418.1],[np.cos(angle),np.sin(angle)],12.3)
        if abs(closed_parts(moved)-expected)>1e-8:
            raise AssertionError('Rigid component placement changed winding or volume')
    return dict(kind=kind,width_m=width,height_m=height,method=method,triangles=len(product['faces']),
                parts=len(np.unique(product['parts'])),volume_m3=actual)


def verify_slots(plan,openings,product):
    if len({row['id'] for row in openings})!=len(openings):
        raise AssertionError('Opening slots lost stable unique identities')
    if len(openings)!=len(np.unique(product['opening_indices'])):
        raise AssertionError('The mesh lost a logical opening')
    total = closed_parts(product)
    room_ids = {identifier for row in plan['storey_room_ids'] for identifier in row}
    for index,row in enumerate(openings):
        if not set(row['room_ids'])<=room_ids or not row['room_ids']:
            raise AssertionError('Opening component has no host room in the existing building plan')
        triangles = product['vertices'][product['faces'][product['opening_indices']==index]]
        angle = np.asarray(row['tangent'])
        world = triangles.reshape(-1,3)-np.array([*row['centre'],row['bottom_z_m']])
        local = np.c_[world[:,:2]@angle,world[:,:2]@np.array([-angle[1],angle[0]]),world[:,2]]
        sill=row['kind']=='window' and row.get('sill',True)
        if not np.allclose(local.min(axis=0),[-row['width_m']/2-CASING_M-(.025 if sill else 0.),
                                             -.16 if sill else -.045,
                                             -CASING_M-.032-.115/12 if sill else -CASING_M if row['kind']=='window' else 0.],atol=1e-8):
            raise AssertionError('Placed module has an unexpected frame/sill envelope')
        if triangles[:,:,2].min()<plan['base_z_m']-1e-8 or triangles[:,:,2].max()>plan['body_top_m']+1e-8:
            raise AssertionError('Opening details escaped the occupied body-height interval')
    return dict(openings=len(openings),windows=sum(r['kind']=='window' for r in openings),
                entrances=sum(r['kind']=='entrance' for r in openings),triangles=len(product['faces']),
                closed_volume_m3=total,stable_room_and_opening_ids=True)


def verify_body_openings(openings,body):
    windows = [row for row in openings if row['kind']=='window']
    pane_parts = np.unique(body['parts'][body['materials']==3])
    if len(pane_parts)!=len(windows):
        raise AssertionError('Detailed openings disagree with the original body panes')
    for row,part in zip(windows,pane_parts):
        faces=body['faces'][body['parts']==part]
        points=body['vertices'][np.unique(faces)]-np.array([*row['centre'],row['bottom_z_m']])
        tangent=np.asarray(row['tangent'])
        local=np.c_[points[:,:2]@tangent,points[:,:2]@np.array([-tangent[1],tangent[0]]),points[:,2]]
        if (not np.allclose(local.min(axis=0),[-row['width_m']/2,.114,0.],atol=1e-8,rtol=0)
                or not np.allclose(local.max(axis=0),[row['width_m']/2,.126,row['height_m']],atol=1e-8,rtol=0)):
            raise AssertionError('Opening descriptor moved away from its actual source pane')


def compare(openings):
    rows = []
    for method in ('ring','bars'):
        for reuse in (False,True):
            times = []
            for repeat in range(22):
                start = time.perf_counter();product=expand(openings,method,reuse)
                if repeat:times.append((time.perf_counter()-start)*1000)
            rows.append(dict(method=method,reuse_templates=reuse,openings=len(openings),repetitions=21,
                             p50_ms=float(np.median(times)),p95_ms=float(np.quantile(times,.95)),
                             triangles=len(product['faces']),array_payload_bytes=sum(a.nbytes for a in product.values())))
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source',type=Path,default=Path('build/experiments/buildings/roof-plans'))
    parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/opening-validation.json'))
    args = parser.parse_args()
    prototypes = [verify_template(kind,w,h,method) for kind,w,h in (('window',1.2,1.2),('window',2.1,1.8),('entrance',1.1,2.15))
                  for method in ('ring','bars')]
    rows = []
    for label in ('rectangle','L-wing','courtyard','vienna-compact','vienna-courtyard','tokyo-compact'):
        path = args.source/f'roof-{label}-flat-plan.json'
        plan=json.loads(path.read_text(),object_hook=decoded)
        openings=slots(plan);product=expand(openings)
        checks=verify_slots(plan,openings,product)
        body_path=path.with_name(path.name.removesuffix('-plan.json')+'-mesh.npz')
        body=dict(np.load(body_path))
        verify_body_openings(openings,body)
        moved=[row|dict(centre=(np.array(row['centre'])+[.2,.1]).tolist()) for row in openings]
        try:
            verify_body_openings(moved,body)
        except AssertionError:
            pass
        else:
            raise AssertionError('Moved opening descriptors passed the independent source-pane check')
        repeat=expand(json.loads(json.dumps(openings)))
        if any(not np.array_equal(product[key],repeat[key]) for key in product):
            raise AssertionError('Source-free opening descriptors changed the expanded mesh')
        report=json.loads(path.with_name(path.name.removesuffix('-plan.json')+'-report.json').read_text())
        rows.append(checks|dict(label=label,source_plan_commit=report['commit'],
                               source_plan_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
                               source_mesh_sha256=hashlib.sha256(body_path.read_bytes()).hexdigest(),shifted_opening_rejected=True))
        print(json.dumps(rows[-1]),flush=True)
    controls=slots(json.loads((args.source/'roof-vienna-compact-flat-plan.json').read_text(),object_hook=decoded))[:48]
    result=dict(**revision(),templates=prototypes,cases=rows,comparison=compare(controls),
                scope='Opening components attached to existing room openings; no complete architecture family, native LOD or lighting admission')
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(dict(status='pass',buildings=len(rows),openings=sum(r['openings'] for r in rows))))


if __name__=='__main__':
    main()
