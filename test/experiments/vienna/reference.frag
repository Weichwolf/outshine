#version 410 core
const float kPi=3.14159265359;
#include "metalRoughBrdf.glsl"
#include "medium.glsl"
#include "clear_air.glsl"
#include "skyViewLookup.glsl"
#include "display.glsl"
#include "environmentSampling.glsl"
#include "archived_sky.glsl"
uniform sampler2D skyView;
uniform sampler2D veil;
uniform sampler2D detailMap;
uniform vec3 detailScale;
uniform vec3 detailTint;
uniform float barkBoxBlend;
uniform bool hasDetail;
uniform float eyeKm;
uniform float directSunFraction;
uniform float glassIor;
uniform bool waterSurface;
uniform float waterIor;
uniform sampler2D waterBedMap;
uniform vec4 waterBedTransform;
uniform vec3 bedAlbedo;
uniform bool shadowPass;
uniform bool reflectionCapture;
uniform vec4 reflectionClip;
uniform vec4 reflectionPlane0;
uniform vec4 reflectionPlane1;
uniform sampler2D reflectionMap;
uniform int reflectionCount;
uniform mat4 viewProjection;
uniform float reflectionRange;
uniform float reflectionTolerance;
uniform mat4 shadowMatrix;
uniform mat4 localShadowMatrix;
uniform mat4 nearShadowMatrix;
uniform sampler2DShadow shadowMap;
uniform sampler2DShadow localShadowMap;
uniform sampler2DShadow nearShadowMap;
uniform vec2 regionalShadowSpan;
uniform vec2 localShadowSpan;
uniform float nearShadowWidth;
in vec3 point;
in vec3 smoothNormal;
flat in vec2 rotation;
out vec4 colour;
uniform vec3 eye;
uniform vec3 sun;
uniform vec4 surface;
uniform float metalness;
uniform float diffuseTransmission;
uniform bool useSkyIrradiance;
uniform vec3 skyIrradiance[9];
uniform sampler2D skyRadiance;
uniform sampler2D skySpecularMap;
uniform sampler2D skyBrdf;
uniform bool linearOutput;
vec2 detailCoordinates(vec3 normal) {
  vec3 axis=abs(normal);
  // Use Blender BOX axes and mirroring for the shared texture.
  if (axis.z>=max(axis.x,axis.y))
    return vec2(normal.z>0. ? -point.y : point.y,point.x);
  if (axis.y>axis.x)
    return vec2(normal.y>0. ? -point.x : point.x,point.z);
  return vec2(normal.x<0. ? -point.y : point.y,point.z);
}

vec4 barkDetail(vec3 normal, float span) {
  vec3 axis=abs(normal);
  vec3 ratios=axis/max(axis.x,max(axis.y,axis.z));
  vec3 w=smoothstep(vec3(1.-barkBoxBlend),vec3(1.),ratios);
  w/=w.x+w.y+w.z;
  vec2 x=vec2(normal.x<0. ? -point.y : point.y,point.z)/span;
  vec2 y=vec2(normal.y>0. ? -point.x : point.x,point.z)/span;
  vec2 z=vec2(normal.z>0. ? -point.y : point.y,point.x)/span;
  return w.x*texture(detailMap,x)+w.y*texture(detailMap,y)+w.z*texture(detailMap,z);
}

vec3 bumpNormal(vec3 normal, vec3 dx, vec3 dy, float height) {
  vec3 reciprocalX=cross(dy,normal),reciprocalY=cross(normal,dx);
  float det=dot(dx,reciprocalX);
  vec3 gradient=sign(det)*(dFdx(height)*reciprocalX+dFdy(height)*reciprocalY);
  return abs(det)>1.e-15 ? normalize(abs(det)*normal-gradient) : normal;
}

vec3 diffuseSky(vec3 n) {
  float x = n.x, y = n.y, z = n.z;
  return max(vec3(0.), .2820947918 * skyIrradiance[0] +
    .4886025119 * (y * skyIrradiance[1] + z * skyIrradiance[2] + x * skyIrradiance[3]) +
    1.092548431 * (x * y * skyIrradiance[4] + y * z * skyIrradiance[5] + x * z * skyIrradiance[7]) +
    .3153915653 * (3. * z * z - 1.) * skyIrradiance[6] +
    .5462742153 * (x * x - y * y) * skyIrradiance[8]);
}

vec3 filteredSky(vec3 direction, float roughness) {
  float level = clamp(roughness, 0., 1.) * float(kEnvironmentSpecularLevels - 1);
  float lower = floor(level);
  float upper = min(lower + 1., float(kEnvironmentSpecularLevels - 1));
  vec3 low = textureLod(skySpecularMap, environmentUv(direction, lower), 0.).rgb;
  vec3 high = textureLod(skySpecularMap, environmentUv(direction, upper), 0.).rgb;
  return mix(low, high, level - lower);
}

float shadowWeight(vec3 uv) {
  float edge = 2. * max(abs(uv.x - .5), abs(uv.y - .5));
  return uv.z > 0. && uv.z < 1. ? 1. - smoothstep(.84, 1., edge) : 0.;
}

float localDepthVisibility(sampler2DShadow depthMap, vec3 uv, vec2 span, float slope) {
  float texel = span.x / float(textureSize(depthMap, 0).x);
  float bias = texel * (.5 + .8 * slope) / span.y;
  return texture(depthMap, vec3(uv.xy, uv.z - bias));
}

float sunVisibility(vec3 position, vec3 normal) {
  vec4 worldPoint = vec4(position, 1.);
  vec3 localUV = (localShadowMatrix * worldPoint).xyz * .5 + .5;
  float localWeight = shadowWeight(localUV);
  float lightAngle = max(0., dot(normal, sun));
  float slope = sqrt(max(0., 1. - lightAngle * lightAngle)) / max(lightAngle, .10);
  vec3 regionalUV = (shadowMatrix * worldPoint).xyz * .5 + .5;
  float regionalVisibility = 1.;
  float outsideVisibility = 1.;
  if (all(greaterThan(regionalUV, vec3(0.))) && all(lessThan(regionalUV, vec3(1.)))) {
    if (localWeight < 1.) {
      float texel = regionalShadowSpan.x / float(textureSize(shadowMap, 0).x);
      float bias = (.30 + 1.45 * texel * slope) / regionalShadowSpan.y;
      regionalVisibility = texture(shadowMap, vec3(regionalUV.xy, regionalUV.z - bias));
    }
    if (localWeight > 0.) {
      // Keep regional occluders on the sunward side of the local map's near plane.
      float localNear = regionalUV.z - localUV.z * localShadowSpan.y / regionalShadowSpan.y;
      outsideVisibility = texture(shadowMap, vec3(regionalUV.xy, localNear));
    }
  }
  if (localWeight == 0.)
    return regionalVisibility;
  vec3 nearUV = (nearShadowMatrix * worldPoint).xyz * .5 + .5;
  float nearWeight = shadowWeight(nearUV);
  float localVisibility = nearWeight < 1.
    ? localDepthVisibility(localShadowMap, localUV, localShadowSpan, slope) : 1.;
  if (nearWeight > 0.) {
    vec2 nearSpan = vec2(nearShadowWidth, localShadowSpan.y);
    float nearVisibility = localDepthVisibility(nearShadowMap, nearUV, nearSpan, slope);
    localVisibility = mix(localVisibility, nearVisibility, nearWeight);
  }
  return mix(regionalVisibility, localVisibility * outsideVisibility, localWeight);
}

vec3 reflectedSky(vec3 ray) {
  ivec2 res=textureSize(skyView,0);
  SkyViewSample s=skyViewSample(6360.,eyeKm,SkyViewLook(ray.z,skyLightViewCos(ray,vec3(0.,0.,1.),sun)), MediumLutSize(float(res.x),float(res.y)));
  return texture(skyView,vec2(s.Uv.U,s.Uv.V)).rgb*(2.5/1.2);
}

vec3 reflectedEnvironment(vec3 ray) {
  vec3 sky = useSkyIrradiance ? archivedSky(skyRadiance, ray) : reflectedSky(ray);
  float distance = length(point.xy - eye.xy);
  if (reflectionCount == 0 || distance >= reflectionRange)
    return sky;
  vec4 worldPoint = vec4(point,1.);
  vec2 signedError = vec2(dot(reflectionPlane0,worldPoint),dot(reflectionPlane1,worldPoint));
  vec2 error = abs(signedError);
  int layer = reflectionCount > 1 && error.y < error.x ? 1 : 0;
  if (error[layer] > reflectionTolerance)
    return sky;
  vec3 normal = layer == 0 ? reflectionPlane0.xyz : reflectionPlane1.xyz;
  vec4 projection = viewProjection * vec4(point-2.*signedError[layer]*normal,1.);
  if (projection.w<=0.)
    return sky;
  vec2 uv = .5 + .5*projection.xy/projection.w;
  if (any(lessThan(uv,vec2(0.))) || any(greaterThan(uv,vec2(1.))))
    return sky;
  vec2 extent = vec2(textureSize(reflectionMap,0));
  float height = extent.y / float(reflectionCount);
  float first = float(layer) * height;
  vec2 centre = vec2(uv.x*extent.x, uv.y*height+first);
  vec2 lo = vec2(.5, first+.5);
  vec2 hi = vec2(extent.x-.5, first+height-.5);
  float blur = max(1., 14.*surface.a);
  vec4 scene = .4 * texture(reflectionMap,clamp(centre,lo,hi)/extent);
  scene += .15 * texture(reflectionMap,clamp(centre+vec2(blur,0.),lo,hi)/extent);
  scene += .15 * texture(reflectionMap,clamp(centre-vec2(blur,0.),lo,hi)/extent);
  scene += .15 * texture(reflectionMap,clamp(centre+vec2(0.,blur),lo,hi)/extent);
  scene += .15 * texture(reflectionMap,clamp(centre-vec2(0.,blur),lo,hi)/extent);
  float weight = (1.-smoothstep(.8*reflectionRange,reflectionRange,distance)) *
                 (1.-smoothstep(.75*reflectionTolerance,reflectionTolerance,error[layer]));
  return weight*scene.rgb + sky * (1.-weight*scene.a);
}

float waterFresnel(float cosine) {
  float nv = clamp(cosine, 0., 1.);
  float nt = sqrt(1. - (1. - nv * nv) / (waterIor * waterIor));
  float s = (nv - waterIor * nt) / (nv + waterIor * nt);
  float p = (waterIor * nv - nt) / (waterIor * nv + nt);
  return .5 * (s * s + p * p);
}

void main(){
  if (shadowPass) { gl_FragDepth=gl_FragCoord.z; return; }
  if (reflectionCapture && dot(reflectionClip,vec4(point,1.))<0.) discard;
  vec3 view=normalize(eye-point);
  vec3 dx=dFdx(point),dy=dFdy(point);
  vec3 n=normalize(cross(dx,dy));
  if(dot(n,view)<0.) n=-n;
  vec3 geometricNormal=n;
  bool bark=dot(smoothNormal,smoothNormal)>.01;
  if(bark) {
    n=normalize(smoothNormal);
    if(dot(n,view)<0.) n=-n;
  }
  vec4 material=surface;
  if (hasDetail) {
    vec3 objectNormal=vec3(
        dot(vec2(rotation.x,rotation.y),n.xy),
        dot(vec2(-rotation.y,rotation.x),n.xy),n.z);
    vec2 uv=detailCoordinates(objectNormal);
    vec4 fine=bark ? barkDetail(objectNormal,detailScale.x) : texture(detailMap,uv/detailScale.x);
    float macro=bark ? barkDetail(objectNormal,detailScale.y).a : texture(detailMap,uv/detailScale.y).a;
    material.rgb*=fine.r*(1.68+.64*macro)*mix(vec3(1.),detailTint,fine.a);
    material.a=fine.g;
    n=bumpNormal(n,dx,dy,fine.b*detailScale.z);
  }
  vec3 h=normalize(view+sun);
  float nl=max(0.,dot(n,sun)),nv=max(.001,dot(n,view));
  float glassF0=(glassIor-1.)/(glassIor+1.);
  vec3 dielectricF0=vec3(glassIor>1. ? glassF0*glassF0 : .04);
  vec3 f0=mix(dielectricF0,material.rgb,metalness);
  Brdf b=metalRoughBrdf(material.rgb*(1.-metalness),f0,1.,pow(material.a,4.),nl,nv,max(0.,dot(n,h)),max(0.,dot(view,h)));
  float visibility = sunVisibility(point, geometricNormal);
  vec3 reflected=(b.diffuse+b.specular)*nl;
  vec3 transmitted=material.rgb*(max(0.,dot(n,-sun))/kPi);
  vec3 environment = material.rgb *
    mix(vec3(.065,.085,.115), vec3(.31,.40,.53), clamp(.5+.5*n.z,0.,1.));
  if (useSkyIrradiance) {
    vec3 diffuseColour = material.rgb * (1. - metalness);
    vec3 front = diffuseColour * diffuseSky(n);
    if (!waterSurface && glassIor <= 1.) {
      float roughness = clamp(material.a, 0., 1.);
      vec2 brdf = textureLod(skyBrdf, vec2(roughness, nv), 0.).rg;
      vec3 reflectance = clamp(f0 * brdf.x + vec3(brdf.y), 0., 1.);
      front = (1. - reflectance) * front +
              reflectance * filteredSky(reflect(-view, n), roughness);
    }
    environment = mix(front, diffuseColour * diffuseSky(-n), diffuseTransmission);
  }
  float directSun = 2.5 * directSunFraction;
  vec3 light = mix(reflected, transmitted, diffuseTransmission) * directSun * visibility + environment;
  gl_FragDepth=log2(1.+1./gl_FragCoord.w)/log2(240001.);
  if (reflectionCapture) { colour=vec4(light,1.); return; }
  if (waterSurface) {
    vec3 ray=reflect(-view,n);
    vec2 bedUV=(point.xy-waterBedTransform.xy)*waterBedTransform.zw;
    vec3 transmission=texture(waterBedMap,bedUV).rgb;
    vec3 bodyColour=mix(surface.rgb,bedAlbedo,transmission);
    vec3 bodyLight = useSkyIrradiance
      ? bodyColour * (diffuseSky(n) + (directSun / kPi) * nl * visibility)
      : bodyColour * .28 * mix(.58, 1., visibility);
    light = mix(bodyLight, reflectedEnvironment(ray), waterFresnel(dot(n, view)));
  } else if (glassIor>1.) {
    vec3 ray=reflect(-view,n);
    vec3 sky=reflectedSky(normalize(vec3(ray.xy,max(ray.z,.001))));
    vec3 environment=mix(vec3(.07,.065,.057),sky,smoothstep(-.23,.07,ray.z));
    light+=environment*brdfFresnel(f0,1.,nv);
  }
  // The veil carries extinction. The mapping uses only the shared planet geometry.
  Medium air = referenceAir(0.);
  vec3 ray = -view;
  vec3 transmission = segmentTransmission(veil, air, eyeKm, ray.z, length(point - eye) * .001);
  ivec2 skySize = textureSize(skyView, 0);
  MediumUv hazeUv = skyViewAtmosphereSample(air.BottomRadiusKm, eyeKm,
      SkyViewLook(ray.z, skyLightViewCos(ray, vec3(0., 0., 1.), sun)),
      MediumLutSize(float(skySize.x), float(skySize.y)));
  vec3 airlight = texture(skyView, vec2(hazeUv.U, hazeUv.V)).rgb * (2.5 / 1.2);
  light = mix(airlight, light, transmission);
  colour=vec4(linearOutput ? light : pow(filmic(max(light,vec3(0.))),vec3(1./2.2)),1.);
}
