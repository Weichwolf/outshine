float convexProxyHit(vec3 direction, vec4 planes[7], out uint surface) {
  float enter = 0.0;
  float leave = 3.402823e38;
  surface = 0u;
  for (uint i = 0u; i < 7u; ++i) {
    float facing = dot(direction, planes[i].xyz);
    float plane = planes[i].w;
    if (facing == 0.0) {
      if (plane < 0.0) { return 0.0; }
      continue;
    }
    float t = plane / facing;
    if (facing < 0.0) {
      if (t > enter) {
        enter = t;
        surface = i;
      }
    } else {
      leave = min(leave, t);
    }
  }
  return enter <= leave ? enter : 0.0;
}
