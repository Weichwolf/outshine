#version 450
#extension GL_GOOGLE_include_directive : require
#include "medium.glsl"
#include "skyViewLookup.glsl"
#include "mediumView.glsl"
layout(location = 0) in vec2 ndc;
layout(location = 0) out vec4 colour;
layout(set = 2, binding = 0) uniform sampler2D scene;
layout(set = 2, binding = 1) uniform sampler2D depth;
layout(set = 2, binding = 2) uniform sampler2D skyView;
layout(set = 2, binding = 3) uniform sampler2D veil;
layout(std140, set = 3, binding = 2) uniform SharedView { MediumView frame; };
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

  MediumViewRay ray = mediumViewRay(frame, vec3(gl_FragCoord.xy, written));
  vec3 dir = ray.Direction;
  float reachKm = ray.Air.DistanceKm;
  float radiusKm = ray.Air.RadiusKm;
  float cosZenith = ray.Air.CosView;
  float endRadiusKm =
      sqrt(max(frame.Air.BottomRadiusKm * frame.Air.BottomRadiusKm,
               radiusKm * radiusKm + reachKm * reachKm + 2.0 * radiusKm * reachKm * cosZenith));
  float endCosZenith = clamp((radiusKm * cosZenith + reachKm) / endRadiusKm, -1.0, 1.0);

  vec3 through;
  if (mediumGroundReach(frame.Air, radiusKm, cosZenith) < 0.0) {
    vec3 atEye =
        textureLod(veil, mediumTransmittanceUv(frame.Air, MediumLook(radiusKm, cosZenith), textureSize(veil, 0)), 0.0).rgb;
    vec3 atEnd =
        textureLod(veil, mediumTransmittanceUv(frame.Air, MediumLook(endRadiusKm, endCosZenith), textureSize(veil, 0)), 0.0).rgb;
    through = clamp(atEye / max(atEnd, vec3(1.0e-6)), vec3(0.0), vec3(1.0));
  } else {
    vec3 atEye =
        textureLod(veil, mediumTransmittanceUv(frame.Air, MediumLook(radiusKm, -cosZenith), textureSize(veil, 0)), 0.0).rgb;
    vec3 atEnd =
        textureLod(veil, mediumTransmittanceUv(frame.Air, MediumLook(endRadiusKm, -endCosZenith), textureSize(veil, 0)), 0.0).rgb;
    through = clamp(atEnd / max(atEye, vec3(1.0e-6)), vec3(0.0), vec3(1.0));
  }

  vec3 inscattered =
      skyViewAt(skyView, dir, normalize(frame.WorldUp.xyz * frame.EyeRadiusKm + ray.OriginOffsetM * 0.001), normalize(frame.SunDir.xyz), radiusKm,
                frame.Air.BottomRadiusKm) *
      frame.Illuminance;

  return vec4(lit.rgb * through + inscattered * (1.0 - through), lit.a);
}

void main() { colour = shaded(); }
