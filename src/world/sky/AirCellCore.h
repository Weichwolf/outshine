#ifndef OUTSHINE_WORLD_SKY_AIRCELLCORE_H
#define OUTSHINE_WORLD_SKY_AIRCELLCORE_H

AIR_INLINE float airCellValue(float a, float b, float c, float u) AIR_NOEXCEPT {
  const float complement = 1.0f - u;
  return a * complement * complement + 2.0f * b * u * complement + c * u * u;
}

AIR_INLINE float airCellDepth(float a, float b, float c, float u, float width) AIR_NOEXCEPT {
  const float square = u * u;
  const float cube = square * u;
  return width *
         (a * (u - square + cube / 3.0f) + b * (square - 2.0f * cube / 3.0f) + c * cube / 3.0f);
}

AIR_INLINE float airCellOpacity(float depth) AIR_NOEXCEPT {
  if (depth < kAirCellThinDepth) {
    return depth * (1.0f + depth * (-0.5f + depth * (kAirCellInverseFactorial3 +
                                                     depth * (-kAirCellInverseFactorial4 +
                                                              depth * kAirCellInverseFactorial5))));
  }
  return 1.0f - exp(-depth);
}

AIR_INLINE float airCellNode(int index) AIR_NOEXCEPT {
  if (index == 0) { return kAirCellGaussOuterNode; }
  if (index == 1) { return kAirCellGaussInnerNode; }
  if (index == 2) { return 1.0f - kAirCellGaussInnerNode; }
  return 1.0f - kAirCellGaussOuterNode;
}

AIR_INLINE float airCellWeight(int index) AIR_NOEXCEPT {
  return index == 0 || index == 3 ? kAirCellGaussOuterWeight : kAirCellGaussInnerWeight;
}

AIR_INLINE AirCellChannel airIntegrateThinCell(AirCellInput cell, float depth) AIR_NOEXCEPT {
  const float fraction = cell.DistanceKm / cell.WidthKm;
  const float complement = 1.0f - fraction;
  const float extinctionA = cell.ExtinctionA;
  const float extinctionB = cell.ExtinctionA * complement + cell.ExtinctionB * fraction;
  const float extinctionC =
      airCellValue(cell.ExtinctionA, cell.ExtinctionB, cell.ExtinctionC, fraction);
  const float sourceA = cell.SourceA;
  const float sourceB = cell.SourceA * complement + cell.SourceB * fraction;
  const float sourceC = airCellValue(cell.SourceA, cell.SourceB, cell.SourceC, fraction);
  const float tauA = cell.DistanceKm * extinctionA;
  const float tauB = cell.DistanceKm * extinctionB;
  const float tauC = cell.DistanceKm * extinctionC;
  const float integral = cell.DistanceKm / 3.0f * sourceA + cell.DistanceKm / 3.0f * sourceB +
                         cell.DistanceKm / 3.0f * sourceC;
  const float moment =
      cell.DistanceKm * kAirCellMomentScale *
      (sourceA * (10 * tauA + 4 * tauB + tauC) + sourceB * (16 * tauA + 10 * tauB + 4 * tauC) +
       sourceC * (kAirCellMomentFarNear * tauA + 16 * tauB + 10 * tauC));
  const float opacityScale = depth > 0.0f ? airCellOpacity(depth) / depth : 1.0f;
  AirCellChannel result;
  result.RadiancePerLux = opacityScale * (integral - moment) / (1.0f - 0.5f * depth);
  result.Transmittance = exp(-depth);
  return result;
}

AIR_INLINE AirCellChannel airIntegrateCell(AirCellInput cell) AIR_NOEXCEPT {
  AirCellChannel result;
  result.RadiancePerLux = 0.0f;
  result.Transmittance = 1.0f;
  const float width = cell.WidthKm;
  const float distance = cell.DistanceKm;
  if (distance <= 0.0f) { return result; }
  const float fraction = distance / width;
  const float depth =
      airCellDepth(cell.ExtinctionA, cell.ExtinctionB, cell.ExtinctionC, fraction, width);
  if (depth <= kAirCellMomentDepth) { return airIntegrateThinCell(cell, depth); }
  const float firstDepth = airCellDepth(
      cell.ExtinctionA, cell.ExtinctionB, cell.ExtinctionC, fraction * airCellNode(0), width);
  float numerator = 0.0f;
  float denominator = 0.0f;
  for (int index = 0; index < 4; ++index) {
    const float u = fraction * airCellNode(index);
    const float sampleDepth =
        airCellDepth(cell.ExtinctionA, cell.ExtinctionB, cell.ExtinctionC, u, width);
    const float weighted = airCellWeight(index) * exp(-max(0.0f, sampleDepth - firstDepth));
    numerator += weighted * airCellValue(cell.SourceA, cell.SourceB, cell.SourceC, u);
    denominator += weighted * airCellValue(cell.ExtinctionA, cell.ExtinctionB, cell.ExtinctionC, u);
  }
  const float scale = denominator > 0.0f ? airCellOpacity(depth) / denominator : distance;
  result.RadiancePerLux = numerator * scale;
  result.Transmittance = exp(-depth);
  return result;
}

#endif
