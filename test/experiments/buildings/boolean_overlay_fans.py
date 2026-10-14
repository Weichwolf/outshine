"""Insert registered semantic edge points with a bounded fan for each CDT triangle."""

import numpy as np
import shapely


def conform(roots,owners,nodes,number,templates,max_triangles):
    _,normal,_,axes=nodes.projection(number)
    available=np.unique(np.r_[roots.ravel(),nodes.boundary_points(number)])
    xy=np.asarray([nodes.coordinates[int(r)][axes] for r in available])
    output=[];supports=[]
    for index,triangle in enumerate(roots):
        corners=np.asarray([nodes.coordinates[int(r)] for r in triangle]);projected=corners[:,axes]
        if shapely.Polygon(projected).area==0.:continue
        boundary=[];split=False
        for a,b in [(0,1),(1,2),(2,0)]:
            start,stop=projected[a],projected[b];direction=stop-start;length=direction@direction
            delta=xy-start;station=(delta@direction)/length
            distance=np.linalg.norm(delta-station[:,None]*direction,axis=1)
            selected=(station>0)&(station<1)&(distance<=2*nodes.precision)&~np.isin(available,triangle)
            values=available[selected][np.argsort(station[selected])]
            boundary.extend([int(triangle[a]),*map(int,values)]);split|=bool(len(values))
        if not split:
            output.append(tuple(map(int,triangle)));supports.append(templates[int(owners[index])]);continue
        center=nodes.add(('fan',number,*sorted(map(int,triangle))),corners.mean(axis=0))
        for first,last in zip(boundary,boundary[1:]+boundary[:1]):
            points=np.asarray([nodes.coordinates[first],nodes.coordinates[last],nodes.coordinates[center]])
            area=np.cross(points[1]-points[0],points[2]-points[0])@normal
            if area==0.:continue
            face=(first,last,center) if area>0 else (last,first,center)
            output.append(face);supports.append(templates[int(owners[index])])
        if len(output)>max_triangles:raise ValueError('Semantic fan capacity exceeded')
    return np.asarray(output,dtype=np.int64).reshape(-1,3),tuple(supports)
