"""Share new semantic boundary points through original logical 3D edge identities."""

import numpy as np


class OverlayNodes:
    def __init__(self,product):
        self.product=product;self.coordinates={};self.edges={};self.known={};self.frames={}
        for index,root in enumerate(product.roots):
            point=product.mesh['vertices'][index]
            if int(root) in self.coordinates and not np.array_equal(self.coordinates[int(root)],point):
                raise ValueError('Native logical root has inconsistent positions')
            self.coordinates[int(root)]=point.copy()
        self.next=max(self.coordinates,default=-1)+1
        self.precision=max(1e-12,64*np.finfo(float).eps*max(1.,float(abs(product.mesh['vertices']).max(initial=0))))

    def original(self,number):
        ids=self.product.mesh['faces'][number]
        return self.product.mesh['vertices'][ids],self.product.roots[ids]

    def projection(self,number):
        if number in self.frames:return self.frames[number]
        corners,_=self.original(number)
        normal=np.cross(corners[1]-corners[0],corners[2]-corners[0]);normal/=np.linalg.norm(normal)
        axis=int(np.argmax(abs(normal)));axes=[i for i in range(3) if i!=axis]
        return corners,normal,axis,axes

    def add(self,key,point):
        if key in self.known:
            root=self.known[key]
            if np.linalg.norm(point-self.coordinates[root])>4*self.precision:
                raise ValueError('Semantic point key exceeds coordinate uncertainty')
            return root
        root=self.next;self.next+=1;self.known[key]=root;self.coordinates[root]=point
        return root

    def point(self,number,xy):
        corners,normal,axis,axes=self.projection(number);_,roots=self.original(number)
        point=corners[0].copy();point[axes]=xy
        point[axis]-=normal[axes]@(np.asarray(xy)-corners[0,axes])/normal[axis]
        distances=np.linalg.norm(corners-point,axis=1);near=np.flatnonzero(distances<=2*self.precision)
        if len(near)>1:raise ValueError('Semantic point cannot distinguish original corners')
        if len(near):return int(roots[near[0]])
        for a,b in [(0,1),(1,2),(2,0)]:
            delta=corners[b]-corners[a];length=delta@delta;station=float((point-corners[a])@delta/length)
            if not 0<station<1 or np.linalg.norm(point-(corners[a]+station*delta))>self.precision:continue
            first,last=sorted((int(roots[a]),int(roots[b])));edge=(first,last)
            start,stop=self.coordinates[first],self.coordinates[last];direction=stop-start
            ratio=float((point-start)@direction/(direction@direction));ratio=round(ratio,14)
            exact=start+ratio*direction
            root=self.add(('edge',*edge,ratio),exact)
            self.edges.setdefault(edge,{})[root]=ratio
            return root
        key=('inside',number,*np.rint(np.asarray(xy)/self.precision).astype(np.int64))
        return self.add(key,point)

    def boundary_points(self,number):
        _,roots=self.original(number);result=list(map(int,roots))
        for a,b in [(0,1),(1,2),(2,0)]:result.extend(self.edges.get(tuple(sorted((int(roots[a]),int(roots[b])))),{}))
        return tuple(dict.fromkeys(result))
