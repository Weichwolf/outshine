"""Verify edge sweeps against analytic strips, connected rims and actual PRT side surfaces."""

import argparse
import json
from pathlib import Path

import numpy as np
import shapely

from esri_oracle import CASES,EXAMPLES,generate
from evidence import revision
from outshine_ref.buildings.modelling.components import Branch
from outshine_ref.buildings.modelling.extrude import MODES
from outshine_ref.buildings.modelling.shape import Shape
from outshine_ref.buildings.modelling.wire import Edge
from provider_contract import attributes
from samples import read_site
from verify_modelling_extrude import near


def area(mesh):
    triangles=mesh['vertices'][mesh['faces']]
    return float(np.linalg.norm(np.cross(triangles[:,1]-triangles[:,0],triangles[:,2]-triangles[:,0]),axis=1).sum()/2)


def reject(operation):
    try:operation()
    except ValueError:return
    raise AssertionError('An invalid edge sweep was accepted')


def single_edge_controls():
    edge=Edge((0,1),7,('preserve','extrude.top'),((.2,.3),(.8,.9)))
    source=Shape.from_edges([[0.,0.,0.],[4.,0.,0.]],[edge],[[0.,1.,0.]])
    rows=[]
    for mode in MODES:
        for distance in (-2.,2.):
            product=source.extrude(distance,mode);mesh=product.emit();face=product.geometry.polygons[0]
            near(mesh['vertices'][list(face.outer)],[[0,0,0],[4,0,0],[4,distance,0],[0,distance,0]])
            near(area(mesh),8.);near(product.scope.size,[4.,distance,0.])
            if face.uvs!=(((.2,.3),(.8,.9),(.8,.9),(.2,.3)),) or face.material!=7 or face.tags!=frozenset(('preserve','extrude.side.outer')):
                raise AssertionError('Edge sweep lost corner UVs/material or retained old automatic tags')
            if len(product.geometry.polygons)!=1 or len(mesh['faces'])!=2:raise AssertionError('An edge sweep fabricated caps')
            rows.append(dict(mode=mode,distance_m=distance,triangles=2,area_m2=8.))
    if source.extrude(0.) is not source:raise AssertionError('Zero-distance edge sweep rebuilt its geometry')
    reject(lambda:source.extrude(2.,'invalid'))
    reject(lambda:source.extrude(float('nan')))
    return rows


def connected_controls():
    vertices=np.array([[0.,0.,0.],[8.,0.,0.],[8.,0.,5.],[0.,0.,5.]])
    edges=[Edge((i,(i+1)%4),tags=('rim',)) for i in range(4)]
    normals=np.array([[0.,0.,-1.],[1.,0.,0.],[0.,0.,1.],[-1.,0.,0.]])
    wire=Shape.from_edges(vertices,edges,normals);distance=2.;diagonal=distance/np.sqrt(2.)
    smooth=wire.extrude(distance,'vertex.normal');mesh=smooth.emit();faces=smooth.geometry.polygons
    expected=vertices+diagonal*np.array([[-1.,0.,-1.],[1.,0.,-1.],[1.,0.,1.],[-1.,0.,1.]])
    tops={}
    for i,face in enumerate(faces):
        near(mesh['vertices'][face.outer[3]],expected[i]);near(mesh['vertices'][face.outer[2]],expected[(i+1)%4])
        tops.setdefault(face.outer[3],0);tops[face.outer[3]]+=1;tops.setdefault(face.outer[2],0);tops[face.outer[2]]+=1
    if set(tops.values())!={2} or len(tops)!=4:raise AssertionError('Connected edge extrusion duplicated shared top endpoints')
    near(area(mesh),(8+2*diagonal)*(5+2*diagonal)-40.)
    flat=wire.extrude(distance);near(area(flat.emit()),26.*distance)
    if len({v for face in flat.geometry.polygons for v in face.outer[2:]})!=8:
        raise AssertionError('Per-edge normal sweeps incorrectly welded separated corners')
    disconnected=Shape.from_edges(vertices[[0,1,1,2]], [Edge((0,1)),Edge((2,3))],normals[:2])
    output=disconnected.extrude(distance,'vertex.normal');points=output.emit()['vertices'];a,b=output.geometry.polygons
    if np.allclose(points[a.outer[2]],points[b.outer[3]]):raise AssertionError('Coincident independent endpoints were implicitly connected')
    source=Shape.from_polygon(shapely.box(0,0,8,5)).extrude(4).comp('e',Branch('all','='))[0]
    product=source.extrude(2.,'world.up')
    if product.comp_info!=source.comp_info or len(product.geometry.polygons)!=8 or len(product.sweep_info.omitted_edges)!=4:
        raise AssertionError('World-up cube sweep lost predecessor attributes or failed to report zero-area vertical edges')
    tilted=Shape.from_edges([[0.,0.,0.],[4.,3.,0.]],[Edge((0,1))],[[0.,0.,1.]])
    top=tilted.extrude(0.,'world.up.flatTop');near(area(top.emit()),6.)
    if len(top.geometry.polygons[0].outer)!=3:raise AssertionError('Flat-top endpoint contact failed to become a valid triangle')
    cancelled=Shape.from_edges([[0.,0.,0.],[4.,0.,0.],[8.,0.,0.]],[Edge((0,1)),Edge((1,2))],[[0.,1.,0.],[0.,-1.,0.]])
    reject(lambda:cancelled.extrude(2.,'vertex.normal'))
    parallel=Shape.from_edges([[0.,0.,0.],[0.,4.,0.]],[Edge((0,1))],[[1.,0.,0.]])
    empty=parallel.extrude(2.,'world.up')
    if len(empty.emit()['faces']) or empty.sweep_info.omitted_edges!=((0,'zero-area sweep'),):
        raise AssertionError('A wholly tangential sweep fabricated a surface or lost its source reason')
    spatial=Shape.from_edges([[0.,0.,0.],[4.,0.,0.],[4.,0.,4.]],[Edge((0,1)),Edge((1,2))],[[0.,1.,0.],[1.,0.,0.]])
    spatial=spatial.extrude(2.,'vertex.normal')
    if len(spatial.geometry.polygons)!=2 or len(spatial.emit()['faces'])!=4:raise AssertionError('Spatial vertex sweep lost warped quads')
    return dict(shared_endpoint_ids=4,disconnected_endpoints_preserved=True,per_edge_gaps=True,reported_zero_area_edges=4,
                flat_top_triangle=True,empty_parallel_sweep_reported=True,spatial_warped_quads=2)


def vertical_surface_comparison(own,reference_vertices,reference_indices):
    reference=reference_vertices[reference_indices];vectors=np.cross(reference[:,1]-reference[:,0],reference[:,2]-reference[:,0])
    wall_ids=np.flatnonzero(abs(vectors[:,1])<1e-8);matched=set();seen=[];own_triangles=own['vertices'][own['faces']]
    for triangle in own_triangles:
        normal=np.cross(triangle[1]-triangle[0],triangle[2]-triangle[0]);normal/=np.linalg.norm(normal)
        if normal[0]<-1e-9 or (abs(normal[0])<=1e-9 and normal[2]<0):normal=-normal
        origin=triangle[0]
        if any(abs(np.dot(normal,previous-normal))<1e-8 and abs(np.dot(normal,origin-previous_origin))<1e-7 for previous,previous_origin in seen):continue
        seen.append((normal,origin));tangent=np.array([normal[2],0.,-normal[0]]);basis=np.array([tangent,[0.,1.,0.]])
        reference_ids=[i for i in wall_ids if np.max(abs((reference[i]-origin)@normal))<1e-7]
        selected=[t for t in own_triangles if np.max(abs((t-origin)@normal))<1e-7]
        a=shapely.union_all([shapely.Polygon((t-origin)@basis.T) for t in selected])
        b=shapely.union_all([shapely.Polygon((reference[i]-origin)@basis.T) for i in reference_ids])
        if a.symmetric_difference(b).area>1e-6:raise AssertionError('Python edge sweep differs from the PRT wall surface')
        matched.update(reference_ids)
    if matched!=set(wall_ids):raise AssertionError('PRT wall surfaces were omitted from the comparison')
    return dict(planes=len(seen),matched_prt_wall_triangles=len(matched))


def reference_controls():
    rows=[]
    for label,site,identifier in CASES:
        sample=next(row for row in read_site(site)[0] if row['id']==identifier);data=attributes(sample['properties'])
        generator,attrs,record=generate(sample,'extrusion_rule.rpk')
        model=generator.generate_model([attrs],str(EXAMPLES/'data/extrusion_rule.rpk'),'com.esri.pyprt.PyEncoder',{'emitGeometry':True,'triangulate':True})[0]
        vertices=np.asarray(model.get_vertices()).reshape(-1,3);indices=np.asarray(model.get_indices()).reshape(-1,3)
        source=Shape.from_polygon(sample['polygon'],data['minHeight'],tags=(identifier,))
        for kind in ('e','fe'):
            wire=source.comp(kind,Branch('all','='))[0];product=wire.extrude(data['bodyHeight'],'world.up');mesh=product.emit()
            near([mesh['vertices'].min(axis=0),mesh['vertices'].max(axis=0)],record['bounds_prt_m'])
            rows.append(dict(label=label,kind=kind,source_id=identifier,source_sha256=sample['source_sha256'],prt_version=record['prt_version'],
                             **vertical_surface_comparison(mesh,vertices,indices)))
    return rows


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/modelling-edge-extrusion-validation.json'))
    args=parser.parse_args();record=revision()|dict(status='pass',single_edges=single_edge_controls(),connected=connected_controls(),prt_surfaces=reference_controls(),
        prt_scope='Actual vertical wall surfaces of standard extrusion RPK on three real footprints; no isolated CGA edge-normal/vertex-normal rule',
        pending=['mixed primitive extrusion','zero/cancelled normal SDK fallback equivalence','UV channels beyond the first','isolated PRT edge-normal and pivot rule'])
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(record,indent=2)+'\n')
    print(json.dumps(dict(status='pass',single_edge_cases=len(record['single_edges']),prt_surface_cases=len(record['prt_surfaces']))))


if __name__=='__main__':main()
