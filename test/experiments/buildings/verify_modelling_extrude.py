"""Check polygon topology, extrusion modes, data inheritance and PRT reference surfaces."""

import argparse
import json
from pathlib import Path

import numpy as np
import shapely

from esri_oracle import CASES,generate
from evidence import revision
from modelling_extrude import MODES,edge_owners,euler
from modelling_faces import EXTRUSION_TAGS,Face
from outshine_ref.buildings.modelling.scope import rotation
from modelling_shape import Shape
from provider_contract import attributes
from samples import read_site


def near(actual,expected,tolerance=1e-8):
    if not np.allclose(actual,expected,rtol=0.,atol=tolerance):raise AssertionError(f'{actual} != {expected}')


def volume(mesh):
    triangles=mesh['vertices'][mesh['faces']];triangles=triangles-triangles.mean(axis=(0,1))
    return float(np.einsum('ij,ij->i',triangles[:,0],np.cross(triangles[:,1],triangles[:,2])).sum()/6)


def closed(mesh):
    rounded=np.rint(mesh['vertices']*1e8).astype(np.int64)
    _,inverse=np.unique(rounded,axis=0,return_inverse=True);triangles=inverse[mesh['faces']]
    if np.any(np.diff(np.sort(triangles,axis=1),axis=1)==0):raise AssertionError('Extrusion contains a degenerate triangle')
    edges=np.concatenate([triangles[:,[0,1]],triangles[:,[1,2]],triangles[:,[2,0]]])
    _,counts=np.unique(np.sort(edges,axis=1),axis=0,return_counts=True)
    if np.any(counts!=2):raise AssertionError('Extrusion is not a closed two-manifold surface')
    forward=(edges[:,0]<edges[:,1]).astype(np.int8)*2-1
    _,labels=np.unique(np.sort(edges,axis=1),axis=0,return_inverse=True)
    sums=np.bincount(labels,weights=forward)
    if np.any(sums!=0):raise AssertionError('Neighbouring triangle winding is inconsistent')
    return dict(welded_vertices=int(inverse.max())+1,triangles=len(triangles),closed_edges=len(counts))


def polygon_controls():
    polygons=[shapely.box(0.,0.,8.,5.),shapely.Polygon([(0.,0.),(9.,0.),(9.,3.),(3.,3.),(3.,8.),(0.,8.)]),
              shapely.Polygon([(0.,0.),(12.,0.),(12.,10.),(0.,10.)],[[(3.,2.),(9.,2.),(9.,8.),(3.,8.)]])]
    rows=[]
    for polygon in polygons:
        source=Shape.from_polygon(polygon,2.,material=4,tags=('source-test',))
        for mode in MODES:
            for distance in (-3.,3.):
                product=source.extrude(distance,mode);mesh=product.emit();check=closed(mesh)
                near(volume(mesh),polygon.area*abs(distance));near(product.scope.size[1],distance)
                near([mesh['vertices'][:,1].min(),mesh['vertices'][:,1].max()],sorted([2.,2.+distance]))
                if not all(face.material==4 and 'source-test' in face.tags for face in product.geometry.polygons):
                    raise AssertionError('Extrusion lost source material or face tags')
                masks=set(mesh['face_tag_mask'].tolist())
                expected={1,2,8}|({4} if polygon.interiors else set())
                if masks!=expected:raise AssertionError('Extrusion component tags do not match the boundary roles')
                cap_count=sum('extrude.top' in face.tags for face in product.geometry.polygons)
                if cap_count!=1:raise AssertionError('A polygon cap was replaced with logical triangle fragments')
                if not all(len(face.holes)==len(polygon.interiors) for face in product.geometry.polygons if 'extrude.top' in face.tags):
                    raise AssertionError('Logical cap lost its courtyard boundary')
                rows.append(dict(area_m2=polygon.area,holes=len(polygon.interiors),mode=mode,distance_m=distance,
                                 volume_m3=volume(mesh),logical_faces=len(product.geometry.polygons),**check))
    source=Shape.from_polygon(polygons[0],2.)
    for mode in MODES:
        result=source.extrude(0.,mode).emit()
        near(result['vertices'][:,1],2.);near(volume(result),0.)
        if set(result['face_tag_mask'])!={9}:raise AssertionError('Zero extrusion fabricated side walls')
    return rows


def roof_controls():
    vertices=np.array([[0.,0.,0.],[4.,0.,0.],[4.,2.,4.],[0.,2.,4.],[4.,0.,8.],[0.,0.,8.]])
    faces=[Face((3,2,1,0),material=1,tags=('left',)),Face((5,4,2,3),material=4,tags=('right',))]
    source=Shape.from_faces(vertices,faces)
    fake=Shape.from_faces(vertices,faces,vertex_normals=np.tile([1.,0.,0.],(len(vertices),1)))
    near(source.extrude(3.,'vertex.normal').emit()['vertices'],fake.extrude(3.,'vertex.normal').emit()['vertices'])
    rows=[]
    for mode in MODES:
        for distance in (-3.,3.):
            result=source.extrude(distance,mode);mesh=result.emit()
            if mode=='face.normal':
                for material in (1,4):closed(mesh|dict(faces=mesh['faces'][mesh['materials']==material]))
                expected=2*4*np.hypot(2.,4.)*abs(distance)
            else:
                closed(mesh);expected=32*abs(distance) if mode=='world.up' else 64. if mode=='world.up.flatTop' else None
            if expected is not None:near(volume(mesh),expected)
            if mode=='world.up.flatTop':
                top=mesh['faces'][mesh['face_tag_mask']==EXTRUSION_TAGS['extrude.top']]
                near(mesh['vertices'][top][:,:,1],3. if distance>0 else -1.)
            rows.append(dict(mode=mode,distance_m=distance,volume_m3=volume(mesh),triangles=len(mesh['faces'])))
    for distance in (0.,1.,-1.):
        result=source.extrude(distance,'world.up.flatTop').emit()
        if distance>=0:
            for material in (1,4):closed(result|dict(faces=result['faces'][result['materials']==material]))
        else:closed(result)
        near(volume(result),32.)
        rows.append(dict(mode='world.up.flatTop',distance_m=distance,volume_m3=volume(result),triangles=len(result['faces']),
                         topology='two separately closed wedges touch at the ridge' if distance>=0 else 'closed envelope touching the eaves'))
    disconnected=Shape.from_faces(vertices[[3,2,1,0,5,4,2,3]],[Face((0,1,2,3)),Face((4,5,6,7))])
    normal=disconnected.extrude(3.,'vertex.normal').emit();individual=disconnected.extrude(3.,'face.normal').emit()
    near(normal['vertices'],individual['vertices'])
    connected=source.extrude(3.,'world.up')
    if any(len(value)!=2 for value in edge_owners(connected.geometry.polygons).values()):
        raise AssertionError('Connected extrusion lost its logical face adjacency')
    near(connected.extrude(0.,'vertex.normal').emit()['vertices'],connected.emit()['vertices'])
    return rows


def uv_and_scope_controls():
    vertices=np.array([[0.,0.,0.],[4.,0.,0.],[4.,0.,-3.],[0.,0.,-3.]])
    uv=(((.1,.2),(.8,.2),(.8,.9),(.1,.9)),)
    face=Face((0,1,2,3),material=7,tags=('custom',),uvs=uv)
    base=Shape.from_faces(vertices,[face]).r(27.,38.,19.).t(3.,-2.,5.)
    for mode in ('face.normal','world.up','world.up.flatTop','vertex.normal'):
        result=base.extrude(5.,mode)
        for item in result.geometry.polygons:
            if 'extrude.side.outer' in item.tags:
                values=item.uvs[0]
                if not (values[0]==values[3] and values[1]==values[2]):raise AssertionError('Side UV inheritance differs from A B B A')
            elif 'extrude.top' in item.tags:
                if sorted(item.uvs[0])!=sorted(uv[0]):raise AssertionError('Extrusion lost source corner UVs')
        mesh=result.emit();closed(mesh)
        local=(mesh['vertices']-result.scope.origin)@result.scope.axes
        low,high=local.min(axis=0),local.max(axis=0)
        near(low,np.minimum(0.,result.scope.size));near(high,np.maximum(0.,result.scope.size))
    for angles in ((12.,27.,-33.),(90.,90.,-11.),(-73.,-90.,48.)):
        matrix=rotation(angles);near(rotation(euler(matrix)),matrix)
    return ['corner UV inheritance on tilted faces','tight signed scope bounds and new up direction',
            'Euler basis conversion including both singular orientations']


def real_prt_controls():
    rows=[];sites={}
    for label,site,identifier in CASES:
        if site not in sites:sites[site]=read_site(site)[0]
        sample=next(row for row in sites[site] if row['id']==identifier);data=attributes(sample['properties'])
        own=Shape.from_polygon(sample['polygon'],data['minHeight'],tags=(identifier,)).extrude(data['bodyHeight'],'world.up')
        mesh=own.emit();check=closed(mesh);_,_,reference=generate(sample,'extrusion_rule.rpk')
        near(volume(mesh),reference['surface_integral_m3'],max(1e-6,reference['surface_integral_m3']*1e-8))
        near(np.array([mesh['vertices'].min(axis=0),mesh['vertices'].max(axis=0)]),reference['bounds_prt_m'])
        rows.append(dict(label=label,source_id=identifier,source_sha256=sample['source_sha256'],
                         source_holes=len(sample['polygon'].interiors),own_volume_m3=volume(mesh),
                         reference_volume_m3=reference['surface_integral_m3'],prt_version=reference['prt_version'],**check))
    return rows


def rejected_controls():
    shape=Shape.from_polygon(shapely.box(0.,0.,4.,3.));points=np.array([[0.,0.,0.],[1.,0.,0.],[1.,1.,1.],[0.,0.,1.]])
    cases=[('self-intersecting footprint',lambda:Shape.from_polygon(shapely.Polygon([(0,0),(2,2),(0,2),(2,0)]))),
           ('nonplanar input constructor',lambda:Shape.from_faces(points,[Face((0,1,2,3))])),
           ('invalid corner UV cardinality',lambda:Face((0,1,2),uvs=(((0.,0.),(1.,0.)),))),
           ('non-finite distance',lambda:shape.extrude(float('nan'))),('unknown mode',lambda:shape.extrude(3.,'unknown'))]
    for name,operation in cases:
        try:operation()
        except ValueError:continue
        raise AssertionError('Invalid input silently accepted: '+name)
    return [name for name,_ in cases]


def synthetic_prt_controls():
    polygon=shapely.box(0.,0.,8.,5.)
    sample=dict(id='analytic-extrusion-rectangle',polygon=polygon,source_sha256='analytic-no-source-tile',
                properties=dict(render_height=5.,render_min_height=2.))
    rows=[]
    for distance in (-3.,0.,3.):
        mesh=Shape.from_polygon(polygon,2.).extrude(distance).emit()
        _,_,reference=generate(sample,'extrusion_rule.rpk',dict(minBuildingHeight=distance,maxBuildingHeight=distance))
        near(volume(mesh),reference['surface_integral_m3'])
        near([mesh['vertices'].min(axis=0),mesh['vertices'].max(axis=0)],reference['bounds_prt_m'])
        if len(mesh['faces'])!=reference['triangles']:raise AssertionError('Planar/solid reference triangle count differs')
        rows.append(dict(distance_m=distance,triangles=len(mesh['faces']),volume_m3=volume(mesh),prt_version=reference['prt_version']))
    return rows


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/modelling-extrusion-validation.json'))
    args=parser.parse_args()
    record=revision()|dict(status='pass',polygon_controls=polygon_controls(),roof_controls=roof_controls(),
                           uv_scope_controls=uv_and_scope_controls(),real_prt_controls=real_prt_controls(),
                           rejected_controls=rejected_controls(),
                           synthetic_prt_controls=synthetic_prt_controls(),
                           pending=['edge extrusion and comp(e/fe)','tangential or collapsed face sweep',
                                    'UV channels beyond the first','isolated PRT flat-top/vertex-normal rule controls','mergeVertices for disconnected coincident faces'])
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(record,indent=2)+'\n')
    print(json.dumps(dict(status='pass',polygon_cases=len(record['polygon_controls']),roof_cases=len(record['roof_controls']),
                          real_prt_cases=len(record['real_prt_controls']))))


if __name__=='__main__':main()
