#version 410 core
const float kPi=3.14159265359;
#include "metalRoughBrdf.glsl"
#include "medium.glsl"
#include "skyViewLookup.glsl"
#include "display.glsl"
uniform sampler2D skyView;
uniform float eyeKm;
uniform bool waterSurface;
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
  if (waterSurface) {
    vec3 ray=reflect(-view,n);
    ivec2 res=textureSize(skyView,0);
    SkyViewSample s=skyViewSample(6360.,eyeKm,SkyViewLook(ray.z,skyLightViewCos(ray,vec3(0.,0.,1.),sun)), MediumLutSize(float(res.x),float(res.y)));
    vec3 reflected=texture(skyView,vec2(s.Uv.U,s.Uv.V)).rgb;
    light=mix(surface.rgb*.28,reflected*(2.5/1.2),brdfFresnel(vec3(.0204),1.,nv));
  }
  colour=vec4(pow(filmic(max(light,vec3(0.))),vec3(1./2.2)),1.);
  gl_FragDepth=log2(1.+1./gl_FragCoord.w)/log2(240001.);
}
