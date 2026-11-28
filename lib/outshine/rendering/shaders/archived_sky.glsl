vec3 archivedSky(sampler2D radiance, vec3 direction) {
  float longitude = dot(direction.xy, direction.xy) > 1.e-10
      ? atan(direction.y, direction.x) : 0.;
  vec2 uv = vec2(.5 - longitude / (2. * kPi),
                 .5 + asin(clamp(direction.z, -1., 1.)) / kPi);
  return textureLod(radiance, uv, 0.).rgb;
}
