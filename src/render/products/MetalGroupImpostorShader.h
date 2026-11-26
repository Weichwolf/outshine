#ifndef OUTSHINE_RENDER_PRODUCTS_METALGROUPIMPOSTORSHADER_H
#define OUTSHINE_RENDER_PRODUCTS_METALGROUPIMPOSTORSHADER_H

#import <Foundation/Foundation.h>

namespace outshine::Render {

inline NSString *const GroupImpostorShader = @R"(
struct GroupPatch {
  packed_ushort4 rect;
  float depth, slopeX, slopeY;
};
struct GroupAxes {
  float4 origin;
  float4 alongX;
  float4 alongY;
  float4 alongDepth;
  float4 textureRegion;
};
struct GroupVaryings {
  float4 position [[position]];
  float3 localPosition;
  float2 uv;
};
vertex GroupVaryings group_vertex(uint cornerIndex [[vertex_id]], uint patchIndex [[instance_id]],
                                 const device GroupPatch *patches [[buffer(0)]],
                                 constant float4x4 &clip [[buffer(1)]],
                                 constant GroupAxes &axes [[buffer(2)]]) {
  constexpr ushort2 corners[6] = {{0, 0}, {0, 1}, {1, 1}, {0, 0}, {1, 1}, {1, 0}};
  const GroupPatch patch = patches[patchIndex];
  const float2 least = float2(patch.rect.xy);
  const float2 span = float2(patch.rect.zw) - least;
  const float2 offset = float2(corners[cornerIndex]) * span;
  const float2 p = least + offset;
  const float depth = patch.depth + dot(offset, float2(patch.slopeX, patch.slopeY));
  const float3 world = axes.origin.xyz + axes.alongX.xyz * p.x +
      axes.alongY.xyz * p.y + axes.alongDepth.xyz * depth;
  const float2 uv = (p - axes.textureRegion.xy) * axes.textureRegion.zw;
  return {clip * float4(world, 1.0), world, uv};
}
float3 groupNormal(float2 packed) {
  float2 v = packed * 2.0 - 1.0;
  const float z = 1.0 - abs(v.x) - abs(v.y);
  if (z < 0.0) { v = (1.0 - abs(v.yx)) * select(float2(-1.0), float2(1.0), v >= 0.0); }
  return normalize(float3(v, z));
}
float4 groupColour(GroupVaryings in, constant SurfaceLight &light, uint fractional,
                   texture2d<float> colours, texture2d<float> normals, bool capture = false) {
  constexpr sampler colourSampler(coord::normalized, address::clamp_to_edge,
                                   filter::linear, mip_filter::linear);
  constexpr sampler normalSampler(coord::normalized, address::clamp_to_edge,
                                   filter::nearest, mip_filter::nearest);
  const float4 alphaColour = colours.sample(colourSampler, in.uv);
  const float weight = light.eyeAndImpostorWeight.w;
  const float coverage = fractional != 0 ? alphaColour.a * weight
                                          : (alphaColour.a >= 0.5 ? weight : 0.0);
  if (coverage < 1.0 && lodNoise(in.position.xy) >= coverage) {
    discard_fragment();
  }
  const float3 normal = groupNormal(normals.sample(normalSampler, in.uv).xy);
  const half3 albedo = half3(alphaColour.rgb / alphaColour.a);
  const Varyings surface = {in.position, in.localPosition, normal, half4(albedo, 1.0h), Overlay};
  return tileColour(surface, light, capture);
}
fragment float4 group_capture_fragment(GroupVaryings in [[stage_in]],
                                       constant SurfaceLight &light [[buffer(0)]],
                                       constant uint &fractional [[buffer(1)]],
                                       texture2d<float> colours [[texture(0)]],
                                       texture2d<float> normals [[texture(1)]]) {
  return groupColour(in, light, fractional, colours, normals, true);
}
fragment float4 group_fragment(GroupVaryings in [[stage_in]],
                               constant SurfaceLight &light [[buffer(0)]],
                               constant uint &fractional [[buffer(1)]],
                               texture2d<float> colours [[texture(0)]],
                               texture2d<float> normals [[texture(1)]]) {
  return groupColour(in, light, fractional, colours, normals);
}
fragment TileFragment group_composite_fragment(GroupVaryings in [[stage_in]],
                                               constant SurfaceLight &light [[buffer(0)]],
                                               constant uint &fractional [[buffer(1)]],
                                               texture2d<float> colours [[texture(0)]],
                                               texture2d<float> normals [[texture(1)]]) {
  return {groupColour(in, light, fractional, colours, normals), in.position.z};
}
)";

}
#endif
