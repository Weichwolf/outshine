
vec3 normalFromMap(vec3 vertexNormal, vec4 tangent, vec3 tap, float scale,
                                   bool front) {
  vec3 n = normalize(vertexNormal);
  vec3 t = normalize(tangent.xyz - n * dot(n, tangent.xyz));
  vec3 b = cross(n, t) * tangent.w;
  vec3 scaled = vec3(tap.xy * scale, tap.z);
  vec3 mapped = normalize(t * scaled.x + b * scaled.y + n * scaled.z);
  return mapped * (front ? 1.0 : -1.0);
}
