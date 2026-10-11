import json,sys,time
from pathlib import Path
import numpy as np
import shapely
sys.path.insert(0,'test/experiments/infrastructure')
from infrastructure_turn_roadbed_experiment import prepared_network,building_reservations,reservations,motor_surface,directed_module_routes
from infrastructure_cut_setbacks import restore_cut_plan,fork_cut_plan,displaced_source_section,check_external_sections,CutSetbackRejected
from shapely.ops import split
from infrastructure_shared_frames import frame_key
from infrastructure_source_roadbeds import construct_route_roadbed
from infrastructure_turn_roadbeds import roadbed_curves,turn_roadbed,compose_roadbeds,preserves_voids,holes
from infrastructure_junction_surfaces import same_island_winding
from infrastructure_vehicle_sweep import turn_sampler


def local_partition(core,road,point,tangent,old):
    scale=max(1.,float(np.abs(np.r_[core.bounds,road.bounds]).max()))
    precision=float(2.**np.floor(np.log2(16*np.finfo(float).eps*scale)))
    core,road,old=shapely.set_precision(np.array([core,road,old],dtype=object),precision)
    tangent=tangent/np.linalg.norm(tangent);normal=np.array([-tangent[1],tangent[0]])
    extent=np.linalg.norm(np.subtract(road.bounds[2:],road.bounds[:2]))+1
    ray=shapely.LineString([point-normal*extent,point+normal*extent])
    spans=[g for g in shapely.get_parts(road.intersection(ray))
           if g.geom_type=='LineString' and g.distance(shapely.Point(point))<1e-7]
    if len(spans)!=1:raise CutSetbackRejected('ambiguous_source_section')
    ends=shapely.get_coordinates(spans[0])[[0,-1]]
    direction=(ends[1]-ends[0])/np.linalg.norm(ends[1]-ends[0])
    cutter=shapely.LineString(ends+np.array([-1e-6,1e-6])[:,None]*direction)
    pieces=list(shapely.get_parts(shapely.set_precision(split(road,cutter),precision)))
    touched=[p for p in pieces if shapely.covered_by(old,shapely.buffer(p.boundary,1e-7))]
    if len(touched)!=1 or len(pieces)<2:raise CutSetbackRejected('no_unique_local_transfer')
    moved=touched[0];new_core=core.union(moved);new_road=shapely.union_all([p for p in pieces if p is not moved])
    line=shapely.set_precision(spans[0],precision)
    shared=new_core.boundary.intersection(new_road.boundary)
    retained=core.boundary.intersection(road.boundary).difference(shapely.buffer(old,1e-7))
    desired=shapely.union_all([retained,line])
    if shapely.hausdorff_distance(shared,desired)>1e-7:raise CutSetbackRejected('shared_section_mismatch')
    assert core.union(road).symmetric_difference(new_core.union(new_road)).area==0
    assert new_core.intersection(new_road).area==0 and new_core.is_valid and new_road.is_valid
    return new_core,new_road,line

def setback_network(network,module,sections,distance):
    poses,_=network.poses(module,{})
    result=fork_cut_plan(network,module,sections)
    changes=[];affected={module}
    for section in sorted(sections):
        contacts=[p for p in poses if p['section']==section]
        assert len({(p['edge'],p['station']) for p in contacts})==1
        pose=contacts[0];point,tangent,station=displaced_source_section(network,pose,distance)
        corridor=pose['corridor'];core=result.shape(module);road=result.shape(corridor)
        new_core,new_road,line=local_partition(core,road,point,tangent,result.sections[section])
        result.shapes[module],result.shapes[corridor]=new_core,new_road
        result.sections[section]=line
        for port in result.junctions[module]:
            if port['section']==section:port['line']=line
        affected.add(corridor)
        changes.append(dict(junction=module,section=section,edge=pose['edge'],station=station,corridor=corridor,
                shared_section=shapely.get_coordinates(line).tolist(),transferred_area_m2=new_core.area-core.area))
    check_external_sections(result,affected)
    for owner in affected:result.records.pop(owner,None)
    return result,changes,affected

root=Path('build/experiments/infrastructure');name='ZuerichHauptbahnhof';module=102
before=json.loads((root/'window-1000-body'/f'{name}-routes.json').read_text())
frames=json.loads((root/'boundary-optimized/hermite-routes.json').read_text())
network=prepared_network(root,before,next(p['frames'] for p in frames['places'] if p['place']==name))
cuts=json.loads((root/'cut-setbacks'/f'{name}-cut-setbacks.json').read_text());restore_cut_plan(network,cuts['plan'])
shape=network.shape(module);poses,_=network.poses(module,{})
floor=motor_surface(shape,network.graph,network.tree,network.mesh,network.vehicle,network.curbs['corner_radius_m'],network.curbs['arc_error_m'])
receipt=json.loads((root/'inputs'/f'{name}-input.json').read_text())['receipt']
region=floor.envelope.buffer(network.vehicle['widthM'])
buildings,tiles=building_reservations(receipt,region)
reserved=buildings.union(reservations(network,region).difference(floor))
for old_pair in ((2,4),):
    wanted=[frame_key(poses[i])[:3] for i in old_pair]
    sections={poses[i]['section'] for i in old_pair}
    for setback in (8.,):
        start=time.perf_counter()
        try:
            trial,changes,affected=setback_network(network,module,sections,setback)
            trial_poses,_=trial.poses(module,{})
            keys=[frame_key(p)[:3] for p in trial_poses]
            pair=tuple(keys.index(k) for k in wanted)
            trial_floor=floor if trial.shape(module).bounds==shape.bounds else motor_surface(trial.shape(module),trial.graph,
                    trial.tree,trial.mesh,trial.vehicle,trial.curbs['corner_radius_m'],trial.curbs['arc_error_m'])
            routes,_=directed_module_routes(trial.graph,trial_poses,trial.shape(module),trial.tree)
            notes=[]
            for fit,note in roadbed_curves(trial_poses[pair[0]],trial_poses[pair[1]],trial.vehicle):
                if fit is None:
                    notes.append(note);continue
                profile=note['profile']
                support,audit=turn_roadbed(fit,trial_poses[pair[0]]['position'],trial_poses[pair[1]]['position'],trial.vehicle,
                                    trial.curbs['arc_error_m'],profile=profile)
                if support is None:notes.append(note|audit);continue
                a,b,c,d=support.bounds
                local=trial_floor.intersection(shapely.box(a-6,b-6,c+6,d+6))
                budgets=[.9,1.25,1.5,1.75,2.,2.5,3.,4.,6.]
                minimum=next((r for r in budgets if shapely.covers(shapely.buffer(local,r,quad_segs=16),support)),None)
                base,core,planned,physical,quantum=compose_roadbeds(trial_floor,trial.shape(module),[support])
                addition=planned.difference(base)
                xy=turn_sampler(profile)(fit,np.linspace(0,1,129))[0][0]
                points=np.vstack((trial_poses[pair[0]]['position'],xy,trial_poses[pair[1]]['position']))
                points=points[np.r_[True,np.linalg.norm(np.diff(points,axis=0),axis=1)>1e-10]]
                path=shapely.LineString(points)
                section_hits=[int(i) for i in shapely.STRtree(list(trial.sections.values())).query(addition,predicate='dwithin',distance=1e-7)]
                ids=trial.mesh[1].query(addition,predicate='intersects')
                owners=set(map(int,trial.product['face_owners'][ids]))|affected
                collisions={int(owner):float(addition.intersection(trial.shape(owner)).area) for owner in owners if owner!=module
                            and addition.intersection(trial.shape(owner)).area>1e-8}
                notes.append(note|dict(required_flare_upper_m=minimum,added_area_m2=addition.area,
                        public_parts=[len(shapely.get_parts(s)) for s in (base,planned)],
                        public_holes=[len([ring for p in shapely.get_parts(s) for ring in p.interiors]) for s in (base,planned)],
                        reservation_overlap_m2=addition.intersection(reserved).area,
                        public_voids_preserved=preserves_voids(base,planned),physical_voids_preserved=preserves_voids(core,physical),
                        source_winding_preserved=same_island_winding(path,routes[pair],base),path_simple=path.is_simple,
                        shared_section_hits=section_hits,other_owner_collisions=collisions))
                original,after=holes(base),holes(planned)
                index=shapely.STRtree(after)
                point_pairs=index.query(shapely.point_on_surface(original),predicate='within')
                missing=sorted(set(range(len(original)))-set(map(int,point_pairs[0])))
                pairs=index.query(original,predicate='intersects')
                area=shapely.area(shapely.intersection(original[pairs[0]],after[pairs[1]]))
                pairs=pairs[:,area>0]
                notes[-1].update(overlap_bijection=bool(len(pairs[0])==len(original)==len(after)
                    and len(np.unique(pairs[0]))==len(original) and len(np.unique(pairs[1]))==len(after)),
                    marker_failures=[dict(old_area_m2=original[i].area,bounds=original[i].bounds,
                        overlapping_new_areas_m2=[after[j].area for j in pairs[1][pairs[0]==i]]) for i in missing])
                def bounded(surface):
                    shells=shapely.polygons(shapely.get_exterior_ring(shapely.get_parts(surface)))
                    return shapely.get_parts(shapely.union_all(shells).difference(surface))
                original,after=bounded(base),bounded(planned)
                index=shapely.STRtree(after)
                pairs=index.query(original,predicate='intersects')
                area=shapely.area(shapely.intersection(original[pairs[0]],after[pairs[1]]))
                pairs=pairs[:,area>0]
                notes[-1].update(old_check_accepts_identity=preserves_voids(base,base),
                    bounded_void_counts=[len(original),len(after)],
                    actual_void_bijection=bool(len(pairs[0])==len(original)==len(after)
                        and len(np.unique(pairs[0]))==len(original) and len(np.unique(pairs[1]))==len(after)))
            report=dict(pair=old_pair,setback_m=setback,notes=notes)
            print(json.dumps(report),flush=True)
            (root/'roadbeds/local-cut-envelope-zuerich-102.json').write_text(json.dumps(report,indent=2)+'\n')
        except CutSetbackRejected as error:
            print(json.dumps(dict(pair=old_pair,setback_m=setback,rejected=str(error))),flush=True)
