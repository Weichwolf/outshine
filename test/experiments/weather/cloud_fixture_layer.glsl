#ifndef OUTSHINE_CLOUD_FIXTURE_LAYER_GLSL
#define OUTSHINE_CLOUD_FIXTURE_LAYER_GLSL

#include "cloudLayer.glsl"

struct CloudFixtureInput {
  vec4 Shell;
  vec4 Field;
  vec4 Drift;
};

CloudLayer cloudFixtureLayer(vec4 shell, vec4 field, vec4 drift) {
  vec3 broad = -drift.xyz / 1400.0;
  vec3 detail = broad * 3.1;
  return CloudLayer(shell,
                    field,
                    vec4(0.0),
                    uvec4(ivec3(floor(broad)), 0),
                    vec4(fract(broad), 0.0),
                    uvec4(ivec3(floor(detail)), 0),
                    vec4(fract(detail), 0.0));
}

#endif
