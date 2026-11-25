float facadeStoreyBand(float openingHeight, float storey, float width, float storeysPerM) {
  float fraction = min(kStoreyBandHeightM * max(storeysPerM, 0.0f), 1.0f - openingHeight);
  return periodicBand(
      fract(storey) + 0.5f, 0.5f * (1.0f - fraction), 0.5f * (1.0f + fraction), width);
}

FacadeGlazing
facadeBandedWindow(float opening, float pane, float band, vec3 albedo, float roughness) {
  FacadeGlazing result = facadeWindow(opening, pane, albedo, roughness);
  result.albedo = result.albedo - albedo * (kStoreyBandDarkening * clamp(band, 0.0f, 1.0f - opening));
  return result;
}
