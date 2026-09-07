vec4 fullscreenPosition() {
  const vec2 corners[3] = vec2[3](vec2(-1.0, -1.0), vec2(3.0, -1.0), vec2(-1.0, 3.0));
  return vec4(corners[gl_VertexIndex], 0.0, 1.0);
}
