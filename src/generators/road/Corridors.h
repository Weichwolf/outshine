#ifndef OUTSHINE_GENERATORS_ROAD_CORRIDORS_H
#define OUTSHINE_GENERATORS_ROAD_CORRIDORS_H

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <scenario/Event.h>

#include "Earth.h"
#include "Fit.h"
#include "GroundMesher.h"
#include "GroundStack.h"
#include "OsmField.h"
#include "RoadMesher.h"
#include "StreetField.h"
#include "TangentFrame.h"
#include "Wayfinding.h"
#include "scene/Geometry.h"
#include "spatial/Drape.h"

namespace outshine::Generators {

class Corridors {
public:
  explicit Corridors(const RoadMesher &sweeper) : Sweeper_(sweeper) {}

  struct Site {
    const outshine::Ground::GroundStack &Stack;
    const TangentFrame &Standing;
    const Drape &Draped;
    const std::shared_ptr<const ClassStructure> &Classes;
    std::chrono::steady_clock::time_point CensusAt;
  };

  void Lay(const Site &site,
           Geometry &ground,
           std::vector<Yields> *corridor,
           std::vector<Measure> *notes) const;

private:
  struct Meets {
    double EastM = 0.0;
    double SouthM = 0.0;
    uint64_t Named = 0;
  };

  struct Paving {
    const outshine::Ground::GroundStack &Stack;
    const outshine::Ground::StreetField &Ways;
    const outshine::Ground::OsmField &Vectors;
    std::span<const double> Points;
    const std::unordered_map<uint64_t, uint32_t> &SharedNodes;
    const Drape &Draped;
    const TangentFrame &Standing;
    const std::shared_ptr<const ClassStructure> &Classes;
    int WaterRow = -1;
  };

  struct Paved {
    std::vector<Measure> Notes;
    std::vector<std::vector<RoadStation>> Designed;
    std::vector<RoadStation> Along;
    std::vector<RoadStation> Finer;
    double FitRadiusTightestM = 0.0;
    size_t FitsMeasured = 0;
    std::vector<double> FitEastNorth;
    double TightestDemandM = 0.0;
    std::vector<double> DeckM;
    std::vector<double> TrimM;
    std::unordered_map<uint64_t, std::vector<Meets>> AtCrossing;
    std::unordered_map<uint64_t, std::vector<RoadGate>> Gates;
    std::unordered_map<uint64_t, double> EndM;
    std::unordered_map<uint64_t, double> GroundEndM;
    RoadTallied Swept;
    size_t ChordAdded = 0;
    size_t DecksOverWater = 0;
    size_t AskedOverBridge = 0;
    size_t NamedOverBridge = 0;
    size_t WetOverBridge = 0;
    size_t RefusedWays = 0;
    size_t LaidWays = 0;
    size_t GroundWays = 0;
    size_t FitLaid = 0;
    size_t FitRefused = 0;
    size_t FitUndrivable = 0;
    size_t FitTooTight = 0;
    size_t FitUnsplittable = 0;
    size_t FitCuts = 0;
    size_t CrossingsSeen = 0;
    size_t PairsTested = 0;
    size_t PairsPruned = 0;
    size_t FullestCell = 0;
    double CrossNetworkMs = 0.0;
    double CrossSweepMs = 0.0;
    double CrossFilingMs = 0.0;
    double CrossDecksMs = 0.0;
    size_t DecksRaised = 0;
    double MostRaisedM = 0.0;
    size_t RampsRaised = 0;
    double SteepestRamp = 0.0;
    size_t EndsTrimmed = 0;
    size_t EndsStillCrossing = 0;
    double SharpestForkDeg = 0.0;
    double DeepestTrimM = 0.0;
    double ShortestByM = 0.0;
    double LongestShortM = 0.0;
    size_t CapsBit = 0;
    double MostOverWaterM = 0.0;
    double FitMs = 0.0;
    double WaterMs = 0.0;
    double SweepMs = 0.0;
    double RestMs = 0.0;
  };

  static void RefineChords(const Paving &on, Paved &into);

  struct Spanning {
    size_t Here = 0;
    size_t Next = 0;
  };

  [[nodiscard]] static size_t StepsAcross(const Paving &on, Spanning between);

  [[nodiscard]] static uint64_t SharedNodeAt(const Paving &on, double latDeg, double lonDeg);

  [[nodiscard]] static bool
  StationsAlong(const Paving &on,
                const outshine::Ground::StreetField::Way &lane,
                const std::function<bool(LongitudeLatitude, uint64_t)> &station);

  void DesignLane(const Paving &on,
                  const outshine::Ground::StreetField::Way &lane,
                  size_t laneAt,
                  Paved &into) const;

  static double LeastSeen(double held, double seen);
  static void NotesFit(Paved &into, const Fitted &got);
  static void FitAlongLane(Paved &into);
  static void TrimLaneEnds(size_t laneAt, Paved &into);
  static void FitLane(size_t laneAt, Paved &into);

  static void LayLanesIntoNetwork(const outshine::Ground::StreetField &ways,
                                  std::span<const double> points,
                                  Path::Network &net,
                                  std::vector<size_t> &netToLane);
  static void
  FileCrossing(const Path::Network::Crossing &one, const TangentFrame &standing, Paved &into);
  static void RaiseDeckOver(const Path::Network::Crossing &one,
                            const Paving &on,
                            std::span<const size_t> netToLane,
                            Paved &into);
  static void Crosses(const Paving &on, Paved &into);

  struct Ends {
    std::array<uint64_t, 2> Key{};
    std::array<double, 4> At{};
  };

  [[nodiscard]] static std::optional<Ends> EndsOf(const outshine::Ground::OsmField &vectors,
                                                  const outshine::Ground::StreetField::Way &lane);

  struct Grounded {
    double EastM = 0.0;
    double SouthM = 0.0;
    double GradeM = 0.0;
  };

  [[nodiscard]] static std::optional<Grounded> GroundUnder(const Paving &on, LongitudeLatitude at);

  static void RaisesEnds(std::span<const uint64_t> key, double deckM, Paved &into);

  [[nodiscard]] static double HighestDeckM(const Paved &over);

  static void EasesRamps(const outshine::Ground::StreetField &ways,
                         const outshine::Ground::OsmField &vectors,
                         double mostDeckM,
                         Paved &into);

  static void GradesApproaches(const Paving &on, Paved &into);

  static void SeedsBridgeEnds(const Paving &on, Paved &into);

  static void Bridges(const Paving &on, Paved &into);

  static void Shortens(const outshine::Ground::StreetField &ways,
                       const outshine::Ground::OsmField &vectors,
                       Paved &into);

  [[nodiscard]] static double StepAlongM(std::span<const RoadStation> along, size_t at);

  [[nodiscard]] static std::vector<double> ReachedAlong(std::span<const RoadStation> along);

  static void MarksWaterCrossing(const Paving &on, size_t laneAt, Paved &into);

  static void LevelsDeckOrApproach(const Paving &on,
                                   const outshine::Ground::StreetField::Way &lane,
                                   size_t laneAt,
                                   Paved &into);

  enum class Pass : uint8_t { Designing, Paving };

  [[nodiscard]] static constexpr std::string_view Doing(Pass pass) {
    return pass == Pass::Designing ? "designing" : "paving";
  }

  void PaveLane(const Paving &on,
                Pass pass,
                size_t laneAt,
                Paved &into,
                std::vector<Yields> &corridor,
                RoadRaised &pavement) const;

  [[nodiscard]] static double LevelsWhereWaysMeet(Paved &into);

  [[nodiscard]] size_t RaisesTheJunctionBodies(const outshine::Ground::GroundMaterials &wearing,
                                               Paved &into,
                                               RoadRaised &pavement) const;

  static void TellsWhatTheFitFound(Paved &into);
  static void HandsThePavingOver(const outshine::Ground::GroundMaterials &wearing,
                                 const RoadRaised &pavement,
                                 Paved &into,
                                 Geometry &ground);

  [[nodiscard]] static std::unordered_map<uint64_t, uint32_t>
  SharedNodesOf(const outshine::Ground::StreetField &ways, std::span<const double> points);

  static void Notes(Paved &into, std::string what, double how, const char *unit) {
    into.Notes.push_back({.What = std::move(what), .How = how, .Unit = unit});
  }

  const RoadMesher &Sweeper_;
};

} // namespace outshine::Generators
#endif
