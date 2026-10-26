float Infinite() { return uintBitsToFloat(0x7f800000u); }
bool Interval(vec3 origin,vec3 inverse,vec3 low,vec3 high,
              out float entry,out float leave,out uint entering,out uint exiting) {
  vec3 a=(low-origin)*inverse,b=(high-origin)*inverse;
  vec3 near=min(a,b),far=max(a,b);
  bvec3 parallel=isinf(inverse);
  if(any(parallel)) {
    if(((parallel.x && (origin.x<low.x || origin.x>high.x)) || (parallel.y && (origin.y<low.y || origin.y>high.y)) || (parallel.z && (origin.z<low.z || origin.z>high.z)))) return false;
    near=mix(near,vec3(-Infinite()),parallel);far=mix(far,vec3(Infinite()),parallel);
  }
  entry=max(near.x,max(near.y,near.z));
  leave=min(far.x,min(far.y,far.z));
  uint first=near.x==entry?0u:(near.y==entry?1u:2u);
  uint last=far.x==leave?0u:(far.y==leave?1u:2u);
  entering=2u*first+(inverse[first]>0.0?1u:2u);
  exiting=2u*last+(inverse[last]>0.0?2u:1u);
  return entry<=leave && leave>metric.w;
}
bool NodeHit(uint level,uint x,uint y,vec3 origin,vec3 inverse,
             out float entry,out float leave,out uint entering,out uint exiting,inout uint evaluations) {
#ifdef BOX_QUERY_WORK
  ++evaluations;
#endif
  uvec4 info=levels[level];
  if(x>=info.z || y>=info.w) return false;
  uint address=info.x+y*info.y+x;
  uint code=(data[address/4u]>>((address%4u)*8u))&255u;
  float height=uintBitsToFloat(data[code]);
  if(height==0.0) return false;
  uint scale=1u<<level;
  float inset=(1.0-metric.y)*metric.x*0.5;
  vec3 low=vec3(vec2(x,y)*float(scale)*metric.x+inset,0.0);
  vec3 high=vec3(vec2(min((x+1u)*scale,shape.x),min((y+1u)*scale,shape.y))*metric.x-inset,height);
  return Interval(origin,inverse,low,high,entry,leave,entering,exiting);
}
uint Packed(uint level,uint x,uint y) { return (level<<28u)|(y<<14u)|x; }
