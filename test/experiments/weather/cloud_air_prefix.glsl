#ifndef OUTSHINE_CLOUD_AIR_PREFIX_GLSL
#define OUTSHINE_CLOUD_AIR_PREFIX_GLSL

#include "mediumCell.glsl"
#include "cloudPrefix.glsl"

struct CloudAirPrefix {
  AirPrefix Clear;
  CloudPrefix Cloud;
  float ClearBeginM;
  float BeginM;
  float EndM;
  bool HasAir;
  bool HasCloud;
  AirTransfer AtStart;
  uint CellsVisited;
};

void cloudAirPrefixPrepare(inout CloudAirPrefix prefix) {
  float extentM = prefix.Cloud.ExtentM;
  float clearEndM = prefix.ClearBeginM + prefix.Clear.Ray.DistanceKm * 1000.0;
  prefix.HasAir = prefix.BeginM >= prefix.ClearBeginM && prefix.BeginM < clearEndM;
  float airEndM = prefix.BeginM < prefix.ClearBeginM ? prefix.ClearBeginM :
      (prefix.HasAir ? prefix.ClearBeginM + prefix.Clear.EndKm * 1000.0 : extentM);
  prefix.HasCloud = prefix.BeginM >= prefix.Cloud.BeginM && prefix.BeginM < prefix.Cloud.EndM;
  float cloudEndM = prefix.BeginM < prefix.Cloud.BeginM ? prefix.Cloud.BeginM : prefix.Cloud.EndM;
  prefix.EndM = min(extentM, min(airEndM, cloudEndM));
}

CloudAirPrefix cloudAirPrefixBegin(CloudLayer layer, vec3 originM, vec3 direction,
                                   float physicalExtentM, CloudLight light,
                                   AirPrefix clear, float clearBeginM) {
  CloudAirPrefix prefix;
  prefix.Clear = clear;
  prefix.Cloud = cloudPrefixBegin(layer, originM, direction, physicalExtentM, light);
  prefix.ClearBeginM = clearBeginM;
  prefix.BeginM = 0.0;
  prefix.AtStart = AirTransfer(vec3(0.0), vec3(1.0));
  prefix.CellsVisited = 0u;
  cloudAirPrefixPrepare(prefix);
  return prefix;
}

AirTransfer cloudAirPrefixPartial(inout CloudAirPrefix prefix, float queryM) {
  if (queryM <= prefix.BeginM) { return AirTransfer(vec3(0.0), vec3(1.0)); }
  if (prefix.HasCloud) { cloudPrefixEvaluate(prefix.Cloud); }
  float extinction = prefix.HasCloud ? prefix.Cloud.ExtinctionPerKm : 0.0;
  vec3 emission = prefix.HasCloud ? prefix.Cloud.EmissionPerKmPerLux : vec3(0.0);
  return mediumCellIntegrate(prefix.Clear, prefix.HasAir, prefix.ClearBeginM,
      prefix.BeginM, prefix.EndM, queryM, extinction, emission);
}

AirTransfer cloudAirPrefixAdvance(inout CloudAirPrefix prefix, float queryM,
                                  Medium medium, sampler2D veil, sampler2D multiScatter) {
  float query = clamp(queryM, 0.0, prefix.Cloud.ExtentM);
  if (prefix.Cloud.Layer.field.x <= 0.0) {
    float clearKm = clamp((query - prefix.ClearBeginM) * 0.001, 0.0, prefix.Clear.Ray.DistanceKm);
    return airPrefixAdvance(prefix.Clear, medium, clearKm, veil, multiScatter);
  }
  for (int step = 0; step < 2 * CLOUD_VIEW_SAMPLES + AIR_PREFIX_INTERVALS + 14; ++step) {
    if (query < prefix.EndM || prefix.EndM >= prefix.Cloud.ExtentM) { break; }
    AirTransfer cell = cloudAirPrefixPartial(prefix, prefix.EndM);
    prefix.AtStart.RadiancePerLux += prefix.AtStart.Transmittance * cell.RadiancePerLux;
    prefix.AtStart.Transmittance *= cell.Transmittance;
    ++prefix.CellsVisited;
    prefix.BeginM = prefix.EndM;
    if (prefix.HasAir && prefix.BeginM >= prefix.ClearBeginM + prefix.Clear.EndKm * 1000.0 &&
        prefix.Clear.EndKm < prefix.Clear.Ray.DistanceKm) {
      airPrefixAdvance(prefix.Clear, medium, prefix.Clear.EndKm, veil, multiScatter);
    }
    if (prefix.BeginM >= prefix.Cloud.EndM) { cloudPrefixNext(prefix.Cloud); }
    cloudAirPrefixPrepare(prefix);
  }
  AirTransfer partial = cloudAirPrefixPartial(prefix, query);
  return AirTransfer(prefix.AtStart.RadiancePerLux +
      prefix.AtStart.Transmittance * partial.RadiancePerLux,
      prefix.AtStart.Transmittance * partial.Transmittance);
}

#endif
