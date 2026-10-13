"""Clip native triangle properties, then close exact one-sided sections using GEOS/CDT."""

import numpy as np
import shapely

from modelling_cut_topology import logical_faces
from modelling_faces import Face,FACE_TAG_MASKS
from modelling_vector import cross3


class Slab:
    def __init__(self,source,axis,low,high,material,group):
        self.source=source;self.axis=axis;self.low=low;self.high=high
        self.direction=source.shape.scope.axes[:,axis]*(1. if source.shape.scope.size[axis]>=0 else -1.)
        self.origin=source.shape.scope.origin;self.material=material;self.group=group
        self.points={int(root):source.properties[root,:3].copy() for root in np.unique(source.roots)}
        self.positions={root:float((point-self.origin)@self.direction) for root,point in self.points.items()}
        self.next=max(self.points)+1;self.intersections={};self.tolerance=max(1e-10,abs(source.shape.scope.size[axis])*1e-12)
        self.properties=[];self.roots=[];self.keys=[];self.templates={};self.omitted=set()

    def position(self,root):return self.positions[root]

    def intersection(self,a,b,wa,wb,triangle,bound):
        support=np.flatnonzero((wa+wb)>1e-12)
        if len(support)!=2:raise ValueError('Slab boundary crossed an internal parallel clip edge')
        first,last=sorted(int(self.source.roots[triangle[index]]) for index in support);key=(first,last,bound)
        da,db=self.position(a)-bound,self.position(b)-bound;fraction=da/(da-db);weights=wa+(wb-wa)*fraction
        if abs(self.position(first)-bound)<=self.tolerance:return first,weights
        if abs(self.position(last)-bound)<=self.tolerance:return last,weights
        if key not in self.intersections:
            ca,cb=self.position(first)-bound,self.position(last)-bound;t=ca/(ca-cb)
            point=self.points[first]+(self.points[last]-self.points[first])*t
            point-=((point-self.origin)@self.direction-bound)*self.direction
            root=self.next;self.next+=1;self.points[root]=point;self.intersections[key]=root
            self.positions[root]=float((point-self.origin)@self.direction)
        return self.intersections[key],weights

    def clip(self,vertices,triangle,bound,side):
        result=[]
        for (a,wa),(b,wb) in zip(vertices,vertices[1:]+vertices[:1]):
            da=(self.position(a)-bound)*side;db=(self.position(b)-bound)*side
            ina,inb=da>=-self.tolerance,db>=-self.tolerance
            if ina:result.append((a,wa))
            if ina!=inb:result.append(self.intersection(a,b,wa,wb,triangle,bound))
        return result

    def append(self,roots,properties,key,template,index):
        points=np.asarray([self.points[root] for root in roots])
        if np.linalg.norm(cross3(points[1]-points[0],points[2]-points[0]))<=1e-12:return
        self.roots.extend(roots);self.properties.extend(np.c_[points,np.asarray(properties)])
        self.keys.append(key);self.templates[key]=(template,index)

    def surfaces(self):
        for row,triangle in enumerate(self.source.triangles):
            roots=[int(self.source.roots[i]) for i in triangle];points=np.asarray([self.points[root] for root in roots])
            normal=self.source.geometric_normals[row]
            coordinates=np.asarray([self.position(root) for root in roots]);index=int(self.source.triangle_parts[row])
            if self.source.closed and ((np.all(abs(coordinates-self.low)<=self.tolerance) and normal@self.direction>0) or (
                np.all(abs(coordinates-self.high)<=self.tolerance) and normal@self.direction<0)):
                self.omitted.add(index);continue
            polygon=list(zip(roots,np.eye(3)));polygon=self.clip(polygon,triangle,self.low,1)
            if polygon:polygon=self.clip(polygon,triangle,self.high,-1)
            for i in range(1,len(polygon)-1):
                selected=[polygon[0],polygon[i],polygon[i+1]]
                attributes=np.asarray([weights@self.source.properties[triangle,3:] for _,weights in selected])
                self.append([root for root,_ in selected],attributes,('source',index),self.source.polygons[index],index)

    def section(self,bound,side):
        if not self.source.closed:return
        normal=-self.direction*side;other=next(i for i in range(3) if i!=self.axis)
        x=self.source.shape.scope.axes[:,other];basis=np.asarray([x,cross3(normal,x)]);origin=self.origin+self.direction*bound
        segments=[];retained=[];point_map={}
        for row,triangle in enumerate(self.source.triangles):
            roots=[int(self.source.roots[i]) for i in triangle];points=np.asarray([self.points[root] for root in roots]);distances=(points-origin)@self.direction
            on=abs(distances)<=self.tolerance;face_normal=self.source.geometric_normals[row]
            if on.all():
                if face_normal@normal>0:retained.append(shapely.Polygon((points-origin)@basis.T))
                continue
            if on.sum()==2 and distances[~on][0]*side<0:continue
            intersections=[(root,np.eye(3)[i]) for i,root in enumerate(roots) if on[i]]
            for i,j in ((0,1),(1,2),(2,0)):
                if on[i] or on[j]:continue
                if distances[i]*distances[j]<0:
                    intersections.append(self.intersection(roots[i],roots[j],np.eye(3)[i],np.eye(3)[j],triangle,bound))
            if len(intersections)!=2:continue
            a,b=[self.points[root] for root,_ in intersections]
            if (b-a)@cross3(normal,face_normal)<0:a,b=b,a
            xy=(np.asarray([a,b])-origin)@basis.T;segments.append(xy)
            for root,_ in intersections:point_map[tuple((self.points[root]-origin)@basis.T)]=root
        if not segments:return
        segments=np.asarray(segments);cells=list(shapely.get_parts(shapely.polygonize(shapely.linestrings(segments))))
        kept=[]
        for cell in cells:
            point=np.asarray(cell.representative_point().coords)[0];a,b=segments[:,0],segments[:,1]
            cross=(b[:,0]-a[:,0])*(point[1]-a[:,1])-(b[:,1]-a[:,1])*(point[0]-a[:,0])
            winding=np.count_nonzero((a[:,1]<=point[1])&(b[:,1]>point[1])&(cross>0))-np.count_nonzero((a[:,1]>point[1])&(b[:,1]<=point[1])&(cross<0))
            if winding>0:kept.append(cell)
        region=shapely.union_all(kept)
        if retained:region=region.difference(shapely.union_all(retained))
        if region.is_empty:return
        material=self.material
        if material is None:
            choices={face.material for face in self.source.polygons}
            if len(choices)!=1:raise ValueError('A multi-material source needs an explicit cut material')
            material=next(iter(choices))
        template=Face((0,1,2),material=material,group=self.group,tags=frozenset({'split.cut'}))
        for triangle in shapely.get_parts(shapely.constrained_delaunay_triangles(region)):
            xy=np.asarray(triangle.exterior.coords)[:3];v=xy[1]-xy[0];w=xy[2]-xy[0]
            if v[0]*w[1]-v[1]*w[0]<0:xy=xy[::-1]
            roots=[]
            for point in xy:
                key=tuple(point)
                if key not in point_map:
                    root=self.next;self.next+=1;self.points[root]=origin+point@basis;point_map[key]=root
                roots.append(point_map[key])
            local=(np.asarray([self.points[root] for root in roots])-self.origin)@self.source.shape.scope.axes
            uv=local[:,[i for i in range(3) if i!=self.axis]]
            attrs=np.c_[uv,np.tile(normal,(3,1))] if self.source.authored_normals else uv
            self.append(roots,attrs,('cut',bound),template,None)

    def emit(self):
        if not self.keys:return None,tuple(sorted(self.omitted))
        data=np.asarray(self.properties);mesh=dict(vertices=data[:,:3],uv=data[:,3:5],faces=np.arange(len(data),dtype=np.int32).reshape(-1,3))
        faces,indices=logical_faces(mesh,np.asarray(self.roots),self.keys,self.templates)
        mesh['materials']=np.asarray([faces[i].material for i in mesh['parts']],dtype=np.uint16)
        mesh['groups']=np.asarray([faces[i].group for i in mesh['parts']],dtype=np.uint32)
        mesh['face_tag_mask']=np.asarray([sum(bit for name,bit in FACE_TAG_MASKS.items() if name in faces[i].tags) for i in mesh['parts']],dtype=np.uint8)
        if self.source.authored_normals:
            normals=data[:,5:8];length=np.linalg.norm(normals,axis=1);mesh['normals']=np.divide(normals,length[:,None],out=np.zeros_like(normals),where=length[:,None]>0)
        return (mesh,faces,indices),tuple(sorted(self.omitted))


def cut_geos(source,axis,low,high,material=None,group=0):
    slab=Slab(source,axis,low,high,material,group);slab.surfaces();slab.section(low,1);slab.section(high,-1)
    return slab.emit()
