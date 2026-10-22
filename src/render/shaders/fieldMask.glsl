#ifndef OUTSHINE_FIELD_MASK_GLSL
#define OUTSHINE_FIELD_MASK_GLSL
layout(set = 2, binding = 12) uniform sampler2D fieldHeights;
layout(std140, set = 3, binding = 2) uniform FieldMaskParameters {
  vec4 originPitch;
  uvec4 shape;
  vec4 coverage;
} fieldMask;

bool fieldOccupied(vec3 local, uvec4 descriptor, float roofLevel) {
  if (descriptor.x > 3u || any(isnan(local)) || any(isinf(local))) { return false; }
  const uvec2 base = descriptor.zw;
  if (any(greaterThanEqual(base, fieldMask.shape.xy))) { return false; }
  const uvec2 extent = min(uvec2(fieldMask.shape.z), fieldMask.shape.xy - base);
  const vec3 relative = local - fieldMask.originPitch.xyz;
  const vec2 grid = relative.xz / fieldMask.originPitch.w;
  if (any(isnan(grid)) || any(isinf(grid)) || isnan(relative.y) || isinf(relative.y)) { return false; }
  const vec2 minimum = vec2(base), maximum = vec2(base + extent);
  const float inset = 0.5 * (1.0 - fieldMask.coverage.x);
  ivec2 cell;
  if (descriptor.x == 0u) {
    if (descriptor.y >= extent.x || grid.y < minimum.y || grid.y >= maximum.y) { return false; }
    const float phase = fract(grid.y);
    if (phase < inset || phase > 1.0 - inset) { return false; }
    cell = ivec2(int(base.x + descriptor.y), int(floor(grid.y)));
  } else if (descriptor.x == 1u) {
    if (descriptor.y >= extent.y || grid.x < minimum.x || grid.x >= maximum.x) { return false; }
    const float phase = fract(grid.x);
    if (phase < inset || phase > 1.0 - inset) { return false; }
    cell = ivec2(int(floor(grid.x)), int(base.y + descriptor.y));
  } else {
    if (any(lessThan(grid, minimum)) || any(greaterThanEqual(grid, maximum))) { return false; }
    const vec2 phase = fract(grid);
    if (any(lessThan(phase, vec2(inset))) || any(greaterThan(phase, vec2(1.0 - inset)))) { return false; }
    cell = ivec2(floor(grid));
  }
  const float height = texelFetch(fieldHeights, cell, 0).r;
  if (!(height > 0.0) || isnan(height) || isinf(height)) { return false; }
  if (descriptor.x == 2u) { return height == roofLevel; }
  if (descriptor.x == 3u) { return true; }
  return relative.y >= 0.0 && relative.y <= height;
}
#endif
