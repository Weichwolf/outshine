vec2 shadowReceiverSlope(vec3 dx, vec3 dy) {
  float determinant = dx.x * dy.y - dx.y * dy.x;
  return determinant == 0.0 ? vec2(0.0) :
      vec2(dx.z * dy.y - dy.z * dx.y, dy.z * dx.x - dx.z * dy.x) / determinant;
}

vec4 shadowReceiverDepths(float receiver, vec2 slope, vec2 offset, vec2 stride) {
  float low = receiver + dot(slope, offset);
  vec2 change = slope * stride;
  return vec4(low, low + change.x, low + change.y, low + change.x + change.y);
}

float bilinearShadowVisibility(vec4 depths, vec2 phase, vec4 receiverDepths) {
  return mix(mix(step(depths.x, receiverDepths.x), step(depths.y, receiverDepths.y), phase.x),
             mix(step(depths.z, receiverDepths.z), step(depths.w, receiverDepths.w), phase.x), phase.y);
}

float bilinearShadowVisibility(vec4 depths, vec2 phase, float receiverDepth) {
  return bilinearShadowVisibility(depths, phase, vec4(receiverDepth));
}
