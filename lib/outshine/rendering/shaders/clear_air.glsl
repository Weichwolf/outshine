Medium referenceAir(float aerosolExtinction) {
  return Medium(6360.0, 6460.0, 8.0, 1.2, vec3(.005802, .013558, .033100), .9 * aerosolExtinction,
      vec3(.000650, .001881, .000085), aerosolExtinction, 25.0, 15.0, .8, 0.0, vec3(.10, .13, .07), 0.0);
}

vec3 segmentTransmission(sampler2D veil, Medium air, float startKm, float vertical, float distanceKm) {
  float endKm = max(air.BottomRadiusKm,
      length(vec2(startKm + distanceKm * vertical, distanceKm * sqrt(max(0.0, 1.0 - vertical * vertical)))));
  float endVertical = clamp((startKm * vertical + distanceKm) / endKm, -1.0, 1.0);
  bool aboveGround = mediumGroundReach(air, startKm, vertical) < 0.0;
  float signOfRay = aboveGround ? 1.0 : -1.0;
  ivec2 extent = textureSize(veil, 0);
  vec3 startT = textureLod(veil, mediumTransmittanceUv(air, MediumLook(startKm, signOfRay * vertical), extent), 0.0).rgb;
  vec3 endT = textureLod(veil, mediumTransmittanceUv(air, MediumLook(endKm, signOfRay * endVertical), extent), 0.0).rgb;
  // Both values can be very dark at the horizon even for a short surface segment.
  return clamp(aboveGround ? startT / max(endT, vec3(1e-30)) : endT / max(startT, vec3(1e-30)), 0.0, 1.0);
}
