"""Complete planned shell: independent measures, every slab section and actual actor rays."""

import argparse
import json
from pathlib import Path
import resource
from types import SimpleNamespace
import time

import manifold3d as manifold
import numpy as np
import shapely

from boolean_building_bands import bands,construct
from boolean_building_plan import sources,construct as csg_construct
from boolean_sequence_reference import reference,section,solid_section
from building_vertical import build,level
from evidence import revision
from samples import read_site
from structural_shell_geometry import native,mesh_section,assembly,NativeColumns,inspect_poses
from verify_boolean_sequence import verify as verify_csg
from verify_boolean_surfaces import measure
from verify_vertical import approach,landing_poses,connected_spaces


def cases():
    yield dict(id='two-storey-control',polygon=shapely.box(0,0,14,12),
        properties={'render_height':6.4,'render_min_height':0,'building:levels':2},source_sha256=None)
    for site,key in [('Vienna','Vienna:800900:15'),('Tokyo','Tokyo:1366089900:122')]:
        samples,_=read_site(site);yield next(row for row in samples if row['id']==key)


def expression(prisms):
    entries=[name for name in prisms if name!='outer']
    def union(items):
        if len(items)==1:return items[0]
        split=len(items)//2;return ('union',union(items[:split]),union(items[split:]))
    return ('subtract','outer',union(entries))


def geometry(plan,prisms,shell):
    tree=expression(prisms);expected=reference(prisms,tree)
    actual=measure(SimpleNamespace(mesh=shell.mesh,roots=shell.vertex_roots))
    np.testing.assert_allclose(actual,expected,atol=1e-7,rtol=1e-9)
    solid=native(shell.mesh,shell.vertex_roots);levels=sorted({z for prism in prisms.values() for z in (prism.low,prism.high)})
    error=0.
    for low,high in zip(levels[:-1],levels[1:],strict=True):
        height=(low+high)/2;wanted=section(prisms,tree,height);actual=mesh_section(shell.mesh,shell.vertex_roots,height)
        error=max(error,wanted.symmetric_difference(actual).area)
    if error>1e-7:raise ValueError('Compiled shell differs from an independently planned slab section')
    values=shell.mesh['vertices'];np.testing.assert_allclose([values[:,2].min(),values[:,2].max()],
        [plan['base_z_m'],plan['body_top_m']],atol=1e-8,rtol=0.)
    declared=bands(plan);assert shell.source_ids==tuple(name for name,_,_ in declared)
    np.testing.assert_array_equal(shell.mesh['materials'],np.asarray([material for _,_,material in declared])[shell.source_indices])
    return dict(volume_m3=expected[0],area_m2=expected[1],slabs=len(levels)-1,maximum_section_error_m2=error)


def access(plan,collision):
    connected_spaces(plan);columns=NativeColumns(collision,plan['base_z_m']-1,plan['body_top_m']+1)
    tested=0;minimum=float('inf');landing=approach(plan)
    for index in range(plan['storeys']):
        current=level(plan,index)
        for door in current['doors']:
            centre,tangent=np.asarray(door['centre']),np.asarray(door['tangent']);normal=np.array([tangent[1],-tangent[0]])
            xy=centre+np.linspace(-.45,.45,10)[:,None]*normal
            poses=np.c_[xy,np.full(len(xy),current['floor_z_m'])]
            minimum=min(minimum,inspect_poses(columns,poses));tested+=len(poses)
    for index in range(plan['storeys']-1):
        poses=plan['stair']['path'].copy();poses[:,2]+=index*plan['stair']['rise_m']
        minimum=min(minimum,inspect_poses(columns,poses));tested+=len(poses)
        swept=[]
        for a,b in zip(poses[:-1],poses[1:],strict=True):
            count=max(2,int(np.ceil(np.linalg.norm(b[:2]-a[:2])/.10))+1);points=np.linspace(a,b,count);points[:,2]=max(a[2],b[2]);swept.extend(points)
        minimum=min(minimum,inspect_poses(columns,np.asarray(swept),feet=False));tested+=len(swept)
        ends=np.r_[landing_poses(np.r_[landing,poses[:1,:2]],poses[0,2]),landing_poses(np.r_[poses[-1:,:2],landing[::-1]],poses[-1,2])]
        minimum=min(minimum,inspect_poses(columns,ends));tested+=len(ends)
    return dict(sampled_poses=tested,sampled_min_headroom_m=minimum,torso_radius_m=.30,body_height_m=1.90,native_player_controller=False)


def run(compare):
    results=[]
    for sample in cases():
        start=time.perf_counter();plan=build(sample);planning_ms=(time.perf_counter()-start)*1000
        if plan.get('vertical_status')!='connected-storeys':raise ValueError(sample['id']+': '+str(plan['omissions']))
        prisms=sources(plan);record=dict(id=sample['id'],source_sha256=sample.get('source_sha256'),storeys=plan['storeys'],
            rooms=len(plan['rooms'])*plan['storeys'],planning_ms=planning_ms,original_sources=len(prisms),strategies=[])
        for repeat in range(3):
            start=time.perf_counter();shell=construct(plan);elapsed=(time.perf_counter()-start)*1000
            metrics=geometry(plan,prisms,shell)
            record['strategies'].append(dict(strategy='planned-bands',repeat=repeat,status='pass',generation_ms=elapsed,triangles=len(shell.mesh['faces']),**metrics))
        manifold.Manifold.reserve_ids(101);replay=construct(plan)
        for key in shell.mesh:np.testing.assert_array_equal(shell.mesh[key],replay.mesh[key])
        np.testing.assert_array_equal(shell.source_indices,replay.source_indices);np.testing.assert_array_equal(shell.source_triangles,replay.source_triangles)
        _,collision=assembly(plan,shell);record.update(access(plan,collision))
        if compare:
            for strategy in ('balanced-cutters','sequential-cuts'):
                started=time.perf_counter()
                try:
                    _,node,tree=csg_construct(prisms,strategy);elapsed=(time.perf_counter()-started)*1000;verify_csg(node,reference(prisms,tree))
                    row=dict(strategy=strategy,status='pass',generation_ms=elapsed,triangles=len(node.origins))
                except Exception as error:row=dict(strategy=strategy,status='rejected',elapsed_ms=(time.perf_counter()-started)*1000,reason=str(error))
                record['strategies'].append(row)
        results.append(record);print(sample['id'],'pass',record['sampled_poses'],'poses',flush=True)
    return results


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--compare-csg',action='store_true')
    parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/structural-shell-verification.json'))
    args=parser.parse_args();rows=run(args.compare_csg)
    report=revision()|dict(status='planned-shell-pass',cases=rows,process_peak_rss_mib=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss/1024,
        scope='Complete existing planned structural shells, independent volume/surface/every height slab, immutable geometry/source replay and actual Native ray torso/foot checks. CSG comparisons retain rejections; planned constructor does not replace arbitrary inherited CGA UV/tag/material semantics. No recipe-family/native player/frame/A18 admission.')
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(report,indent=2)+'\n')
