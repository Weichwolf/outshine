"""Conformal source UV and roof-field overlay for positive roof collars."""

from dataclasses import replace

import numpy as np
import shapely

from .facet_overlay import FacetOverlay
from .uv_exterior import partition_exterior_charts

def chart_ownership(source,cells,points,precision,tags=None,targets=None):
    tree=shapely.STRtree(source);pairs=tree.query(points,predicate='within')
    if tags is not None:pairs=pairs[:,tags[pairs[1]]==targets[pairs[0]]]
    counts=np.bincount(pairs[0],minlength=len(points));result=np.full(len(points),-1,dtype=np.int64)
    result[pairs[0]]=pairs[1]
    for number in np.flatnonzero(counts!=1):
        cell=cells[number]
        if cell.area>4*precision*cell.length:raise ValueError('Ambiguous chart cell exceeds raster uncertainty')
        candidates=tree.query(cell,predicate='intersects')
        if tags is not None:candidates=candidates[tags[candidates]==targets[number]]
        if not len(candidates):raise ValueError('Exterior chart cell has no geometric owner')
        areas=shapely.area(shapely.intersection(source[candidates],cell))
        result[number]=candidates[np.argmax(areas)]
    return result


def overlay(corners,plan,field,surface,factor,limit):
    source=list(shapely.polygons(corners));indices=list(range(len(corners)))
    tags=[-1]*len(source)
    roof=[p.polygon for p in field.facets];gradients=[np.asarray(p.gradient)*factor for p in field.facets]
    intercepts=[p.intercept_m*factor for p in field.facets]
    owners=[getattr(p,'facet',i) for i,p in enumerate(field.facets)]
    rings=(plan.footprint.exterior,*plan.footprint.interiors)
    for patch in surface.patches:
        identifier=len(roof)
        roof.append(patch.polygon);gradients.append(np.asarray(patch.gradient));intercepts.append(patch.intercept_m);owners.append(patch.owner)
        ring,edge=plan.facets[patch.owner].boundary;xy=np.asarray(rings[ring].coords)[edge:edge+2]
        for chart in partition_exterior_charts(corners,xy[0],xy[1],patch.polygon,max_regions=limit):
            source.append(chart.polygon);indices.append(chart.triangle);tags.append(identifier)
            if len(source)>limit:raise ValueError('Exterior UV chart capacity exceeded')
    source=shapely.set_precision(np.asarray(source,dtype=object),surface.precision_m)
    roof=shapely.set_precision(np.asarray(roof,dtype=object),surface.precision_m)
    network=shapely.union_all(np.r_[shapely.boundary(source),shapely.boundary(roof)],grid_size=surface.precision_m)
    cells=shapely.get_parts(shapely.polygonize(shapely.get_parts(network)))
    cells=cells[shapely.covers(surface.domain,shapely.point_on_surface(cells))]
    if not len(cells) or len(cells)>limit:raise ValueError('Exterior overlay cell capacity exceeded')
    points=shapely.point_on_surface(cells)
    roof_ids=chart_ownership(roof,cells,points,surface.precision_m)
    targets=np.where(roof_ids<len(field.facets),-1,roof_ids)
    sources=chart_ownership(source,cells,points,surface.precision_m,np.asarray(tags),targets)
    triangles,cell_ids=shapely.get_parts(shapely.constrained_delaunay_triangles(cells),return_index=True)
    if len(triangles)>4*limit:raise ValueError('Exterior triangle capacity exceeded')
    xyz=shapely.get_coordinates(triangles).reshape(-1,4,2)[:,:3].copy()
    delta=xyz[:,1:]-xyz[:,:1]
    reverse=delta[:,0,0]*delta[:,1,1]-delta[:,0,1]*delta[:,1,0]<0
    xyz[reverse]=xyz[reverse][:,[0,2,1]]
    cut=FacetOverlay(xyz,sources[cell_ids],roof_ids[cell_ids],len(cells),surface.precision_m)
    return replace(cut,source_triangles=np.asarray(indices)[cut.source_triangles]),np.asarray(gradients),np.asarray(intercepts),np.asarray(owners)


