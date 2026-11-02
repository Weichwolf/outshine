#ifndef OUTSHINE_GROUND_LITTER_PAGES_GLSL
#define OUTSHINE_GROUND_LITTER_PAGES_GLSL
#include "fieldFootprint.glsl"
#include "fieldPage.glsl"

bool groundLitterPagesObserved(sampler2D atlas, FieldPageLayout shape,
                       vec2 atCells, vec2 dxCells, vec2 dyCells,
                       uint observation, out vec4 sampled, out ivec3 missing) {
  sampled = vec4(0.0);
  missing = ivec3(0);
  FieldFootprint plan = fieldFootprint(dxCells, dyCells, 1.0, 16.0, -0.5, 5.0, 8u);
  if (plan.Count == 0u) { return true; }
  uint lower = uint(floor(plan.Lod));
  float fraction = fract(plan.Lod);
  float total = 0.0;
  uint mips = fraction > 0.0 ? 2u : 1u;
  uint selected = observation % (plan.Count * mips);
  ivec3 used = ivec3(0);
  for (uint tap = 0u; tap < plan.Count; ++tap) {
    float offset = (float(tap) + 0.5) / float(plan.Count) - 0.5;
    vec2 delta = offset * plan.Span * plan.Axis;
    float first;
    if (!fieldPageOffsetSample(atlas, shape, atCells, delta, lower, first, missing)) { return false; }
    if (selected == tap * mips) { used = missing; }
    if (fraction > 0.0) {
      float next;
      if (!fieldPageOffsetSample(atlas, shape, atCells, delta, lower + 1u, next, missing)) { return false; }
      if (selected == tap * mips + 1u) { used = missing; }
      first = mix(first, next, fraction);
    }
    total += first;
  }
  sampled = vec4((total / float(plan.Count) - 0.5) * plan.Fade, plan.Lod, float(plan.Count), plan.Fade);
  missing = used;
  return true;
}

bool groundLitterPages(sampler2D atlas, FieldPageLayout shape,
                       vec2 atCells, vec2 dxCells, vec2 dyCells,
                       out vec4 sampled, out ivec3 missing) {
  return groundLitterPagesObserved(atlas, shape, atCells, dxCells, dyCells, 0u, sampled, missing);
}
#endif
