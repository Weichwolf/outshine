"""Inspect actual offset triangles, topology, signed frames and affine corner channels."""

import argparse
import json
from pathlib import Path

import numpy as np
import shapely

from evidence import revision
from modelling_components import Branch
from modelling_faces import Face
from modelling_metadata import Material
from modelling_shape import Shape


def areas(shape):
    mesh=shape.emit();xyz=mesh['vertices'][mesh['faces']]
    return np.cross(xyz[:,1]-xyz[:,0],xyz[:,2]-xyz[:,0])/2


def source():
    face=Face((0,1,2,3),material=9,tags=frozenset({'source.wall'}),
        uvs=(((0.,0.),(4.,0.),(4.,3.),(0.,3.)),),group=3)
    return Shape.from_faces([[0,0,0],[8,0,0],[8,0,6],[0,0,6]],[face]).with_metadata(
        group_names={3:'wall'},material_defs={9:Material('plaster',{'roughness':.8})}).copy_uv(0,2).scale_uv(2,2.,3.)


def controls():
    original=source();rows=[]
    for name,shape in [('plane',original),('rotated',original.r(31,17,23).t(10,-4,3)),
            ('scaled',original.s(12,0,9)),('mirrored',original.s(-8,0,6).r(17,31,0))]:
        baseline=shape.emit()
        for distance in (-1.,0.,1.):
            result=shape.offset(distance);product=result.emit();vectors=areas(result)
            np.testing.assert_allclose(result.scope.axes,shape.scope.axes,atol=1e-10)
            np.testing.assert_allclose(vectors.sum(axis=0),areas(shape).sum(axis=0),atol=1e-8,rtol=1e-10)
            local=(product['vertices']-shape.scope.origin)@shape.scope.axes
            expected=np.c_[local[:,0]/shape.scope.size[0]*4,local[:,2]/shape.scope.size[2]*3]
            # Compiled native corners carry the per-face channel; unused logical vertices have no stream value.
            used=np.unique(product['faces'])
            np.testing.assert_allclose(product['uv'][used],expected[used],atol=1e-8)
            np.testing.assert_allclose(product['uv2'][used],expected[used]*[2,3],atol=1e-8)
            assert result.geometry.group_names==shape.geometry.group_names and result.geometry.material_defs==shape.geometry.material_defs
            assert all(face.tags=={'source.wall'} and face.group==3 and face.material==9 for face in result.geometry.polygons)
            assert result.offset_info.occluder is None
            selected=shape.offset(distance,'inside');assert selected.offset_info.occluder is shape
            expected_area=(abs(shape.scope.size[0])+2*distance)*(abs(shape.scope.size[2])+2*distance)
            assert abs(np.linalg.norm(areas(selected),axis=1).sum()-expected_area)<1e-8
            if distance:
                children=result.comp('f',Branch('inside'),alignment='noAlign')
                assert len(children)==1 and abs(np.linalg.norm(areas(children[0]),axis=1).sum()-expected_area)<1e-8
            np.testing.assert_array_equal(shape.emit()['vertices'],baseline['vertices'])
            rows.append(dict(case=name,distance_m=distance,faces=len(result.geometry.polygons),triangles=len(product['faces'])))
    collapsed=original.offset(-10.,'inside');assert len(collapsed.emit()['faces'])==0 and collapsed.scope.size==(0.,0.,0.)
    untextured=original.delete_uv(0).delete_uv(2).offset(.5);assert 'uv' not in untextured.emit() and 'uv2' not in untextured.emit()
    courtyard=Shape.from_polygon(shapely.Polygon([(0,0),(20,0),(20,20),(0,20)],holes=[[(8,8),(8,12),(12,12),(12,8)]]))
    assert abs(np.linalg.norm(areas(courtyard.offset(-1.,'inside')),axis=1).sum()-288.)<1e-8
    cube=Shape.from_polygon(shapely.box(0,0,8,6)).extrude(3.).offset(-.2)
    assert len(set(cube.offset_info.source_faces))==6
    bad=Shape.from_faces([[0,0,0],[8,0,0],[8,0,6],[0,0,6]],
        [Face((0,1,2,3),uvs=(((0,0),(4,0),(5,3),(0,3)),))])
    try:bad.offset(1.)
    except NotImplementedError:pass
    else:raise AssertionError('Nonaffine UV extrapolation silently changed the input mapping')
    stream=original.emit()
    authored=Shape.from_mesh(stream|dict(normals=np.tile([0.,-1.,0.],(len(stream['vertices']),1))),polygons=original.geometry.polygons)
    for unsupported in (authored,Shape.from_mesh(stream)):
        try:unsupported.offset(.5)
        except NotImplementedError:pass
        else:raise AssertionError('Unsupported normal or topology contract was silently discarded')
    return rows


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/modelling-offset-verification.json'))
    args=parser.parse_args();record=revision()|dict(status='pass',cases=controls(),
        limits='Logical planar faces and affine UV channels; authored normals, nonaffine UVs, border edge subdivision, full CGA and runtime occlusion remain pending')
    args.output.write_text(json.dumps(record,indent=2)+'\n');print(json.dumps(dict(status='pass',cases=len(record['cases']))))
