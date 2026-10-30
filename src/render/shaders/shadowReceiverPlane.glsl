vec2 shadowReceiverPlaneSlope(vec4 receiver, vec4 directionX, vec4 directionY) {
  float w = max(1.0e-6f, receiver.w);
  vec3 dx = vec3(directionX.x - receiver.x / w * directionX.w,
                  directionX.y - receiver.y / w * directionX.w,
                  directionX.z - receiver.z / w * directionX.w);
  vec3 dy = vec3(directionY.x - receiver.x / w * directionY.w,
                  directionY.y - receiver.y / w * directionY.w,
                  directionY.z - receiver.z / w * directionY.w);
  return shadowReceiverSlope(shadowReceiverVector(dx), shadowReceiverVector(dy));
}
