layout(location = 0) in vec2 grid;
#include "clipmapRings.glsl"
#include "clipmapHeight.glsl"

struct ClipmapPoint { vec3 Position; vec3 World; };

ClipmapPoint clipmapPoint(uint level) {
  ClipmapRing ring = clipmapRings[level];
  float height = clipmapSurface(grid, level, ring.Morph);
  vec2 relative = ring.RelativeCentres.xy + grid * ring.CentreStep.z;
  vec2 world = ring.CentreStep.xy + grid * ring.CentreStep.z;
  return ClipmapPoint(vec3(relative.x, height + s.shift.y, -relative.y),
                       vec3(world.x, height, world.y));
}
