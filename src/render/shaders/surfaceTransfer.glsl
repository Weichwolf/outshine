struct SurfaceTransfer {
  vec3 Source;
  vec3 Transmittance;
};

vec3 surfaceMediumTransmittance(vec3 colour, float thicknessM, float attenuationDistanceM) {
  if (thicknessM <= 0.0 || isinf(attenuationDistanceM)) { return vec3(1.0); }
  return exp(log(max(colour, vec3(1e-5))) * (thicknessM / attenuationDistanceM));
}

SurfaceTransfer coveredSurfaceTransfer(SurfaceTransfer surface, float coverage) {
  return SurfaceTransfer(surface.Source * coverage,
                         vec3(1.0 - coverage) + surface.Transmittance * coverage);
}

vec3 surfaceTransferBehind(SurfaceTransfer surface, vec3 behind) {
  return surface.Source + surface.Transmittance * behind;
}
