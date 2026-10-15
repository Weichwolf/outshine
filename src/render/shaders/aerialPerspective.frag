#version 450
#extension GL_GOOGLE_include_directive : require
#include "medium.glsl"
#include "cloudAerial.glsl"
#include "skyViewLookup.glsl"
layout(location = 0) in vec2 ndc;
layout(location = 0) out vec4 colour;
layout(set = 2, binding = 0) uniform sampler2D scene;
layout(set = 2, binding = 1) uniform sampler2D depth;
layout(set = 2, binding = 2) uniform sampler2D skyView;
layout(set = 2, binding = 3) uniform sampler2D veil;
layout(std140, set = 3, binding = 0) uniform Pushed {
  vec4 right;
  vec4 up;
  vec4 fwd;
  vec4 worldUp;
  vec4 sunDir;
  vec2 tanHalf;
  float illuminance;
  float eyeRadiusKm;
  vec4 depthReconstruction;
  Medium air;
  CloudLayer clouds;
  vec4 cloudOriginM;
  vec4 cloudSunIrradiance;
  vec4 cloudSkyRadiance;
} pushed;
vec3 skyViewAt(sampler2D skyView, vec3 dir, vec3 worldUp,
                               vec3 sunDir, float radiusKm, float bottomRadiusKm) {
  ivec2 extent = textureSize(skyView, 0);
  MediumUv uv = skyViewAtmosphereSample(bottomRadiusKm, radiusKm,
      SkyViewLook(dot(dir, worldUp), skyLightViewCos(dir, worldUp, sunDir)),
      MediumLutSize(float(extent.x), float(extent.y)));
  return textureLod(skyView, vec2(uv.U, uv.V), 0.0).rgb;
}

vec4 shaded() {

  uvec2 px = uvec2(gl_FragCoord.xy);
  vec4 lit = texelFetch(scene, ivec2(px), 0);
  float written = texelFetch(depth, ivec2(px), 0).r;

  if (written <= 0.0 && pushed.clouds.field.x <= 0.0) { return lit; }

  vec3 dir = normalize(pushed.fwd.xyz + ndc.x * pushed.tanHalf.x * pushed.right.xyz +
                         ndc.y * pushed.tanHalf.y * pushed.up.xyz);
  float reachKm = 100000.0;
  if (written > 0.0) {
    vec4 reconstruction = pushed.depthReconstruction;
    float alongView = (reconstruction.x - written * reconstruction.y) /
                      max(reconstruction.z + written * reconstruction.w, 1.0e-9);
    reachKm = alongView * 0.001 / max(1.0e-6, dot(dir, normalize(pushed.fwd.xyz)));
  }

  float radiusKm = pushed.eyeRadiusKm;
  float cosZenith = dot(dir, pushed.worldUp.xyz);
  vec3 skyRadiance = skyViewAt(skyView, dir, pushed.worldUp.xyz, normalize(pushed.sunDir.xyz),
                               radiusKm, pushed.air.BottomRadiusKm);
  vec3 clearLuminance = lit.rgb;
  if (written > 0.0) {
    vec3 through = aerialTransmittance(pushed.air, veil, radiusKm, cosZenith, reachKm);
    clearLuminance = lit.rgb * through + skyRadiance * pushed.illuminance * (1.0 - through);
  }
  if (pushed.clouds.field.x <= 0.0) { return vec4(clearLuminance, lit.a); }

  CloudLight light = CloudLight(normalize(pushed.sunDir.xyz),
                                pushed.cloudSunIrradiance.rgb, pushed.cloudSkyRadiance.rgb);
  CloudAirView air = CloudAirView(pushed.air, radiusKm, cosZenith, skyRadiance);
  vec4 clouds = cloudAerial(pushed.clouds, pushed.cloudOriginM.xyz, dir, reachKm * 1000.0,
                             light, air, veil);
  return vec4(clouds.rgb * pushed.illuminance + clouds.a * clearLuminance, lit.a);
}

void main() { colour = shaded(); }
