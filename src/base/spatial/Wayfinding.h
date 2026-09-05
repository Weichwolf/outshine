#ifndef OUTSHINE_BASE_SPATIAL_WAYFINDING_H
#define OUTSHINE_BASE_SPATIAL_WAYFINDING_H

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
#include <optional>
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

class Network {
public:
  Network(Snap snap, Sphere on) : SnapM_(snap.CellM), RadiusM_(on.RadiusM) {}

  void Lay(std::span<const double> latLonPairs, const WayClass &of);
  [[nodiscard]] size_t Cross();
  [[nodiscard]] bool Weave(std::string &error);

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

  struct Pieces {
    size_t Count = 0;
    size_t Largest = 0;
    size_t UnderFour = 0;
    size_t InUnderFour = 0;
  };

  [[nodiscard]] Pieces InPieces() const;

  struct Found {
    size_t Node = 0;
    double AwayM = 0.0;
  };

  [[nodiscard]] std::optional<Found> Nearest(LongitudeLatitude to) const;
  void Within(LongitudeLatitude of, double reachM, std::vector<size_t> &nodes) const;

private:
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
    double HeightM = 0.0;
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
  using EdgesByCell = std::unordered_map<int64_t, std::vector<std::pair<uint32_t, uint32_t>>>;

  void SortWaysIntoDeclaredOrder();
  void StationsOfWays();
  void SlopesOfWays();
  void CountGrades(Elevated &made) const;
  [[nodiscard]] size_t NodeNear(LongitudeLatitude at, const CellsByKey &byCell) const;
  static void FoldWayInto(Node &node, const Way &from);
  void SnapPointsIntoNodes(std::vector<size_t> &nodeOf, CellsByKey &byCell);
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
  size_t Tied_ = 0;
  bool Woven_ = false;
};

} // namespace outshine::Path

#endif
