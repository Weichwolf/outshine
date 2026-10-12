"""Move joined corridor arms onto one physical cut with distinct logical ports."""

import numpy as np
import shapely

from transport_cut_setbacks import (CutSetbackRejected,displaced_source_section,fork_cut_plan,
    setback_shared_sections,check_external_sections,check_contact_progress)


def partition_axis_ports(line,points):
    ends=shapely.get_coordinates(line)[[0,-1]];direction=(ends[1]-ends[0])/line.length
    points=np.asarray(points);stations=(points-ends[0])@direction
    if (np.any(shapely.distance(shapely.points(points),line)>1e-7) or
            np.any(stations<=0) or np.any(stations>=line.length)):
        raise CutSetbackRejected('source_axis_outside_joined_cut')
    order=np.argsort(stations)
    if np.any(np.diff(stations[order])<=1e-7):raise CutSetbackRejected('joined_axes_are_not_distinct')
    boundaries=np.r_[0.,(stations[order][:-1]+stations[order][1:])*.5,line.length]
    result=[None]*len(points)
    for i,owner in enumerate(order):
        result[owner]=shapely.LineString(ends[0]+boundaries[i:i+2,None]*direction)
    return result


def setback_joined_network(network,module,sections,distance):
    if not 2<=len(sections)<=8 or not np.isfinite(distance) or distance<=0:
        raise ValueError('joined cut needs two to eight sections and a finite positive setback')
    old_poses,_=network.poses(module,{})
    representatives=[]
    for section in sorted(sections):
        contacts=[p for p in old_poses if p['section']==section]
        if not contacts or len({p['edge'] for p in contacts})!=1:
            raise CutSetbackRejected('joined_section_has_no_unique_source_axis')
        representatives.append(min(contacts,key=lambda p:(p['edge'],p['station'],p['direction'])))
    corridors={p['corridor'] for p in representatives}
    if len(corridors)!=1:raise CutSetbackRejected('joined_sections_have_different_corridors')
    corridor=next(iter(corridors));core,road=network.shape(module),network.shape(corridor)
    original=[network.sections[p['section']] for p in representatives];failures=[]
    for seed in representatives:
        try:
            point,tangent,_=displaced_source_section(network,seed,distance)
            new_core,new_road,line=setback_shared_sections(core,road,original,point,tangent)
            crossings=[];stations=[]
            for pose in representatives:
                axis=network.graph['lines'][pose['edge']];crossing=axis.intersection(line)
                if crossing.geom_type!='Point':raise CutSetbackRejected('joined_cut_has_no_unique_axis_crossing')
                station=axis.project(crossing)
                advance=(station-pose['station'])*pose['direction']*(-1 if pose['incoming'] else 1)
                if not 0<station<axis.length or advance<distance-1e-7:
                    raise CutSetbackRejected('joined_axis_did_not_reach_requested_setback')
                crossings.append(np.array(crossing.coords[0]));stations.append(station)
            lines=partition_axis_ports(line,crossings)
            result=fork_cut_plan(network,module,sections)
            result.shapes[module],result.shapes[corridor]=new_core,new_road
            changes=[]
            for pose,station,part in zip(representatives,stations,lines):
                section=pose['section'];result.sections[section]=part
                for port in result.junctions[module]:
                    if port['section']==section:port['line']=part
                changes.append(dict(junction=module,section=section,edge=pose['edge'],station=station,corridor=corridor,
                    shared_section=shapely.get_coordinates(part).tolist(),joined_sections=sorted(sections),
                    joint_transferred_area_m2=new_core.area-core.area))
            affected={module,corridor};check_external_sections(result,affected)
            for owner in affected:result.records.pop(owner,None)
            check_contact_progress(result,old_poses,result.poses(module,{})[0],sections)
            return result,changes,affected
        except CutSetbackRejected as error:failures.append(str(error))
    raise CutSetbackRejected('joined_section_candidates_rejected: '+', '.join(failures))
