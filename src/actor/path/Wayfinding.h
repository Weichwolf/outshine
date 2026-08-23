#ifndef OUTSHINE_ACTOR_PATH_WAYFINDING_H
#define OUTSHINE_ACTOR_PATH_WAYFINDING_H

#include <cstddef>
#include <cstdint>
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
  int Lanes = 0;
};

struct Route {
  bool Found = false;
  double LengthM = 0.0;
  double StraightM = 0.0;
  size_t Reached = 0;
  size_t TurnsRefused = 0;
  size_t StartedFrom = 0;
  std::vector<Leg> Legs;
  std::string Error;
};

class Network {
public:
  Network(double snapM, double sphereRadiusM) : SnapM_(snapM), RadiusM_(sphereRadiusM) {}

  void Lay(const double *latLonPairs, size_t points, double halfWidthM, double maxGradient,
           int lanes);
  [[nodiscard]] bool Weave(std::string &error);

  [[nodiscard]] size_t WayCount() const { return Ways_.size(); }
  [[nodiscard]] size_t PointCount() const { return Points_.size() / 2; }
  [[nodiscard]] size_t NodeCount() const { return Nodes_.size(); }
  [[nodiscard]] size_t EdgeCount() const { return Edges_.size(); }
  [[nodiscard]] size_t JunctionCount() const;
  [[nodiscard]] double SnapM() const { return SnapM_; }

  [[nodiscard]] Route Plan(const Waypoint &from, const Waypoint &to, double tightestM) const;
  [[nodiscard]] bool Nearest(const Waypoint &to, size_t &node, double &awayM) const;
  void Within(const Waypoint &of, double reachM, std::vector<size_t> &nodes) const;

private:
  struct Way {
    size_t First = 0;
    size_t Count = 0;
    double HalfWidthM = 0.0;
    double MaxGradient = 0.0;
    int Lanes = 0;
  };
  struct Node {
    double LatDeg = 0.0;
    double LonDeg = 0.0;
    size_t FirstEdge = 0;
    size_t EdgeCount = 0;
    double HalfWidthM = 0.0;
    double MaxGradient = 0.0;
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
  [[nodiscard]] int64_t RowOf(double latDeg) const;
  [[nodiscard]] static int64_t ColumnIn(const RowShape &shape, double lonDeg);
  [[nodiscard]] static int64_t KeyAt(int64_t row, int64_t column);
  [[nodiscard]] int64_t CellOf(double latDeg, double lonDeg) const;

  double SnapM_ = 0.0;
  double RadiusM_ = 0.0;
  std::vector<double> Points_;
  std::vector<double> Widths_;
  std::vector<double> Gradients_;
  std::vector<int> Lanes_;
  std::vector<Way> Ways_;
  std::vector<Node> Nodes_;
  std::vector<Edge> Edges_;
  std::unordered_map<int64_t, std::vector<size_t>> Cells_;
  bool Woven_ = false;
};

}

#endif
