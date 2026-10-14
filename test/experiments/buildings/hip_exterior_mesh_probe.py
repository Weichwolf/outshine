"""Emit own exterior envelope, original roof field and native UVs as a closed solid."""

from dataclasses import dataclass,replace

import numpy as np
import shapely

from hip_exterior_surface_probe import prepare,prepare_collar
from outshine_ref.buildings.modelling import Face,Shape,prepare_hip
from outshine_ref.buildings.modelling.facet_overlay import FacetOverlay
from outshine_ref.buildings.modelling.hip_sides import close_rim
from outshine_ref.buildings.modelling.hip_surface import prepare_even
from outshine_ref.buildings.modelling.roof_mesh import RoofMesh
from outshine_ref.buildings.modelling.uv_exterior import extrapolate_chart,partition_exterior_charts


@dataclass(frozen=True)
class MeshProof:
    shape: object
    plan: object
    surface: object
    cells: int
    maximum_shared_height_adjustment_m: float
    expected_volume_m3: float
    uv_values_checked: int


def chart_ownership(source,cells,points,precision):
    tree=shapely.STRtree(source);pairs=tree.query(points,predicate='within')
    counts=np.bincount(pairs[0],minlength=len(points));result=np.full(len(points),-1,dtype=np.int64)
    result[pairs[0]]=pairs[1]
    for number in np.flatnonzero(counts!=1):
        cell=cells[number]
        if cell.area>4*precision*cell.length:raise ValueError('Ambiguous chart cell exceeds raster uncertainty')
        candidates=tree.query(cell,predicate='intersects')
        if not len(candidates):raise ValueError('Exterior chart cell has no geometric owner')
        areas=shapely.area(shapely.intersection(source[candidates],cell))
        result[number]=candidates[np.argmax(areas)]
    return result


def overlay(corners,plan,field,surface,factor,limit):
    source=list(shapely.polygons(corners));indices=list(range(len(corners)))
    roof=[p.polygon for p in field.facets];gradients=[np.asarray(p.gradient)*factor for p in field.facets]
    intercepts=[p.intercept_m*factor for p in field.facets]
    owners=[getattr(p,'facet',i) for i,p in enumerate(field.facets)]
    rings=(plan.footprint.exterior,*plan.footprint.interiors)
    for patch in surface.patches:
        roof.append(patch.polygon);gradients.append(np.asarray(patch.gradient));intercepts.append(patch.intercept_m);owners.append(patch.owner)
        ring,edge=plan.facets[patch.owner].boundary;xy=np.asarray(rings[ring].coords)[edge:edge+2]
        for chart in partition_exterior_charts(corners,xy[0],xy[1],patch.polygon,max_regions=limit):
            source.append(chart.polygon);indices.append(chart.triangle)
    source=shapely.set_precision(np.asarray(source,dtype=object),surface.precision_m)
    roof=shapely.set_precision(np.asarray(roof,dtype=object),surface.precision_m)
    network=shapely.union_all(np.r_[shapely.boundary(source),shapely.boundary(roof)],grid_size=surface.precision_m)
    cells=shapely.get_parts(shapely.polygonize(shapely.get_parts(network)))
    cells=cells[shapely.covers(surface.domain,shapely.point_on_surface(cells))]
    if not len(cells) or len(cells)>limit:raise ValueError('Exterior overlay cell capacity exceeded')
    points=shapely.point_on_surface(cells)
    sources=chart_ownership(source,cells,points,surface.precision_m)
    roof_ids=chart_ownership(roof,cells,points,surface.precision_m)
    triangles,cell_ids=shapely.get_parts(shapely.constrained_delaunay_triangles(cells),return_index=True)
    if len(triangles)>4*limit:raise ValueError('Exterior triangle capacity exceeded')
    xyz=shapely.get_coordinates(triangles).reshape(-1,4,2)[:,:3].copy()
    delta=xyz[:,1:]-xyz[:,:1]
    reverse=delta[:,0,0]*delta[:,1,1]-delta[:,0,1]*delta[:,1,0]<0
    xyz[reverse]=xyz[reverse][:,[0,2,1]]
    cut=FacetOverlay(xyz,sources[cell_ids],roof_ids[cell_ids],len(cells),surface.precision_m)
    return replace(cut,source_triangles=np.asarray(indices)[cut.source_triangles]),np.asarray(gradients),np.asarray(intercepts),np.asarray(owners)


def emit(original,polygon,value=3.,mode='byHeight',overhang=.5,even=False,max_cells=65536,shell_depth_m=.1,method='envelope'):
    if not np.isfinite(shell_depth_m) or shell_depth_m<=0:raise ValueError('Roof shell depth must be positive')
    before=original.emit()
    if np.any(abs(before['vertices'][before['faces'],1])>1e-9):raise ValueError('Prototype requires the source plane at y=0')
    plan=prepare_hip(polygon);field=prepare_even(plan,mode) if even else plan;factor=field.factor(value,mode)
    if method not in ('envelope','collar'):raise ValueError('Unknown exterior method')
    build=prepare_collar if method=='collar' else prepare
    surface=build(plan,value,mode,overhang,even,max_pairs=max_cells)
    surface=replace(surface,base_level_m=surface.base_level_m-shell_depth_m)
    xy=before['vertices'][:,[0,2]]*[1.,-1.]-plan.origin;corners=xy[before['faces']]
    attributes=np.concatenate([before['uv'+str(c) if c else 'uv'][before['faces']] for c in range(10)],axis=2)
    cut,gradient,intercept,owners=overlay(corners,plan,field,surface,factor,max_cells)
    points=cut.corners.reshape(-1,2);triangles=np.repeat(cut.source_triangles,3);patches=np.repeat(cut.facets,3)
    uv=np.empty((len(points),20));checked=0
    for triangle in np.unique(triangles):
        selected=triangles==triangle;uv[selected]=extrapolate_chart(corners,attributes,int(triangle),points[selected])
        fit=np.linalg.solve(np.c_[corners[triangle],np.ones(3)],attributes[triangle])
        expected=np.c_[points[selected],np.ones(selected.sum())]@fit
        np.testing.assert_allclose(uv[selected],expected,atol=1e-7,rtol=0.);checked+=expected.size
    heights=(points*gradient[patches]).sum(axis=1)+intercept[patches]
    builder=RoofMesh(tuple(range(10)),np.array([0.,1.,0.]))
    roots=[];upper=[];known={};adjustment=0.
    for xy,height,owner in zip(points,heights,patches,strict=True):
        key=tuple(xy);uncertainty=np.sqrt(2)*surface.precision_m*np.linalg.norm(gradient[owner])+1e-7
        if abs(height-surface.base_level_m)<=uncertainty:height=surface.base_level_m
        if key in known:
            root,prior,prior_owner=known[key];difference=abs(height-prior)
            assert difference<=uncertainty,('Shared exterior height disagreement',difference,uncertainty,key,int(prior_owner),int(owner),float(prior),float(height))
            adjustment=max(adjustment,difference)
        else:root=builder.root(('base',*key));known[key]=(root,float(height),int(owner))
        roots.append(root);upper.append(root if known[key][1]==surface.base_level_m else builder.root(('top',root)))
    heights=np.asarray([known[tuple(xy)][1] for xy in points]);roots=np.asarray(roots);upper=np.asarray(upper)
    base=np.c_[points[:,0]+plan.origin[0],np.full(len(points),surface.base_level_m),-(points[:,1]+plan.origin[1]),uv]
    template=Face((0,1,2),material=original.geometry.polygons[0].material,group=original.geometry.polygons[0].group,
                  tags=original.geometry.polygons[0].tags-{'roof.top','roof.bottom','roof.side.outer','roof.side.inner'})
    order=np.arange(len(base)).reshape(-1,3)[:,::-1].ravel()
    builder.append(base[order],roots[order],'bottom',replace(template,tags=template.tags|{'roof.bottom'}))
    close_rim(builder,np.array([0.,1.,0.]),dict(zip(roots,heights-surface.base_level_m,strict=True)),template)
    top=base.copy();top[:,1]=heights
    for patch in np.unique(patches):
        selected=patches==patch
        builder.append(top[selected],upper[selected],('top',int(patch)),replace(template,tags=template.tags|{'roof.top'}))
    mesh,faces=builder.finish();shape=Shape.from_mesh(mesh,polygons=faces).with_metadata(
        group_names=dict(original.geometry.group_names),material_defs=dict(original.geometry.material_defs))
    expected=sum(p.polygon.area*(np.asarray(p.polygon.centroid.coords[0])@p.gradient+p.intercept_m)*factor
                 for p in field.facets)-plan.footprint.area*surface.base_level_m
    expected+=sum(p.polygon.area*(np.asarray(p.polygon.centroid.coords[0])@p.gradient+p.intercept_m-surface.base_level_m)
                  for p in surface.patches)
    return MeshProof(shape,plan,surface,cut.cells,adjustment,expected,checked)
