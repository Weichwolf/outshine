#ifndef OUTSHINE_SIM_JOURNEY_H
#define OUTSHINE_SIM_JOURNEY_H

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include <outshine/Scenario.h>

#include "Body.h"
#include "DriveTick.h"
#include "Sink.h"
#include "TerrainLoader.h"
#include "Fit.h"
#include "ReferenceLine.h"
#include "Rig.h"
#include "SpeedProfile.h"
#include "Wayfinding.h"

namespace outshine::Data {
class Transport;
}

namespace outshine::Sim {

struct Provision {
  std::string CacheDir;
  std::string AssetsDir;
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

class Journey {
public:
  Journey();
  ~Journey();
  Journey(const Journey &) = delete;
  Journey &operator=(const Journey &) = delete;

  [[nodiscard]] bool Lay(const Between &between, const char *scenarioPath, int zoom,
                         Data::Transport &wire, const Provision &kept, Sink &say);
  [[nodiscard]] Ridden Ride(double dtS, const Taken *taken = nullptr);
  void Close(void);

  [[nodiscard]] const Physics::Body &Carried(void) const;
  [[nodiscard]] const ReferenceLine &Corridor(void) const;
  [[nodiscard]] World::GroundStream &Ground(void) const;
  [[nodiscard]] const Scenario &Declared(void) const;
  [[nodiscard]] double LengthM(void) const;
  [[nodiscard]] double ReserveMs2(void) const;
  void Frame(double &latDeg, double &lonDeg, double &perLatM, double &perLonM) const;

private:
  struct State;
  std::unique_ptr<State> S_;
};

} // namespace outshine::Sim

#endif
