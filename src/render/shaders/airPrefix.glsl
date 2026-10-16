#ifndef OUTSHINE_AIR_PREFIX_GLSL
#define OUTSHINE_AIR_PREFIX_GLSL
#include "airCell.glsl"
#include "airSource.glsl"

#ifndef AIR_PREFIX_INTERVALS
#define AIR_PREFIX_INTERVALS 22
#endif
#ifndef AIR_PREFIX_ACCUMULATE_DEPTH
#define AIR_PREFIX_ACCUMULATE_DEPTH 0
#endif
#if AIR_PREFIX_INTERVALS < 1
#error AIR_PREFIX_INTERVALS must be positive
#endif
#ifndef AIR_SOURCE_PROVIDER
#define AIR_SOURCE_PROVIDER airSourceForRay
#define OUTSHINE_AIR_PREFIX_CLEAR_SOURCE
#endif
#ifndef AIR_PREFIX_GRID_PROVIDER
#define AIR_PREFIX_GRID_PROVIDER airQuadraticGridDistance
#endif
const uint kAirPrefixIntervals = uint(AIR_PREFIX_INTERVALS);
const uint kAirPrefixPhysicalCuts = 8u;

float airQuadraticGridDistance(AirRay ray, uint index) {
  float fraction = float(index) / float(kAirPrefixIntervals);
  return ray.DistanceKm * fraction * fraction;
}

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
#if AIR_PREFIX_ACCUMULATE_DEPTH
  vec3 Depth;
#endif
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
                          sampler2D veil, sampler2D multiScatter, AirSourceRay sourceRay) {
  float gridKm = AIR_PREFIX_GRID_PROVIDER(prefix.Ray, prefix.Grid);
  float physicalKm = airNextPhysical(prefix);
  prefix.EndKm = min(gridKm, physicalKm);
  prefix.EndDiscontinuous = prefix.EndKm == prefix.CutsB.z || prefix.EndKm == prefix.CutsB.w;
  float middleKm = 0.5 * (prefix.BeginKm + prefix.EndKm);
  bool visible = airSunVisible(medium, prefix.Ray, middleKm);
  prefix.A = reuse ? prefix.C :
      AIR_SOURCE_PROVIDER(medium, prefix.Ray, prefix.BeginKm, visible, veil, multiScatter, sourceRay);
  prefix.B = AIR_SOURCE_PROVIDER(medium, prefix.Ray, middleKm, visible, veil, multiScatter, sourceRay);
  prefix.C = AIR_SOURCE_PROVIDER(medium, prefix.Ray, prefix.EndKm, visible, veil, multiScatter, sourceRay);
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

#if AIR_PREFIX_ACCUMULATE_DEPTH
vec3 airPrefixDepth(AirPrefix prefix, float distanceKm) {
  float widthKm = prefix.EndKm - prefix.BeginKm;
  if (distanceKm <= 0.0 || widthKm <= 0.0) { return vec3(0.0); }
  float u = distanceKm / widthKm;
  return vec3(
      airCellDepth(prefix.A.Extinction.r, prefix.B.Extinction.r, prefix.C.Extinction.r, u, widthKm),
      airCellDepth(prefix.A.Extinction.g, prefix.B.Extinction.g, prefix.C.Extinction.g, u, widthKm),
      airCellDepth(prefix.A.Extinction.b, prefix.B.Extinction.b, prefix.C.Extinction.b, u, widthKm));
}
#endif

AirPrefix airPrefixBegin(Medium medium, AirRay ray, sampler2D veil, sampler2D multiScatter,
                         AirSourceRay sourceRay) {
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
#if AIR_PREFIX_ACCUMULATE_DEPTH
  prefix.Depth = vec3(0.0);
#endif
  if (ray.DistanceKm > 0.0) { airPrefixPrepareCell(prefix, medium, false, veil, multiScatter, sourceRay); }
  return prefix;
}

#ifdef OUTSHINE_AIR_PREFIX_CLEAR_SOURCE
AirPrefix airPrefixBegin(Medium medium, AirRay ray, sampler2D veil, sampler2D multiScatter) {
  return airPrefixBegin(medium, ray, veil, multiScatter, AirSourceRay(vec3(0.0), vec3(0.0)));
}
#endif

AirTransfer airPrefixAdvance(inout AirPrefix prefix, Medium medium, float distanceKm,
                             sampler2D veil, sampler2D multiScatter, AirSourceRay sourceRay) {
  for (uint step = 0u;
       step < kAirPrefixIntervals + kAirPrefixPhysicalCuts + 1u &&
           distanceKm >= prefix.EndKm && prefix.EndKm < prefix.Ray.DistanceKm;
       ++step) {
    AirTransfer cell = airPrefixPartial(prefix, prefix.EndKm - prefix.BeginKm);
    prefix.AtStart.RadiancePerLux += prefix.AtStart.Transmittance * cell.RadiancePerLux;
#if AIR_PREFIX_ACCUMULATE_DEPTH
    prefix.Depth += airPrefixDepth(prefix, prefix.EndKm - prefix.BeginKm);
    prefix.AtStart.Transmittance = exp(-prefix.Depth);
#else
    prefix.AtStart.Transmittance *= cell.Transmittance;
#endif
    bool reuse = !prefix.EndDiscontinuous;
    prefix.BeginKm = prefix.EndKm;
    if (prefix.BeginKm >= AIR_PREFIX_GRID_PROVIDER(prefix.Ray, prefix.Grid)) { ++prefix.Grid; }
    airPrefixPrepareCell(prefix, medium, reuse, veil, multiScatter, sourceRay);
  }
  AirTransfer partial = airPrefixPartial(prefix, distanceKm - prefix.BeginKm);
#if AIR_PREFIX_ACCUMULATE_DEPTH
  vec3 transmittance = exp(-prefix.Depth - airPrefixDepth(prefix, distanceKm - prefix.BeginKm));
#else
  vec3 transmittance = prefix.AtStart.Transmittance * partial.Transmittance;
#endif
  return AirTransfer(prefix.AtStart.RadiancePerLux +
      prefix.AtStart.Transmittance * partial.RadiancePerLux,
      transmittance);
}
#ifdef OUTSHINE_AIR_PREFIX_CLEAR_SOURCE
AirTransfer airPrefixAdvance(inout AirPrefix prefix, Medium medium, float distanceKm,
                             sampler2D veil, sampler2D multiScatter) {
  return airPrefixAdvance(prefix, medium, distanceKm, veil, multiScatter,
      AirSourceRay(vec3(0.0), vec3(0.0)));
}
#endif
#endif
