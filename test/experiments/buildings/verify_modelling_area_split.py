"""Actual 3D area fractions, source appearance, almost-planar bounds and invalid axes."""

import argparse
from dataclasses import replace
import json
from pathlib import Path

import numpy as np
import shapely

from evidence import revision
from modelling_extrude_probe import fixtures
from modelling_faces import Face
from modelling_scope import Relative
from modelling_shape import Shape
from modelling_split_sizes import Part,Floating
from verify_modelling_split import near,source_attributes


def area(shape):
    mesh=shape.emit();triangles=mesh['vertices'][mesh['faces']]
    return float(np.linalg.norm(np.cross(triangles[:,1]-triangles[:,0],triangles[:,2]-triangles[:,0]),axis=1).sum()*.5)


def polygon_controls():
    rows=[]
    for label,polygon,elevation,_,source_sha in fixtures():
        base=Shape.from_polygon(polygon,elevation,material=7,tags=(label,))
        frames=[base,base.r(23.,17.,-31.).t(3.,4.,5.),base.s(Relative(-1.),Relative(1.),Relative(1.))]
        for frame,parent in enumerate(frames):
            for axis in ('x','z'):
                children=parent.split_area(axis,*[Part(Floating(1.),str(i)) for i in range(4)],adjust=False)
                areas=[area(child) for child in children];near(areas,[area(parent)/4]*4)
                corners=sum(source_attributes(parent,child) for child in children)
                for i,child in enumerate(children):
                    assert child.split_info.index==i and child.split_info.total==4 and child.split_info.area_interval is not None
                    assert all('split.cut' not in face.tags for face in child.geometry.polygons)
                near(children[0].split_info.interval[0],0.);near(children[-1].split_info.interval[1],abs(parent.scope.size[('x','y','z').index(axis)]))
                rows.append(dict(label=label,axis=axis,frame=frame,source_sha256=source_sha,areas_m2=areas,source_corners_checked=corners))
    return rows


def analytic_controls():
    parent=Shape.from_polygon(shapely.Polygon([(0,0),(4,0),(0,8)]),material=7)
    a,b=parent.split_area('x',Part(Floating(1.)),Part(Floating(1.)))
    near(a.split_info.interval[1],4-np.sqrt(8.));near([area(a),area(b)],[8.,8.])
    partial=parent.split_area('x',Part(Relative(.25),'first'),Part(Relative(.25),'second'))
    near([area(child) for child in partial],[4.,4.])
    repeated=parent.split_area('x',Part(Relative(.3)),repeat=True)
    near([area(child) for child in repeated],[4.8,4.8,4.8,1.6])
    mixed=parent.split_area('x',Part(Floating(1)),Part(4.),Part(Floating(1)))
    near([area(child) for child in mixed],[6.,4.,6.])
    return dict(right_triangle_half_boundary=4-np.sqrt(8.),partial_relative=True,repeat_relative=True,fixed_floating=True)


def attributed_controls():
    polygon=shapely.Polygon([(0,0),(12,0),(12,10),(0,10)],[[(3,2),(9,2),(9,8),(3,8)]])
    parent=Shape.from_polygon(polygon,2.,material=7,tags=('source.uv',));faces=[]
    for face in parent.geometry.polygons:
        uv=tuple(tuple((u+.17*i*i,v-.11*i*i) for i,(u,v) in enumerate(ring)) for ring in face.uvs)
        faces.append(replace(face,uvs=uv,group=8))
    vertices=parent.emit()['vertices'];normals=np.tile([.2,.9,.1],(len(vertices),1))
    parent=Shape.from_faces(vertices,faces,parent.scope,vertex_normals=normals).with_metadata({8:'source'})
    checked=0
    for axis in ('x','z'):
        for child in parent.split_area(axis,Part(Floating(1)),Part(Floating(2))):checked+=source_attributes(parent,child)
    return checked


def curved_controls():
    vertices=np.array([[0.,0.,0.],[4.,0.,0.],[0.,0.,4.],[4.,.6,4.]])
    source=Shape.from_faces(vertices,[Face((0,1,2)),Face((1,3,2))])
    children=source.split_area('x',*[Part(Floating(1))]*4)
    near([area(child) for child in children],[area(source)/4]*4)
    for child in children:source_attributes(source,child)
    return dict(nonflattened_triangle_areas=True,total_area_m2=area(source))


def invalid_controls():
    planar=Shape.from_polygon(shapely.box(0,0,4,4));closed=planar.extrude(4.)
    steep=Shape.from_faces([[0.,0.,0.],[4.,0.,0.],[0.,0.,4.],[4.,10.,4.]],[Face((0,1,2)),Face((1,3,2))])
    assert planar.split_area('x')==()
    calls=[lambda:planar.split_area('y',Part(Floating(1))),lambda:closed.split_area('x',Part(Floating(1))),
        lambda:planar.split_area('u',Part(Floating(1))),lambda:steep.split_area('x',Part(Floating(1))),
        lambda:planar.split_area('x',Part(.0001),repeat=True,max_parts=2),lambda:planar.split_area('x',Part(1.),adjust='noAdjust'),
        lambda:planar.split_area('x',repeat='yes'),lambda:planar.split_area('x',max_parts=0)]
    for call in calls:
        try:call()
        except (ValueError,NotImplementedError):pass
        else:raise AssertionError('Invalid area subdivision accepted')
    return len(calls)


def face_weight_controls():
    angle=np.deg2rad(50.);x=10.+.1*np.cos(angle);y=.1*np.sin(angle)
    source=Shape.from_faces([[0,0,0],[10,0,0],[10,0,10],[0,0,10],[x,y,0],[x,y,10]],
                            [Face((0,3,2,1)),Face((1,2,5,4))])
    children=source.split_area('z',*[Part(Floating(1))]*4)
    near([area(child) for child in children],[25.25]*4)
    for child in children:source_attributes(source,child)
    return dict(unequal_face_areas_m2=[100.,1.],bend_degrees=50.,accepted=True)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/area-split-verification.json'))
    args=parser.parse_args();record=revision()|dict(status='pass',polygons=polygon_controls(),analytic=analytic_controls(),
        attributed_corners=attributed_controls(),almost_planar=curved_controls(),logical_face_mean=face_weight_controls(),invalid=invalid_controls(),
        limits='Exact own triangle-area fractions; isolated SDK/PRT area rules and exact Esri plane averaging/UV reconstruction remain pending')
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(record,indent=2)+'\n')
    print(json.dumps(dict(status='pass',polygon_axis_frame_cases=len(record['polygons']),attributed_corners=record['attributed_corners'])))


if __name__=='__main__':main()
