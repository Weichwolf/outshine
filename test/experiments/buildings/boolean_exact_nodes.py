"""Exact edge identities and incidence before one Float64 output conversion."""

import numpy as np

from boolean_exact_regions import point,orientation
from boolean_overlay_nodes import OverlayNodes


class ExactNodes(OverlayNodes):
    def __init__(self,product):
        super().__init__(product)
        self.exact={root:point(p) for root,p in self.coordinates.items()}

    def register(self,key,xyz):
        if key in self.known:
            root=self.known[key]
            if self.exact[root]!=xyz:raise ValueError('Exact root key changed position')
            return root
        root=self.next;self.next+=1;self.known[key]=root;self.exact[root]=xyz
        self.coordinates[root]=np.asarray(xyz,dtype=float)
        return root

    def projected(self,number,root):
        axes=self.projection(number)[3]
        return tuple(self.exact[int(root)][i] for i in axes)

    def point(self,number,xy):
        corners,_,axis,axes=self.projection(number);_,roots=self.original(number)
        projected=[self.projected(number,r) for r in roots]
        for i,p in enumerate(projected):
            if xy==p:return int(roots[i])
        for a,b in [(0,1),(1,2),(2,0)]:
            start,stop=projected[a],projected[b]
            if orientation(start,stop,xy)!=0:continue
            coordinate=0 if start[0]!=stop[0] else 1
            ratio=(xy[coordinate]-start[coordinate])/(stop[coordinate]-start[coordinate])
            if not 0<ratio<1:continue
            first,last=sorted((int(roots[a]),int(roots[b])))
            if int(roots[a])!=first:ratio=1-ratio
            begin,end=self.exact[first],self.exact[last]
            xyz=tuple(v+ratio*(w-v) for v,w in zip(begin,end,strict=True))
            root=self.register(('edge',first,last,ratio),xyz)
            self.edges.setdefault((first,last),{})[root]=ratio
            return root
        a,b,c=[self.exact[int(root)] for root in roots]
        first=tuple(b[i]-a[i] for i in range(3));last=tuple(c[i]-a[i] for i in range(3))
        normal=tuple(first[(i+1)%3]*last[(i+2)%3]-first[(i+2)%3]*last[(i+1)%3] for i in range(3))
        values=list(a)
        for i,value in zip(axes,xy,strict=True):values[i]=value
        values[axis]=a[axis]-sum(normal[i]*(values[i]-a[i]) for i in axes)/normal[axis]
        return self.register(('inside',number,*xy),tuple(values))


def triangulate(number,regions,nodes,max_triangles):
    local=set(nodes.boundary_points(number))
    for polygon,_ in regions:local.update(nodes.point(number,p) for p in polygon)
    projected={root:nodes.projected(number,root) for root in local}
    triangles=[];supports=[]
    for cell,(polygon,support) in enumerate(regions):
        boundary=[]
        for i,a in enumerate(polygon):
            b=polygon[(i+1)%len(polygon)];coordinate=0 if a[0]!=b[0] else 1
            entries=[]
            for root,p in projected.items():
                if orientation(a,b,p)!=0:continue
                ratio=(p[coordinate]-a[coordinate])/(b[coordinate]-a[coordinate])
                if 0<=ratio<1:entries.append((ratio,root))
            boundary.extend(root for _,root in sorted(entries))
        center=tuple(sum(p[j] for p in polygon)/len(polygon) for j in range(2))
        middle=nodes.point(number,center)
        for i,root in enumerate(boundary):
            triangles.append((middle,root,boundary[(i+1)%len(boundary)]));supports.append(support)
            if len(triangles)>max_triangles:raise ValueError('Exact fan triangle capacity exceeded')
    values=np.asarray(triangles,dtype=np.int64).reshape(-1,3)
    xyz=np.asarray([nodes.coordinates[int(r)] for r in values.ravel()]).reshape(-1,3,3)
    normal=nodes.projection(number)[1]
    reverse=np.cross(xyz[:,1]-xyz[:,0],xyz[:,2]-xyz[:,0])@normal<0
    values[reverse]=values[reverse][:,[0,2,1]]
    return values,tuple(supports)
