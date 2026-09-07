layout(std140, set = 3, binding = DISPLAY_BINDING) uniform Display {
  float exposure;
  uint filmic;
  vec2 pad;
} display;

vec3 filmic(vec3 x) {
  return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), vec3(0.0), vec3(1.0));
}

vec4 displayed(vec4 scene, float sceneDepth) {
  float a = max(sceneDepth > 0.0 ? 1.0 : 0.0, clamp(scene.a, 0.0, 1.0));
  vec3 lit = scene.rgb * display.exposure;
  return vec4(display.filmic != 0u ? filmic(lit) : lit, a);
}
