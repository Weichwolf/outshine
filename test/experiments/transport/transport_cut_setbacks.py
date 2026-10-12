"""Move a shared road cut without widening its surface or losing neighboring routes."""

import copy

import numpy as np
import shapely
from shapely.ops import split

from transport_shared_frames import frame_key


class CutSetbackRejected(ValueError):
    pass


def setback_shared_section(core,road,section,point,tangent):
    area = shapely.union_all([core,road])
    tangent = np.array(tangent,dtype=float,copy=True)
    length = np.linalg.norm(tangent)
    if not np.isfinite(length) or length<=1e-10:
        raise ValueError('section tangent must be finite and nonzero')
    tangent /= length
    scale = max(1.,float(np.abs(area.bounds).max()))
    roundoff = 64*np.finfo(float).eps*scale
    precision = float(2.**np.floor(np.log2(roundoff/4)))
    existing = shapely.get_precision(np.array([core,road,section],dtype=object))
    existing = existing[existing>0]
    if existing.size:
        precision = float(existing.min())
    core,road,section = shapely.set_precision(np.array([core,road,section],dtype=object),precision)
    old_shared = shapely.intersection(shapely.boundary(core),shapely.boundary(road))
    if section.is_empty or not shapely.covered_by(section,shapely.buffer(old_shared,1e-7)):
        raise CutSetbackRejected('original_section_is_not_shared')
    normal = np.array([-tangent[1],tangent[0]])
    extent = np.linalg.norm(np.subtract(road.bounds[2:],road.bounds[:2]))+1
    ray = shapely.LineString([point-normal*extent,point+normal*extent])
    spans = [g for g in shapely.get_parts(shapely.intersection(road,ray))
             if g.geom_type=='LineString' and g.distance(shapely.Point(point))<1e-7]
    if len(spans)!=1:
        raise CutSetbackRejected('ambiguous_source_section')
    ends = shapely.get_coordinates(spans[0])[[0,-1]]
    line = shapely.set_precision(shapely.LineString(ends),precision)
    direction = (ends[1]-ends[0])/line.length
    cutter = shapely.LineString(ends+np.array([-1e-6,1e-6])[:,None]*direction)
    pieces = list(shapely.get_parts(shapely.set_precision(split(road,cutter),precision)))
    touching = [p for p in pieces if shapely.covered_by(section,shapely.buffer(p.boundary,1e-7))]
    if len(pieces)<2 or len(touching)!=1:
        raise CutSetbackRejected('no_unique_local_transfer')
    transferred = touching[0]
    new_core = shapely.union_all([core,transferred])
    new_road = shapely.union_all([p for p in pieces if p is not transferred])
    tolerance = roundoff*area.length
    if shapely.difference(core,new_core).area>tolerance:
        raise CutSetbackRejected('setback_removes_existing_core')
    shared = shapely.intersection(shapely.boundary(new_core),shapely.boundary(new_road))
    retained = old_shared.difference(shapely.buffer(section,1e-7))
    desired = shapely.union_all([retained,line])
    if shared.is_empty or shapely.hausdorff_distance(shared,desired)>1e-7:
        raise CutSetbackRejected('shared_section_mismatch')
    assert shapely.intersection(new_core,new_road).area<=tolerance
    assert shapely.symmetric_difference(area,shapely.union_all([new_core,new_road])).area<=tolerance
    assert new_core.is_valid and new_road.is_valid
    return new_core,new_road,line


def fork_cut_plan(network,module,sections):
    result = copy.copy(network)
    result.shapes = dict(network.shapes)
    result.junctions = dict(network.junctions)
    result.junctions[module] = [dict(p) for p in network.junctions[module]]
    result.sections = dict(network.sections)
    result.registry = {k:v for k,v in network.registry.items() if k!=module}
    result.anchors,result.owners = dict(network.anchors),dict(network.owners)
    result.frames = {k:v for k,v in network.frames.items() if k[0] not in sections}
    result.records = dict(network.records)
    result.domains = {}
    result.original_poses = dict(network.original_poses)
    return result


def displaced_source_section(network,pose,distance):
    axis = network.graph['lines'][pose['edge']]
    station = pose['station']+distance*pose['direction']*(-1 if pose['incoming'] else 1)
    if not 0<station<axis.length:
        raise CutSetbackRejected('source_edge_setback_limit')
    point = np.array(axis.interpolate(station).coords[0])
    tangent = np.array(axis.interpolate(min(axis.length,station+.01)).coords[0])-np.array(
                      axis.interpolate(max(0,station-.01)).coords[0])
    if np.linalg.norm(tangent)<=1e-10:
        raise CutSetbackRejected('degenerate_source_tangent')
    return point,tangent,station


def check_external_sections(network,affected):
    junctions = (affected&network.junctions.keys())|{j for m in affected&network.corridors.keys()
                                                  for j,_ in network.corridors[m]}
    boundaries = {}
    for junction in sorted(junctions):
        for port in network.junctions[junction]:
            for owner in (junction,port['corridor']):
                if owner not in boundaries:
                    boundaries[owner] = shapely.buffer(shapely.boundary(network.shape(owner)),1e-7)
                    shapely.prepare(boundaries[owner])
                if not shapely.covers(boundaries[owner],port['line']):
                    raise CutSetbackRejected('external_neighbor_section_changed')


def check_contact_progress(network,before,after,sections):
    old={frame_key(p)[:3]:p for p in before}
    new={frame_key(p)[:3]:p for p in after}
    if len(old)!=len(before) or len(new)!=len(after) or old.keys()!=new.keys():
        raise CutSetbackRejected('source_contacts_changed_or_ambiguous')
    for key,pose in old.items():
        if pose['section'] not in sections:
            continue
        station=new[key]['station']
        advance=(station-pose['station'])*pose['direction']*(-1 if pose['incoming'] else 1)
        if (new[key]['incoming']!=pose['incoming'] or advance<=0 or
                not 0<station<network.graph['lines'][pose['edge']].length):
            raise CutSetbackRejected('source_contact_did_not_advance_outward')


def setback_network(network,module,sections,distance):
    if module not in network.junctions or not sections or not np.isfinite(distance) or distance<=0:
        raise ValueError('setback requires a junction, sections and a finite positive distance')
    old_poses,_ = network.poses(module,{})
    result = fork_cut_plan(network,module,sections)
    changes,affected = [],{module}
    for section in sorted(sections):
        contacts = [p for p in old_poses if p['section']==section]
        if not contacts:
            raise CutSetbackRejected('source_section_has_no_contact')
        pose = min(contacts,key=lambda p:(p['edge'],p['station'],p['direction']))
        point,tangent,station = displaced_source_section(network,pose,distance)
        corridor = pose['corridor']
        core,road = result.shape(module),result.shape(corridor)
        new_core,new_road,line = setback_shared_section(core,road,result.sections[section],point,tangent)
        result.shapes[module],result.shapes[corridor] = new_core,new_road
        result.sections[section] = line
        for port in result.junctions[module]:
            if port['section']==section:
                port['line'] = line
        affected.add(corridor)
        changes.append(dict(junction=module,section=section,edge=pose['edge'],station=station,corridor=corridor,
                            shared_section=shapely.get_coordinates(line).tolist(),
                            transferred_area_m2=new_core.area-core.area))
    check_external_sections(result,affected)
    for owner in affected:
        result.records.pop(owner,None)
    check_contact_progress(result,old_poses,result.poses(module,{})[0],sections)
    return result,changes,affected


def route_signatures(report,poses,accepted=False):
    keys = [(p['section'],p['edge'],p['direction']) for p in poses]
    if len(set(keys))!=len(keys):
        raise CutSetbackRejected('multiple_source_contacts_on_same_section')
    return {(keys[t['entry']],keys[t['exit']]) for t in report['turns'] if not accepted or t['accepted']}


def external_frames_unchanged(before,after,sections):
    rows = [{frame_key(p):p for p in poses if p['section'] not in sections} for poses in (before,after)]
    return rows[0].keys()==rows[1].keys() and all(np.array_equal(rows[0][k][field],rows[1][k][field])
                                               for k in rows[0] for field in ('position','heading'))


def serialize_cut_plan(network,changes):
    affected = sorted({c[k] for c in changes for k in ('junction','corridor')})
    sections = sorted({c['section'] for c in changes})
    return dict(changes=changes,parts=[dict(module=m,wkb=shapely.to_wkb(network.shape(m),hex=True)) for m in affected],
                sections=[dict(section=s,wkb=shapely.to_wkb(network.sections[s],hex=True)) for s in sections])


def restore_cut_plan(network,plan):
    for row in plan['parts']:
        network.shapes[row['module']] = shapely.from_wkb(row['wkb'])
    for row in plan['sections']:
        network.sections[row['section']] = shapely.from_wkb(row['wkb'])
    for change in plan['changes']:
        for port in network.junctions[change['junction']]:
            if port['section']==change['section']:
                port['line'] = network.sections[change['section']]
        network.registry.pop(change['junction'],None)
    moved = {row['section'] for row in plan['sections']}
    network.frames = {k:v for k,v in network.frames.items() if k[0] not in moved}
    for row in plan['parts']:
        network.records.pop(row['module'],None)
