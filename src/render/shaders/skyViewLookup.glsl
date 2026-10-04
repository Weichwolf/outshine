float skyLightViewCos(vec3 direction, vec3 up, vec3 toSun) {
  vec3 viewPlane = direction - up * dot(direction, up);
  vec3 sunPlane = toSun - up * dot(toSun, up);
  float squared = dot(viewPlane, viewPlane) * dot(sunPlane, sunPlane);
  return squared > 1.0e-10f
      ? clamp(dot(viewPlane, sunPlane) * inversesqrt(squared), -1.0f, 1.0f)
      : 1.0f;
}
