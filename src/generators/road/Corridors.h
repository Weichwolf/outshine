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

  struct Mapped {
    std::shared_ptr<const Path::Network> Network;
    size_t Ways = 0;
    size_t Nodes = 0;
    size_t Edges = 0;
    size_t Junctions = 0;
    Path::Network::Elevated Elevated;
    std::string Refusal;
  };

  [[nodiscard]] static Mapped MapOf(const outshine::Ground::GroundStack &stack);

  struct Site {
    const outshine::Ground::GroundStack &Stack;
    const Path::Network *Network = nullptr;
    const TangentFrame &Standing;
    const Drape &Draped;
    const std::shared_ptr<const ClassStructure> &Classes;
    std::chrono::steady_clock::time_point CensusAt;
    double EyeLatDeg = 0.0;
    double EyeLonDeg = 0.0;
    double FocalPx = 0.0;
  };

  void Lay(const Site &site,
           Geometry &ground,
           std::vector<Yields> *corridor,
           std::vector<Measure> *notes) const;

private:
  struct Meets {
    double EastM = 0.0;
    double NorthM = 0.0;
    uint64_t Named = 0;
  };

  struct Paving {
    const outshine::Ground::GroundStack &Stack;
    const Path::Network *Network = nullptr;
    const outshine::Ground::StreetField &Ways;
    const outshine::Ground::OsmField &Vectors;
    std::span<const double> Points;
    const std::unordered_map<uint64_t, uint32_t> &SharedNodes;
    const Drape &Draped;
    const TangentFrame &Standing;
    const std::shared_ptr<const ClassStructure> &Classes;
    int WaterRow = -1;
    double EyeLatDeg = 0.0;
    double EyeLonDeg = 0.0;
    double FocalPx = 0.0;
  };

  struct Edge {
    uint32_t Lane = 0;
    uint32_t First = 0;
    uint32_t Count = 0;
    std::array<uint64_t, 2> NodeAt{};
    std::array<double, 2> CutM{};
    std::array<double, 2> GradeAtM{};
    std::array<bool, 2> Joined{};
  };

  struct Leg {
    uint32_t Edge = 0;
    uint8_t End = 0;
    double AngleRad = 0.0;
    double HalfM = 0.0;
    double CutM = 0.0;
  };

  struct Junction {
    uint64_t Node = 0;
    double EastM = 0.0;
    double NorthM = 0.0;
    double GradeM = 0.0;
    double SlopeE = 0.0;
    double SlopeN = 0.0;
    std::vector<Leg> Legs;
    std::vector<RoadGate> Gates;
  };

  struct Paved {
    std::vector<Measure> Notes;
    std::vector<std::vector<RoadStation>> Designed;
    std::vector<Edge> Edges;
    std::vector<std::pair<uint32_t, uint32_t>> EdgesOf;
    std::vector<Junction> Junctions;
    std::vector<Yields> UnderJunctions;
    size_t Continuations = 0;
    size_t UnseenWays = 0;
    size_t LegsCut = 0;
    double DeepestCutM = 0.0;
    double SteepestJunction = 0.0;
    size_t JunctionsLevelled = 0;
    double MostOffGroundM = 0.0;
    size_t RampStations = 0;
    double LongestRampM = 0.0;
    double MostLiftedM = 0.0;
    double YieldsMs = 0.0;
    size_t EdgeStations = 0;
    std::vector<RoadStation> Along;
    std::vector<RoadStation> Finer;
    double FitRadiusTightestM = 0.0;
    size_t FitsMeasured = 0;
    std::vector<double> FitEastNorth;
    double TightestDemandM = 0.0;
    std::vector<double> DeckM;
    std::unordered_map<uint64_t, std::vector<Meets>> AtCrossing;
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
  [[nodiscard]] static double AwayM(const Paving &on,
                                    const outshine::Ground::StreetField::Way &lane);

  [[nodiscard]] static bool
  StationsAlong(const Paving &on,
                const outshine::Ground::StreetField::Way &lane,
                const std::function<bool(LongitudeLatitude, uint64_t)> &station);

  static void DesignLane(const Paving &on,
                         const outshine::Ground::StreetField::Way &lane,
                         size_t laneAt,
                         Paved &into);

  static double LeastSeen(double held, double seen);
  static void NotesFit(Paved &into, const Fitted &got);
  static void FitAlongLane(Paved &into);
  static void TrimLaneEnds(const Edge &edge, Paved &into);
  static void FitLane(const Edge &edge, Paved &into);

  static void LayLanesIntoNetwork(const outshine::Ground::StreetField &ways,
                                  std::span<const double> points,
                                  Path::Network &net);
  static void
  FileCrossing(const Path::Network::Crossing &one, const TangentFrame &standing, Paved &into);
  static void RaiseDeckOver(const Path::Network::Crossing &one,
                            const Paving &on,
                            const Path::Network &net,
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
    double NorthM = 0.0;
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

  [[nodiscard]] static double StepAlongM(std::span<const RoadStation> along, size_t at);

  [[nodiscard]] static std::vector<double> ReachedAlong(std::span<const RoadStation> along);

  static void MarksWaterCrossing(const Paving &on, size_t laneAt, Paved &into);

  static void SplitsEdges(Paved &into);
  [[nodiscard]] static std::unordered_map<uint64_t, std::vector<Leg>> LegsOf(const Paving &on,
                                                                             const Paved &into);
  static void GatesOf(std::span<const Leg> legs, const Paved &into, Junction &made);
  static void LiesOnItsPlane(const Paving &on, Junction &made, Paved &into);
  static void PressesUnder(const Junction &made, double rootsM, Paved &into);
  static void ShapeOf(const Paving &on, uint64_t node, std::vector<Leg> &legs, Paved &into);
  static void ShapesJunctions(const Paving &on, Paved &into);
  static void
  DeckOrRamp(const outshine::Ground::StreetField::Way &lane, const Edge &edge, Paved &into);
  static void YieldsOf(const Paving &on,
                       const outshine::Ground::StreetField::Way &lane,
                       Paved &into,
                       std::vector<Yields> &corridor);
  static void IslandOf(const Paving &on,
                       const outshine::Ground::StreetField::Way &lane,
                       std::span<const RoadStation> along,
                       std::vector<Yields> &corridor);
  void PaveEdge(const Paving &on,
                size_t edgeAt,
                Paved &into,
                std::vector<Yields> &corridor,
                RoadRaised &pavement) const;

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
