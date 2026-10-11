"""Partition the actual road surface with complete straight cross-section cuts."""

import time

import numpy as np
import shapely

from infrastructure_flat_junctions import continuation_chains,junction_nodes
from infrastructure_network_plan import boundary_segments
from infrastructure_cut_clearance import motor_model,invalid_vehicle_sections


def section_model(graphs,precision,radius):
    axes,depths,half,nodes = [],[],[],[]
    for graph in graphs:
        graph = continuation_chains(graph,precision)
        selected,widths = junction_nodes(graph,'branch')
        nodes.extend(graph['vertices'][selected])
        lines,edges = graph['lines'],graph['edges']
        reach = np.where(selected[edges],2*(widths[edges]+radius),0)
        axes.extend(lines)
        depths.extend(reach)
        half.extend(graph['half_widths'])
    lines = np.asarray(axes,dtype=object)
    return dict(lines=lines,length=shapely.length(lines),depth=np.asarray(depths).reshape(-1,2),
                half=np.asarray(half),nodes=np.asarray(nodes).reshape(-1,2))


def sections_of(model,precision):
    lines,length,depth = model['lines'],model['length'],model['depth']
    source = np.tile(np.arange(len(lines)),2)
    side = np.repeat([0,1],len(lines))
    clearance = np.maximum(precision,2*model['half'])
    wanted = (depth.T.ravel()>0)&np.tile(length-depth.sum(axis=1)>clearance,2)
    source,side = source[wanted],side[wanted]
    station = np.where(side==0,depth[source,0],length[source]-depth[source,1])
    p = shapely.get_coordinates(shapely.line_interpolate_point(lines[source],station))
    a = shapely.get_coordinates(shapely.line_interpolate_point(lines[source],station-precision*.1))
    b = shapely.get_coordinates(shapely.line_interpolate_point(lines[source],station+precision*.1))
    tangent = b-a
    tangent /= np.linalg.norm(tangent,axis=1)[:,None]
    return p,np.c_[-tangent[:,1],tangent[:,0]],model['half'][source],source,side


def surface_spans(complete,centres,normals,half,precision,boundary):
    xy,tree = boundary
    before,after = np.full(len(centres),np.inf),np.full(len(centres),np.inf)
    extent = np.maximum(half*2,precision)
    limit = np.linalg.norm(np.subtract(complete.bounds[2:],complete.bounds[:2]))+precision
    attempts = 0
    while attempts<32:
        active = np.flatnonzero(~np.isfinite(before)|~np.isfinite(after))
        if not len(active):
            break
        p,n,r = centres[active],normals[active],extent[active]
        probes = shapely.linestrings(np.stack((p-n*r[:,None],p+n*r[:,None]),axis=1))
        pairs = tree.query(probes)
        source = active[pairs[0]]
        a,b = xy[pairs[1],0],xy[pairs[1],1]
        v,offset = b-a,a-centres[source]
        direction = normals[source]
        det = direction[:,0]*v[:,1]-direction[:,1]*v[:,0]
        good = np.abs(det)>1e-14
        source,v,offset,direction,det = source[good],v[good],offset[good],direction[good],det[good]
        t = (offset[:,0]*v[:,1]-offset[:,1]*v[:,0])/det
        u = (offset[:,0]*direction[:,1]-offset[:,1]*direction[:,0])/det
        good = (u>=-1e-12)&(u<=1+1e-12)&(np.abs(t)<=extent[source])
        source,t = source[good],t[good]
        negative,positive = t<0,t>0
        np.minimum.at(before,source[negative],-t[negative])
        np.minimum.at(after,source[positive],t[positive])
        attempts += 1
        if np.all(extent[active]>=limit):
            break
        extent[active] = np.minimum(extent[active]*2,limit)
    return before,after,attempts


def intersecting_sections(spans,normals,complete):
    lines = shapely.linestrings(spans)
    pairs = shapely.STRtree(lines).query(lines,predicate='intersects')
    pairs = pairs[:,pairs[0]<pairs[1]]
    a,b = normals[pairs[0]],normals[pairs[1]]
    crossing = np.abs(a[:,0]*b[:,1]-a[:,1]*b[:,0])>1e-10
    pairs = pairs[:,crossing]
    intersections = shapely.intersection(lines[pairs[0]],lines[pairs[1]])
    good = shapely.contains(complete,shapely.point_on_surface(intersections))
    return pairs[:,good]


def resolved_sections(complete,graphs,precision,radius,segments,vehicle=None,error=.025):
    model = section_model(graphs,precision,radius)
    original = model['depth'].copy()
    boundary = shapely.get_coordinates(segments).reshape(-1,2,2),shapely.STRtree(segments)
    began = time.perf_counter()
    motor = motor_model(graphs,complete,vehicle,radius,error) if vehicle is not None else None
    prepare_ms = (time.perf_counter()-began)*1000
    passes,ray_passes,checked,check_ms = 0,0,0,0.
    adjusted_vehicle_cuts = set()
    for passes in range(32):
        p,n,half,source,side = sections_of(model,precision)
        before,after,attempts = surface_spans(complete,p,n,half,precision,boundary)
        ray_passes = max(ray_passes,attempts)
        good = np.isfinite(before)&np.isfinite(after)
        valid_rows = np.flatnonzero(good)
        spans = np.stack((p[good]-n[good]*before[good,None],p[good]+n[good]*after[good,None]),axis=1)
        conflicts = intersecting_sections(spans,n[good],complete)
        began = time.perf_counter()
        invalid,poses = (invalid_vehicle_sections(spans,motor,vehicle,error) if motor is not None else
                         (np.empty(0,dtype=int),0))
        check_ms += (time.perf_counter()-began)*1000
        checked += poses
        adjusted_vehicle_cuts.update((int(source[i]),int(side[i])) for i in valid_rows[invalid])
        if (not conflicts.size and not len(invalid)) or passes==31:
            break
        changed = valid_rows[np.unique(np.r_[conflicts.ravel(),invalid])]
        model['depth'][source[changed],side[changed]] *= 2
        model['depth'] = np.minimum(model['depth'],model['length'][:,None])
    omitted = []
    failed = np.unique(np.r_[conflicts.ravel(),invalid])
    invalid_set = set(invalid.tolist())
    for i in failed:
        row = valid_rows[i]
        omitted.append(dict(source_chain=int(source[row]),end=int(side[row]),
                            reason='vehicle_port_clearance_budget' if i in invalid_set else 'cut_conflict_budget'))
    accepted = np.ones(len(spans),dtype=bool)
    accepted[failed] = False
    geometry = dict(cut_centres=p[good][accepted],cut_normals=n[good][accepted],cut_spans=spans[accepted])
    report = dict(cuts=int(accepted.sum()),unresolved_cuts=int((~good).sum())+len(omitted),omitted_cuts=omitted,
                  vehicle_cut_adjustments=len(adjusted_vehicle_cuts),vehicle_poses_checked=checked,
                  motor_prepare_ms=prepare_ms,motor_clearance_ms=check_ms,ray_expansions=ray_passes,
                  cut_adjustment_passes=passes,adjusted_ends=int((model['depth']>original).sum()),
                  remaining_cut_conflicts=0,remaining_vehicle_cut_failures=0,
                  absorbed_subwidth_remainders=int(np.count_nonzero((model['depth'].sum(axis=1)>0)&
                      (model['length']-model['depth'].sum(axis=1)>precision)&
                      (model['length']-model['depth'].sum(axis=1)<=2*model['half']))),
                  absorbed_short_chains=int(((original.sum(axis=1)<model['length'])&
                                             (model['depth'].sum(axis=1)>=model['length'])).sum()))
    return geometry,model['nodes'],report


def cuts_plan(complete,graphs,precision,radius,vehicle=None,error=.025):
    shapely.prepare(complete)
    segments = boundary_segments(complete)
    geometry,nodes,report = resolved_sections(complete,graphs,precision,radius,segments,vehicle,error)
    cut_xy = geometry['cut_spans']+geometry['cut_normals'][:,None,:]*np.array([-precision,precision])[None,:,None]
    cuts = shapely.linestrings(cut_xy)
    lines = shapely.get_parts(shapely.union_all(np.r_[segments,cuts]))
    parts = shapely.get_parts(shapely.polygonize(lines))
    shapely.prepare(complete)
    parts = parts[shapely.contains(complete,shapely.point_on_surface(parts))]
    shapely.prepare(parts)
    pairs = shapely.STRtree(shapely.points(nodes)).query(parts,predicate='contains')
    roles = np.zeros(len(parts),dtype=int)
    roles[pairs[0]] = 1
    junctions = shapely.get_parts(shapely.union_all(parts[roles==1]))
    corridors = shapely.get_parts(shapely.union_all(parts[roles==0]))
    parts = np.r_[junctions,corridors]
    roles = np.r_[np.ones(len(junctions),dtype=int),np.zeros(len(corridors),dtype=int)]
    coverage = abs(float(shapely.area(parts).sum())-complete.area)
    if not np.all(shapely.is_valid(parts)) or coverage>max(1e-7,complete.area*1e-12):
        raise ValueError('straight cuts do not partition the original road surface')
    report['scope'] = 'Straight complete cuts; junction classification and traffic routes remain experimental.'
    return parts,roles,report,geometry
