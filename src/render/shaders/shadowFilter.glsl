float bilinearShadowVisibility(vec4 depths, vec2 phase, float receiverDepth) {
  return mix(mix(step(depths.x, receiverDepth), step(depths.y, receiverDepth), phase.x),
             mix(step(depths.z, receiverDepth), step(depths.w, receiverDepth), phase.x), phase.y);
}
