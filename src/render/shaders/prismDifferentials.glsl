void prismPositionDifferentials(mat4 localFromClip, vec3 point, vec3 localNormal,
                                vec3 u, vec3 v, float clipW, vec2 viewportSize,
                                out vec3 dx, out vec3 dy) {
  vec3 r = localFromClip[2].xyz - point * localFromClip[2].w;
  vec3 bx = localFromClip[0].xyz - point * localFromClip[0].w;
  vec3 by = localFromClip[1].xyz - point * localFromClip[1].w;
  float denominator = dot(localNormal, r);
  dx = clipW * (bx - r * (dot(localNormal, bx) / denominator)) * (2.0 / viewportSize.x);
  dy = clipW * (by - r * (dot(localNormal, by) / denominator)) * (-2.0 / viewportSize.y);
  dx = u * dot(dx, u) + v * dot(dx, v);
  dy = u * dot(dy, u) + v * dot(dy, v);
}
