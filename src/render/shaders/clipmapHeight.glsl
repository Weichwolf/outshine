layout(set = 0, binding = 0) uniform sampler2DArray clipmapHeights;

float clipmapNode(ivec2 node, ivec4 address) {
  ivec2 texel = ((node + address.xy) % address.z + address.z) % address.z;
  return texelFetch(clipmapHeights, ivec3(texel, address.w), 0).x;
}

float clipmapHeight(vec2 grid, ivec4 address) {
  ivec2 cell = ivec2(floor(grid));
  vec2 uv = fract(grid);
  float a = clipmapNode(cell, address);
  if (all(equal(uv, vec2(0.0)))) { return a; }
  float b = clipmapNode(cell + ivec2(1, 0), address);
  float c = clipmapNode(cell + ivec2(1, 1), address);
  float d = clipmapNode(cell + ivec2(0, 1), address);
  return uv.x >= uv.y ? (1.0 - uv.x) * a + (uv.x - uv.y) * b + uv.y * c
                       : (1.0 - uv.y) * a + (uv.y - uv.x) * d + uv.x * c;
}

float clipmapSurface(vec2 grid, uint level, vec4 morph) {
  ClipmapRing ring = clipmapRings[level];
  float fine = clipmapHeight(grid, ring.HeightAddress);
  if (ring.CentreStep.w == 0.0) { return fine; }
  float weight = clipmapMorph(grid, morph);
  if (weight == 0.0) { return fine; }
  vec2 parentGrid = 0.5 * grid + vec2(ring.Offsets.xy);
  float coarse = clipmapHeight(parentGrid, clipmapRings[level + 1u].HeightAddress);
  return mix(fine, coarse, weight);
}
