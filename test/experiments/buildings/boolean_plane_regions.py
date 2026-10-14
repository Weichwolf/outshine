"""One shared planar network avoids independently rounded adjacent semantic regions."""

import numpy as np
import shapely

from outshine_ref.buildings.modelling.faces import plane
from boolean_overlay_regions import OverlayRegion


def partitions(product,nodes,max_cells):
    planes=[]
    for number,triangle in enumerate(product.mesh['vertices'][product.mesh['faces']]):
        owner=int(product.operands[number]);source=product.sources[owner]
        part=int(source.mesh['parts'][product.source_triangles[number]])
        normal=plane(source.mesh['vertices'],source.shape.geometry.polygons[part])[0]
        if normal[np.argmax(abs(normal))]<0:normal=-normal
        match=next((g for g in planes if abs(g['normal']@normal-1)<1e-12
                    and np.max(abs((triangle-g['origin'])@g['normal']))<=8*nodes.precision),None)
        if match is None:
            match=dict(normal=normal,origin=triangle[0],indices=[]);planes.append(match)
        match['indices'].append(number)
    result=[[] for _ in product.mesh['faces']]
    for group in planes:
        normal=group['normal'];axis=int(np.argmax(abs(normal)));axes=[i for i in range(3) if i!=axis]
        numbers=np.asarray(group['indices']);native=product.mesh['vertices'][product.mesh['faces'][numbers]]
        for index,corners in zip(numbers,native,strict=True):
            sign=1. if np.cross(corners[1]-corners[0],corners[2]-corners[0])@normal>0 else -1.
            nodes.frames[int(index)]=(corners,normal*sign,axis,axes)
        shapes=shapely.set_precision(shapely.polygons(native[:,:,axes]),nodes.precision)
        if np.any(shapely.is_empty(shapes)):raise ValueError('Native plane triangle cannot resolve precision')
        source_polygons=[];source_ids=[]
        for owner,source in enumerate(product.sources):
            triangles=source.mesh['vertices'][source.mesh['faces']]
            selected=np.all(abs((triangles-group['origin'])@normal)<=8*nodes.precision,axis=1)
            for index in np.flatnonzero(selected):
                polygon=shapely.set_precision(shapely.Polygon(triangles[index][:,axes]),nodes.precision)
                if polygon.is_empty:continue
                source_polygons.append(polygon);source_ids.append((owner,int(index)))
        source_polygons=np.asarray(source_polygons,dtype=object)
        network=shapely.union_all(np.r_[shapely.boundary(shapes),shapely.boundary(source_polygons)],grid_size=nodes.precision)
        cells=shapely.get_parts(shapely.polygonize(shapely.get_parts(network)))
        domain=shapely.union_all(shapes,grid_size=nodes.precision)
        cells=cells[shapely.covers(domain,shapely.point_on_surface(cells))]
        if len(cells)>max_cells:raise ValueError('Shared semantic plane cell capacity exceeded')
        points=shapely.point_on_surface(cells)
        native_pairs=shapely.STRtree(shapes).query(points,predicate='within')
        counts=np.bincount(native_pairs[0],minlength=len(cells))
        if np.any(counts!=1):raise ValueError('Shared plane cell has ambiguous native membership')
        selected=np.empty(len(cells),dtype=np.int32);selected[native_pairs[0]]=native_pairs[1]
        pairs=shapely.STRtree(source_polygons).query(points,predicate='within')
        supports=[[] for _ in cells]
        for cell,source in pairs.T:supports[int(cell)].append(source_ids[int(source)])
        for number,cell in enumerate(cells):
            support=tuple(sorted(supports[number]))
            if not support or len(support)>2 or len({o for o,_ in support})!=len(support):
                raise ValueError('Shared semantic cell has missing/overlapping source support')
            native_id=int(numbers[selected[number]])
            result[native_id].append(OverlayRegion(cell,support))
    if any(not group for group in result):raise ValueError('Shared semantic planes lost a native triangle')
    return tuple(tuple(parts) for parts in result)
