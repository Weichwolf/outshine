#version 450

layout(set = 2, binding = 0) uniform sampler2D colourMap;
layout(set = 2, binding = 1) uniform sampler2D normalMap;
layout(set = 2, binding = 2) uniform sampler2D metalRoughMap;
layout(set = 2, binding = 3) uniform sampler2D emissiveMap;
layout(set = 2, binding = 4) uniform sampler2D specularStrengthMap;
layout(set = 2, binding = 5) uniform sampler2D specularTintMap;
layout(set = 2, binding = 6) uniform sampler2D behindMap;
layout(set = 2, binding = 7) uniform sampler2D shadowMap;
layout(location = 0) out vec4 colour;

void main() {
  vec2 uv = gl_FragCoord.xy / vec2(64.0);
  colour = (texture(colourMap, uv) + texture(normalMap, uv) + texture(metalRoughMap, uv) +
            texture(emissiveMap, uv) + texture(specularStrengthMap, uv) +
            texture(specularTintMap, uv) + texture(behindMap, uv) + texture(shadowMap, uv)) /
           8.0;
}
