#version 450

layout(set = 2, binding = 0) uniform sampler2D capturedDepth;
layout(set = 2, binding = 1) uniform sampler2D capturedNormal;
layout(set = 2, binding = 2) uniform sampler2D capturedIdentity;
layout(set = 2, binding = 3) uniform sampler2D capturedBase;
layout(set = 2, binding = 4) uniform sampler2D capturedMetalRough;
layout(std140, set = 3, binding = 0) uniform Reprojection {
  mat4 sourceFromTarget;
  mat4 targetFromSource;
  vec4 targetExtent;
} r;

layout(location = 0) out vec4 outHdr;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outIdentity;
layout(location = 3) out vec4 outBase;
layout(location = 4) out vec2 outMetalRough;

void main() {
  vec2 targetNdc = gl_FragCoord.xy / r.targetExtent.xy * vec2(2.0, -2.0) + vec2(-1.0, 1.0);
  vec4 sourceClip = r.sourceFromTarget * vec4(targetNdc, 1.0, 1.0);
  if (sourceClip.w <= 0.0) { discard; }
  vec2 sourceNdc = sourceClip.xy / sourceClip.w;
  vec2 uv = sourceNdc * vec2(0.5, -0.5) + vec2(0.5);
  if (any(lessThan(uv, vec2(0.0))) || any(greaterThanEqual(uv, vec2(1.0)))) { discard; }
  ivec2 pixel = ivec2(uv * vec2(textureSize(capturedDepth, 0)));
  float depth = texelFetch(capturedDepth, pixel, 0).r;
  if (depth <= 0.0) { discard; }
  vec4 targetClip = r.targetFromSource * vec4(sourceNdc, depth, 1.0);
  if (targetClip.w <= 0.0) { discard; }
  float targetDepth = targetClip.z / targetClip.w;
  if (targetDepth <= 0.0 || targetDepth > 1.0) { discard; }
  gl_FragDepth = targetDepth;
  outHdr = vec4(0.0);
  outNormal = texelFetch(capturedNormal, pixel, 0);
  outIdentity = texelFetch(capturedIdentity, pixel, 0);
  outBase = texelFetch(capturedBase, pixel, 0);
  outMetalRough = texelFetch(capturedMetalRough, pixel, 0).rg;
}
