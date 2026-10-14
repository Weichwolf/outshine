"""Actual source roofs, room openings, independent lower solids and negative actor controls."""

import argparse
import json
from pathlib import Path
import resource
from types import SimpleNamespace
import time

import manifold3d as manifold
import numpy as np
import shapely

from boolean_building_bands import construct
from boolean_building_plan import sources
from boolean_sequence_reference import section,reference,solid_section
from evidence import revision
from pitched_room_details import complete,structure,plan_for
from provider_contract import attributes
from showcase_tiles import view_for
from structural_shell_geometry import native,mesh_section,assembly
from tile_buildings import SITES,read_tile
from tile_roof_envelopes import choose,roof_surface,pitched_candidates,RoofChoice
from verify_boolean_surfaces import measure
from verify_structural_shell import access,expression
from verify_openings import verify_slots,verify_body_openings
from opening_details import slots,expand
from building_vertical import mesh as body_mesh,level
from outshine_ref.buildings.modelling import heightfield_envelope


def selected(site,rows):
    ids=pitched_candidates(rows,view_for(rows))
    return [row for row in rows if row['id'] in ids]


def geometry(row,p,choice,top,wedge,plan,shell):
    metrics=measure(SimpleNamespace(mesh=shell.mesh,roots=shell.vertex_roots))
    np.testing.assert_allclose(metrics,[shell.volume_m3,shell.area_m2],atol=1e-7,rtol=1e-9)
    prisms=sources(plan);tree=expression(prisms);lower_volume,lower_area=reference(prisms,tree)
    triangles=top['vertices'][top['faces']];vectors=np.cross(triangles[:,1]-triangles[:,0],triangles[:,2]-triangles[:,0])
    roof_volume=float((vectors[:,2]/2*(triangles[:,:,2].mean(axis=1)-choice.eave_m)).sum())
    roof_area=float(np.linalg.norm(vectors,axis=1).sum()/2)
    edges={}
    for triangle in triangles:
        for a,b in zip(triangle,np.roll(triangle,-1,axis=0),strict=True):
            key=tuple(sorted((tuple(a),tuple(b))));edges.setdefault(key,[]).append((a,b))
    boundary=sum(float(np.linalg.norm(b[:2]-a[:2])*((a[2]+b[2])/2-choice.eave_m))
                 for entries in edges.values() if len(entries)==1 for a,b in entries)
    expected=[lower_volume+roof_volume,lower_area+roof_area+boundary-row['polygon'].area]
    np.testing.assert_allclose(metrics,expected,atol=1e-7,rtol=1e-9)
    lower=construct(plan)
    other=manifold.Manifold.batch_boolean([native(lower.mesh,lower.vertex_roots),native(wedge)],manifold.OpType.Add)
    if other.status()!=manifold.Error.NoError:raise ValueError('Independent lower structure/roof composition rejected')
    try:
        np.testing.assert_allclose(metrics,[other.volume(),other.surface_area()],atol=1e-7,rtol=1e-9)
        comparison=dict(status='pass')
    except AssertionError as error:
        comparison=dict(status='rejected',reason=str(error),volume_m3=other.volume(),area_m2=other.surface_area())
    actual=native(shell.mesh,shell.vertex_roots)
    error=(actual-other).volume()+(other-actual).volume()
    if abs(error)>1e-7:raise ValueError('Independent structure occupies different volume')
    levels=sorted({choice.eave_m,p['buildingHeight']}|{z for s in prisms.values() for z in (s.low,s.high)})
    maximum=0.;tested=0
    for a,b in zip(levels[:-1],levels[1:],strict=True):
        if np.nextafter(a,b)>=b:continue
        height=(a+b)/2
        wanted=section(prisms,tree,height) if height<choice.eave_m else shapely.Polygon()
        if height>=choice.eave_m:
            wanted=solid_section(native(wedge),height)
        value=mesh_section(shell.mesh,shell.vertex_roots,height)
        maximum=max(maximum,value.symmetric_difference(wanted).area);tested+=1
    if maximum>1e-7:raise ValueError('Complete room/roof sections differ from planned lower rooms and independent roof')
    np.testing.assert_array_equal([shell.mesh['vertices'][:,2].min(),shell.mesh['vertices'][:,2].max()],[p['minHeight'],p['buildingHeight']])
    parent=shell.source_indices==0
    values=shell.mesh['vertices'][shell.mesh['faces'][parent]]
    normals=shell.mesh['normals'][shell.mesh['faces'][parent]][:,0]
    roof=normals[:,2]>.01
    if not roof.any():raise ValueError('Room subtraction lost the complete upper roof')
    projected=shapely.union_all([shapely.Polygon(t[:,:2]) for t in values[roof]])
    if projected.symmetric_difference(row['polygon']).area>1e-7:raise ValueError('Room openings lost a roof domain or filled a courtyard')
    return dict(volume_m3=metrics[0],area_m2=metrics[1],height_sections=tested,maximum_section_error_m2=maximum,
        independent_volume_error_m3=float(error),source_triangles=len(shell.source_triangles),composition=comparison)


def negatives(plan,choice,top,material):
    envelope=heightfield_envelope(top,plan['base_z_m'],wall_material=material)
    sealed=dict(plan);sealed['doors']=[]
    shell=structure(sealed,envelope,material);_,collision=assembly(sealed,shell,frames=True)
    try:access(plan,collision)
    except ValueError:door=True
    else:raise ValueError('A sealed actual entrance passed actor clearance')
    shell=structure(plan,envelope,material)
    floor=plan['floor_z_m'];hole=shapely.union_all(plan['rooms'])
    from outshine_ref.buildings.modelling import PrismVolume,union_prisms
    removed=union_prisms([PrismVolume('missing-foot-support',hole,plan['base_z_m']-.1,floor+.01)])
    _,collision=assembly(plan,shell,frames=True);collision=collision-native(removed.mesh,removed.vertex_roots)
    try:access(plan,collision)
    except ValueError:foot=True
    else:raise ValueError('Missing floor passed sampled foot support')
    windows=[prism for name,prism in sources(plan).items() if '/window-' in name]
    if not windows:raise ValueError('Window negative control requires a real declared window')
    blocks=union_prisms([PrismVolume('sealed-window-'+str(index),prism.polygon,prism.low,prism.high)
                         for index,prism in enumerate(windows)])
    closed=native(shell.mesh,shell.vertex_roots)+native(blocks.mesh,blocks.vertex_roots)
    try:window_access(plan,closed)
    except ValueError:window=True
    else:raise ValueError('A sealed actual window still opened into its room')
    return dict(sealed_entrance_rejected=door,missing_floor_rejected=foot,sealed_window_rejected=window)


def window_access(plan,shell):
    solid=shell if isinstance(shell,manifold.Manifold) else native(shell.mesh,shell.vertex_roots);tested=0
    for index in range(plan['storeys']):
        current=level(plan,index)
        for window in current['windows']:
            centre=np.asarray(window['centre']);tangent=np.asarray(window['tangent']);normal=np.array([tangent[1],-tangent[0]])
            height=current['floor_z_m']+(window['sill_m']+window['head_m'])/2
            target=centre-normal*.5
            if not any(current['rooms'][owner].covers(shapely.Point(target)) for owner in window['rooms']):
                raise ValueError('Window ray does not end in its declared room')
            for shift in (-.45,0.,.45):
                a=np.r_[centre+normal*.35+tangent*shift,height];b=np.r_[target+tangent*shift,height]
                if solid.ray_cast(a.tolist(),b.tolist()):raise ValueError('Actual structure seals a declared window opening')
                tested+=1
    return dict(window_rays=tested,declared_room_contact=True)


def controls():
    results=[]
    for identifier,polygon,top_m in [('rectangle',shapely.box(0,0,10,6),5.),('square',shapely.box(0,0,7,7),5.)]:
        row=dict(id=identifier,polygon=polygon,properties=dict(render_height=top_m,render_min_height=0.),clipped=False)
        p=attributes(row['properties']);choice=choose(row,p);top,wedge,_=roof_surface(row,choice)
        _,info,plan,shell=complete(row,p,3,choice,top)
        results.append(dict(id=identifier,kind=choice.kind,**geometry(row,p,choice,top,wedge,plan,shell),
            access=info['access'],negative_controls=negatives(plan,choice,top,3),window_access=window_access(plan,shell)))
    for identifier,polygon,top_m,eave_m in [('too-low',shapely.box(0,0,10,6),3.5,1.8),
                                         ('narrow-multistorey',shapely.box(0,0,3,4),8.,6.5)]:
        row=dict(id=identifier,polygon=polygon,properties=dict(render_height=top_m,render_min_height=0.),clipped=False)
        choice=RoofChoice('ridge',top_m-eave_m,eave_m,0.,'analytic negative','insufficient room/core envelope')
        try:plan_for(row,choice)
        except ValueError as error:results.append(dict(id=identifier,status='rejected',reason=str(error)))
        else:raise ValueError('Undersized negative room/core control unexpectedly received a connected plan')
    return results


def benchmark(row,p,choice,top,plan,repeats):
    _,wedge,_=roof_surface(row,choice)
    values={'direct-envelope-openings':[],'planned-bands-roof-union':[]}
    def produce(name):
        if name=='direct-envelope-openings':return structure(plan,heightfield_envelope(top,p['minHeight'],wall_material=3),3)
        lower=construct(plan)
        return manifold.Manifold.batch_boolean([native(lower.mesh,lower.vertex_roots),native(wedge)],manifold.OpType.Add)
    direct=produce('direct-envelope-openings');other=produce('planned-bands-roof-union')
    np.testing.assert_allclose([direct.volume_m3,direct.area_m2],[other.volume(),other.surface_area()],atol=1e-7,rtol=1e-9)
    for index in range(repeats+2):
        for name in values:
            began=time.perf_counter();produce(name)
            if index>=2:values[name].append((time.perf_counter()-began)*1000)
    return dict(id=row['id'],phases={name:dict(samples=repeats,p50_ms=float(np.percentile(data,50)),p95_ms=float(np.percentile(data,95)))
        for name,data in values.items()},scope='Same prepared source roof and complete room plan; actual closed volume and area matched first. Host Python construction only, no native frame/device claim.')


def run(output,repeats):
    output.mkdir(parents=True,exist_ok=True);analytic=controls();cases=[];rejected=[];receipts=[];examples=[]
    for site in SITES:
        rows,receipt=read_tile(site);receipts.append(receipt['receipt'])
        for row in selected(site,rows):
            p=attributes({k:v for k,v in row['properties'].items() if k not in ('colour','color','building:colour')});choice=choose(row,p)
            try:
                top,wedge,_=roof_surface(row,choice);mesh,info,plan,shell=complete(row,p,3,choice,top)
            except ValueError as error:
                rejected.append(dict(id=row['id'],reason=str(error)));continue
            metrics=geometry(row,p,choice,top,wedge,plan,shell)
            openings=slots(plan);frames=expand(openings)
            frame_check=verify_slots(plan,openings,frames);verify_body_openings(openings,body_mesh(plan))
            record=dict(id=row['id'],kind=choice.kind,**info,**metrics,framing=frame_check,window_access=window_access(plan,shell))
            if choice.kind not in [e[2].kind for e in examples]:
                record['negative_controls']=negatives(plan,choice,top,3);examples.append((row,p,choice,top,plan))
            replay=structure(plan,heightfield_envelope(top,p['minHeight'],wall_material=3),3)
            for key in shell.mesh:np.testing.assert_array_equal(replay.mesh[key],shell.mesh[key])
            np.testing.assert_array_equal(replay.source_triangles,shell.source_triangles)
            cases.append(record);print(row['id'],'pass',info['access']['sampled_poses'],'poses',flush=True)
        print(json.dumps(dict(site=site,complete=sum(c['id'].startswith(site+'/') for c in cases),rejected=sum(c['id'].startswith(site+'/') for c in rejected))),flush=True)
    if not cases:raise ValueError('No actual pitched-roof room source passed')
    timings=[benchmark(row,p,choice,top,plan,repeats) for row,p,choice,top,plan in examples] if repeats else []
    report=revision()|dict(status='pass',controls=analytic,cases=cases,rejected=rejected,receipts=receipts,timings=timings,
        process_peak_rss_mib=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss/1024,
        scope='Bounded actual near source prototypes with complete roof, shared generic rooms/openings and sampled real actor. Attic use, all source programmes, native cache/player/frame/device gates remain unproved.')
    (output/'verification.json').write_text(json.dumps(report,indent=2)+'\n')


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/pitched-rooms'))
    parser.add_argument('--repeats',type=int,default=21);args=parser.parse_args();run(args.output,args.repeats)
