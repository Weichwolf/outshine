#ifndef OUTSHINE_DRIVER_JOURNEY_H
#define OUTSHINE_DRIVER_JOURNEY_H

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include <outshine/Scenario.h>

#include "Body.h"
#include "Fit.h"
#include "ReferenceLine.h"
#include "Rig.h"
#include "SpeedProfile.h"
#include "Wayfinding.h"

namespace outshine::Driver {

class Sink {
public:
  virtual ~Sink() = default;
  virtual void Number(const char *what, double value, const char *unit) = 0;
  virtual void Claim(bool held, const char *why) = 0;
  virtual void Near(double got, double want, double within, const char *unit, const char *why) = 0;
  virtual void Say(const std::string &line) = 0;
};

struct Between {
  double FromLatDeg = 0.0;
  double FromLonDeg = 0.0;
  double ToLatDeg = 0.0;
  double ToLonDeg = 0.0;
};

struct Laid {
  double StraightM = 0.0;
  double RouteM = 0.0;
  double CorridorM = 0.0;
  double FromAwayM = 0.0;
  double ToAwayM = 0.0;
  double TileGroundM = 0.0;
  double QuantumM = 0.0;
  double SnapM = 0.0;
  size_t Tiles = 0;
  size_t Features = 0;
  size_t Points = 0;
  size_t Ways = 0;
  size_t NotACarriageway = 0;
  std::string NotCarriageways;
  size_t Nodes = 0;
  size_t Junctions = 0;
  size_t Edges = 0;
  size_t Legs = 0;
  size_t TurnsRefused = 0;
  size_t Vertices = 0;
  size_t Kept = 0;
  size_t Corners = 0;
  size_t Straights = 0;
  size_t Undrivable = 0;
  double TightestRadiusM = 0.0;
  double SharpestTurnRad = 0.0;
  double WorstVertexOffsetM = 0.0;
  double DriftM = 0.0;
  double FetchedS = 0.0;
  double SampledS = 0.0;
  size_t Posts = 0;
  size_t Holes = 0;
  double LowestM = 0.0;
  double HighestM = 0.0;
  double FromHeightM = 0.0;
  double ToHeightM = 0.0;
  double CutM = 0.0;
  double FillM = 0.0;
  double MovedM = 0.0;
  double SteepestGrade = 0.0;
  double SteepestGradeAtM = 0.0;
  double GentlestLimit = 0.0;
  size_t Ungraded = 0;
  size_t Unlaned = 0;
  double NarrowestLaneM = 0.0;
  double WidestLaneM = 0.0;
  double MostAsideM = 0.0;
  double LeadM = 0.0;
  size_t LedIn = 0;
  size_t InsideTight = 0;
  double BudgetM = 0.0;
  double PlannedBudgetM = 0.0;
  double FloorRatio = 0.0;
  double SlowestMs = 0.0;
  double FastestMs = 0.0;
  double MeanMs = 0.0;
  size_t CrestsBound = 0;
  double CrestHeldMs = 0.0;
  double CrestHeldAtM = 0.0;
  std::string Error;
};

struct Ridden {
  bool Found = false;
  bool Arrived = false;
  bool Lost = false;
  bool PastTravel = false;
  bool PastLimit = false;
  bool OffTheRoad = false;
  size_t Airborne = 0;
  double AlongM = 0.0;
  double SpeedMs = 0.0;
  double PlannedMs = 0.0;
  double InLaneM = 0.0;
  double AsideM = 0.0;
  double EdgeM = 0.0;
  double RatioOfHold = 0.0;
  double CurvaturePerM = 0.0;
  double CurvatureRatePerM = 0.0;
  double ReachedM = 0.0;
  double TopMs = 0.0;
  double WorstOffsetM = 0.0;
  double WorstOffsetAtM = 0.0;
  double WorstRatio = 0.0;
  size_t MostAirborne = 0;
  double AirborneAtM = 0.0;
  double BrokeAtM = 0.0;
  double LeftTheRoadAtM = 0.0;
  double LeftByM = 0.0;
  double LeftAtMs = 0.0;
  double LeftPlannedMs = 0.0;
  double LeftCurvature = 0.0;
  double LeftRate = 0.0;
  double LeftLaneM = 0.0;
  double LeftEdgeM = 0.0;
  double LeftAsideM = 0.0;
  double LeftAcrossM = 0.0;
  size_t OffTheSurface = 0;
  double SimulatedS = 0.0;
};

class Journey {
public:
  Journey();
  ~Journey();
  Journey(const Journey &) = delete;
  Journey &operator=(const Journey &) = delete;

  [[nodiscard]] bool Lay(const Between &between, const char *scenarioPath, int zoom, Sink &say);
  [[nodiscard]] Ridden Ride(double dtS);
  void Close(void);

  [[nodiscard]] const Physics::Body &Carried(void) const;
  [[nodiscard]] const ReferenceLine &Corridor(void) const;
  [[nodiscard]] const Scenario &Declared(void) const;
  [[nodiscard]] double LengthM(void) const;

private:
  struct State;
  std::unique_ptr<State> S_;
};

} // namespace outshine::Driver

#endif
