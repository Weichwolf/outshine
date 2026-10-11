"""Build bounded junction regions with straight outer cuts through source bands."""

import numpy as np
import shapely
from scipy.sparse import coo_matrix
from scipy.sparse.csgraph import connected_components
from scipy.spatial import cKDTree

from infrastructure_flat_corners import rounded_plan


def junction_nodes(graph,mode):
    edges,vertices,half = graph['edges'],graph['vertices'],graph['half_widths']
    degree = np.bincount(edges.ravel(),minlength=len(vertices))
    maximum,minimum = np.zeros(len(vertices)),np.full(len(vertices),np.inf)
    for side in (0,1):
        np.maximum.at(maximum,edges[:,side],half)
        np.minimum.at(minimum,edges[:,side],half)
    selected = degree>=2 if mode=='all' else (degree>=3)|((degree==2)&(maximum-minimum>1e-7))
    return selected,maximum


def continuation_chains(graph,precision):
    edges,lines = graph['edges'],graph['lines']
    selected,_ = junction_nodes(graph,'branch')
    nodes = edges.T.ravel()
    degree = np.bincount(nodes,minlength=len(graph['vertices']))
    joined = np.flatnonzero((degree==2)&~selected)
    if not len(joined):
        return graph
    order = np.argsort(nodes,kind='stable')
    offsets = np.r_[0,np.cumsum(degree)]
    incident = np.tile(np.arange(len(edges)),2)[order]
    pairs = incident[offsets[joined,None]+[0,1]]
    links = coo_matrix((np.ones(len(pairs)),pairs.T),shape=(len(edges),len(edges))).tocsr()
    count,labels = connected_components(links,directed=False)
    order = np.argsort(labels,kind='stable')
    merged = shapely.line_merge(shapely.multilinestrings(lines[order],indices=labels[order]))
    if np.any(shapely.get_type_id(merged)!=1):
        raise ValueError('a physical continuation chain branches')
    xy,owners = shapely.get_coordinates(merged,return_index=True)
    offsets = np.r_[0,np.cumsum(np.bincount(owners,minlength=count))]
    ends = xy[np.c_[offsets[:-1],offsets[1:]-1]].reshape(-1,2)
    distance,vertices = cKDTree(graph['vertices']).query(ends)
    if distance.max(initial=0)>precision*1e-4:
        raise ValueError('a physical continuation chain loses its source endpoints')
    half = np.zeros(count)
    np.maximum.at(half,labels,graph['half_widths'])
    return dict(vertices=graph['vertices'],edges=vertices.reshape(-1,2),lines=merged,half_widths=half)


def port_plan(graphs,precision,corner_radius,arc_error):
    pieces,widths,centres,radii = [],[],[],[]
    for graph in graphs:
        graph = continuation_chains(graph,precision)
        selected,half = junction_nodes(graph,'branch')
        centres.extend(graph['vertices'][selected])
        radii.extend(half[selected]+precision*2)
        lines,edges = graph['lines'],graph['edges']
        lengths = shapely.length(lines)
        xy,owners = shapely.get_coordinates(lines,return_index=True)
        offsets = np.r_[0,np.cumsum(np.bincount(owners,minlength=len(lines)))]
        steps = np.r_[0,np.linalg.norm(np.diff(xy,axis=0),axis=1)]
        steps[offsets[:-1]] = 0
        cumulative = np.cumsum(steps)
        stations = cumulative-cumulative[offsets[:-1]][owners]
        source = np.tile(np.arange(len(lines)),2)
        nodes = np.r_[edges[:,0],edges[:,1]]
        sides = np.repeat([0,1],len(lines))
        good = selected[nodes]
        source,nodes,sides = source[good],nodes[good],sides[good]
        reach = np.minimum(2*(half[nodes]+corner_radius),lengths[source])
        cuts = np.where(sides==0,reach,lengths[source]-reach)
        points = shapely.get_coordinates(shapely.line_interpolate_point(lines[source],cuts))
        whole = reach>=lengths[source]
        other = edges[source[whole],1-sides[whole]]
        centres.extend(graph['vertices'][other])
        radii.extend(graph['half_widths'][source[whole]]+precision*2)
        for index,side,cut,p in zip(source,sides,cuts,points):
            a,b = offsets[index:index+2]
            local,s = xy[a:b],stations[a:b]
            part = np.vstack((local[s<cut],p)) if side==0 else np.vstack((p,local[s>cut]))
            part = part[np.r_[True,np.any(np.diff(part,axis=0)!=0,axis=1)]]
            if len(part)>=2:
                pieces.append(part)
                widths.append(graph['half_widths'][index]+precision*2)
    if not pieces:
        return shapely.GeometryCollection(),dict(partial_bands=0,junction_nodes=0)
    coordinates = np.concatenate(pieces)
    indices = np.repeat(np.arange(len(pieces)),[len(p) for p in pieces])
    lines = shapely.linestrings(coordinates,indices=indices)
    bands = shapely.buffer(lines,widths,quad_segs=2,cap_style='flat',join_style='round')
    disks = shapely.buffer(shapely.points(np.asarray(centres).reshape(-1,2)),
                           np.asarray(radii)/np.cos(np.pi/16),quad_segs=4)
    kernel = shapely.union_all(np.r_[bands,disks])
    if corner_radius>0:
        kernel,_ = rounded_plan(kernel,corner_radius,arc_error)
    return kernel,dict(partial_bands=len(pieces),junction_nodes=len(centres),
                       scope='Partition mask only; it does not widen the physical road surface.')
