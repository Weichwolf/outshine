#version 450
#extension GL_GOOGLE_include_directive : require
#include "medium.glsl"
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
  float cosView = dot(dir, worldUp);
  vec3 side = cross(worldUp, dir);
  float sideLength = length(side);
  float lightViewCos = 1.0;
  if (sideLength > 1.0e-5) {
    side = side / sideLength;
    vec3 forward = normalize(cross(side, worldUp));
    vec2 onPlane = normalize(vec2(dot(sunDir, forward), dot(sunDir, side)));
    lightViewCos = onPlane.x;
  }
  float toHorizon = sqrt(max(0.0, radiusKm * radiusKm - bottomRadiusKm * bottomRadiusKm));
  float beta = acos(clamp(toHorizon / radiusKm, -1.0, 1.0));
  float zenithToHorizon = OUTSHINE_PI - beta;

  float angle = min(acos(clamp(cosView, -1.0, 1.0)), zenithToHorizon);
  float widthPx = float(textureSize(skyView, 0).x);
  float heightPx = float(textureSize(skyView, 0).y);
  float coord = angle / zenithToHorizon;
  coord = 1.0 - sqrt(max(0.0, 1.0 - coord));
  float v = coord * 0.5;
  float u = sqrt(max(0.0, -lightViewCos * 0.5 + 0.5));
  u = (u + 0.5 / widthPx) * (widthPx / (widthPx + 1.0));
  v = (v + 0.5 / heightPx) * (heightPx / (heightPx + 1.0));
  return textureLod(skyView, vec2(u, v), 0.0).rgb;
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
  if (cosZenith > 0.0) {
    vec3 atEye =
        textureLod(veil, mediumTransmittanceUv(pushed.air, MediumLook(radiusKm, cosZenith)), 0.0).rgb;
    vec3 atEnd =
        textureLod(veil, mediumTransmittanceUv(pushed.air, MediumLook(endRadiusKm, endCosZenith)), 0.0).rgb;
    through = clamp(atEye / max(atEnd, vec3(1.0e-6)), vec3(0.0), vec3(1.0));
  } else {
    vec3 atEye =
        textureLod(veil, mediumTransmittanceUv(pushed.air, MediumLook(radiusKm, -cosZenith)), 0.0).rgb;
    vec3 atEnd =
        textureLod(veil, mediumTransmittanceUv(pushed.air, MediumLook(endRadiusKm, -endCosZenith)), 0.0).rgb;
    through = clamp(atEnd / max(atEye, vec3(1.0e-6)), vec3(0.0), vec3(1.0));
  }

  vec3 inscattered =
      skyViewAt(skyView, dir, pushed.worldUp.xyz, normalize(pushed.sunDir.xyz), radiusKm,
                pushed.air.BottomRadiusKm) *
      pushed.illuminance;

  return vec4(lit.rgb * through + inscattered * (1.0 - through), lit.a);
}

void main() { colour = shaded(); }
