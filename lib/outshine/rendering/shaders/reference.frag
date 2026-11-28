#version 410 core
const float kPi=3.14159265359;
const int kHorizonCover=1;
const int kLandscapeCover=2;
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
uniform bool planarDetail;
uniform bool hasGroundBlend;
uniform sampler2D overlayMap;
uniform vec3 overlayColour;
uniform vec3 overlayScale;
uniform vec3 overlayTint;
uniform bool hasShoreSubstrate;
uniform vec4 shoreTransform;
uniform vec4 wetMineral;
uniform vec4 crustMineral;
uniform vec4 gravelMineral;
uniform vec2 crustGrain;
uniform int terrainCoverMode;
uniform sampler2D terrainCover;
uniform vec4 terrainTransform;
uniform float eyeKm;
uniform vec3 directSunRadiance;
uniform sampler2D cloudSunMap;
uniform float cloudSunMapScale;
uniform float cloudSunMapTopM;
uniform vec3 cloudSunMapOrigin;
uniform float glassIor;
uniform bool waterSurface;
uniform float waterIor;
uniform vec4 waterBedTransform;
uniform vec3 bedAlbedo;
uniform bool shadowPass;
uniform bool blendDetailAxes;
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
uniform sampler2D localDepthMap;
uniform sampler2D nearDepthMap;
uniform vec2 regionalShadowSpan;
uniform vec2 localShadowSpan;
uniform float nearShadowWidth;
in vec3 point;
in vec3 smoothNormal;
in float blendWeight;
flat in vec2 rotation;
out vec4 colour;
uniform vec3 eye;
uniform vec3 sun;
uniform vec4 surface;
uniform float metalness;
uniform float diffuseTransmission;
uniform vec3 backfaceReflectance;
uniform vec3 skyIrradiance[9];
uniform sampler2D skyRadiance;
uniform sampler2D skySpecularMap;
uniform sampler2D skyBrdf;
uniform bool linearOutput;
#ifdef OUTSHINE_LOCAL_SKYLIGHT
#include "local_skylight.glsl"
#endif
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
  vec3 unoccluded = max(vec3(0.), .2820947918 * skyIrradiance[0] +
    .4886025119 * (y * skyIrradiance[1] + z * skyIrradiance[2] + x * skyIrradiance[3]) +
    1.092548431 * (x * y * skyIrradiance[4] + y * z * skyIrradiance[5] + x * z * skyIrradiance[7]) +
    .3153915653 * (3. * z * z - 1.) * skyIrradiance[6] +
    .5462742153 * (x * x - y * y) * skyIrradiance[8]);
#ifdef OUTSHINE_LOCAL_SKYLIGHT
  return occludedSkylight(unoccluded, n);
#else
  return unoccluded;
#endif
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

const float kLocalBlockerReachM = 64.;
const vec2 penumbraSamples[16] = vec2[16](
  vec2(0.1767767, 0.0000000), vec2(-0.2257722, 0.2068258),
  vec2(0.0345581, -0.3937712), vec2(0.2845712, 0.3711728),
  vec2(-0.5222232, -0.0923739), vec2(0.4946954, -0.3146847),
  vec2(-0.1654659, 0.6155250), vec2(-0.3155615, -0.6075944),
  vec2(0.6846422, 0.2500302), vec2(-0.7122561, 0.2940090),
  vec2(0.3433545, -0.7337286), vec2(0.2537302, 0.8089320),
  vec2(-0.7647459, -0.4431859), vec2(0.8971340, -0.1972324),
  vec2(-0.5475069, 0.7787722), vec2(-0.1264868, -0.9760897));

float localDepthVisibility(sampler2DShadow depthMap, sampler2D rawDepth, vec3 uv,
                           vec2 span, mat4 projection, vec3 normal, float slope) {
  float texel = span.x / float(textureSize(depthMap, 0).x);
  float bias = texel * (.5 + .8 * slope) / span.y;
  float base = texture(depthMap, vec3(uv.xy, uv.z - bias));
  float facing = dot(normal, sun);
  if (abs(facing) < .1)
    return base;

  // Compare neighboring samples against the receiver plane at each offset.
  vec3 right = normalize(vec3(projection[0][0], projection[1][0], projection[2][0]));
  vec3 up = normalize(vec3(projection[0][1], projection[1][1], projection[2][1]));
  vec2 receiverPlane = vec2(dot(normal, right), dot(normal, up)) *
                       (span.x / (facing * span.y));
  float angularRadius = tan(kSunHalfAngleRad);
  float searchRadius = angularRadius * kLocalBlockerReachM / span.x;
  float totalGap = 0.;
  int blockers = 0;
  for (int i = -1; i < penumbraSamples.length(); ++i) {
    vec2 offset = i < 0 ? vec2(0.) : penumbraSamples[i] * searchRadius;
    float receiver = uv.z + dot(receiverPlane, offset);
    float gap = receiver - texture(rawDepth, uv.xy + offset).r;
    if (gap > bias) {
      totalGap += min(kLocalBlockerReachM, gap * span.y);
      ++blockers;
    }
  }
  if (blockers == 0)
    return base;

  float radius = angularRadius * (totalGap / float(blockers));
  if (radius <= .5 * texel)
    return base;
  float visibility = 0.;
  for (int i = 0; i < penumbraSamples.length(); ++i) {
    vec2 offset = penumbraSamples[i] * (radius / span.x);
    float receiver = uv.z + dot(receiverPlane, offset) - bias;
    visibility += texture(depthMap, vec3(uv.xy + offset, receiver));
  }
  return visibility / float(penumbraSamples.length());
}

float sunVisibility(vec3 position, vec3 normal) {
  vec4 worldPoint = vec4(position, 1.);
  vec3 localUV = (localShadowMatrix * worldPoint).xyz * .5 + .5;
  float localWeight = shadowWeight(localUV);
  // Transmitting leaves can be lit on either side of the receiver plane.
  float lightAngle = abs(dot(normal, sun));
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
    ? localDepthVisibility(localShadowMap, localDepthMap, localUV, localShadowSpan,
                           localShadowMatrix, normal, slope) : 1.;
  if (nearWeight > 0.) {
    vec2 nearSpan = vec2(nearShadowWidth, localShadowSpan.y);
    float nearVisibility = localDepthVisibility(nearShadowMap, nearDepthMap, nearUV,
                                                nearSpan, nearShadowMatrix, normal, slope);
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
  vec3 sky = archivedSky(skyRadiance, ray);
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
  if(dot(smoothNormal,smoothNormal)>.01) {
    n=normalize(smoothNormal);
    if(dot(n,view)<0.) n=-n;
  }
  vec4 material=surface;
  if (!gl_FrontFacing)
    material.rgb=backfaceReflectance;
  vec4 coverage=vec4(0.);
  float mineralGrain=1.;
  if (terrainCoverMode != 0) {
    vec2 uv=(point.xy-terrainTransform.xy)*terrainTransform.zw;
    if (all(greaterThanEqual(uv,vec2(0.))) && all(lessThanEqual(uv,vec2(1.))))
      coverage=texture(terrainCover,uv);
    if (terrainCoverMode==kHorizonCover && coverage.a>0.) material=coverage;
  }
  if (hasDetail) {
    vec3 objectNormal=vec3(
        dot(vec2(rotation.x,rotation.y),n.xy),
        dot(vec2(-rotation.y,rotation.x),n.xy),n.z);
    vec2 uv=planarDetail ? vec2(-point.y,point.x) : detailCoordinates(objectNormal);
    vec4 fine=blendDetailAxes ? barkDetail(objectNormal,detailScale.x) : texture(detailMap,uv/detailScale.x);
    if (hasShoreSubstrate)
      mineralGrain=clamp(fine.r*crustGrain.x+crustGrain.y,0.,1.);
    float macro=blendDetailAxes ? barkDetail(objectNormal,detailScale.y).a : texture(detailMap,uv/detailScale.y).a;
    material.rgb*=fine.r*(1.68+.64*macro)*mix(vec3(1.),detailTint,fine.a);
    material.a=fine.g;
    float height = fine.b * detailScale.z;
    if (hasGroundBlend) {
      vec2 overlayUv = detailCoordinates(objectNormal);
      vec4 overlay = texture(overlayMap,overlayUv/overlayScale.x);
      float broad = texture(overlayMap,overlayUv/overlayScale.y).a;
      float weight = pow(smoothstep(0.,1.,blendWeight),1.35 - .70 * overlay.a);
      vec3 albedo = overlayColour * overlay.r * (1.68 + .64 * broad) *
                    mix(vec3(1.),overlayTint,overlay.a);
      material = mix(material,vec4(albedo,overlay.g),weight);
      height = mix(height,overlay.b * overlayScale.z,weight);
    }
    n=bumpNormal(n,dx,dy,height);
  }
  if (terrainCoverMode==kLandscapeCover)
    material.rgb=material.rgb*(1.-coverage.a)+coverage.rgb;
  if (hasShoreSubstrate) {
    vec2 uv=(point.xy-shoreTransform.xy)*shoreTransform.zw;
    vec3 weights=texture(overlayMap,uv).rgb;
    material=mix(material,gravelMineral,weights.b);
    material=mix(mix(material,wetMineral,weights.r),crustMineral,weights.g*mineralGrain);
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
  vec3 diffuseColour = material.rgb * (1. - metalness);
  vec3 front = diffuseColour * diffuseSky(n);
  if (!waterSurface && glassIor <= 1.) {
    float roughness = clamp(material.a, 0., 1.);
    vec2 brdf = textureLod(skyBrdf, vec2(roughness, nv), 0.).rg;
    vec3 reflectance = clamp(f0 * brdf.x + vec3(brdf.y), 0., 1.);
    front = (1. - reflectance) * front +
            reflectance * filteredSky(reflect(-view, n), roughness);
  }
  vec3 environment = mix(front, diffuseColour * diffuseSky(-n), diffuseTransmission);
  vec3 directSun = directSunRadiance;
  if (cloudSunMapScale > 0. && point.z < cloudSunMapTopM) {
    vec3 offset = point - cloudSunMapOrigin;
    vec2 projected = offset.xy - offset.z * sun.xy / max(sun.z, .01);
    directSun *= texture(cloudSunMap, projected * cloudSunMapScale + .5).r;
  }
  vec3 light = mix(reflected, transmitted, diffuseTransmission) * directSun * visibility + environment;
  gl_FragDepth=log2(1.+1./gl_FragCoord.w)/log2(240001.);
  if (reflectionCapture) { colour=vec4(light,1.); return; }
  if (waterSurface) {
    vec3 ray=reflect(-view,n);
    vec2 bedUV=(point.xy-waterBedTransform.xy)*waterBedTransform.zw;
    vec3 transmission=texture(detailMap,bedUV).rgb;
    vec3 bodyColour=mix(surface.rgb,bedAlbedo,transmission);
    vec3 bodyLight = bodyColour * (diffuseSky(n) + (directSun / kPi) * nl * visibility);
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
