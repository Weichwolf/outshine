#ifndef OUTSHINE_WORLD_SKY_CLEARSUNCORE_H
#define OUTSHINE_WORLD_SKY_CLEARSUNCORE_H

MEDIUM_INLINE bool
mediumSunVisible(MEDIUM_ARG medium, MediumLook point, float groundLiftKm) SUN_NOEXCEPT {
  const float radiusKm = point.RadiusKm - groundLiftKm;
  const float heightKm = radiusKm - medium.BottomRadiusKm;
  if (heightKm < 0.0f) { return false; }
  if (point.CosZenith > 0.0f) { return true; }
  const float projection = radiusKm * point.CosZenith;
  return projection * projection < heightKm * (radiusKm + medium.BottomRadiusKm);
}

#endif
