#ifndef OUTSHINE_BASE_SPATIAL_WAYFINDING_H
#define OUTSHINE_BASE_SPATIAL_WAYFINDING_H

#include <cstddef>
#include <cstdint>
#include <expected>
#include <string_view>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace outshine::Path {

inline constexpr size_t kMaxNetworkPoints = 4000000;
inline constexpr size_t kMaxRouteLegs = 262144;
inline constexpr double kStartReachM = 250.0;

[[nodiscard]] double ApartM(double fromLatDeg, double fromLonDeg, double toLatDeg,
                            double toLonDeg, double sphereRadiusM);

struct Waypoint {
  double LatDeg = 0.0;
  double LonDeg = 0.0;
};

struct Leg {
  Waypoint At;
  double AlongM = 0.0;
  double HalfWidthM = 0.0;
  double MaxGradient = 0.0;
  double MinRadiusM = 0.0;
  int Lanes = 0;
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
  Network(double snapM, double sphereRadiusM) : SnapM_(snapM), RadiusM_(sphereRadiusM) {}

  void Lay(std::span<const double> latLonPairs, double halfWidthM, double maxGradient,
           int lanes, double minRadiusM = 0.0, bool spans = false);
  [[nodiscard]] size_t Cross();
  [[nodiscard]] bool Weave(std::string &error);

  [[nodiscard]] size_t WayCount() const { return Ways_.size(); }
  [[nodiscard]] size_t PointCount() const { return Points_.size() / 2; }
  [[nodiscard]] size_t NodeCount() const { return Nodes_.size(); }
  [[nodiscard]] size_t EdgeCount() const { return Edges_.size(); }
  [[nodiscard]] size_t TiedToEdges() const { return Tied_; }
  [[nodiscard]] size_t CrossingsJoined() const { return Joined_; }
  [[nodiscard]] size_t CrossingsLeftAlone() const { return LeftAlone_; }
  [[nodiscard]] size_t CellsInTheTieIndex() const { return IndexedCells_; }
  [[nodiscard]] size_t JunctionCount() const;

  struct Crossing {
    uint32_t OverWay = 0, UnderWay = 0;
    double LatDeg = 0.0, LonDeg = 0.0;
    uint32_t OverAt = 0, UnderAt = 0;
  };

  struct Swept {
    size_t Found = 0;
    size_t PairsTested = 0;
    size_t FullestCell = 0;
    size_t PairsPruned = 0;
  };

  [[nodiscard]] std::expected<Swept, std::string_view> Crossings(
      std::vector<Crossing> &into) const;
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

  [[nodiscard]] Route Plan(const Waypoint &from, const Waypoint &to, double tightestM) const;
  [[nodiscard]] size_t Reaches(std::span<const size_t> from) const;

  struct Pieces {
    size_t Count = 0;
    size_t Largest = 0;
    size_t UnderFour = 0;
    size_t InUnderFour = 0;
  };
  [[nodiscard]] Pieces InPieces() const;
  [[nodiscard]] bool Nearest(const Waypoint &to, size_t &node, double &awayM) const;
  void Within(const Waypoint &of, double reachM, std::vector<size_t> &nodes) const;

private:
  struct Way {
    size_t First = 0;
    size_t Count = 0;
    double MinLat = 0.0, MinLon = 0.0, MaxLat = 0.0, MaxLon = 0.0;
    double HalfWidthM = 0.0;
    double MaxGradient = 0.0;
    double MinRadiusM = 0.0;
    int Lanes = 0;
    bool Spans = false;
  };
  struct Node {
    double LatDeg = 0.0;
    double LonDeg = 0.0;
    size_t FirstEdge = 0;
    size_t EdgeCount = 0;
    double HalfWidthM = 0.0;
    double MaxGradient = 0.0;
    double MinRadiusM = 0.0;
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
  [[nodiscard]] RowShape ShapeRow(int64_t row) const;
  [[nodiscard]] RowShape ShapeRowOver(int64_t row, double cellM) const;
  [[nodiscard]] int64_t RowOf(double latDeg) const;
  [[nodiscard]] int64_t RowOver(double latDeg, double cellM) const;
  [[nodiscard]] static int64_t ColumnIn(const RowShape &shape, double lonDeg);
  [[nodiscard]] static int64_t KeyAt(int64_t row, int64_t column);
  [[nodiscard]] int64_t CellOf(double latDeg, double lonDeg) const;

  double SnapM_ = 0.0;
  double RadiusM_ = 0.0;
  size_t IndexedCells_ = 0;
  size_t Joined_ = 0;
  size_t LeftAlone_ = 0;
  std::vector<double> Points_;
  std::vector<uint32_t> WayOf_;
  std::vector<Way> Ways_;
  std::vector<Node> Nodes_;
  std::vector<Edge> Edges_;
  std::unordered_map<int64_t, std::vector<size_t>> Cells_;
  size_t Tied_ = 0;
  bool Woven_ = false;
};

}

#endif
