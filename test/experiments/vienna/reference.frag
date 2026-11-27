#version 410 core
const float kPi=3.14159265359;
#include "metalRoughBrdf.glsl"
#include "medium.glsl"
#include "skyViewLookup.glsl"
#include "display.glsl"
uniform sampler2D skyView;
uniform float eyeKm;
uniform bool waterSurface;
uniform bool shadowPass;
uniform mat4 shadowMatrix;
uniform sampler2DShadow shadowMap;
in vec3 point;
out vec4 colour;
uniform vec3 eye;
uniform vec3 sun;
uniform vec4 surface;
void main(){
  if (shadowPass) { gl_FragDepth=gl_FragCoord.z; return; }
  vec3 view=normalize(eye-point);
  vec3 n=normalize(cross(dFdx(point),dFdy(point)));
  if(dot(n,view)<0.) n=-n;
  vec3 h=normalize(view+sun);
  float nl=max(0.,dot(n,sun)),nv=max(.001,dot(n,view));
  Brdf b=metalRoughBrdf(surface.rgb,vec3(.04),1.,pow(surface.a,4.),nl,nv,max(0.,dot(n,h)),max(0.,dot(view,h)));
  vec3 uvz=(shadowMatrix*vec4(point,1.)).xyz*.5+.5;
  float visibility=1.;
  if(all(greaterThan(uvz,vec3(0.))) && all(lessThan(uvz,vec3(1.)))){
    float texel=4800./float(textureSize(shadowMap, 0).x);
    float slope=sqrt(max(0.,1.-nl*nl))/max(nl,.10);
    float bias=(.30+1.45*texel*slope)/18000.;
    visibility=texture(shadowMap,vec3(uvz.xy,uvz.z-bias));
  }
  vec3 light=(b.diffuse+b.specular)*nl*2.5*visibility+surface.rgb*mix(vec3(.065,.085,.115),vec3(.31,.40,.53),clamp(.5+.5*n.z,0.,1.));
  if (waterSurface) {
    vec3 ray=reflect(-view,n);
    ivec2 res=textureSize(skyView,0);
    SkyViewSample s=skyViewSample(6360.,eyeKm,SkyViewLook(ray.z,skyLightViewCos(ray,vec3(0.,0.,1.),sun)), MediumLutSize(float(res.x),float(res.y)));
    vec3 reflected=texture(skyView,vec2(s.Uv.U,s.Uv.V)).rgb;
    light=mix(surface.rgb*.28,reflected*(2.5/1.2),brdfFresnel(vec3(.0204),1.,nv))*mix(.58,1.,visibility);
  }
  colour=vec4(pow(filmic(max(light,vec3(0.))),vec3(1./2.2)),1.);
  gl_FragDepth=log2(1.+1./gl_FragCoord.w)/log2(240001.);
}
