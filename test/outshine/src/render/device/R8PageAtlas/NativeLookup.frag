#version 450
#extension GL_GOOGLE_include_directive : require
layout(std430, set = 0, binding = 0) readonly buffer Queries { vec4 queries[]; };
layout(set = 0, binding = 1) uniform sampler2D atlas;
layout(std430, set = 0, binding = 2) readonly buffer PageLookup {
  uvec4 header;
  uvec4 entries[];
};
uvec4 fieldPageEntry(uint index) { return entries[index]; }
#include "fieldPage.glsl"
layout(location = 0) out vec4 colour;
void main() {
  vec4 query = queries[uint(gl_FragCoord.x)];
  FieldPageLayout shape = FieldPageLayout(header.x, header.y, header.z, header.w);
  float value;
  ivec3 missing;
  bool found = fieldPageSample(atlas, shape, query.xy, uint(query.z), value, missing);
  colour = vec4(value, found ? 1.0 : 0.0, float(missing.z), 1.0);
}
