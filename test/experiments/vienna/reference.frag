#version 410 core
const float kPi=3.14159265359;
#include "metalRoughBrdf.glsl"
in vec3 point;
out vec4 colour;
uniform vec3 eye;
uniform vec3 sun;
uniform vec4 surface;
void main(){
  vec3 view=normalize(eye-point);
  vec3 n=normalize(cross(dFdx(point),dFdy(point)));
  if(dot(n,view)<0.) n=-n;
  vec3 h=normalize(view+sun);
  float nl=max(0.,dot(n,sun)),nv=max(.001,dot(n,view));
  Brdf b=metalRoughBrdf(surface.rgb,vec3(.04),1.,pow(surface.a,4.),nl,nv,max(0.,dot(n,h)),max(0.,dot(view,h)));
  vec3 light=(b.diffuse+b.specular)*nl*2.5+surface.rgb*mix(.18,.52,clamp(.5+.5*n.z,0.,1.));
  colour=vec4(pow(max(light,vec3(0.)),vec3(1./2.2)),1.);
  gl_FragDepth=log2(1.+1./gl_FragCoord.w)/log2(240001.);
}
