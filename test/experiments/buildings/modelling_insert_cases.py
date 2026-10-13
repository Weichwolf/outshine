"""Off-origin attributed solids and reusable frame modules for insertion controls."""

from dataclasses import replace

import numpy as np
import shapely

from outshine_ref.buildings.modelling.faces import Face
from outshine_ref.buildings.modelling.metadata import Material
from outshine_ref.buildings.modelling.shape import Shape


def block():
    shape=Shape.from_polygon(shapely.box(-2.,-9.,2.,-1.),3.,material=7,tags=('authored.module',)).extrude(2.,'world.up')
    faces=tuple(replace(face,group=8) for face in shape.geometry.polygons)
    mesh=shape.emit();normals=np.tile([2.,3.,5.],(len(mesh['vertices']),1))
    return Shape.from_faces(mesh['vertices'],faces,shape.scope,vertex_normals=normals).with_metadata(
        {8:'module'},{7:Material('module',{'color':(.3,.5,.6)})})


def frame():
    polygon=shapely.Polygon([(0.,0.),(1.,0.),(1.,1.4),(0.,1.4)],
                            [[(.075,.075),(.925,.075),(.925,1.325),(.075,1.325)]])
    points=[];holes=[]
    for ring in (polygon.exterior,*polygon.interiors):
        indices=[]
        for x,y in list(ring.coords)[:-1]:indices.append(len(points));points.append((x,y,0.))
        holes.append(tuple(indices))
    return Shape.from_faces(points,[Face(holes[0],tuple(holes[1:]),material=7,tags=('module.frame',))]).extrude(.12)
