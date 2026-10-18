#version 450
#extension GL_GOOGLE_include_directive : require
#include "medium.glsl"
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

  if (written <= 0.0) { return lit; }

  vec3 dir = normalize(pushed.fwd.xyz + ndc.x * pushed.tanHalf.x * pushed.right.xyz +
                         ndc.y * pushed.tanHalf.y * pushed.up.xyz);
  vec4 reconstruction = pushed.depthReconstruction;
  float alongView = (reconstruction.x - written * reconstruction.y) /
                    max(reconstruction.z + written * reconstruction.w, 1.0e-9);
  float reachKm = alongView * 0.001 / max(1.0e-6, dot(dir, normalize(pushed.fwd.xyz)));

  float radiusKm = pushed.eyeRadiusKm;
  float cosZenith = dot(dir, pushed.worldUp.xyz);
  float endRadiusKm =
      sqrt(max(pushed.air.BottomRadiusKm * pushed.air.BottomRadiusKm,
               radiusKm * radiusKm + reachKm * reachKm + 2.0 * radiusKm * reachKm * cosZenith));
  float endCosZenith = clamp((radiusKm * cosZenith + reachKm) / endRadiusKm, -1.0, 1.0);

  vec3 through;
  if (mediumGroundReach(pushed.air, radiusKm, cosZenith) < 0.0) {
    vec3 atEye =
        textureLod(veil, mediumTransmittanceUv(pushed.air, MediumLook(radiusKm, cosZenith), textureSize(veil, 0)), 0.0).rgb;
    vec3 atEnd =
        textureLod(veil, mediumTransmittanceUv(pushed.air, MediumLook(endRadiusKm, endCosZenith), textureSize(veil, 0)), 0.0).rgb;
    through = clamp(atEye / max(atEnd, vec3(1.0e-6)), vec3(0.0), vec3(1.0));
  } else {
    vec3 atEye =
        textureLod(veil, mediumTransmittanceUv(pushed.air, MediumLook(radiusKm, -cosZenith), textureSize(veil, 0)), 0.0).rgb;
    vec3 atEnd =
        textureLod(veil, mediumTransmittanceUv(pushed.air, MediumLook(endRadiusKm, -endCosZenith), textureSize(veil, 0)), 0.0).rgb;
    through = clamp(atEnd / max(atEye, vec3(1.0e-6)), vec3(0.0), vec3(1.0));
  }

  vec3 inscattered =
      skyViewAt(skyView, dir, pushed.worldUp.xyz, normalize(pushed.sunDir.xyz), radiusKm,
                pushed.air.BottomRadiusKm) *
      pushed.illuminance;

  return vec4(lit.rgb * through + inscattered * (1.0 - through), lit.a);
}

void main() { colour = shaded(); }
