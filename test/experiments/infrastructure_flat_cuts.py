"""Partition the actual road surface with complete straight cross-section cuts."""

import numpy as np
import shapely

from infrastructure_flat_junctions import continuation_chains,junction_nodes
from infrastructure_network_plan import boundary_segments


def cross_sections(graphs,precision,radius):
    centres,normals,half,nodes = [],[],[],[]
    for graph in graphs:
        graph = continuation_chains(graph,precision)
        selected,widths = junction_nodes(graph,'branch')
        nodes.extend(graph['vertices'][selected])
        lines,edges = graph['lines'],graph['edges']
        length = shapely.length(lines)
        reach = np.where(selected[edges],2*(widths[edges]+radius),0)
        source = np.tile(np.arange(len(lines)),2)
        side = np.repeat([0,1],len(lines))
        wanted = selected[edges.T.ravel()]&(np.tile(length-reach.sum(axis=1),2)>precision)
        source,side = source[wanted],side[wanted]
        station = np.where(side==0,reach[source,0],length[source]-reach[source,1])
        p = shapely.get_coordinates(shapely.line_interpolate_point(lines[source],station))
        a = shapely.get_coordinates(shapely.line_interpolate_point(lines[source],station-precision*.1))
        b = shapely.get_coordinates(shapely.line_interpolate_point(lines[source],station+precision*.1))
        tangent = b-a
        tangent /= np.linalg.norm(tangent,axis=1)[:,None]
        centres.extend(p)
        normals.extend(np.c_[-tangent[:,1],tangent[:,0]])
        half.extend(graph['half_widths'][source])
    return np.asarray(centres).reshape(-1,2),np.asarray(normals).reshape(-1,2),np.asarray(half),np.asarray(nodes).reshape(-1,2)


def surface_spans(complete,centres,normals,half,precision):
    segments = boundary_segments(complete)
    xy = shapely.get_coordinates(segments).reshape(-1,2,2)
    tree = shapely.STRtree(segments)
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


def cuts_plan(complete,graphs,precision,radius):
    centres,normals,half,nodes = cross_sections(graphs,precision,radius)
    before,after,attempts = surface_spans(complete,centres,normals,half,precision)
    good = np.isfinite(before)&np.isfinite(after)
    p,n = centres[good],normals[good]
    cut_xy = np.stack((p-n*(before[good]+precision)[:,None],p+n*(after[good]+precision)[:,None]),axis=1)
    cuts = shapely.linestrings(cut_xy)
    lines = shapely.get_parts(shapely.union_all(np.r_[boundary_segments(complete),cuts]))
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
    report = dict(cuts=len(cuts),unresolved_cuts=int((~good).sum()),ray_expansions=attempts,
                  scope='Straight complete cuts; junction classification and traffic routes remain experimental.')
    geometry = dict(cut_centres=p,cut_normals=n,
                    cut_spans=np.stack((p-n*before[good,None],p+n*after[good,None]),axis=1))
    return parts,roles,report,geometry
