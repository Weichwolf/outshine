#version 410 core
const float kPi=3.14159265359;
#include "metalRoughBrdf.glsl"
#include "medium.glsl"
#include "clear_air.glsl"
#include "skyViewLookup.glsl"
#include "display.glsl"
uniform sampler2D skyView;
uniform sampler2D veil;
uniform sampler2D detailMap;
uniform vec3 detailScale;
uniform bool hasDetail;
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
uniform float metalness;
vec2 detailCoordinates(vec3 normal) {
  vec3 axis=abs(normal);
  if (axis.z>=max(axis.x,axis.y)) return point.xy;
  return axis.x>axis.y ? point.yz : point.xz;
}

vec3 bumpNormal(vec3 normal, vec3 dx, vec3 dy, float height) {
  vec3 reciprocalX=cross(dy,normal),reciprocalY=cross(normal,dx);
  float det=dot(dx,reciprocalX);
  vec3 gradient=sign(det)*(dFdx(height)*reciprocalX+dFdy(height)*reciprocalY);
  return abs(det)>1.e-15 ? normalize(abs(det)*normal-gradient) : normal;
}

void main(){
  if (shadowPass) { gl_FragDepth=gl_FragCoord.z; return; }
  vec3 view=normalize(eye-point);
  vec3 dx=dFdx(point),dy=dFdy(point);
  vec3 n=normalize(cross(dx,dy));
  if(dot(n,view)<0.) n=-n;
  vec3 geometricNormal=n;
  vec4 material=surface;
  if (hasDetail) {
    vec2 uv=detailCoordinates(n);
    vec3 fine=texture(detailMap,uv/detailScale.x).rgb;
    float macro=texture(detailMap,uv/detailScale.y).a;
    material.rgb*=fine.r*(1.68+.64*macro);
    material.a=fine.g;
    n=bumpNormal(n,dx,dy,fine.b*detailScale.z);
  }
  vec3 h=normalize(view+sun);
  float nl=max(0.,dot(n,sun)),nv=max(.001,dot(n,view));
  vec3 f0=mix(vec3(.04),material.rgb,metalness);
  Brdf b=metalRoughBrdf(material.rgb*(1.-metalness),f0,1.,pow(material.a,4.),nl,nv,max(0.,dot(n,h)),max(0.,dot(view,h)));
  vec3 uvz=(shadowMatrix*vec4(point,1.)).xyz*.5+.5;
  float visibility=1.;
  if(all(greaterThan(uvz,vec3(0.))) && all(lessThan(uvz,vec3(1.)))){
    float texel=4800./float(textureSize(shadowMap, 0).x);
    float lightAngle=max(0.,dot(geometricNormal,sun));
    float slope=sqrt(max(0.,1.-lightAngle*lightAngle))/max(lightAngle,.10);
    float bias=(.30+1.45*texel*slope)/18000.;
    visibility=texture(shadowMap,vec3(uvz.xy,uvz.z-bias));
  }
  vec3 light=(b.diffuse+b.specular)*nl*2.5*visibility+material.rgb*mix(vec3(.065,.085,.115),vec3(.31,.40,.53),clamp(.5+.5*n.z,0.,1.));
  if (waterSurface) {
    vec3 ray=reflect(-view,n);
    ivec2 res=textureSize(skyView,0);
    SkyViewSample s=skyViewSample(6360.,eyeKm,SkyViewLook(ray.z,skyLightViewCos(ray,vec3(0.,0.,1.),sun)), MediumLutSize(float(res.x),float(res.y)));
    vec3 reflected=texture(skyView,vec2(s.Uv.U,s.Uv.V)).rgb;
    light=mix(surface.rgb*.28,reflected*(2.5/1.2),brdfFresnel(vec3(.0204),1.,nv))*mix(.58,1.,visibility);
  }
  Medium air = clearAir();
  vec3 ray = -view;
  vec3 transmission = segmentTransmission(veil, air, eyeKm, ray.z, length(point - eye) * .001);
  ivec2 skySize = textureSize(skyView, 0);
  MediumUv hazeUv = skyViewAtmosphereSample(air.BottomRadiusKm, eyeKm,
      SkyViewLook(ray.z, skyLightViewCos(ray, vec3(0., 0., 1.), sun)),
      MediumLutSize(float(skySize.x), float(skySize.y)));
  vec3 airlight = texture(skyView, vec2(hazeUv.U, hazeUv.V)).rgb * (2.5 / 1.2);
  light = mix(airlight, light, transmission);
  colour=vec4(pow(filmic(max(light,vec3(0.))),vec3(1./2.2)),1.);
  gl_FragDepth=log2(1.+1./gl_FragCoord.w)/log2(240001.);
}
