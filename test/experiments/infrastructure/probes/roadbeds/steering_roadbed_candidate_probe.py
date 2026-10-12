"""Check a steering-seeded C2 road mesh and report cuts of individual source buildings."""

import argparse
import json
import math
from pathlib import Path
import subprocess
import sys
import time

import numpy as np
import shapely

sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
from infrastructure_building_clearance import features,polygon_parts,GIRTH_M,indexed_cut,choose,plot_geometry
from infrastructure_cut_setback_experiment import digest,implementation_files
from infrastructure_fitted_roadbeds import spline_roadbed
from infrastructure_junction_surfaces import same_island_winding
from infrastructure_roadbed_island_cleanup import close_split_pockets
from infrastructure_spline_routes import checked_spline_path
from infrastructure_turn_roadbed_experiment import mesh_coverage
from infrastructure_turn_roadbeds import roadbed_site_check,roadbed_lateral_reach


def source_buildings(receipt,region):
    lat,lon=receipt['origin'];scale=math.cos(math.radians(lat))
    anchor=np.array([lon/360*GIRTH_M,math.asinh(math.tan(math.radians(lat)))*GIRTH_M/(2*math.pi)])
    records={}
    for tile in receipt['source_tiles']:
        z,x,y=tile['tile'];span=GIRTH_M/(1<<z)
        corner=(np.array([x*span,-y*span])+[-GIRTH_M/2,GIRTH_M/2]-anchor)*scale
        if not shapely.box(corner[0],corner[1]-span*scale,corner[0]+span*scale,corner[1]).intersects(region):continue
        path=Path(tile['path']);assert digest(path)==tile['sha256']
        for ordinal,(layer,extent,kind,props,parts) in enumerate(features(path.read_bytes())):
            if layer!='building' or kind!=3:continue
            rings=[((np.asarray(p)/extent+[x,y])*[span,-span]+[-GIRTH_M/2,GIRTH_M/2]-anchor)*scale for p in parts]
            for part,polygon in enumerate(polygon_parts(rings)):
                if not polygon.intersects(region):continue
                assert polygon.is_valid,'source building requires an explicit repair recipe'
                identity=dict(tile=tile['tile'],tile_sha256=tile['sha256'],feature=ordinal,part=part,properties=props)
                key=shapely.normalize(polygon).wkb
                if key not in records:records[key]=dict(geometry=polygon,sources=[])
                records[key]['sources'].append(identity)
    return list(records.values())


def candidate(data,note):
    controls=np.array(note['curve_controls']);vehicle,error=data['vehicle'],data['error_m']
    for pose,points,reverse in ((data['entry'],controls[0],False),(data['exit'],controls[-1,::-1],True)):
        assert np.allclose(points[0],pose['position'],atol=1e-7,rtol=0),'seed endpoint differs from its source contact'
        tangent=points[1]-points[0]
        if reverse:tangent=-tangent
        assert np.linalg.norm(tangent)>0
        assert np.allclose(tangent/np.linalg.norm(tangent),pose['heading'],atol=1e-8,rtol=0),'seed tangent differs from its source contact'
    floor,reserved,source=(shapely.from_wkb(data[k]) for k in ('surface','reservations','source_route'))
    support,construction=spline_roadbed(controls,vehicle,error)
    assert support is not None,construction
    support=support.difference(reserved).intersection(floor.buffer(roadbed_lateral_reach(vehicle)))
    support,pockets=close_split_pockets(floor,support,reserved)
    planned,addition,reason=roadbed_site_check(support,floor,reserved,vehicle,error)
    assert reason is None,reason
    path,proof=checked_spline_path(controls,planned,vehicle,error)
    assert path is not None,proof
    assert same_island_winding(path,source,floor),'source island winding changed'
    piece=dict(profile='G2-quintic-spline',fit=dict(controls=controls,
               maximum_curvature_per_m=np.array([proof['curvature_bound_per_m']])),
               begin=np.array(data['entry']['position']),end=np.array(data['exit']['position']))
    mesh,actual=mesh_coverage(planned,vehicle,[piece],error)
    return dict(surface=actual,roadbed=support,addition=addition,path=path,pieces=[piece],
                construction=construction,pockets=pockets,proof=proof,mesh_audit=mesh),floor


def building_cuts(records,floor,planned):
    buildings=np.array([r['geometry'] for r in records],dtype=object)
    adjusted,audit=indexed_cut(buildings,np.array([planned],dtype=object))
    baseline,_=indexed_cut(buildings,np.array([floor],dtype=object))
    if len(buildings):
        selected,selection=choose(buildings,adjusted,.25,25.,3.)
        old_selected,_=choose(buildings,baseline,.25,25.,3.)
    else:
        selected,old_selected=adjusted,baseline
        selection=dict(cut=0,discarded=0,affected_fraction_limit=.25,removed_area_fraction=0.)
    assert np.all(shapely.is_valid(selected))
    assert np.all(shapely.area(shapely.intersection(selected,planned))<=1e-8)
    rows=[]
    for record,original,old,new,used,old_used in zip(records,buildings,baseline,adjusted,selected,old_selected):
        pieces=shapely.get_parts(new)
        rows.append(dict(sources=record['sources'],original=original,clipped=new,adjusted=used,
            original_area_m2=original.area,baseline_cut_area_m2=original.area-old.area,
            total_cut_area_m2=original.area-new.area,additional_cut_area_m2=old.area-new.area,
            selected_area_m2=used.area,additional_selected_cut_area_m2=old_used.area-used.area,
            unusable_cut_area_m2=new.area-used.area,baseline_parts=shapely.area(shapely.get_parts(old)).tolist(),
            removed_fraction=(original.area-new.area)/original.area,valid=bool(new.is_valid),
            remaining_parts=len(pieces),part_areas_m2=shapely.area(pieces).tolist(),
            retains_3m_interior=bool(not new.buffer(-1.5).is_empty)))
    return rows,dict(spatial_search=audit,selection=selection,minimum_area_m2=25.,minimum_width_m=3.)


def render(report,output):
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    fig,axes=plt.subplots(1,2,figsize=(14,7),layout='constrained')
    floor=report['floor'];road=report['candidate'];bounds=road['path'].envelope.buffer(8).bounds
    for ax,field,title in zip(axes,('original','adjusted'),('Source buildings / original public road','C2 road mesh / road-first building cuts')):
        plot_geometry(ax,[r[field] for r in report['buildings']],'#dfcbb6')
        plot_geometry(ax,[floor if field=='original' else road['surface']],'#bfd7db')
        if field=='adjusted':plot_geometry(ax,[road['addition']],'#6faca8')
        xy=shapely.get_coordinates(road['path']);ax.plot(*xy.T,color='#315865',linewidth=1)
        ax.set(xlim=(bounds[0],bounds[2]),ylim=(bounds[1],bounds[3]),aspect='equal',title=title,xlabel='East [m]',ylabel='North [m]')
    fig.suptitle('Wien 1480: route 4 → 1 — standalone native mesh; neighboring products still pending')
    fig.savefig(output,dpi=150);plt.close(fig)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input',type=Path,required=True)
    parser.add_argument('--curve',type=Path,required=True)
    parser.add_argument('--receipt',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=True);begin=time.perf_counter()
    data=json.loads(args.input.read_text());note=json.loads(args.curve.read_text())
    road,floor=candidate(data,note)
    records=source_buildings(json.loads(args.receipt.read_text())['receipt'],road['path'].envelope.buffer(12))
    buildings,audit=building_cuts(records,floor,road['surface'])
    report=dict(candidate=road,floor=floor,buildings=buildings,building_search=audit,
        input_sha256={str(p):digest(p) for p in (args.input,args.curve,args.receipt)},
        implementation_sha256={str(p.relative_to(Path.cwd())):digest(p) for p in implementation_files(Path(__file__))},
        measured_commit=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),
        elapsed_ms=(time.perf_counter()-begin)*1000,
        scope='Standalone native road mesh and per-source building cuts; coupled owners, all routes and complete windows remain required.')
    def encode(value):
        if isinstance(value,shapely.Geometry):return value.wkb.hex()
        if isinstance(value,np.ndarray):return value.tolist()
        if isinstance(value,np.generic):return value.item()
        raise TypeError(type(value).__name__)
    (args.output/'candidate.json').write_text(json.dumps(report,indent=2,default=encode)+'\n')
    render(report,args.output/'candidate.png')
    affected=[{k:r[k] for k in ('original_area_m2','baseline_cut_area_m2','additional_cut_area_m2','removed_fraction','remaining_parts','part_areas_m2','retains_3m_interior')} for r in buildings if r['total_cut_area_m2']>1e-8]
    print(json.dumps(dict(native_body=True,added_road_area_m2=road['addition'].area,
                         buildings=len(buildings),affected=affected,elapsed_ms=report['elapsed_ms'])))


if __name__=='__main__':main()
