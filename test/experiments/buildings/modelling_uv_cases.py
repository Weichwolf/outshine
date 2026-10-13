"""Adjacent faces sharing geometry and owned texture coordinates for UV controls."""

import numpy as np
import shapely

from outshine_ref.buildings.modelling.faces import Face
from outshine_ref.buildings.modelling.metadata import Material
from outshine_ref.buildings.modelling.shape import Shape


def adjacent():
    vertices=np.array([[0.,0.,0.],[1.,0.,0.],[3.,0.,0.],[0.,2.,0.],[1.,2.,0.],[3.,2.,0.]])
    uv=np.array([[2.,3.],[4.,3.],[8.,3.],[2.,7.],[4.,7.],[8.,7.]])
    rings=((0,1,4,3),(1,2,5,4));faces=[Face(ring,material=7+i,group=i,tags=('source.face.'+str(i),),
        uvs=(tuple(tuple(pair) for pair in uv[list(ring)]),)) for i,ring in enumerate(rings)]
    mesh=dict(vertices=vertices,faces=np.array([[0,1,4],[0,4,3],[1,2,5],[1,5,4]],dtype=np.int32),uv=uv,
              parts=np.array([0,0,1,1],dtype=np.int32),materials=np.array([7,7,8,8],dtype=np.uint16),
              groups=np.array([0,0,1,1],dtype=np.uint32),normals=np.tile([0.,0.,1.],(6,1)))
    return Shape.from_mesh(mesh,polygons=faces,group_names={0:'left',1:'right'},material_defs={7:Material('left',{'roughness':.6}),8:Material('right',{'roughness':.4})})


def courtyard():
    return Shape.from_polygon(shapely.Polygon([(0.,0.),(6.,0.),(6.,4.),(0.,4.)],holes=[[(2.,1.),(2.,3.),(4.,3.),(4.,1.)]]))
