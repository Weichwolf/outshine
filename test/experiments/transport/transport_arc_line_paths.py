"""Generate tangent circle-line-circle seeds; curvature continuity is fitted later."""

import numpy as np
import shapely


def arc_line_arc_seeds(entry,exit,radius):
    left=lambda h:np.array([-h[1],h[0]])
    for first in (-1,1):
        a=entry['position']+first*radius*left(entry['heading'])
        for last in (-1,1):
            b=exit['position']+last*radius*left(exit['heading'])
            delta=b-a;distance=np.linalg.norm(delta)
            offset=(last-first)*radius
            if distance<=1e-8 or abs(offset)>distance:continue
            angle=np.arctan2(delta[1],delta[0])-np.arcsin(offset/distance)
            tangent=np.array([np.cos(angle),np.sin(angle)])
            p=a-first*radius*left(tangent);q=b-last*radius*left(tangent)
            assert np.dot(q-p,tangent)>=-1e-8
            assert abs(np.dot(q-p,left(tangent)))<1e-8
            parts=[]
            for center,start,end,turn in ((a,entry['position'],p,first),(b,q,exit['position'],last)):
                v=start-center;w=end-center
                begin=np.arctan2(v[1],v[0])
                sweep=turn*((turn*(np.arctan2(w[1],w[0])-begin))%(2*np.pi))
                stations=begin+np.linspace(0,sweep,max(3,int(np.ceil(abs(sweep)*radius/.25))+1))
                parts.append(center+radius*np.c_[np.cos(stations),np.sin(stations)])
            assert np.allclose(first*left((p-a)/radius),tangent)
            assert np.allclose(last*left((q-b)/radius),tangent)
            xy=np.concatenate(parts)
            xy=xy[np.r_[True,np.linalg.norm(np.diff(xy,axis=0),axis=1)>1e-10]]
            yield (first,last),shapely.LineString(xy)
