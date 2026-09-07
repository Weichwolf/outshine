#version 450
#extension GL_GOOGLE_include_directive : require
#include "medium.glsl"
#define SCENE_FLOAT(name, value) const float name = value;
#include "../stages/SceneConstants.inc"
#undef SCENE_FLOAT
layout(location = 0) in vec2 ndc;
layout(location = 0) out vec4 colour;
layout(location = 1) out vec2 velocity;
layout(set = 2, binding = 0) uniform sampler2D skyView;
layout(set = 2, binding = 1) uniform sampler2D veil;
layout(std140, set = 3, binding = 0) uniform Pushed {
  vec4 right;
  vec4 up;
  vec4 fwd;
  vec4 worldUp;
  vec4 sunDir;
  vec2 tanHalf;
  float illuminance;
  float eyeRadiusKm;
  float bottomRadiusKm;
  float topRadiusKm;
  float sunHalfAngleRad;
  float pad;
  Medium air;
} pushed;
void main() {

  vec3 dir = normalize(pushed.fwd.xyz + ndc.x * pushed.tanHalf.x * pushed.right.xyz +
                         ndc.y * pushed.tanHalf.y * pushed.up.xyz);
  vec3 worldUp = pushed.worldUp.xyz;
  float cosView = dot(dir, worldUp);

  vec3 side = cross(worldUp, dir);
  float sideLength = length(side);
  float lightViewCos = 1.0;
  if (sideLength > 1.0e-5) {
    side = side / sideLength;
    vec3 forward = normalize(cross(side, worldUp));
    vec2 lightOnPlane =
        normalize(vec2(dot(pushed.sunDir.xyz, forward), dot(pushed.sunDir.xyz, side)));
    lightViewCos = lightOnPlane.x;
  }

  float radiusKm = pushed.eyeRadiusKm;
  float toHorizon = sqrt(max(0.0, radiusKm * radiusKm - pushed.bottomRadiusKm * pushed.bottomRadiusKm));
  float beta = acos(clamp(toHorizon / radiusKm, -1.0, 1.0));
  float zenithToHorizon = OUTSHINE_PI - beta;
  bool hitsGround = acos(clamp(cosView, -1.0, 1.0)) > zenithToHorizon;

  float widthPx = float(textureSize(skyView, 0).x);
  float heightPx = float(textureSize(skyView, 0).y);
  float v;
  if (!hitsGround) {
    float coord = acos(clamp(cosView, -1.0, 1.0)) / zenithToHorizon;
    coord = 1.0 - sqrt(max(0.0, 1.0 - coord));
    v = coord * 0.5;
  } else {
    float coord = (acos(clamp(cosView, -1.0, 1.0)) - zenithToHorizon) / beta;
    v = sqrt(max(0.0, coord)) * 0.5 + 0.5;
  }
  float u = sqrt(max(0.0, -lightViewCos * 0.5 + 0.5));
  u = (u + 0.5 / widthPx) * (widthPx / (widthPx + 1.0));
  v = (v + 0.5 / heightPx) * (heightPx / (heightPx + 1.0));

  vec3 luminance = textureLod(skyView, vec2(u, v), 0.0).rgb * pushed.illuminance;

  float cosToSun = dot(dir, normalize(pushed.sunDir.xyz));
  if (!hitsGround && cosToSun > cos(pushed.sunHalfAngleRad)) {
    float reach = mediumTopReach(pushed.air, radiusKm, cosView);
    float span = sqrt(max(0.0, pushed.topRadiusKm * pushed.topRadiusKm -
                               pushed.bottomRadiusKm * pushed.bottomRadiusKm));
    float ground = sqrt(max(0.0, radiusKm * radiusKm -
                                 pushed.bottomRadiusKm * pushed.bottomRadiusKm));
    float shortest = pushed.topRadiusKm - radiusKm;
    float longest = ground + span;
    vec2 uv = vec2((reach - shortest) / max(1.0e-6, longest - shortest), ground / max(1.0e-6, span));
    vec3 through = textureLod(veil, uv, 0.0).rgb;
    float solid = OUTSHINE_PI * pushed.sunHalfAngleRad * pushed.sunHalfAngleRad;
    luminance += through * (pushed.illuminance / solid);
  }

  colour = vec4(luminance, 1.0);
  velocity = vec2(kVelocityStatic, kVelocityStatic);
}
