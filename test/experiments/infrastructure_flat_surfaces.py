"""Partition a flat physical network into shared junctions and corridor surfaces."""

import time
import numpy as np
import shapely

from geos_triangulation import triangles_of
from infrastructure_flat_corners import rounded_plan
from infrastructure_flat_junctions import junction_nodes,port_plan
from infrastructure_flat_ports import port_sections
from infrastructure_flat_cuts import cuts_plan
from infrastructure_network_plan import boundary_segments
from infrastructure_vertex_pool import welded_vertices


def junction_seeds(graphs,mode):
    points,radii = [],[]
    for graph in graphs:
        selected,radius = junction_nodes(graph,mode)
        points.extend(graph['vertices'][selected])
        radii.extend(radius[selected]*1.5)
    return np.asarray(points).reshape(-1,2),np.asarray(radii)


def load_graphs(path,report):
    data = np.load(path)
    graphs = []
    for i,stats in enumerate(report['graphs']):
        offsets = data[f'{i}_line_offsets']
        xy = data[f'{i}_line_coordinates']
        lines = np.array([shapely.LineString(xy[a:b]) for a,b in zip(offsets[:-1],offsets[1:])],dtype=object)
        owners = data[f'{i}_owner_sources']
        cuts = data[f'{i}_owner_offsets']
        half = np.maximum.reduceat(data['source_widths'][owners],cuts[:-1])*.5
        graphs.append(dict(vertices=data[f'{i}_vertices'],edges=data[f'{i}_edges'],
                           lines=lines,half_widths=half,tier=stats['tier']))
    return graphs


def physical_plan(graphs,precision,junction_mode,corner_radius=0,arc_error=.025):
    lines = np.concatenate([g['lines'] for g in graphs])
    half = np.concatenate([g['half_widths'] for g in graphs])
    bands = shapely.buffer(lines,half,quad_segs=2,cap_style='round',join_style='round')
    complete = shapely.set_precision(shapely.set_precision(shapely.union_all(bands),precision),0)
    details,geometry = {},{}
    if corner_radius>0:
        geometry['original_boundary'] = shapely.get_coordinates(boundary_segments(complete)).reshape(-1,2,2)
        complete,details['curbs'] = rounded_plan(complete,corner_radius,arc_error)
    if junction_mode=='cuts':
        parts,roles,details['cuts'],sections = cuts_plan(complete,graphs,precision,corner_radius)
        geometry.update(sections)
        return complete,parts,roles,details,geometry
    if junction_mode=='ports':
        kernel,_ = port_plan(graphs,precision,corner_radius,arc_error)
    else:
        points,radii = junction_seeds(graphs,junction_mode)
        kernel = shapely.union_all(shapely.buffer(shapely.points(points),radii,quad_segs=4))
    junctions = complete.intersection(kernel)
    corridors = complete.difference(junctions)
    parts = np.r_[shapely.get_parts(junctions),shapely.get_parts(corridors)]
    roles = np.r_[np.ones(len(shapely.get_parts(junctions)),dtype=int),
                  np.zeros(len(shapely.get_parts(corridors)),dtype=int)]
    good = (shapely.get_type_id(parts)==3) & (shapely.area(parts)>0)
    parts,roles = parts[good],roles[good]
    if not np.all(shapely.is_valid(parts)):
        raise ValueError('invalid flat module')
    coverage = abs(float(shapely.area(parts).sum())-complete.area)
    if coverage>max(1e-7,complete.area*1e-12):
        raise ValueError('module partition lost or duplicated area')
    return complete,parts,roles,details,geometry


def shared_mesh(complete,parts,roles,precision):
    polygons = triangles_of(shapely.MultiPolygon(parts.tolist()))
    xy = shapely.get_coordinates(polygons).reshape(-1,4,2)[:,:3]
    vertices,indices = np.unique(xy.reshape(-1,2),axis=0,return_inverse=True)
    vertices,labels,displacement = welded_vertices(vertices,precision*1e-4)
    indices = labels[indices]
    indices = indices.reshape(-1,3)
    emitted = vertices[indices]
    a,b = emitted[:,1]-emitted[:,0],emitted[:,2]-emitted[:,0]
    signed = (a[:,0]*b[:,1]-a[:,1]*b[:,0])*.5
    collapsed = signed==0
    collapsed_area = float(shapely.area(polygons[collapsed]).sum())
    if collapsed_area>max(1e-7,complete.area*1e-12):
        raise ValueError('interface welding collapses a significant surface')
    indices,xy,signed = indices[~collapsed],xy[~collapsed],signed[~collapsed]
    indices[signed<0] = indices[signed<0][:,[0,2,1]]
    area = np.abs(signed)
    centres = shapely.points(xy.mean(axis=1))
    shapely.prepare(parts)
    pairs = shapely.STRtree(centres).query(parts,predicate='contains')
    counts = np.bincount(pairs[1],minlength=len(indices))
    if np.any(counts!=1):
        raise ValueError('mesh face has ambiguous module ownership')
    owners = np.empty(len(indices),dtype=int)
    owners[pairs[1]] = pairs[0]
    edges = np.r_[indices[:,[0,1]],indices[:,[1,2]],indices[:,[2,0]]]
    faces = np.tile(np.arange(len(indices)),3)
    unique,labels,counts = np.unique(np.sort(edges,axis=1),axis=0,return_inverse=True,return_counts=True)
    if np.any(counts>2):
        raise ValueError('non-manifold flat surface')
    boundary = shapely.points(vertices[unique[counts==1]].mean(axis=1))
    segments = boundary_segments(complete)
    nearest = shapely.STRtree(segments).nearest(boundary)
    error = shapely.distance(boundary,segments[nearest]).max(initial=0)
    if error>precision*.01:
        raise ValueError(f'flat mesh contains a crack or T-junction: {error}')
    order = np.argsort(labels,kind='stable')
    starts = np.r_[0,np.cumsum(counts)]
    shared = np.flatnonzero(counts==2)
    first,second = faces[order[starts[shared]]],faces[order[starts[shared]+1]]
    different = owners[first]!=owners[second]
    ports = unique[shared[different]]
    adjacent = np.c_[owners[first[different]],owners[second[different]]]
    coverage = abs(float(area.sum())-complete.area)
    if coverage>max(1e-7,complete.area*1e-12):
        raise ValueError('flat triangle coverage differs from the physical plan')
    return dict(vertices=vertices,indices=indices,face_owners=owners,part_roles=roles,
                port_edges=ports,port_parts=adjacent),dict(vertices=len(vertices),triangles=len(indices),
                modules=len(parts),junction_modules=int(np.count_nonzero(roles)),
                shared_port_segments=len(ports),area_m2=complete.area,coverage_error_m2=coverage,
                numerically_collapsed_faces=int(collapsed.sum()),collapsed_original_area_m2=collapsed_area,
                maximum_boundary_error_m=float(error))


def solve(graphs,precision=.001,junction_mode='all',corner_radius=0,arc_error=.025):
    began = time.perf_counter()
    complete,parts,roles,details,geometry = physical_plan(graphs,precision,junction_mode,corner_radius,arc_error)
    plan_ms = (time.perf_counter()-began)*1000
    began = time.perf_counter()
    product,report = shared_mesh(complete,parts,roles,precision)
    report['ports'],product['unresolved_port_centres'] = port_sections(product,precision*.01)
    product.update(geometry)
    report.update(details)
    report.update(plan_ms=plan_ms,mesh_and_audit_ms=(time.perf_counter()-began)*1000,junction_mode=junction_mode,
                  scope='Flat shared surfaces only; smooth driving curves, heights and collision delivery remain open.')
    return product,report
