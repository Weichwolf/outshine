"""Check line/point topology, component frames, direction inheritance and real OSM bounds."""

import argparse
from dataclasses import replace
import json
from pathlib import Path

import numpy as np
import shapely

from esri_oracle import CASES,generate
from evidence import revision
from outshine_ref.buildings.modelling.components import Branch,Surface
from outshine_ref.buildings.modelling.scope import Frame
from outshine_ref.buildings.modelling.selectors import tagged
from outshine_ref.buildings.modelling.shape import Shape
from outshine_ref.buildings.modelling.wire import Edge,Point
from outshine_ref.buildings.modelling.wire_components import MINIMUM_UNIQUE_EDGE_M,WireSet
from provider_contract import attributes
from samples import read_site
from verify_modelling_extrude import near


def reject(operation):
    try:operation()
    except ValueError:return
    raise AssertionError('Invalid wire geometry was accepted')


def frame_controls():
    source=Shape.from_polygon(shapely.box(0,0,8,5),2,tags=('preserve',)).extrude(4)
    source=source.r(17,29,-31,'scopeCenter').t(2,-3,5)
    source=replace(source,scope=replace(source.scope,object_frame=Frame((31,-17,9),(24,37,-12))))
    rows=[]
    for kind,count in (('e',12),('fe',24),('v',8),('fv',24)):
        data=WireSet(source,kind)
        for alignment in ('zUp','noAlign'):
            products=source.comp(kind,alignment=alignment)
            if len(products)!=count:raise AssertionError('Wire component count is incorrect')
            for index,product in enumerate(products):
                record=data.records[index];mesh=product.emit();vertices=data.vertices[list(record.element.vertices)]
                key='edges' if kind in ('e','fe') else 'points'
                near(mesh['vertices'][mesh[key][0]],vertices)
                info=product.comp_info
                if (info.index,info.total,info.source_indices,info.kind)!=(index,count,(index,),kind):
                    raise AssertionError('Wire comp attributes or index sequence is incorrect')
                primitive=product.geometry.edges or product.geometry.points
                if primitive[0].tags!=record.element.tags or primitive[0].uvs!=record.element.uvs or primitive[0].material!=record.element.material:
                    raise AssertionError('Wire component lost tags, material or corner UVs')
                if alignment=='zUp':
                    near(product.scope.pivot_origin,vertices[0]);near(product.scope.axes[:,0],record.tangent)
                    near(product.scope.axes[:,2],record.normal)
                    near(product.scope.size,[np.linalg.norm(vertices[1]-vertices[0]) if kind in ('e','fe') else 0.,0.,0.])
                else:
                    if product.scope.pivot!=source.scope.pivot or product.scope.orientation!=source.scope.orientation:
                        raise AssertionError('Wire noAlign changed parent pivot/orientation')
                if len(mesh['faces']):raise AssertionError('Line/point extraction fabricated triangles')
                for array in (product.geometry.unit_vertices,*product.geometry.fields.values()):
                    if array.flags.writeable:raise AssertionError('Wire payload is mutable')
            rows.append(dict(kind=kind,alignment=alignment,components=count))
    return rows


def topology_controls():
    cube=Shape.from_polygon(shapely.box(0,0,8,5)).extrude(4);surface=Surface(cube)
    expected_counts={'e':4,'fe':8,'v':4,'fv':12}
    for kind,count in expected_counts.items():
        if len(cube.comp(kind,Branch(tagged('extrude.top'))))!=count:raise AssertionError('Inherited face tags do not identify the expected wire components')
    edges=WireSet(cube,'e')
    for record in edges.records:
        key=tuple(sorted(record.element.vertices));wanted=surface.normals[list(surface.edge_owners[key])].sum(axis=0);wanted/=np.linalg.norm(wanted)
        near(record.normal,wanted)
    for kind,count in (('e',1),('fe',1),('v',8),('fv',8)):
        if len(cube.comp(kind,Branch('all','=:')))!=count:raise AssertionError('Wire connectivity used faces instead of shared vertices')
    courtyard=shapely.Polygon([(0,0),(12,0),(12,10),(0,10)],[[(3,2),(9,2),(9,8),(3,8)]])
    sheet=Shape.from_polygon(courtyard)
    if len(sheet.comp('e',Branch('all','=:')))!=2:raise AssertionError('Outer and courtyard boundary wires were connected')
    if len(sheet.comp('fe'))!=8 or len(sheet.comp('fv'))!=8:raise AssertionError('Hole boundary components disappeared')
    wire=cube.comp('e',Branch('all','='))[0]
    if len(wire.comp('e'))!=12 or len(wire.comp('v'))!=8:raise AssertionError('A grouped wire lost its native topology on re-extraction')
    point=wire.comp('v')[0]
    if len(point.comp('v'))!=1 or point.comp('e'):raise AssertionError('Point re-extraction fabricated edges')
    short=Shape.from_polygon(shapely.box(0,0,.0004,2.))
    if len(short.comp('e'))!=2 or len(short.comp('fe'))!=4:raise AssertionError('Unique edge extraction differs from the SDK short-edge threshold')
    return dict(cube_counts=dict(e=12,fe=24,v=8,fv=24),top_tag_counts=expected_counts,
                courtyard_loops=2,minimum_unique_edge_m=MINIMUM_UNIQUE_EDGE_M,wire_reextraction=True)


def normal_and_ownership_controls():
    vertices=np.array([[0.,0.,0.],[4.,0.,0.],[20.,30.,40.]])
    edge=Shape.from_edges(vertices,[Edge((0,1),material=7,tags=('own',),uvs=((.2,.3),(.8,.9)))],[[0.,1.,0.]])
    vertices[:]=999.
    near(edge.emit()['vertices'],[[0.,0.,0.],[4.,0.,0.]])
    changed=edge.s(8.,0.,0.).r(0.,0.,90.).emit();near(changed['vertices'],[[0.,0.,0.],[0.,8.,0.]])
    near(changed['edges_normals'],[[-1.,0.,0.]])
    points=Shape.from_points([[2.,3.,4.],[200.,300.,400.]],[Point((0,),tags=('point',))],[[0.,1.,0.]])
    near(points.scope.size,[0.,0.,0.]);near(points.t(1.,2.,3.).emit()['vertices'],[[3.,5.,7.]])
    grouped=Shape.from_polygon(shapely.box(0,0,8,5)).extrude(4).comp('e',Branch('all','='))[0]
    transformed=grouped.s(9.,7.,5.).r(21.,13.,-9.).emit();vectors=np.diff(transformed['vertices'][transformed['edges']],axis=1)[:,0]
    near(np.einsum('ij,ij->i',vectors,transformed['edges_normals']),np.zeros(len(vectors)))
    reject(lambda:Edge((0,0)));reject(lambda:Point((True,)));reject(lambda:Edge((0,1),uvs=((0.,0.),)))
    reject(lambda:Shape.from_edges([[0,0,0],[0,0,0]],[Edge((0,1))],[[0,1,0]]))
    reject(lambda:Shape.from_points([[0,0,0]],[Point((0,))],[[0,0,0]]))
    return dict(owned_payload_and_compaction=True,normal_inverse_transpose=True,invalid_cases=5)


def reference_controls():
    rows=[];sites={}
    for label,site,identifier in CASES:
        if site not in sites:sites[site]=read_site(site)[0]
        sample=next(row for row in sites[site] if row['id']==identifier);data=attributes(sample['properties'])
        body=Shape.from_polygon(sample['polygon'],data['minHeight'],tags=(identifier,)).extrude(data['bodyHeight'],'world.up')
        _,_,reference=generate(sample,'extrusion_rule.rpk')
        for kind in ('e','fe','v','fv'):
            products=body.comp(kind);points=np.concatenate([product.emit()['vertices'] for product in products])
            near([points.min(axis=0),points.max(axis=0)],reference['bounds_prt_m'])
            rows.append(dict(label=label,kind=kind,components=len(products),source_id=identifier,source_sha256=sample['source_sha256'],prt_version=reference['prt_version']))
    return rows


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/modelling-wire-validation.json'))
    args=parser.parse_args();result=revision()|dict(status='pass',frames=frame_controls(),topology=topology_controls(),
        normals=normal_and_ownership_controls(),real_prt_bounds=reference_controls(),
        prt_scope='Bounds of edge/point components against a standard extrusion RPK; no isolated CGA e/fe/v/fv oracle',
        pending=['edge extrusion','roof-specific edge selectors','streetWidth maps','material/group/hole components','degenerate-normal SDK fallback equivalence'])
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(dict(status='pass',frame_groups=len(result['frames']),real_prt_bounds=len(result['real_prt_bounds']))))


if __name__=='__main__':main()
