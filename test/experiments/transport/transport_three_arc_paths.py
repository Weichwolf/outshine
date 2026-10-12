"""Generate tangent three-circle return-turn seeds; body and G2 checks remain separate."""

import numpy as np
import shapely


def three_arc_seeds(entry,exit,radius):
 left=lambda h:np.array([-h[1],h[0]])
 for sign in (-1,1):
  a=entry['position']+sign*radius*left(entry['heading'])
  c=exit['position']+sign*radius*left(exit['heading'])
  delta=c-a;distance=np.linalg.norm(delta)
  if distance<=1e-8 or distance>4*radius:continue
  normal=left(delta/distance);offset=np.sqrt((2*radius)**2-(distance*.5)**2)
  for side in (-1,1):
   b=(a+c)*.5+side*offset*normal
   points=[entry['position'],(a+b)*.5,(b+c)*.5,exit['position']]
   parts=[];tangents=[]
   for center,start,end,turn in zip((a,b,c),points[:-1],points[1:],(sign,-sign,sign)):
    v=start-center;w=end-center
    angle=np.arctan2(v[1],v[0]);sweep=turn*((turn*(np.arctan2(w[1],w[0])-angle))%(2*np.pi))
    stations=angle+np.linspace(0,sweep,max(3,int(np.ceil(abs(sweep)*radius/.25))+1))
    parts.append(center+radius*np.c_[np.cos(stations),np.sin(stations)])
    tangents.append((turn*left(v/radius),turn*left(w/radius)))
   assert np.allclose(tangents[0][0],entry['heading']) and np.allclose(tangents[-1][-1],exit['heading'])
   assert all(np.allclose(tangents[i][-1],tangents[i+1][0]) for i in range(2))
   yield (sign,side),shapely.LineString(np.concatenate([parts[0],parts[1][1:],parts[2][1:]]))

