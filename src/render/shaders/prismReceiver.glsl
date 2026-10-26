vec3 prismTangentDirection(vec3 value) {
  float scale = max(max(abs(value.x), abs(value.y)), abs(value.z));
  return scale == 0.0 ? vec3(0.0) : value / scale;
}
vec4 prismReceiverPlane(mat3 lightFromWorld, mat3 worldFromLocal, vec3 localNormal) {
  vec3 first = localNormal.x != 0.0 ? worldFromLocal[1] : worldFromLocal[0];
  vec3 second = localNormal.z != 0.0 ? worldFromLocal[1] : worldFromLocal[2];
  first = prismTangentDirection(lightFromWorld * prismTangentDirection(first));
  second = prismTangentDirection(lightFromWorld * prismTangentDirection(second));
  vec3 normal = cross(first, second);
  if (normal.z == 0.0) { return vec4(0.0); }
  vec3 plane = vec3(-2.0 * normal.x, 2.0 * normal.y, normal.z);
  float scale = max(max(abs(plane.x), abs(plane.y)), abs(plane.z));
  return vec4(plane / (normal.z < 0.0 ? -scale : scale), 1.0);
}
float prismPlaneShadowVisibility(vec4 depths, vec2 phase, float receiver,
                                  vec4 plane, vec2 offset, vec2 stride) {
  vec4 visibility;
  if (plane.w == 0.0) {
    visibility = step(depths, vec4(receiver));
  } else {
    float low = dot(plane.xy, offset);
    vec2 change = plane.xy * stride;
    vec4 test = (vec4(receiver) - depths) * plane.z +
                vec4(low, low + change.x, low + change.y, low + change.x + change.y);
    visibility = step(vec4(0.0), test);
  }
  return mix(mix(visibility.x, visibility.y, phase.x),
             mix(visibility.z, visibility.w, phase.x), phase.y);
}
