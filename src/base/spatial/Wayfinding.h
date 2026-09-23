#ifndef OUTSHINE_BASE_SPATIAL_WAYFINDING_H
#define OUTSHINE_BASE_SPATIAL_WAYFINDING_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <string_view>
#include <limits>
#include <type_traits>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <utility>
#include <vector>

#include "Earth.h"

namespace outshine::Path {

inline constexpr size_t kMaxNetworkPoints = 4000000;
inline constexpr size_t kMaxRouteLegs = 262144;
inline constexpr double kStartReachM = 250.0;

struct Sphere {
  double RadiusM = kEarthMeanRadiusM;
};

struct Snap {
  double CellM = 0.0;
};

struct RowColumn {
  int64_t Row = 0;
  int64_t Column = 0;
};

[[nodiscard]] double ApartM(LongitudeLatitude from, LongitudeLatitude to, Sphere on);

struct Leg {
  LongitudeLatitude At;
  double AlongM = 0.0;
  double HalfWidthM = 0.0;
  double MaxGradient = 0.0;
  double MinRadiusM = 0.0;
  double Friction = 0.0;
  int Lanes = 0;
};

struct WayClass {
  double HalfWidthM = 0.0;
  double MaxGradient = 0.0;
  double MinRadiusM = 0.0;
  double Friction = 0.0;
  double SpeedMps = 0.0;
  int Lanes = 0;
  int Priority = 0;
  bool Oneway = false;
  bool Sealed = false;
  bool Spans = false;
  size_t Tag = 0;
};

struct Route {
  bool Found = false;
  double LengthM = 0.0;
  double StraightM = 0.0;
  size_t Reached = 0;
  size_t Component = 0;
  size_t TurnsRefused = 0;
  size_t StartedFrom = 0;
  size_t ArrivedAt = 0;
  size_t EndComponent = 0;
  std::vector<Leg> Legs;
  std::string Error;
};

class NetworkWeaveJob;
class NetworkCrossingJob;
class NetworkElevationJob;

class Network {
public:
  [[nodiscard]] static std::expected<Network, std::string_view> Create(Snap snap, Sphere on);

  [[nodiscard]] std::expected<void, std::string_view> Lay(std::span<const double> latLonPairs,
                                                          const WayClass &of);
  [[nodiscard]] size_t Cross();

  struct WeaveTimings {
    double SortMs = 0.0;
    double SnapMs = 0.0;
    double EdgesMs = 0.0;
    double TieMs = 0.0;
    double PackMs = 0.0;
  };

  [[nodiscard]] bool Weave(std::string &error, WeaveTimings *timings = nullptr);

  [[nodiscard]] size_t WayCount() const { return Ways_.size(); }

  [[nodiscard]] size_t TagOf(size_t way) const { return way < Ways_.size() ? Ways_[way].Tag : 0; }

  [[nodiscard]] size_t PointCount() const { return Points_.size() / 2; }

  [[nodiscard]] size_t NodeCount() const { return Nodes_.size(); }

  [[nodiscard]] size_t EdgeCount() const { return Edges_.size(); }

  [[nodiscard]] size_t TiedToEdges() const { return Tied_; }

  [[nodiscard]] size_t CrossingsJoined() const { return Joined_; }

  [[nodiscard]] size_t CrossingsLeftAlone() const { return LeftAlone_; }

  [[nodiscard]] size_t CellsInTheTieIndex() const { return IndexedCells_; }

  [[nodiscard]] size_t JunctionCount() const;

  struct Elevated {
    size_t Points = 0;
    size_t Refused = 0;
    double SteepestGrade = 0.0;
    size_t OverTenPercent = 0;
    size_t OverThirtyPercent = 0;
    double SteepestSealedGrade = 0.0;
    size_t SealedOverTenPercent = 0;
  };

  using HeightSource = std::function<std::optional<double>(LongitudeLatitude)>;
  [[nodiscard]] Elevated Elevate(const HeightSource &heightOf);

  struct Station {
    double HeightM = 0.0;
    double Grade = 0.0;
  };

  struct Along {
    size_t Way = 0;
    double StationM = 0.0;
  };

  [[nodiscard]] std::optional<Station> Profile(Along at) const;
  [[nodiscard]] double LengthM(size_t way) const;

  struct Crossing {
    uint32_t OverWay = 0, UnderWay = 0;
    double LatitudeDeg = 0.0, LongitudeDeg = 0.0;
    uint32_t OverAt = 0, UnderAt = 0;
  };

  struct Swept {
    size_t Found = 0;
    size_t PairsTested = 0;
    size_t FullestCell = 0;
    size_t PairsPruned = 0;
    size_t CandidatePairs = 0;
    double SpanMs = 0.0;
    double SegmentsMs = 0.0;
    double GridMs = 0.0;
    double FilingMs = 0.0;
    double TestMs = 0.0;
    double CacheMs = 0.0;
  };

  [[nodiscard]] std::expected<Swept, std::string_view> Crossings(std::vector<Crossing> &into) const;

  [[nodiscard]] double SnapM() const { return SnapM_; }

  [[nodiscard]] size_t PointStreamBytes() const {
    return Points_.size() * sizeof(double) + WayOf_.size() * sizeof(uint32_t);
  }

  [[nodiscard]] size_t PointStreamHeldBytes() const {
    return Points_.capacity() * sizeof(double) + WayOf_.capacity() * sizeof(uint32_t);
  }

  [[nodiscard]] size_t BytesPerPoint() const {
    const size_t points = Points_.size() / 2;
    return points > 0 ? PointStreamBytes() / points : 0;
  }

  [[nodiscard]] Route Plan(LongitudeLatitude from, LongitudeLatitude to, double tightestM) const;
  [[nodiscard]] size_t Reaches(std::span<const size_t> from) const;

  struct ComponentStatistics {
    size_t Count = 0;
    size_t Largest = 0;
    size_t UnderFour = 0;
    size_t InUnderFour = 0;
  };

  [[nodiscard]] ComponentStatistics WeakComponents() const;

  struct Found {
    size_t Node = 0;
    double AwayM = 0.0;
  };

  [[nodiscard]] std::expected<std::optional<Found>, std::string_view>
  Nearest(LongitudeLatitude to) const;
  [[nodiscard]] std::expected<void, std::string_view>
  Within(LongitudeLatitude of, double reachM, std::vector<size_t> &nodes) const;

private:
  friend class NetworkWeaveJob;
  friend class NetworkCrossingJob;
  friend class NetworkElevationJob;

  Network(Snap snap, Sphere on) : SnapM_(snap.CellM), RadiusM_(on.RadiusM) {}

  class PhysicalAdjacency {
  public:
    explicit PhysicalAdjacency(size_t nodes) : Degree_(nodes) {}

    void Adopt(size_t nodes, std::unordered_set<uint64_t> &&edges);
    void Connect(size_t from, size_t to);
    void Disconnect(size_t from, size_t to);
    [[nodiscard]] bool AccumulateDegrees(size_t itemsMost);
    [[nodiscard]] bool Release(size_t itemsMost);

    [[nodiscard]] size_t Degree(size_t node) const { return Degree_[node]; }

  private:
    std::vector<size_t> Degree_;
    std::unordered_set<uint64_t> Edges_;
    std::optional<std::unordered_set<uint64_t>::const_iterator> DegreeCursor_;
  };

  [[nodiscard]] static uint64_t PhysicalEdgeKey(size_t from, size_t to);
  static constexpr double kTenPercent = 0.10;
  static constexpr double kThirtyPercent = 0.30;
  [[nodiscard]] bool PrepareWeave(std::string &error);
  [[nodiscard]] static std::optional<double> SampleFiniteHeight(const HeightSource &source,
                                                                LongitudeLatitude at);

  struct Way {
    size_t First = 0;
    size_t Count = 0;
    double MinLat = 0.0, MinLon = 0.0, MaxLat = 0.0, MaxLon = 0.0;
    double HalfWidthM = 0.0;
    double MaxGradient = 0.0;
    double MinRadiusM = 0.0;
    double Friction = 0.0;
    double SpeedMps = 0.0;
    int Lanes = 0;
    int Priority = 0;
    bool Oneway = false;
    bool Sealed = false;
    bool Spans = false;
    size_t Tag = 0;
  };

  struct Node {
    double LatitudeDeg = 0.0;
    double LongitudeDeg = 0.0;
    size_t FirstEdge = 0;
    size_t EdgeCount = 0;
    double HalfWidthM = 0.0;
    double MaxGradient = 0.0;
    double MinRadiusM = 0.0;
    double Friction = 0.0;
    int Lanes = 0;
  };

  struct Edge {
    size_t To = 0;
    double LengthM = 0.0;
  };

  struct RowShape {
    int64_t Row = 0;
    double LonCellDeg = 0.0;
    int64_t Columns = 1;
  };

  using CellsByKey = std::unordered_map<int64_t, std::vector<size_t>>;
  using OutgoingEdges = std::vector<std::vector<Edge>>;

  class EdgesByCell {
  public:
    using Entries = std::vector<std::pair<uint32_t, uint32_t>>;

    [[nodiscard]] Entries &Cell(int64_t key);
    [[nodiscard]] const Entries *Find(int64_t key) const;
    [[nodiscard]] bool Release(size_t itemsMost);

  private:
    static constexpr size_t kShards = 256;
    [[nodiscard]] static size_t ShardOf(int64_t key) noexcept;

    std::array<std::unordered_map<int64_t, Entries>, kShards> Shards_;
    size_t NextReleaseShard_ = 0;
  };

  [[nodiscard]] bool LocalTurnAllowsRadius(const Edge &incoming,
                                           size_t previousNode,
                                           const Edge &outgoing,
                                           double minimumRadiusM) const;

  class RouteSearch;

  struct RouteTrace {
    size_t Arrived = 0;
    std::span<const size_t> Predecessors;
    std::span<const size_t> Starts;
  };

  [[nodiscard]] std::expected<void, std::string_view> ReconstructRoute(RouteTrace trace,
                                                                       Route &out) const;

  [[nodiscard]] bool WayLess(size_t a, size_t b) const;
  void SortWaysIntoDeclaredOrder();
  void StationsOfWays();
  void SlopesOfWays();
  void CountGrades(Elevated &made) const;
  [[nodiscard]] size_t NodeNear(LongitudeLatitude at, const CellsByKey &byCell) const;
  static void FoldWayInto(Node &node, const Way &from);
  void SnapOnePoint(size_t point, std::vector<size_t> &nodeOf, CellsByKey &byCell);
  void SnapPointsIntoNodes(std::vector<size_t> &nodeOf, CellsByKey &byCell);
  void AppendWayEdge(const Way &way,
                     size_t step,
                     std::span<const size_t> nodeOf,
                     OutgoingEdges &outgoing) const;
  void EdgesFromWays(std::span<const size_t> nodeOf, OutgoingEdges &outgoing) const;

  struct Spanned {
    static constexpr double kBeyond = std::numeric_limits<double>::infinity();

    double WestLon = kBeyond;
    double EastLon = -kBeyond;
    double SouthLat = kBeyond;
    double NorthLat = -kBeyond;
  };

  [[nodiscard]] size_t SegmentsOfWays(std::vector<uint32_t> &segWay,
                                      std::vector<uint32_t> &segAt) const;
  [[nodiscard]] Spanned SpanOfPoints(std::vector<double> &lon) const;

  struct Sweeping {
    std::span<const double> Lon;
    std::span<const uint32_t> SegAt;
    size_t Segments = 0;
  };

  struct Gridded {
    double CellDeg = 1.0;
    size_t Cells = 0;
    uint64_t Wide = 0;
    uint64_t High = 0;
  };

  [[nodiscard]] std::expected<Gridded, std::string_view> GridOver(const Sweeping &over,
                                                                  Spanned box) const;
  [[nodiscard]] static uint32_t SquareIn(const Gridded &grid, Spanned box, LongitudeLatitude at);

  struct Filed {
    uint32_t Seg = 0;
    uint32_t Way = 0;
    double Ax = 0.0, Ay = 0.0, Bx = 0.0, By = 0.0;
  };

  static_assert(sizeof(Filed) == 2 * sizeof(uint32_t) + 4 * sizeof(double));
  static_assert(std::is_trivially_copyable_v<Filed>);

  [[nodiscard]] static std::optional<LongitudeLatitude> CrossingOf(const Filed &one,
                                                                   const Filed &two);

  struct Filing {
    std::span<const uint32_t> SegAt;
    std::span<const Filed> InCell;
  };

  struct CellSpan {
    uint32_t Square = 0;
    uint32_t From = 0;
    uint32_t To = 0;
  };

  static void CrossingsInCell(const Filing &filed,
                              CellSpan span,
                              const Gridded &grid,
                              Spanned box,
                              std::vector<Crossing> &into,
                              Swept &swept);

  struct EdgeEnds {
    size_t From = 0;
    size_t To = 0;
  };

  [[nodiscard]] bool
  IndexOneEdge(EdgeEnds ends, double tieReachM, EdgesByCell &byEdgeCell, std::string &error);
  [[nodiscard]] bool
  IndexEdgeCell(EdgeEnds ends, RowColumn at, EdgesByCell &byEdgeCell, std::string &error);
  [[nodiscard]] bool IndexEdgesByCell(const OutgoingEdges &outgoing,
                                      double tieReachM,
                                      EdgesByCell &byEdgeCell,
                                      std::string &error);

  struct PerDegree {
    double Lat = 0.0;
    double Lon = 0.0;
  };

  struct NearestEdge {
    size_t From = 0;
    size_t To = 0;
    double AwayM = 0.0;
  };

  [[nodiscard]] double TieReachM() const;
  [[nodiscard]] PerDegree MetresPerDegreeAt(const Node &at) const;
  void
  MarkEdgeOverCells(EdgeEnds ends, bool holding, double tieReachM, EdgesByCell &byEdgeCell) const;
  [[nodiscard]] double AwayFromEdgeM(const Node &end, EdgeEnds ends, PerDegree per) const;
  [[nodiscard]] NearestEdge
  NearestEdgeTo(size_t loose, const EdgesByCell &byEdgeCell, double tieReachM) const;
  [[nodiscard]] bool SpliceInto(size_t loose,
                                NearestEdge best,
                                double tieReachM,
                                OutgoingEdges &outgoing,
                                EdgesByCell &byEdgeCell);
  [[nodiscard]] bool TieLooseEnds(OutgoingEdges &outgoing, std::string &error);

  [[nodiscard]] RowShape ShapeRow(int64_t row) const;
  [[nodiscard]] RowShape ShapeRowOver(int64_t row, Snap over) const;
  [[nodiscard]] int64_t RowOf(double latDeg) const;
  [[nodiscard]] int64_t RowOver(double latDeg, Snap over) const;
  [[nodiscard]] static int64_t ColumnIn(const RowShape &shape, double lonDeg);
  [[nodiscard]] static int64_t KeyAt(RowColumn at);
  [[nodiscard]] int64_t CellOf(LongitudeLatitude at) const;

  double SnapM_ = 0.0;
  double RadiusM_ = 0.0;
  size_t IndexedCells_ = 0;
  size_t Joined_ = 0;
  size_t LeftAlone_ = 0;
  std::vector<double> Points_;
  std::vector<uint32_t> WayOf_;
  std::vector<uint32_t> NodeOfPoint_;
  std::vector<double> HeightsM_;
  std::vector<double> StationM_;
  std::vector<double> SlopeM_;
  std::vector<Way> Ways_;
  std::vector<Node> Nodes_;
  std::vector<Edge> Edges_;
  std::unordered_map<int64_t, std::vector<size_t>> Cells_;
  mutable std::vector<Crossing> CachedCrossings_;
  mutable std::optional<Swept> CachedSweep_;
  size_t Tied_ = 0;
  bool Woven_ = false;
};

class NetworkWeaveJob {
public:
  struct SliceWorst {
    double SortMs = 0.0;
    double MergeMs = 0.0;
    double ReserveMs = 0.0;
    double CopyMs = 0.0;
    double BeginSnapMs = 0.0;
    double SnapMs = 0.0;
    double EdgesMs = 0.0;
    double IndexMs = 0.0;
    double AdjacencyBeginMs = 0.0;
    double AdjacencyMs = 0.0;
    double TieMs = 0.0;
    double PublishMs = 0.0;
  };

  [[nodiscard]] static std::expected<NetworkWeaveJob, std::string> Begin(Network &&network);
  NetworkWeaveJob(const NetworkWeaveJob &) = delete;
  NetworkWeaveJob &operator=(const NetworkWeaveJob &) = delete;
  NetworkWeaveJob(NetworkWeaveJob &&) noexcept = default;
  NetworkWeaveJob &operator=(NetworkWeaveJob &&) noexcept = default;

  [[nodiscard]] std::expected<bool, std::string> Advance(size_t itemsMost);
  [[nodiscard]] std::expected<Network, std::string_view> Take() &&;
  [[nodiscard]] std::expected<bool, std::string_view> ReleaseTemporary(size_t itemsMost);

  [[nodiscard]] SliceWorst LongestSlices() const noexcept { return Worst_; }

private:
  enum class Stage : uint8_t {
    SortWays,
    MergeWays,
    ReservePointCopy,
    ReserveOwnerCopy,
    ReserveWayCopy,
    CopyWays,
    BeginSnap,
    SnapPoints,
    BuildEdges,
    IndexEdges,
    BeginAdjacency,
    BuildAdjacency,
    TieEnds,
    Publish,
    Done
  };
  enum class ReleaseStage : uint8_t { Cells, Outgoing, EdgeCells, Adjacency, Done };
  explicit NetworkWeaveJob(Network &&network);
  void SortWays();
  void MergeWays(size_t itemsMost);
  void CopyWays(size_t itemsMost);
  void BeginSnap();
  void SnapPoints(size_t itemsMost);
  void BuildEdges(size_t itemsMost);
  [[nodiscard]] std::expected<void, std::string> IndexEdges(size_t itemsMost);
  void BeginAdjacency();
  void BuildAdjacency(size_t itemsMost);
  void TieEnds(size_t itemsMost);
  void Publish();

  Network Network_;

  struct RunCursor {
    size_t At = 0;
    size_t End = 0;
  };

  static constexpr size_t kWayRun = 512;
  std::vector<size_t> Order_;
  std::vector<size_t> SortedOrder_;
  std::vector<RunCursor> RunHeap_;
  std::vector<double> SortedPoints_;
  std::vector<uint32_t> SortedWayOf_;
  std::vector<Network::Way> SortedWays_;
  std::vector<size_t> NodeOf_;
  Network::CellsByKey ByCell_;
  Network::OutgoingEdges Outgoing_;
  Network::EdgesByCell ByEdgeCell_;
  std::unordered_set<uint64_t> Indexed_;
  Network::PhysicalAdjacency Adjacency_{0};

  struct EdgeIndexCursor {
    Network::EdgeEnds Ends;
    int64_t Row = 0;
    int64_t LastRow = 0;
    int64_t Column = 0;
    int64_t LastColumn = 0;
    std::optional<Network::RowShape> Shape;
  };

  std::optional<EdgeIndexCursor> IndexCursor_;
  double TieReachM_ = 0.0;
  size_t NextPoint_ = 0;
  size_t NextSort_ = 0;
  size_t NextCopyWay_ = 0;
  size_t NextCopyPoint_ = 0;
  size_t NextWay_ = 0;
  size_t NextStep_ = 1;
  size_t NextNode_ = 0;
  size_t NextEdge_ = 0;
  size_t NextRelease_ = 0;
  SliceWorst Worst_;
  Stage Stage_ = Stage::SortWays;
  ReleaseStage ReleaseStage_ = ReleaseStage::Cells;
};

class NetworkCrossingJob {
public:
  struct SliceWorst {
    double SetupMs = 0.0;
    double TestMs = 0.0;
    double PublishMs = 0.0;
  };

  struct Result {
    Network Graph;
    Network::Swept Statistics;
  };

  [[nodiscard]] static std::expected<NetworkCrossingJob, std::string_view> Begin(Network &&network);
  NetworkCrossingJob(const NetworkCrossingJob &) = delete;
  NetworkCrossingJob &operator=(const NetworkCrossingJob &) = delete;
  NetworkCrossingJob(NetworkCrossingJob &&) noexcept = default;
  NetworkCrossingJob &operator=(NetworkCrossingJob &&) noexcept = default;

  [[nodiscard]] std::expected<bool, std::string_view> Advance(size_t pairsMost);
  [[nodiscard]] std::expected<Result, std::string_view> Take() &&;

  [[nodiscard]] SliceWorst LongestSlices() const noexcept { return Worst_; }

private:
  enum class Stage : uint8_t {
    CountCells,
    PrefixCells,
    AllocateCells,
    FillCells,
    CountPairs,
    TestPairs,
    Publish,
    Done
  };
  explicit NetworkCrossingJob(Network &&network);

  struct SquareCursor {
    uint32_t FirstX = 0;
    uint32_t LastX = 0;
    uint32_t LastY = 0;
    uint32_t X = 0;
    uint32_t Y = 0;
    Network::Filed Filed;
  };

  [[nodiscard]] SquareCursor SquaresOf(size_t segment) const;
  void AdvanceSquares(size_t itemsMost);
  void PrefixCells(size_t itemsMost);
  void AllocateCells();
  void CountPairs(size_t itemsMost);
  void TestPairs(size_t pairsMost);
  void Publish();

  Network Network_;
  std::vector<double> LongitudeDeg_;
  std::vector<uint32_t> SegmentWay_;
  std::vector<uint32_t> SegmentAt_;
  std::vector<uint32_t> Holds_;
  std::vector<uint32_t> Filled_;
  std::vector<uint32_t> CellStarts_;
  std::vector<Network::Filed> FiledInCell_;
  std::vector<Network::Crossing> Found_;
  Network::Spanned Span_;
  Network::Gridded Grid_;
  Network::Swept Statistics_;
  size_t NextSegment_ = 0;
  size_t NextSetupCell_ = 0;
  std::optional<SquareCursor> SquareCursor_;
  size_t NextCell_ = 0;
  uint32_t NextOne_ = 0;
  uint32_t NextTwo_ = 0;
  SliceWorst Worst_;
  Stage Stage_ = Stage::CountCells;
};

class NetworkElevationJob {
public:
  struct Budget {
    size_t SamplesMost = 0;
    size_t ProfilePointsMost = 0;
  };

  struct SliceWorst {
    double SampleNodesMs = 0.0;
    double WritePointsMs = 0.0;
    double StationsMs = 0.0;
    double SlopesMs = 0.0;
    double GradesMs = 0.0;
  };

  struct Result {
    Network Graph;
    Network::Elevated Statistics;
  };

  [[nodiscard]] static NetworkElevationJob Begin(Network &&network, Network::HeightSource source);
  NetworkElevationJob(const NetworkElevationJob &) = delete;
  NetworkElevationJob &operator=(const NetworkElevationJob &) = delete;
  NetworkElevationJob(NetworkElevationJob &&) noexcept = default;
  NetworkElevationJob &operator=(NetworkElevationJob &&) noexcept = default;

  [[nodiscard]] std::expected<bool, std::string_view> Advance(size_t itemsMost);
  [[nodiscard]] std::expected<bool, std::string_view> Advance(Budget budget);
  [[nodiscard]] std::expected<Result, std::string_view> Take() &&;

  [[nodiscard]] SliceWorst LongestSlices() const noexcept { return Worst_; }

private:
  enum class Stage : uint8_t {
    SampleNodes,
    WritePoints,
    PrepareStations,
    Stations,
    PrepareSlopes,
    Slopes,
    CountGrades,
    Done
  };
  NetworkElevationJob(Network &&network, Network::HeightSource source);
  void SampleNodes(size_t itemsMost);
  void WritePoints(size_t itemsMost);
  void PrepareStations();
  void BuildStations(size_t itemsMost);
  void PrepareSlopes();
  void BuildSlopes(size_t itemsMost);
  void CountGrades(size_t itemsMost);

  Network Network_;
  Network::HeightSource HeightOf_;
  std::vector<std::optional<double>> AtNode_;
  Network::Elevated Statistics_;
  size_t NextNode_ = 0;
  size_t NextPoint_ = 0;
  size_t NextWay_ = 0;
  size_t NextProfilePoint_ = 0;
  SliceWorst Worst_;
  Stage Stage_ = Stage::SampleNodes;
};

}

#endif
