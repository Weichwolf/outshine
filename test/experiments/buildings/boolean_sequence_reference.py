"""Independent original-prism expressions for measures and generated room sections."""

import numpy as np
import shapely
import manifold3d as manifold

from boolean_surface_probe import local_mesh
from outshine_ref.buildings.modelling import Frame
from outshine_ref.buildings.modelling.axes import cga_to_enu


def section(prisms,expression,height):
    if isinstance(expression,str):
        source=prisms[expression]
        return source.polygon if source.low<height<source.high else shapely.Polygon()
    operation,a,b=expression
    left,right=section(prisms,a,height),section(prisms,b,height)
    return getattr(left,dict(union='union',subtract='difference',intersect='intersection')[operation])(right)


def reference(prisms,expression):
    levels=sorted({level for prism in prisms.values() for level in (prism.low,prism.high)})
    previous=shapely.Polygon();volume=area=0.
    for low,high in zip(levels[:-1],levels[1:],strict=True):
        current=section(prisms,expression,(low+high)/2)
        if current.area==0:current=shapely.Polygon()
        volume+=current.area*(high-low);area+=current.length*(high-low)+current.symmetric_difference(previous).area
        previous=current
    return volume,area+previous.area


def generated_section(node,height):
    product=node.product.overlay.product;mesh=local_mesh(node.operand.shape,product.object_frame)
    points=cga_to_enu(mesh['vertices']);roots=product.roots
    unique,first,inverse=np.unique(roots,return_index=True,return_inverse=True)
    target=first[inverse];indices=np.arange(len(roots));selected=indices!=target
    encoded=manifold.Mesh64(np.ascontiguousarray(points),np.asarray(mesh['faces'],dtype=np.uint64),
        merge_from_vert=indices[selected].astype(np.uint64),merge_to_vert=target[selected].astype(np.uint64))
    solid=manifold.Manifold(encoded)
    if solid.status()!=manifold.Error.NoError:raise ValueError('Generated room section rejected compiled logical geometry')
    return solid_section(solid,height)


def solid_section(solid,height):
    polygons=solid.slice(height).to_polygons()
    if not len(polygons):return shapely.Polygon()
    rings=[shapely.Polygon(np.asarray(ring)) for ring in polygons]
    depths=[sum(other.covers(ring.representative_point()) and other.area>ring.area for other in rings) for ring in rings]
    current=shapely.Polygon()
    for depth in sorted(set(depths)):
        layer=shapely.union_all([ring for ring,value in zip(rings,depths,strict=True) if value==depth])
        current=current.union(layer) if depth%2==0 else current.difference(layer)
    return current
