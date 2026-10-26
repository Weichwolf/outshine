#ifndef OUTSHINE_EXPERIMENT_LINEAR_MEDIUM_SOURCE_GLSL
#define OUTSHINE_EXPERIMENT_LINEAR_MEDIUM_SOURCE_GLSL

#ifndef SOURCE_LINEAR_UNSHADED
#define SOURCE_LINEAR_UNSHADED 0
#endif

struct LinearMediumPrefix {
  DepthSourcePrefix Source;
  CloudPrefix Cloud;
  vec3 Radiance;
  float CloudDepth;
  float ClearBeginM;
  float ClearEndM;
  float ClearLengthKm;
  float BeginM;
  float EndM;
  bool HasAir;
  bool HasCloud;
  bool EndsAir;
  bool EndsCloud;
  bool Valid;
  uint Cells;
};

void linearMediumPrepare(inout LinearMediumPrefix prefix) {
  float extentM = prefix.Cloud.ExtentM;
  prefix.HasAir = prefix.BeginM >= prefix.ClearBeginM && prefix.BeginM < prefix.ClearEndM;
  float sourceEnd = prefix.Source.EndKm >= prefix.ClearLengthKm ?
      prefix.ClearEndM : prefix.ClearBeginM + prefix.Source.EndKm * 1000.0;
  float airEnd = prefix.BeginM < prefix.ClearBeginM ? prefix.ClearBeginM :
      (prefix.HasAir ? sourceEnd : extentM);
  prefix.HasCloud = prefix.BeginM >= prefix.Cloud.BeginM && prefix.BeginM < prefix.Cloud.EndM;
  float cloudEnd = prefix.BeginM < prefix.Cloud.BeginM ? prefix.Cloud.BeginM : prefix.Cloud.EndM;
  prefix.EndM = min(extentM, min(airEnd, cloudEnd));
  prefix.EndsAir = prefix.HasAir && prefix.EndM == airEnd;
  prefix.EndsCloud = prefix.HasCloud && prefix.EndM == cloudEnd;
}

float linearMediumCloudDepth(LinearMediumPrefix prefix, float queryM) {
  return prefix.CloudDepth + prefix.Cloud.ExtinctionPerKm *
      clamp(queryM - prefix.Cloud.BeginM, 0.0, prefix.Cloud.EndM - prefix.Cloud.BeginM) * 0.001;
}

vec3 linearMediumThrough(LinearMediumPrefix prefix, vec3 airSigma, float queryM) {
  float airKm = clamp(queryM - prefix.ClearBeginM, 0.0,
      prefix.ClearEndM - prefix.ClearBeginM) * 0.001;
  return exp(-airSigma * airKm - linearMediumCloudDepth(prefix, queryM));
}

vec3 linearMediumPartial(inout LinearMediumPrefix prefix, AirSourceTerms terms, float queryM) {
  if (queryM <= prefix.BeginM || prefix.EndM <= prefix.BeginM) { return vec3(0.0); }
  if (prefix.HasCloud) { cloudPrefixEvaluate(prefix.Cloud); }
  float cloudSigma = prefix.HasCloud ? prefix.Cloud.ExtinctionPerKm : 0.0;
  vec3 sourceA = prefix.HasCloud ? prefix.Cloud.EmissionPerKmPerLux : vec3(0.0);
  vec3 sourceB = sourceA;
  if (prefix.HasAir) {
    float sourceWidthKm = prefix.Source.EndKm - prefix.Source.BeginKm;
    float first = clamp(((prefix.BeginM - prefix.ClearBeginM) * 0.001 -
        prefix.Source.BeginKm) / sourceWidthKm, 0.0, 1.0);
    float last = clamp(((prefix.EndM - prefix.ClearBeginM) * 0.001 -
        prefix.Source.BeginKm) / sourceWidthKm, first, 1.0);
    sourceA += terms.DirectEmission * mix(prefix.Source.SolarA, prefix.Source.SolarB, first) +
        terms.DiffuseEmission;
    sourceB += terms.DirectEmission * mix(prefix.Source.SolarA, prefix.Source.SolarB, last) +
        terms.DiffuseEmission;
  }
  vec3 sigma = (prefix.HasAir ? terms.Extinction : vec3(0.0)) + cloudSigma;
  float distanceKm = (min(queryM, prefix.EndM) - prefix.BeginM) * 0.001;
  vec3 integral = transferIntegral(sigma, distanceKm);
  vec3 weightB = depthSourceFirstMoment(sigma, distanceKm, integral) /
      ((prefix.EndM - prefix.BeginM) * 0.001);
  return linearMediumThrough(prefix, terms.Extinction, prefix.BeginM) *
      ((integral - weightB) * sourceA + weightB * sourceB);
}

vec3 linearMediumAdvance(inout LinearMediumPrefix prefix, AirRay ray,
                         AirSourceRay metric, AirSourceTerms terms, float queryM) {
  const int bound = SOURCE_DEPTH_INTERVALS + 2 * CLOUD_VIEW_SAMPLES + 8;
  for (int step = 0; step < bound; ++step) {
    if (queryM < prefix.EndM || prefix.EndM >= prefix.Cloud.ExtentM) { break; }
    if (prefix.EndM <= prefix.BeginM) { prefix.Valid = false; break; }
    prefix.Radiance += linearMediumPartial(prefix, terms, prefix.EndM);
    ++prefix.Cells;
    prefix.BeginM = prefix.EndM;
    if (prefix.EndsAir &&
        prefix.Source.EndKm < ray.DistanceKm) {
      depthSourceNext(prefix.Source, ray, metric);
    }
    if (prefix.EndsCloud) {
      prefix.CloudDepth = linearMediumCloudDepth(prefix, prefix.Cloud.EndM);
      cloudPrefixNext(prefix.Cloud);
    }
    linearMediumPrepare(prefix);
  }
  return prefix.Radiance + linearMediumPartial(prefix, terms, queryM);
}

void main() {
  if (SOURCE_EYE_CLOUD_COVER <= 0.0) { depthSourceAirControl(); return; }
  uint index = gl_GlobalInvocationID.x;
  if (index >= uint(from.rays.length())) { return; }
  ProbeRay inputRay = from.rays[index];
  float azimuth = uintBitsToFloat(inputRay.Pad);
  float horizontal = sqrt(max(0.0, 1.0 - inputRay.Ray.CosView * inputRay.Ray.CosView));
  vec3 direction = vec3(horizontal * cos(azimuth), inputRay.Ray.CosView, horizontal * sin(azimuth));
  vec3 originM = vec3(0.0, (inputRay.Ray.RadiusKm - pushed.Air.BottomRadiusKm) * 1000.0, 0.0);
  AirPathSpan span = airPhysicalSpan(pushed.Air, inputRay.Ray);
  AirRay ray = airPathRay(pushed.Air, inputRay.Ray, span);
  if (ray.DistanceKm <= 0.0) { depthSourceAirControl(); return; }
  AirSourceRay metric = AirSourceRay(originM + direction * (span.BeginKm * 1000.0), direction);
  AirSourceTerms terms = airSourceTermsAt(pushed.Air, ray, 0.0,
      airSunVisible(pushed.Air, ray, 0.0), veil, multiScatter);
  LinearMediumPrefix prefix;
  if (SOURCE_LINEAR_UNSHADED != 0 || sourceWeatherLayer().field.x <= 0.0) {
    prefix.Source.MassAtStart = vec3(0.0);
    prefix.Source.BeginKm = 0.0;
    prefix.Source.EndKm = ray.DistanceKm;
    prefix.Source.SolarA = 1.0;
    prefix.Source.SolarB = 1.0;
    prefix.Source.Grid = 1u;
    prefix.Source.Evaluations = 0u;
  } else {
    prefix.Source = depthSourceBegin(ray, metric);
  }
  prefix.Radiance = vec3(0.0);
  prefix.CloudDepth = 0.0;
  prefix.ClearBeginM = span.BeginKm * 1000.0;
  prefix.ClearEndM = span.EndKm * 1000.0;
  prefix.ClearLengthKm = ray.DistanceKm;
  prefix.BeginM = 0.0;
  prefix.Cells = 0u;
  prefix.Valid = true;
  CloudLayer layer = sourceWeatherLayer();
  layer.field.x = SOURCE_EYE_CLOUD_COVER;
  CloudLight light = CloudLight(sourceSunDirection(), vec3(1.0, 0.97, 0.92), vec3(0.10, 0.12, 0.16));
  prefix.Cloud = cloudPrefixBegin(layer, originM, direction, span.EndKm * 1000.0, light);
  linearMediumPrepare(prefix);
  for (uint at = inputRay.QueryBegin; at < inputRay.QueryEnd; ++at) {
    float queryM = clamp(query.distanceKm[at] * 1000.0, 0.0, prefix.Cloud.ExtentM);
    vec3 radiance = linearMediumAdvance(prefix, ray, metric, terms, queryM);
    vec3 through = linearMediumThrough(prefix, terms.Extinction, queryM);
    bool valid = prefix.Valid && prefix.Cells < 4096u && prefix.Cloud.DensityEvaluations < 64u &&
        prefix.Cloud.SourceEvaluations < 64u &&
        (queryM <= prefix.EndM || prefix.EndM >= prefix.Cloud.ExtentM);
    uint counts = prefix.Cells | (prefix.Cloud.DensityEvaluations << 12u) |
        (prefix.Cloud.SourceEvaluations << 18u) | (valid ? 0u : 0x80000000u);
    into.samples[at] = ProbeSample(radiance.r, radiance.g, radiance.b,
        through.r, through.g, through.b, prefix.Source.Evaluations, counts);
  }
}

#endif
