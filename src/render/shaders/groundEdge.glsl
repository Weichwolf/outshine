float groundHalfFootprint(vec2 differentialX, vec2 differentialY) {
  float east = abs(differentialX.x) + abs(differentialY.x);
  float north = abs(differentialX.y) + abs(differentialY.y);
  return 0.5f * sqrt(east * east + north * north);
}

float groundEdgeWeight(float distanceM, float reachM, float halfFootprintM) {
  if (reachM <= 0.0f) {
    if (halfFootprintM > 0.0f) {
      return clamp(0.5f + 0.5f * distanceM / halfFootprintM, 0.0f, 1.0f);
    }
    return distanceM > 0.0f ? 1.0f : (distanceM < 0.0f ? 0.0f : 0.5f);
  }
  float magnitude = abs(distanceM);
  float negative;
  if (magnitude + halfFootprintM <= reachM) {
    float u = 0.5f - 0.5f * magnitude / reachM;
    float radius = 0.5f * halfFootprintM / reachM;
    negative = u * u * (3.0f - 2.0f * u) + radius * radius * (1.0f - 2.0f * u);
  } else if (halfFootprintM > 0.0f && magnitude < reachM + halfFootprintM) {
    float edge = halfFootprintM - magnitude;
    float u = clamp(0.5f + 0.5f * edge / reachM, 0.0f, 1.0f);
    float area = edge >= reachM ? edge : 2.0f * reachM * u * u * u * (1.0f - 0.5f * u);
    negative = area / (2.0f * halfFootprintM);
  } else {
    negative = 0.0f;
  }
  return distanceM > 0.0f ? 1.0f - negative : negative;
}
