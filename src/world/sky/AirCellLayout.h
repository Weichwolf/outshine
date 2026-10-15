#ifndef OUTSHINE_WORLD_SKY_AIRCELLLAYOUT_H
#define OUTSHINE_WORLD_SKY_AIRCELLLAYOUT_H

struct AirCellInput {
  float WidthKm;
  float DistanceKm;
  float ExtinctionA;
  float ExtinctionB;
  float ExtinctionC;
  float SourceA;
  float SourceB;
  float SourceC;
};

struct AirCellChannel {
  float RadiancePerLux;
  float Transmittance;
};

#endif
