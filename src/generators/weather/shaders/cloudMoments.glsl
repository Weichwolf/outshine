#ifndef OUTSHINE_CLOUD_MOMENTS_GLSL
#define OUTSHINE_CLOUD_MOMENTS_GLSL

struct CloudMomentSum {
  float OpticalDepth;
  float WeightedDistanceM;
};

float cloudMomentOpacity(float optical) {
  if (optical < 0.1) {
    return optical * (1.0 - optical * (0.5 - optical * (1.0 / 6.0 - optical *
        (1.0 / 24.0 - optical * (1.0 / 120.0 - optical / 720.0)))));
  }
  return 1.0 - exp(-optical);
}

float cloudMomentCentroid(float optical) {
  if (optical < 0.5) {
    float square = optical * optical;
    return 0.5 - optical / 12.0 + optical * square / 720.0 -
        optical * square * square / 30240.0;
  }
  return 1.0 / optical - exp(-optical) / cloudMomentOpacity(optical);
}

CloudMomentSum cloudMomentBegin() {
  return CloudMomentSum(0.0, 0.0);
}

void cloudMomentCell(inout CloudMomentSum sum, float beginM, float lengthM, float optical) {
  if (optical <= 0.0 || lengthM <= 0.0) { return; }
  float weight = exp(-sum.OpticalDepth) * cloudMomentOpacity(optical);
  float centroidM = beginM + lengthM * cloudMomentCentroid(optical);
  sum.WeightedDistanceM += weight * centroidM;
  sum.OpticalDepth += optical;
}

vec2 cloudMomentValue(CloudMomentSum sum) {
  return vec2(cloudMomentOpacity(sum.OpticalDepth), sum.WeightedDistanceM);
}

#endif
