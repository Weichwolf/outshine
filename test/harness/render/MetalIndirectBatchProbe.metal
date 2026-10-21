#include <metal_stdlib>
using namespace metal;

struct Vertex {
  float4 Position [[position]];
  float4 Color;
};

vertex Vertex vertex_main(uint vertexId [[vertex_id]], uint instance [[instance_id]]) {
  constexpr float2 corners[3] = {float2(0, 0), float2(8, 0), float2(0, 8)};
  const float2 cell = float2(instance % 64, instance / 64) * 12.0f + 1.0f;
  const float2 pixel = cell + corners[vertexId];
  const float2 ndc = pixel / float2(1280, 720) * 2.0f - 1.0f;
  return {float4(ndc, 0.5f, 1), float4(float3(0.2f, 0.4f, 0.6f) + float(instance % 4) * 0.05f, 1)};
}

fragment float4 fragment_main(Vertex input [[stage_in]]) {
  return input.Color;
}
