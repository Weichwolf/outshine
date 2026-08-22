#ifndef OUTSHINE_SIM_CORRIDORLAY_H
#define OUTSHINE_SIM_CORRIDORLAY_H

#include <string>
#include <vector>

#include <outshine/Scenario.h>

#include "Fit.h"
#include "ReferenceLine.h"
#include "Rigging.h"
#include "Sink.h"
#include "SpeedProfile.h"
#include "TerrainLoader.h"
#include "Wayfinding.h"

namespace outshine::Sim {

struct Corridor {
  ReferenceLine Line;
  outshine::Fitted Fitted;
  SpeedProfile Profile;
  std::vector<double> RoadM, HalfWidthM, LaneHalfM, AsideM, FineAside, FineEdge;
  double FineM = 2.0;
  double SpanM = 0.0;
  double NarrowestLaneM = 0.0;
  double BudgetM = 0.0;
  double HoldWithinM = 0.0;
  double ReserveMs2 = 0.0;
  double FrameLat = 0.0, FrameLon = 0.0, PerLatM = 1.0, PerLonM = 1.0;
};

[[nodiscard]] bool LayCorridor(const Path::Route &route, Ground::GroundStream &ground,
                               const Vehicle &car, const Rigged &stood, double quantumM,
                               double tightestM, double middleLat, double sphereRadiusM,
                               Sink &say, Corridor &out, std::string &error);

}

#endif
