vec2 temporalHistoryUv(vec2 uv, vec2 motionNdc, vec2 jitterUv) {
  return vec2(uv.x - 0.5 * motionNdc.x + jitterUv.x,
              uv.y + 0.5 * motionNdc.y + jitterUv.y);
}
