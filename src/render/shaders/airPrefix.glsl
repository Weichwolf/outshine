#ifndef OUTSHINE_AIR_PREFIX_GLSL
#define OUTSHINE_AIR_PREFIX_GLSL
#include "airCell.glsl"
#include "airSource.glsl"

#ifndef AIR_PREFIX_INTERVALS
#define AIR_PREFIX_INTERVALS 22
#endif
const uint kAirPrefixIntervals = uint(AIR_PREFIX_INTERVALS);

struct AirTransfer {
  vec3 RadiancePerLux;
  vec3 Transmittance;
};

struct AirPrefix {
  AirRay Ray;
  vec4 CutsA;
  vec4 CutsB;
  uint Grid;
  uint SourceEvaluations;
  float BeginKm;
  float EndKm;
  bool EndDiscontinuous;
  AirSource A;
  AirSource B;
  AirSource C;
  AirTransfer AtStart;
};

float airNextPhysical(AirPrefix prefix) {
  float nextKm = prefix.Ray.DistanceKm;
  for (int index = 0; index < 4; ++index) {
    if (prefix.CutsA[index] > prefix.BeginKm) { nextKm = min(nextKm, prefix.CutsA[index]); }
    if (prefix.CutsB[index] > prefix.BeginKm) { nextKm = min(nextKm, prefix.CutsB[index]); }
  }
  return nextKm;
}

void airPrefixPrepareCell(inout AirPrefix prefix, Medium medium, bool reuse,
                          sampler2D veil, sampler2D multiScatter) {
  float unit = float(prefix.Grid) / float(kAirPrefixIntervals);
  float gridKm = prefix.Ray.DistanceKm * unit * unit;
  float physicalKm = airNextPhysical(prefix);
  prefix.EndKm = min(gridKm, physicalKm);
  prefix.EndDiscontinuous = prefix.EndKm == prefix.CutsB.z || prefix.EndKm == prefix.CutsB.w;
  float middleKm = 0.5 * (prefix.BeginKm + prefix.EndKm);
  bool visible = airSunVisible(medium, prefix.Ray, middleKm);
  prefix.A = reuse ? prefix.C :
      airSourceAt(medium, prefix.Ray, prefix.BeginKm, visible, veil, multiScatter);
  prefix.B = airSourceAt(medium, prefix.Ray, middleKm, visible, veil, multiScatter);
  prefix.C = airSourceAt(medium, prefix.Ray, prefix.EndKm, visible, veil, multiScatter);
  prefix.SourceEvaluations += reuse ? 2u : 3u;
  prefix.B.Extinction = max(vec3(0.0), 2.0 * prefix.B.Extinction -
      0.5 * (prefix.A.Extinction + prefix.C.Extinction));
  prefix.B.Emission = max(vec3(0.0), 2.0 * prefix.B.Emission -
      0.5 * (prefix.A.Emission + prefix.C.Emission));
}

AirTransfer airPrefixPartial(AirPrefix prefix, float distanceKm) {
  AirTransfer result;
  for (int channel = 0; channel < 3; ++channel) {
    AirCellInput cell;
    cell.WidthKm = prefix.EndKm - prefix.BeginKm;
    cell.DistanceKm = distanceKm;
    cell.ExtinctionA = prefix.A.Extinction[channel];
    cell.ExtinctionB = prefix.B.Extinction[channel];
    cell.ExtinctionC = prefix.C.Extinction[channel];
    cell.SourceA = prefix.A.Emission[channel];
    cell.SourceB = prefix.B.Emission[channel];
    cell.SourceC = prefix.C.Emission[channel];
    AirCellChannel evaluated = airIntegrateCell(cell);
    result.RadiancePerLux[channel] = evaluated.RadiancePerLux;
    result.Transmittance[channel] = evaluated.Transmittance;
  }
  return result;
}

AirPrefix airPrefixBegin(Medium medium, AirRay ray, sampler2D veil, sampler2D multiScatter) {
  AirPrefix prefix;
  prefix.Ray = ray;
  float ozoneLowKm = medium.BottomRadiusKm + medium.OzoneCentreKm - medium.OzoneHalfWidthKm;
  float ozoneMidKm = medium.BottomRadiusKm + medium.OzoneCentreKm;
  float ozoneHighKm = medium.BottomRadiusKm + medium.OzoneCentreKm + medium.OzoneHalfWidthKm;
  prefix.CutsA = vec4(airShellRoots(ray, ozoneLowKm), airShellRoots(ray, ozoneMidKm));
  prefix.CutsB = vec4(airShellRoots(ray, ozoneHighKm), airShadowRoots(medium, ray));
  prefix.Grid = 1u;
  prefix.SourceEvaluations = 0u;
  prefix.BeginKm = 0.0;
  prefix.EndKm = 0.0;
  prefix.EndDiscontinuous = true;
  prefix.A = AirSource(vec3(0.0), vec3(0.0));
  prefix.B = prefix.A;
  prefix.C = prefix.A;
  prefix.AtStart = AirTransfer(vec3(0.0), vec3(1.0));
  if (ray.DistanceKm > 0.0) { airPrefixPrepareCell(prefix, medium, false, veil, multiScatter); }
  return prefix;
}

AirTransfer airPrefixAdvance(inout AirPrefix prefix, Medium medium, float distanceKm,
                             sampler2D veil, sampler2D multiScatter) {
  while (distanceKm >= prefix.EndKm && prefix.EndKm < prefix.Ray.DistanceKm) {
    AirTransfer cell = airPrefixPartial(prefix, prefix.EndKm - prefix.BeginKm);
    prefix.AtStart.RadiancePerLux += prefix.AtStart.Transmittance * cell.RadiancePerLux;
    prefix.AtStart.Transmittance *= cell.Transmittance;
    bool reuse = !prefix.EndDiscontinuous;
    prefix.BeginKm = prefix.EndKm;
    float unit = float(prefix.Grid) / float(kAirPrefixIntervals);
    if (prefix.BeginKm >= prefix.Ray.DistanceKm * unit * unit) { ++prefix.Grid; }
    airPrefixPrepareCell(prefix, medium, reuse, veil, multiScatter);
  }
  AirTransfer partial = airPrefixPartial(prefix, distanceKm - prefix.BeginKm);
  return AirTransfer(prefix.AtStart.RadiancePerLux +
      prefix.AtStart.Transmittance * partial.RadiancePerLux,
      prefix.AtStart.Transmittance * partial.Transmittance);
}
#endif
