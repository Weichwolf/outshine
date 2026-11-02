#version 450
#extension GL_GOOGLE_include_directive : require

struct TextureQuery {
  vec4 PositionDx;
  vec4 DyScaleBias;
  vec4 Control;
};

layout(std430, set = 0, binding = 0) readonly buffer Inputs {
  TextureQuery queries[];
};

layout(set = 0, binding = 1) uniform sampler2D preparedField;

layout(std140, set = 2, binding = 0) uniform Pushed {
  uint count;
  uint first;
  uint width;
  uint unused;
};

#include "terrain_litter_sampling.glsl"
layout(location = 0) out vec4 sampled;
layout(location = 1) out vec4 footprint;

void main() {
  uint index = uint(gl_FragCoord.x);
  if (index >= count) { discard; }
  TextureQuery query = queries[index];
  if (query.Control.y == 0.0 || query.Control.y == 2.0) {
    sampled = vec4(
        litterTexel(preparedField, query.PositionDx.xy, query.Control.x, query.Control.y == 2.0),
        query.Control.x,
        1.0,
        1.0);
    footprint = vec4(0.0);
    return;
  }
  LitterSample result = sampleLitterTexture(preparedField,
                                            query.PositionDx.xy,
                                            query.PositionDx.zw,
                                            query.DyScaleBias.xy,
                                            query.DyScaleBias.z,
                                            query.DyScaleBias.w,
                                            uint(query.Control.z),
                                            query.Control.y == 3.0);
  sampled = result.ValueLodCountFade;
  footprint = result.LengthsSpanFootprint;
}
