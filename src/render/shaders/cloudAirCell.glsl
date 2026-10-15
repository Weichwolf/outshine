#ifndef OUTSHINE_CLOUD_AIR_CELL_GLSL
#define OUTSHINE_CLOUD_AIR_CELL_GLSL

#include "airPrefix.glsl"

vec3 cloudAirRestricted(vec3 coefficients, float begin, float end) {
  float nearA = mix(coefficients.x, coefficients.y, begin);
  float nearB = mix(coefficients.y, coefficients.z, begin);
  float farA = mix(coefficients.x, coefficients.y, end);
  float farB = mix(coefficients.y, coefficients.z, end);
  return vec3(mix(nearA, nearB, begin), mix(nearA, nearB, end), mix(farA, farB, end));
}

AirTransfer cloudAirCell(AirPrefix clear, bool hasAir, float clearBeginM,
                         float beginM, float endM, float queryM,
                         float cloudExtinctionPerKm, vec3 cloudEmissionPerKmPerLux) {
  AirTransfer result = AirTransfer(vec3(0.0), vec3(1.0));
  if (endM <= beginM || queryM <= beginM) { return result; }
  float begin = 0.0;
  float end = 0.0;
  if (hasAir) {
    float widthKm = clear.EndKm - clear.BeginKm;
    begin = clamp(((beginM - clearBeginM) * 0.001 - clear.BeginKm) / widthKm, 0.0, 1.0);
    end = clamp(((endM - clearBeginM) * 0.001 - clear.BeginKm) / widthKm, begin, 1.0);
  }
  for (int channel = 0; channel < 3; ++channel) {
    vec3 extinction = vec3(0.0);
    vec3 emission = vec3(0.0);
    if (hasAir) {
      extinction = cloudAirRestricted(vec3(clear.A.Extinction[channel],
          clear.B.Extinction[channel], clear.C.Extinction[channel]), begin, end);
      emission = cloudAirRestricted(vec3(clear.A.Emission[channel],
          clear.B.Emission[channel], clear.C.Emission[channel]), begin, end);
    }
    extinction += cloudExtinctionPerKm;
    emission += cloudEmissionPerKmPerLux[channel];
    AirCellInput cell;
    cell.WidthKm = (endM - beginM) * 0.001;
    cell.DistanceKm = (min(queryM, endM) - beginM) * 0.001;
    cell.ExtinctionA = extinction.x;
    cell.ExtinctionB = extinction.y;
    cell.ExtinctionC = extinction.z;
    cell.SourceA = emission.x;
    cell.SourceB = emission.y;
    cell.SourceC = emission.z;
    AirCellChannel evaluated = airIntegrateCell(cell);
    result.RadiancePerLux[channel] = evaluated.RadiancePerLux;
    result.Transmittance[channel] = evaluated.Transmittance;
  }
  return result;
}

#endif
