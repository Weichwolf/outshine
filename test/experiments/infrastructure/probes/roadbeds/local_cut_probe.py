import json,sys,time
from pathlib import Path
import numpy as np
import shapely
from shapely.ops import split
sys.path.insert(0,'test/experiments/infrastructure')
from infrastructure_turn_roadbed_experiment import prepared_network
from infrastructure_cut_setbacks import restore_cut_plan,displaced_source_section

root=Path('build/experiments/infrastructure');name='ZuerichHauptbahnhof';module=102
before=json.loads((root/'window-1000-body'/f'{name}-routes.json').read_text())
frames=json.loads((root/'boundary-optimized/hermite-routes.json').read_text())
network=prepared_network(root,before,next(p['frames'] for p in frames['places'] if p['place']==name))
cuts=json.loads((root/'cut-setbacks'/f'{name}-cut-setbacks.json').read_text());restore_cut_plan(network,cuts['plan'])
poses,_=network.poses(module,{})
reports=[]
for index in (2,4):
    pose=poses[index];core=network.shape(module);road=network.shape(pose['corridor']);old=network.sections[pose['section']]
    scale=max(1.,float(np.abs(np.r_[core.bounds,road.bounds]).max()))
    precision=float(2.**np.floor(np.log2(16*np.finfo(float).eps*scale)))
    core,road,old=shapely.set_precision(np.array([core,road,old],dtype=object),precision)
    for distance in (4.,8.,12.):
        point,tangent,station=displaced_source_section(network,pose,distance)
        tangent=tangent/np.linalg.norm(tangent);normal=np.array([-tangent[1],tangent[0]])
        extent=np.linalg.norm(np.subtract(road.bounds[2:],road.bounds[:2]))+1
        ray=shapely.LineString([point-normal*extent,point+normal*extent])
        spans=[g for g in shapely.get_parts(road.intersection(ray)) if g.geom_type=='LineString' and g.distance(shapely.Point(point))<1e-7]
        row=dict(pose=index,section=pose['section'],corridor=pose['corridor'],corridor_ports=network.corridors[pose['corridor']],
                 distance_m=distance,point=point.tolist(),station=station,point_in_road=bool(road.covers(shapely.Point(point))),
                 spans=len(spans),old_section=shapely.get_coordinates(old).tolist())
        if len(spans)==1:
            ends=shapely.get_coordinates(spans[0])[[0,-1]]
            direction=(ends[1]-ends[0])/np.linalg.norm(ends[1]-ends[0])
            cutter=shapely.LineString(ends+np.array([-1e-6,1e-6])[:,None]*direction)
            pieces=list(shapely.get_parts(shapely.set_precision(split(road,cutter),precision)))
            touched=[p for p in pieces if shapely.covered_by(old,shapely.buffer(p.boundary,1e-7))]
            row.update(pieces=len(pieces),touched=len(touched),areas=[p.area for p in pieces])
            if len(touched)==1 and len(pieces)>1:
                moved=touched[0];new_core=core.union(moved);new_road=shapely.union_all([p for p in pieces if p is not moved])
                shared=shapely.intersection(new_core.boundary,new_road.boundary)
                retained=shapely.intersection(core.boundary,road.boundary).difference(shapely.buffer(old,1e-7))
                desired=shapely.union_all([retained,spans[0]])
                row.update(transferred_area_m2=moved.area,shared_mismatch_m=shapely.hausdorff_distance(shared,desired),
                    native_footprint_difference_m2=core.union(road).symmetric_difference(new_core.union(new_road)).area)
        reports.append(row)
        print(json.dumps(row),flush=True)
(root/'roadbeds/local-cut-zuerich-102.json').write_text(json.dumps(reports,indent=2)+'\n')
