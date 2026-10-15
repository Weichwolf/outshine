#ifndef OUTSHINE_EXPERIMENT_AIRPREFIXPROBELAYOUT_H
#define OUTSHINE_EXPERIMENT_AIRPREFIXPROBELAYOUT_H

struct ProbeRay {
  AirRay Ray;
  uint QueryBegin;
  uint QueryEnd;
  uint Pad;
};

struct ProbeSample {
  float RadianceR;
  float RadianceG;
  float RadianceB;
  float ThroughR;
  float ThroughG;
  float ThroughB;
  uint SourceEvaluations;
  uint Pad;
};
#endif
