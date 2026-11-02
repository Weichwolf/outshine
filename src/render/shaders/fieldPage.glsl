#ifndef OUTSHINE_FIELD_PAGE_GLSL
#define OUTSHINE_FIELD_PAGE_GLSL
#include "surfaceHash.glsl"

struct FieldPageLayout {
  uint CoreTexels;
  uint Columns;
  uint Entries;
  uint Probes;
};

bool fieldPageSlot(ivec3 key, FieldPageLayout shape, out uint slot) {
  uint first = surfaceHash(key);
  for (uint probe = 0u; probe < shape.Probes; ++probe) {
    uvec4 entry = fieldPageEntry((first + probe) & (shape.Entries - 1u));
    if (entry.w == 0u) { return false; }
    if (all(equal(ivec3(entry.xyz), key))) {
      slot = entry.w - 1u;
      return true;
    }
  }
  return false;
}

bool fieldPageOffsetSample(sampler2D atlas, FieldPageLayout shape,
                           vec2 at, vec2 delta, uint mip, out float value, out ivec3 missing) {
  value = 0.0;
  float texelsPerCell = 16.0 * exp2(-float(mip));
  vec2 texel = at * texelsPerCell;
  ivec2 tile = ivec2(floor(texel / float(shape.CoreTexels)));
  vec2 local = texel - vec2(tile) * float(shape.CoreTexels) + delta * texelsPerCell;
  ivec2 shift = ivec2(floor(local / float(shape.CoreTexels)));
  tile += shift;
  uint slot;
  missing = ivec3(tile, int(mip));
  if (!fieldPageSlot(missing, shape, slot)) { return false; }
  vec2 offset = local - vec2(shift) * float(shape.CoreTexels) + 1.0;
  uvec2 origin = uvec2(slot % shape.Columns, slot / shape.Columns) * (shape.CoreTexels + 2u);
  vec2 uv = (vec2(origin) + offset) / vec2(textureSize(atlas, 0));
  value = textureLod(atlas, uv, 0.0).r;
  return true;
}

bool fieldPageSample(sampler2D atlas, FieldPageLayout shape,
                     vec2 at, uint mip, out float value, out ivec3 missing) {
  return fieldPageOffsetSample(atlas, shape, at, vec2(0.0), mip, value, missing);
}
#endif
