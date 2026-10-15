#ifndef OUTSHINE_WORLD_SKY_AIRPATHLAYOUT_H
#define OUTSHINE_WORLD_SKY_AIRPATHLAYOUT_H

struct AirQuadratic {
  float Quadratic;
  float HalfLinear;
  float Constant;
};

struct AirRoots {
  float First;
  float Second;
};

struct AirPathSpan {
  float BeginKm;
  float EndKm;
};

#endif
