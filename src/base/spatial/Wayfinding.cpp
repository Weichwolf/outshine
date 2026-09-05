#include "math/Units.h"
#include "Wayfinding.h"

#include <cstdio>

#include "math/Vec2.h"

#include <array>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <numbers>
#include <optional>
#include <cmath>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <queue>
#include <vector>
#include <utility>
#include <unordered_map>

namespace outshine::Path {

constexpr uint64_t kWordMost = 0xFFFFFFFFull;

namespace {

constexpr double kDegToRad = std::numbers::pi / kDegPerHalfTurn;

double MetresPerDegreeLat(double sphereRadiusM) {
  return sphereRadiusM * kDegToRad;
}

double MetresPerDegreeLon(double latDeg, Sphere on) {
  const double shrink = std::cos(latDeg * kDegToRad);
  return MetresPerDegreeLat(on.RadiusM) * (shrink > kLeastRunM ? shrink : kLeastRunM);
}

[[nodiscard]] double LonApartDeg(double toDeg, double fromDeg) {
  const double apart = toDeg - fromDeg;
  return apart - kDegPerTurn * std::floor(apart / kDegPerTurn + 0.5);
}

} // namespace

double ApartM(LongitudeLatitude from, LongitudeLatitude to, Sphere on) {
  const double fromLat = from.LatitudeDeg * kDegToRad;
  const double toLat = to.LatitudeDeg * kDegToRad;
  const double byLat = (to.LatitudeDeg - from.LatitudeDeg) * kDegToRad;
  double byLon = to.LongitudeDeg - from.LongitudeDeg;
  while (byLon > kDegPerHalfTurn) { byLon -= kDegPerTurn; }
  while (byLon < -kDegPerHalfTurn) { byLon += kDegPerTurn; }
  byLon *= kDegToRad;

  const double half =
      std::sin(0.5 * byLat) * std::sin(0.5 * byLat) +
      std::cos(fromLat) * std::cos(toLat) * std::sin(0.5 * byLon) * std::sin(0.5 * byLon);
  return 2.0 * on.RadiusM * std::asin(std::sqrt(half < 1.0 ? half : 1.0));
}

void Network::Lay(std::span<const double> latLonPairs, const WayClass &of) {
  const size_t points = latLonPairs.size() / 2;
  if (points < 2) { return; }
  Way way;
  way.First = Points_.size() / 2;
  way.Count = points;
  way.HalfWidthM = of.HalfWidthM;
  way.MaxGradient = of.MaxGradient;
  way.MinRadiusM = of.MinRadiusM;
  way.Friction = of.Friction;
  way.Lanes = of.Lanes;
  way.SpeedMps = of.SpeedMps;
  way.Priority = of.Priority;
  way.Oneway = of.Oneway;
  way.Sealed = of.Sealed;
  way.Tag = of.Tag;
  way.Spans = of.Spans;
  const auto mine = static_cast<uint32_t>(Ways_.size());
  way.MinLat = way.MaxLat = latLonPairs[0];
  way.MinLon = way.MaxLon = latLonPairs[1];
  for (size_t which = 0; which < points; ++which) {
    const double lat = latLonPairs[2 * which];
    const double lon = latLonPairs[2 * which + 1];
    way.MinLat = lat < way.MinLat ? lat : way.MinLat;
    way.MaxLat = lat > way.MaxLat ? lat : way.MaxLat;
    way.MinLon = lon < way.MinLon ? lon : way.MinLon;
    way.MaxLon = lon > way.MaxLon ? lon : way.MaxLon;
  }
  Ways_.push_back(way);

  for (size_t which = 0; which < points; ++which) {
    Points_.push_back(latLonPairs[2 * which]);
    Points_.push_back(latLonPairs[2 * which + 1]);
    WayOf_.push_back(mine);
  }
  Woven_ = false;
}

Network::RowShape Network::ShapeRow(int64_t row) const {
  return ShapeRowOver(row, Snap{.CellM = SnapM_});
}

Network::RowShape Network::ShapeRowOver(int64_t row, Snap over) const {
  const double cellM = over.CellM;
  RowShape shape;
  shape.Row = row;
  const double latCell = cellM / MetresPerDegreeLat(RadiusM_);
  const double rowLat = (static_cast<double>(row) + 0.5) * latCell;
  shape.LonCellDeg = cellM / MetresPerDegreeLon(rowLat, Sphere{.RadiusM = RadiusM_});
  const double columns = std::ceil(kDegPerTurn / shape.LonCellDeg);
  shape.Columns = columns > 1.0 ? static_cast<int64_t>(columns) : 1;
  return shape;
}

int64_t Network::RowOf(double latDeg) const {
  return RowOver(latDeg, Snap{.CellM = SnapM_});
}

int64_t Network::RowOver(double latDeg, Snap over) const {
  const double latCell = over.CellM / MetresPerDegreeLat(RadiusM_);
  return static_cast<int64_t>(std::floor(latDeg / latCell));
}

int64_t Network::ColumnIn(const RowShape &shape, double lonDeg) {
  const double wrapped =
      lonDeg - kDegPerTurn * std::floor((lonDeg + kDegPerHalfTurn) / kDegPerTurn);
  const auto column =
      static_cast<int64_t>(std::floor((wrapped + kDegPerHalfTurn) / shape.LonCellDeg));
  return ((column % shape.Columns) + shape.Columns) % shape.Columns;
}

int64_t Network::KeyAt(RowColumn at) {
  const auto high = static_cast<uint64_t>(at.Row) << 32u;
  const auto low = static_cast<uint64_t>(at.Column) & 0xffffffffULL;
  return static_cast<int64_t>(high ^ low);
}

int64_t Network::CellOf(LongitudeLatitude at) const {
  const int64_t row = RowOf(at.LatitudeDeg);
  return KeyAt({.Row = row, .Column = ColumnIn(ShapeRow(row), at.LongitudeDeg)});
}

size_t Network::Cross() {
  Joined_ = 0;
  LeftAlone_ = 0;
  std::vector<Crossing> found;
  const auto swept = Crossings(found);
  if (!swept) { return 0; }

  struct Cut {
    uint32_t After = 0;
    double LatitudeDeg = 0.0, LongitudeDeg = 0.0;
    double Along = 0.0;
  };

  std::vector<std::vector<Cut>> perWay(Ways_.size());
  for (const Crossing &held : found) {
    if (Ways_[held.OverWay].Spans || Ways_[held.UnderWay].Spans) {
      ++LeftAlone_;
      continue;
    }
    ++Joined_;
    const std::array<uint32_t, 2> sides = {{held.OverAt, held.UnderAt}};
    const std::array<uint32_t, 2> ways = {{held.OverWay, held.UnderWay}};
    for (int side = 0; side < 2; ++side) {
      const Way &way = Ways_[ways[side]];
      const uint32_t local = sides[side] - static_cast<uint32_t>(way.First);
      const double fromLat = Points_[2 * static_cast<size_t>(sides[side])];
      const double fromLon = Points_[2 * sides[side] + 1];
      const double toLat = Points_[2 * sides[side] + 2];
      const double toLon = Points_[2 * sides[side] + 3];
      const double runLat = toLat - fromLat;
      const double runLon = LonApartDeg(toLon, fromLon);
      const double square = runLat * runLat + runLon * runLon;
      const double along = square > 0.0 ? ((held.LatitudeDeg - fromLat) * runLat +
                                           LonApartDeg(held.LongitudeDeg, fromLon) * runLon) /
                                              square
                                        : 0.0;
      perWay[ways[side]].push_back(Cut{.After = local,
                                       .LatitudeDeg = held.LatitudeDeg,
                                       .LongitudeDeg = held.LongitudeDeg,
                                       .Along = along});
    }
  }
  if (Joined_ == 0) { return 0; }

  std::vector<double> laid;
  std::vector<uint32_t> owner;
  laid.reserve(Points_.size() + 4 * Joined_);
  owner.reserve(WayOf_.size() + 2 * Joined_);
  for (size_t which = 0; which < Ways_.size(); ++which) {
    Way &way = Ways_[which];
    std::vector<Cut> &cuts = perWay[which];
    std::ranges::sort(cuts, [](const Cut &one, const Cut &two) {
      return one.After != two.After ? one.After < two.After : one.Along < two.Along;
    });
    const size_t began = laid.size() / 2;
    size_t next = 0;
    for (size_t at = 0; at + 1 < way.Count; ++at) {
      laid.push_back(Points_[2 * (way.First + at)]);
      laid.push_back(Points_[2 * (way.First + at) + 1]);
      owner.push_back(static_cast<uint32_t>(which));
      while (next < cuts.size() && cuts[next].After == at) {
        laid.push_back(cuts[next].LatitudeDeg);
        laid.push_back(cuts[next].LongitudeDeg);
        owner.push_back(static_cast<uint32_t>(which));
        ++next;
      }
    }
    laid.push_back(Points_[2 * (way.First + way.Count - 1)]);
    laid.push_back(Points_[2 * (way.First + way.Count - 1) + 1]);
    owner.push_back(static_cast<uint32_t>(which));
    way.First = began;
    way.Count = laid.size() / 2 - began;
  }
  Points_ = std::move(laid);
  WayOf_ = std::move(owner);
  Woven_ = false;
  return Joined_;
}

void Network::SortWaysIntoDeclaredOrder() {
  std::vector<size_t> order(Ways_.size());
  for (size_t at = 0; at < order.size(); ++at) { order[at] = at; }
  std::ranges::sort(order, [this](size_t a, size_t b) {
    const Way &wa = Ways_[a];
    const Way &wb = Ways_[b];
    const size_t count = wa.Count < wb.Count ? wa.Count : wb.Count;
    for (size_t at = 0; at < 2 * count; ++at) {
      const double da = Points_[2 * wa.First + at];
      const double db = Points_[2 * wb.First + at];
      if (da != db) { return da < db; }
    }
    if (wa.Count != wb.Count) { return wa.Count < wb.Count; }
    if (wa.HalfWidthM != wb.HalfWidthM) { return wa.HalfWidthM < wb.HalfWidthM; }
    if (wa.MaxGradient != wb.MaxGradient) { return wa.MaxGradient < wb.MaxGradient; }
    if (wa.MinRadiusM != wb.MinRadiusM) { return wa.MinRadiusM < wb.MinRadiusM; }
    return wa.Lanes < wb.Lanes;
  });
  std::vector<double> points;
  std::vector<uint32_t> wayOf;
  std::vector<Way> ways;
  points.reserve(Points_.size());
  wayOf.reserve(WayOf_.size());
  ways.reserve(Ways_.size());
  for (const size_t which : order) {
    Way moved = Ways_[which];
    const size_t first = moved.First;
    moved.First = points.size() / 2;
    const auto mine = static_cast<uint32_t>(ways.size());
    ways.push_back(moved);
    for (size_t at = 0; at < moved.Count; ++at) {
      points.push_back(Points_[2 * (first + at)]);
      points.push_back(Points_[2 * (first + at) + 1]);
      wayOf.push_back(mine);
    }
  }
  Points_ = std::move(points);
  WayOf_ = std::move(wayOf);
  Ways_ = std::move(ways);
}

size_t Network::NodeNear(LongitudeLatitude at, const CellsByKey &byCell) const {
  const int64_t rowHere = RowOf(at.LatitudeDeg);
  const RowShape mine = ShapeRow(rowHere);
  for (int64_t row = rowHere - 1; row <= rowHere + 1; ++row) {
    const RowShape shape = ShapeRow(row);
    const int64_t reachCols =
        static_cast<int64_t>(std::ceil(mine.LonCellDeg / shape.LonCellDeg)) + 1;
    const int64_t centre = ColumnIn(shape, at.LongitudeDeg);
    const int64_t span = shape.Columns < 2 * reachCols + 1 ? shape.Columns : 2 * reachCols + 1;
    for (int64_t step = 0; step < span; ++step) {
      const int64_t column =
          ((centre + step - reachCols) % shape.Columns + shape.Columns) % shape.Columns;
      const auto seen = byCell.find(KeyAt({.Row = row, .Column = column}));
      if (seen == byCell.end()) { continue; }
      for (const size_t candidate : seen->second) {
        if (ApartM(at,
                   {.LongitudeDeg = Nodes_[candidate].LongitudeDeg,
                    .LatitudeDeg = Nodes_[candidate].LatitudeDeg},
                   Sphere{.RadiusM = RadiusM_}) <= SnapM_) {
          return candidate;
        }
      }
    }
  }
  return Nodes_.size();
}

void Network::FoldWayInto(Node &node, const Way &from) {
  node.HalfWidthM = std::max(from.HalfWidthM, node.HalfWidthM);
  if (from.Friction > 0.0 && (node.Friction <= 0.0 || from.Friction < node.Friction)) {
    node.Friction = from.Friction;
  }
  if (node.Lanes <= 0) { node.Lanes = from.Lanes; }
  if (node.MaxGradient <= 0.0 || (from.MaxGradient > 0.0 && from.MaxGradient < node.MaxGradient)) {
    node.MaxGradient = from.MaxGradient;
  }
  if (from.MinRadiusM <= 0.0 || node.MinRadiusM <= 0.0) {
    node.MinRadiusM = 0.0;
  } else if (from.MinRadiusM < node.MinRadiusM) {
    node.MinRadiusM = from.MinRadiusM;
  }
}

void Network::SnapPointsIntoNodes(std::vector<size_t> &nodeOf, CellsByKey &byCell) {
  for (size_t point = 0; point < Points_.size() / 2; ++point) {
    const LongitudeLatitude at{.LongitudeDeg = Points_[2 * point + 1],
                               .LatitudeDeg = Points_[2 * point]};
    const Way &from = Ways_[WayOf_[point]];
    const size_t found = NodeNear(at, byCell);
    if (found == Nodes_.size()) {
      Node made;
      made.LatitudeDeg = at.LatitudeDeg;
      made.LongitudeDeg = at.LongitudeDeg;
      made.HalfWidthM = from.HalfWidthM;
      made.Friction = from.Friction;
      made.MaxGradient = from.MaxGradient;
      made.MinRadiusM = from.MinRadiusM;
      made.Lanes = from.Lanes;
      Nodes_.push_back(made);
      byCell[CellOf(at)].push_back(found);
    } else {
      FoldWayInto(Nodes_[found], from);
    }
    nodeOf[point] = found;
  }
}

void Network::EdgesFromWays(std::span<const size_t> nodeOf, OutgoingEdges &outgoing) const {
  for (const Way &way : Ways_) {
    for (size_t step = 1; step < way.Count; ++step) {
      const size_t from = nodeOf[way.First + step - 1];
      const size_t to = nodeOf[way.First + step];
      if (from == to) { continue; }
      const double lengthM = ApartM(
          {.LongitudeDeg = Nodes_[from].LongitudeDeg, .LatitudeDeg = Nodes_[from].LatitudeDeg},
          {.LongitudeDeg = Nodes_[to].LongitudeDeg, .LatitudeDeg = Nodes_[to].LatitudeDeg},
          Sphere{.RadiusM = RadiusM_});
      outgoing[from].push_back(Edge{.To = to, .LengthM = lengthM});
      outgoing[to].push_back(Edge{.To = from, .LengthM = lengthM});
    }
  }
}

bool Network::IndexOneEdge(EdgeEnds ends,
                           double tieReachM,
                           EdgesByCell &byEdgeCell,
                           std::string &error) {
  const Node &a = Nodes_[ends.From];
  const Node &b = Nodes_[ends.To];
  const int64_t firstRow = RowOver(a.LatitudeDeg < b.LatitudeDeg ? a.LatitudeDeg : b.LatitudeDeg,
                                   Snap{.CellM = tieReachM});
  const int64_t lastRow = RowOver(a.LatitudeDeg < b.LatitudeDeg ? b.LatitudeDeg : a.LatitudeDeg,
                                  Snap{.CellM = tieReachM});
  for (int64_t row = firstRow; row <= lastRow; ++row) {
    const RowShape shape = ShapeRowOver(row, Snap{.CellM = tieReachM});
    const int64_t one = ColumnIn(shape, a.LongitudeDeg);
    const int64_t two = ColumnIn(shape, b.LongitudeDeg);
    const int64_t firstColumn = one < two ? one : two;
    const int64_t lastColumn = one < two ? two : one;
    for (int64_t column = firstColumn; column <= lastColumn; ++column) {
      if (IndexedCells_ >= kMaxNetworkPoints) {
        error = "the tie index would hold more than " + std::to_string(kMaxNetworkPoints) +
                " cells for " + std::to_string(Nodes_.size()) +
                " nodes, which is a graph this network cannot weave at a snap of " +
                std::to_string(SnapM_) + " m";
        return false;
      }
      ++IndexedCells_;
      byEdgeCell[KeyAt({.Row = row,
                        .Column = ((column % shape.Columns) + shape.Columns) % shape.Columns})]
          .emplace_back(static_cast<uint32_t>(ends.From), static_cast<uint32_t>(ends.To));
    }
  }
  return true;
}

bool Network::IndexEdgesByCell(const OutgoingEdges &outgoing,
                               double tieReachM,
                               EdgesByCell &byEdgeCell,
                               std::string &error) {
  for (size_t from = 0; from < Nodes_.size(); ++from) {
    for (const Edge &edge : outgoing[from]) {
      if (edge.To < from) { continue; }
      if (!IndexOneEdge({.From = from, .To = edge.To}, tieReachM, byEdgeCell, error)) {
        return false;
      }
    }
  }
  return true;
}

double Network::TieReachM() const {
  double tieReachM = SnapM_;
  for (const Node &held : Nodes_) { tieReachM = std::max(2.0 * held.HalfWidthM, tieReachM); }
  return tieReachM;
}

Network::PerDegree Network::MetresPerDegreeAt(const Node &at) const {
  return {.Lat = ApartM({.LongitudeDeg = at.LongitudeDeg, .LatitudeDeg = at.LatitudeDeg},
                        {.LongitudeDeg = at.LongitudeDeg, .LatitudeDeg = at.LatitudeDeg + 1.0},
                        Sphere{.RadiusM = RadiusM_}),
          .Lon = ApartM({.LongitudeDeg = at.LongitudeDeg, .LatitudeDeg = at.LatitudeDeg},
                        {.LongitudeDeg = at.LongitudeDeg + 1.0, .LatitudeDeg = at.LatitudeDeg},
                        Sphere{.RadiusM = RadiusM_})};
}

void Network::MarkEdgeOverCells(EdgeEnds ends,
                                bool holding,
                                double tieReachM,
                                EdgesByCell &byEdgeCell) const {
  const Node &a = Nodes_[ends.From];
  const Node &b = Nodes_[ends.To];
  const auto one = static_cast<uint32_t>(ends.From < ends.To ? ends.From : ends.To);
  const auto two = static_cast<uint32_t>(ends.From < ends.To ? ends.To : ends.From);
  const int64_t firstRow = RowOver(a.LatitudeDeg < b.LatitudeDeg ? a.LatitudeDeg : b.LatitudeDeg,
                                   Snap{.CellM = tieReachM});
  const int64_t lastRow = RowOver(a.LatitudeDeg < b.LatitudeDeg ? b.LatitudeDeg : a.LatitudeDeg,
                                  Snap{.CellM = tieReachM});
  for (int64_t row = firstRow; row <= lastRow; ++row) {
    const RowShape shape = ShapeRowOver(row, Snap{.CellM = tieReachM});
    const int64_t here = ColumnIn(shape, a.LongitudeDeg);
    const int64_t there = ColumnIn(shape, b.LongitudeDeg);
    const int64_t firstColumn = here < there ? here : there;
    const int64_t lastColumn = here < there ? there : here;
    for (int64_t column = firstColumn; column <= lastColumn; ++column) {
      const int64_t key =
          KeyAt({.Row = row, .Column = ((column % shape.Columns) + shape.Columns) % shape.Columns});
      std::vector<std::pair<uint32_t, uint32_t>> &cell = byEdgeCell[key];
      if (holding) {
        cell.emplace_back(one, two);
        continue;
      }
      for (size_t at = 0; at < cell.size(); ++at) {
        if (cell[at].first != one || cell[at].second != two) { continue; }
        cell.erase(cell.begin() + static_cast<ptrdiff_t>(at));
        break;
      }
    }
  }
}

double Network::AwayFromEdgeM(const Node &end, EdgeEnds ends, PerDegree per) const {
  const double ax = (Nodes_[ends.From].LongitudeDeg - end.LongitudeDeg) * per.Lon;
  const double ay = (Nodes_[ends.From].LatitudeDeg - end.LatitudeDeg) * per.Lat;
  const double bx = (Nodes_[ends.To].LongitudeDeg - end.LongitudeDeg) * per.Lon;
  const double by = (Nodes_[ends.To].LatitudeDeg - end.LatitudeDeg) * per.Lat;
  const double dx = bx - ax;
  const double dy = by - ay;
  const double span = dx * dx + dy * dy;
  const double along = std::clamp(span > 0.0 ? -(ax * dx + ay * dy) / span : 0.0, 0.0, 1.0);
  const double cx = ax + along * dx;
  const double cy = ay + along * dy;
  return std::sqrt(cx * cx + cy * cy);
}

Network::NearestEdge
Network::NearestEdgeTo(size_t loose, const EdgesByCell &byEdgeCell, double tieReachM) const {
  const Node &end = Nodes_[loose];
  const double reachM = end.HalfWidthM > 0.0 ? 2.0 * end.HalfWidthM : SnapM_;
  const PerDegree per = MetresPerDegreeAt(end);

  NearestEdge best{.From = Nodes_.size(), .To = Nodes_.size(), .AwayM = reachM};
  const int64_t rowHere = RowOver(end.LatitudeDeg, Snap{.CellM = tieReachM});
  const int64_t rowReach = static_cast<int64_t>(std::ceil(reachM / tieReachM)) + 1;
  for (int64_t row = rowHere - rowReach; row <= rowHere + rowReach; ++row) {
    const RowShape shape = ShapeRowOver(row, Snap{.CellM = tieReachM});
    const int64_t centre = ColumnIn(shape, end.LongitudeDeg);
    const int64_t colReach =
        static_cast<int64_t>(std::ceil(reachM / (shape.LonCellDeg * per.Lon))) + 1;
    for (int64_t step = -colReach; step <= colReach; ++step) {
      const int64_t column = ((centre + step) % shape.Columns + shape.Columns) % shape.Columns;
      const auto seen = byEdgeCell.find(KeyAt({.Row = row, .Column = column}));
      if (seen == byEdgeCell.end()) { continue; }
      for (const auto &held : seen->second) {
        const EdgeEnds ends{.From = held.first, .To = held.second};
        if (ends.From == loose || ends.To == loose) { continue; }
        const double awayM = AwayFromEdgeM(end, ends, per);
        const double touchM = end.HalfWidthM + Nodes_[ends.From].HalfWidthM;
        if (awayM >= best.AwayM || awayM > touchM) { continue; }
        best = {.From = ends.From, .To = ends.To, .AwayM = awayM};
      }
    }
  }
  return best;
}

bool Network::SpliceInto(size_t loose,
                         NearestEdge best,
                         double tieReachM,
                         OutgoingEdges &outgoing,
                         EdgesByCell &byEdgeCell) {
  const auto unlink = [&outgoing](size_t from, size_t to) {
    std::vector<Edge> &held = outgoing[from];
    for (size_t at = 0; at < held.size(); ++at) {
      if (held[at].To != to) { continue; }
      held.erase(held.begin() + static_cast<ptrdiff_t>(at));
      return true;
    }
    return false;
  };
  if (!unlink(best.From, best.To) || !unlink(best.To, best.From)) { return false; }
  MarkEdgeOverCells({.From = best.From, .To = best.To}, false, tieReachM, byEdgeCell);

  const auto link = [this, &outgoing](size_t from, size_t to) {
    const double lengthM =
        ApartM({.LongitudeDeg = Nodes_[from].LongitudeDeg, .LatitudeDeg = Nodes_[from].LatitudeDeg},
               {.LongitudeDeg = Nodes_[to].LongitudeDeg, .LatitudeDeg = Nodes_[to].LatitudeDeg},
               Sphere{.RadiusM = RadiusM_});
    outgoing[from].push_back(Edge{.To = to, .LengthM = lengthM});
    outgoing[to].push_back(Edge{.To = from, .LengthM = lengthM});
  };
  link(best.From, loose);
  link(loose, best.To);
  MarkEdgeOverCells({.From = best.From, .To = loose}, true, tieReachM, byEdgeCell);
  MarkEdgeOverCells({.From = loose, .To = best.To}, true, tieReachM, byEdgeCell);
  return true;
}

bool Network::TieLooseEnds(OutgoingEdges &outgoing, std::string &error) {
  const double tieReachM = TieReachM();
  EdgesByCell byEdgeCell;
  if (!IndexEdgesByCell(outgoing, tieReachM, byEdgeCell, error)) { return false; }

  Tied_ = 0;
  for (size_t loose = 0; loose < Nodes_.size(); ++loose) {
    if (outgoing[loose].size() != 1) { continue; }
    const NearestEdge best = NearestEdgeTo(loose, byEdgeCell, tieReachM);
    if (best.From == Nodes_.size()) { continue; }
    if (SpliceInto(loose, best, tieReachM, outgoing, byEdgeCell)) { ++Tied_; }
  }
  return true;
}

bool Network::Weave(std::string &error) {
  Nodes_.clear();
  Edges_.clear();
  Cells_.clear();
  Woven_ = false;

  if (Ways_.empty()) {
    error = "a network is woven from 1..N ways and this one carries none";
    return false;
  }
  if (Points_.size() / 2 > kMaxNetworkPoints) {
    error = "a network of " + std::to_string(Points_.size() / 2) + " points reaches the bound of " +
            std::to_string(kMaxNetworkPoints);
    return false;
  }
  if (!(SnapM_ > 0.0)) {
    error = "a snapping distance is what makes two ways meet, and this network declares " +
            std::to_string(SnapM_) + " m";
    return false;
  }

  SortWaysIntoDeclaredOrder();

  std::vector<size_t> nodeOf(Points_.size() / 2, 0);
  CellsByKey byCell;
  SnapPointsIntoNodes(nodeOf, byCell);

  std::vector<std::vector<Edge>> outgoing(Nodes_.size());
  EdgesFromWays(nodeOf, outgoing);
  if (!TieLooseEnds(outgoing, error)) { return false; }

  for (size_t node = 0; node < Nodes_.size(); ++node) {
    Nodes_[node].FirstEdge = Edges_.size();
    Nodes_[node].EdgeCount = outgoing[node].size();
    for (const Edge &edge : outgoing[node]) { Edges_.push_back(edge); }
  }

  Cells_ = std::move(byCell);
  Woven_ = true;
  return true;
}

namespace {

struct Ends {
  Vec2 From;
  Vec2 To;
};

[[nodiscard]] std::optional<Vec2> SegmentsMeet(Ends a, Ends b) {
  const double ax = a.From[0];
  const double ay = a.From[1];
  const double rx = a.To[0] - ax;
  const double ry = a.To[1] - ay;
  const double cx = b.From[0];
  const double cy = b.From[1];
  const double sx = b.To[0] - cx;
  const double sy = b.To[1] - cy;
  const double denominator = rx * sy - ry * sx;
  if (denominator == 0.0) { return std::nullopt; }
  const double along = ((cx - ax) * sy - (cy - ay) * sx) / denominator;
  const double across = ((cx - ax) * ry - (cy - ay) * rx) / denominator;
  if (along <= 0.0 || along >= 1.0 || across <= 0.0 || across >= 1.0) { return std::nullopt; }
  return Vec2{{ax + along * rx, ay + along * ry}};
}

} // namespace

namespace {

double AboutTheMeridian(double lonDeg) {
  while (lonDeg > kDegPerHalfTurn) { lonDeg -= kDegPerTurn; }
  while (lonDeg < -kDegPerHalfTurn) { lonDeg += kDegPerTurn; }
  return lonDeg;
}

} // namespace

uint32_t Network::SquareIn(const Gridded &grid, Spanned box, LongitudeLatitude at) {
  const double fx = std::floor((at.LongitudeDeg - box.WestLon) / grid.CellDeg);
  const double fy = std::floor((at.LatitudeDeg - box.SouthLat) / grid.CellDeg);
  const uint64_t x = fx <= 0.0 ? 0u : static_cast<uint64_t>(fx);
  const uint64_t y = fy <= 0.0 ? 0u : static_cast<uint64_t>(fy);
  return static_cast<uint32_t>((y < grid.High ? y : grid.High - 1u) * grid.Wide +
                               (x < grid.Wide ? x : grid.Wide - 1u));
}

size_t Network::SegmentsOfWays(std::vector<uint32_t> &segWay, std::vector<uint32_t> &segAt) const {
  size_t segments = 0;
  for (const Way &way : Ways_) { segments += way.Count > 1 ? way.Count - 1 : 0; }
  segWay.assign(segments, 0);
  segAt.assign(segments, 0);
  size_t made = 0;
  for (size_t which = 0; which < Ways_.size(); ++which) {
    const Way &way = Ways_[which];
    for (size_t a = 0; a + 1 < way.Count; ++a) {
      segWay[made] = static_cast<uint32_t>(which);
      segAt[made] = static_cast<uint32_t>(way.First + a);
      ++made;
    }
  }
  return segments;
}

Network::Spanned Network::SpanOfPoints(std::vector<double> &lon) const {
  const size_t points = Points_.size() / 2;
  const double aboutLon = Points_[1];
  Spanned over;
  for (size_t at = 0; at < points; ++at) {
    double away = Points_[2 * at + 1] - aboutLon;
    while (away > kDegPerHalfTurn) { away -= kDegPerTurn; }
    while (away < -kDegPerHalfTurn) { away += kDegPerTurn; }
    lon[at] = aboutLon + away;
    over.WestLon = std::min(lon[at], over.WestLon);
    over.EastLon = std::max(lon[at], over.EastLon);
    over.SouthLat = std::min(Points_[2 * at], over.SouthLat);
    over.NorthLat = std::max(Points_[2 * at], over.NorthLat);
  }
  return over;
}

std::expected<Network::Gridded, std::string_view> Network::GridOver(const Sweeping &over,
                                                                    Spanned box) const {
  Gridded grid;
  double reachSum = 0.0;
  for (size_t seg = 0; seg < over.Segments; ++seg) {
    const size_t first = over.SegAt[seg];
    const double byLon = std::fabs(over.Lon[first + 1] - over.Lon[first]);
    const double byLat = std::fabs(Points_[2 * first + 2] - Points_[2 * first]);
    reachSum += byLon > byLat ? byLon : byLat;
  }
  grid.CellDeg = reachSum > 0.0 ? 2.0 * reachSum / static_cast<double>(over.Segments) : 1.0;
  grid.Wide = static_cast<uint64_t>(std::floor((box.EastLon - box.WestLon) / grid.CellDeg)) + 2u;
  grid.High = static_cast<uint64_t>(std::floor((box.NorthLat - box.SouthLat) / grid.CellDeg)) + 2u;
  grid.Cells = static_cast<size_t>(grid.Wide * grid.High);
  if (grid.Wide > kWordMost / grid.High) {
    return std::unexpected(
        "the network's extent over its mean segment reach needs more squares than a 32-bit "
        "square index holds");
  }

  return grid;
}

void Network::CrossingsInCell(const Filing &filed,
                              CellSpan span,
                              const Gridded &grid,
                              Spanned box,
                              std::vector<Crossing> &into,
                              Swept &swept) {
  const uint32_t begins = span.From;
  const uint32_t ends = span.To;
  for (uint32_t one = begins; one + 1u < ends; ++one) {
    const Filed &ours = filed.InCell[one];
    const double loX = std::fmin(ours.Ax, ours.Bx);
    const double hiX = std::fmax(ours.Ax, ours.Bx);
    const double loY = std::fmin(ours.Ay, ours.By);
    const double hiY = std::fmax(ours.Ay, ours.By);
    for (uint32_t two = one + 1u; two < ends; ++two) {
      const Filed &yours = filed.InCell[two];
      if (ours.Way == yours.Way) { continue; }

      if (hiX < std::fmin(yours.Ax, yours.Bx) || std::fmax(yours.Ax, yours.Bx) < loX ||
          hiY < std::fmin(yours.Ay, yours.By) || std::fmax(yours.Ay, yours.By) < loY) {
        ++swept.PairsPruned;
        continue;
      }

      ++swept.PairsTested;
      const std::optional<Vec2> met =
          SegmentsMeet({.From = Vec2{{ours.Ax, ours.Ay}}, .To = Vec2{{ours.Bx, ours.By}}},
                       {.From = Vec2{{yours.Ax, yours.Ay}}, .To = Vec2{{yours.Bx, yours.By}}});
      if (!met || SquareIn(grid, box, {.LongitudeDeg = (*met)[0], .LatitudeDeg = (*met)[1]}) !=
                      span.Square) {
        continue;
      }
      into.push_back(Crossing{.OverWay = ours.Way,
                              .UnderWay = yours.Way,
                              .LatitudeDeg = (*met)[1],
                              .LongitudeDeg = AboutTheMeridian((*met)[0]),
                              .OverAt = static_cast<uint32_t>(filed.SegAt[ours.Seg]),
                              .UnderAt = static_cast<uint32_t>(filed.SegAt[yours.Seg])});
    }
  }
}

std::expected<Network::Swept, std::string_view>
Network::Crossings(std::vector<Crossing> &into) const {
  into.clear();
  Swept swept;
  const size_t points = Points_.size() / 2;
  if (Ways_.size() < 2 || points < 4) { return swept; }

  std::vector<double> lon(points, 0.0);
  const Spanned over = SpanOfPoints(lon);
  std::vector<uint32_t> segWay;
  std::vector<uint32_t> segAt;
  const size_t segments = SegmentsOfWays(segWay, segAt);
  if (segments < 2) { return swept; }

  const std::expected<Gridded, std::string_view> grid =
      GridOver({.Lon = lon, .SegAt = segAt, .Segments = segments}, over);
  if (!grid) { return std::unexpected(grid.error()); }
  const size_t cells = grid->Cells;
  const uint64_t wide = grid->Wide;

  std::vector<uint32_t> holds(cells + 1u, 0);
  const auto squareOf = [&](double atLon, double atLat) {
    return SquareIn(*grid, over, {.LongitudeDeg = atLon, .LatitudeDeg = atLat});
  };
  const auto bucketOf = [](uint32_t square) { return static_cast<size_t>(square); };

  const auto overSquares = [&](size_t seg, auto &&visit) {
    const size_t first = segAt[seg];
    const double lo = std::fmin(lon[first], lon[first + 1]);
    const double hi = std::fmax(lon[first], lon[first + 1]);
    const double bottom = std::fmin(Points_[2 * first], Points_[2 * first + 2]);
    const double top = std::fmax(Points_[2 * first], Points_[2 * first + 2]);
    const uint32_t from = squareOf(lo, bottom);
    const uint32_t to = squareOf(hi, top);
    for (uint32_t y = from / static_cast<uint32_t>(wide); y <= to / static_cast<uint32_t>(wide);
         ++y) {
      for (uint32_t x = from % static_cast<uint32_t>(wide); x <= to % static_cast<uint32_t>(wide);
           ++x) {
        visit(static_cast<uint32_t>(y * wide + x));
      }
    }
  };

  std::vector<Filed> inCell;
  {
    for (size_t seg = 0; seg < segments; ++seg) {
      overSquares(seg, [&](uint32_t square) { ++holds[bucketOf(square) + 1u]; });
    }
    for (size_t cell = 0; cell < cells; ++cell) { holds[cell + 1u] += holds[cell]; }
    std::vector<uint32_t> filled(holds.begin(), holds.end() - 1);
    inCell.assign(holds[cells], Filed{});
    for (size_t seg = 0; seg < segments; ++seg) {
      const size_t first = segAt[seg];
      const Filed held{.Seg = static_cast<uint32_t>(seg),
                       .Way = segWay[seg],
                       .Ax = lon[first],
                       .Ay = Points_[2 * first],
                       .Bx = lon[first + 1],
                       .By = Points_[2 * first + 2]};
      overSquares(seg, [&](uint32_t square) { inCell[filled[bucketOf(square)]++] = held; });
    }
  }
  for (size_t cell = 0; cell < cells; ++cell) {
    swept.FullestCell = std::max<size_t>(holds[cell + 1u] - holds[cell], swept.FullestCell);
  }

  for (size_t cell = 0; cell < cells; ++cell) {
    CrossingsInCell(
        {.SegAt = segAt, .InCell = inCell},
        {.Square = static_cast<uint32_t>(cell), .From = holds[cell], .To = holds[cell + 1u]},
        *grid,
        over,
        into,
        swept);
  }
  swept.Found = into.size();
  return swept;
}

size_t Network::JunctionCount() const {
  size_t junctions = 0;
  for (const Node &node : Nodes_) {
    if (node.EdgeCount > 2) { ++junctions; }
  }
  return junctions;
}

std::optional<Network::Found> Network::Nearest(LongitudeLatitude to) const {
  if (Nodes_.empty()) { return std::nullopt; }
  std::vector<size_t> found;
  for (int widening = 0;; ++widening) {
    const double reachM = 4.0 * SnapM_ * std::pow(4.0, widening);
    if (!(reachM < std::numbers::pi * RadiusM_)) { break; }
    Within(to, reachM, found);
    if (!found.empty()) { break; }
  }
  if (found.empty()) { Within(to, std::numbers::pi * RadiusM_, found); }
  size_t best = found.empty() ? 0 : found.front();
  double bestAway =
      ApartM({.LongitudeDeg = to.LongitudeDeg, .LatitudeDeg = to.LatitudeDeg},
             {.LongitudeDeg = Nodes_[best].LongitudeDeg, .LatitudeDeg = Nodes_[best].LatitudeDeg},
             Sphere{.RadiusM = RadiusM_});
  for (const size_t which : found) {
    const double away = ApartM(
        {.LongitudeDeg = to.LongitudeDeg, .LatitudeDeg = to.LatitudeDeg},
        {.LongitudeDeg = Nodes_[which].LongitudeDeg, .LatitudeDeg = Nodes_[which].LatitudeDeg},
        Sphere{.RadiusM = RadiusM_});
    if (away < bestAway) {
      bestAway = away;
      best = which;
    }
  }
  return Found{.Node = best, .AwayM = bestAway};
}

void Network::Within(LongitudeLatitude of, double reachM, std::vector<size_t> &nodes) const {
  nodes.clear();
  const int64_t across = static_cast<int64_t>(std::ceil(reachM / SnapM_)) + 1;
  const uint64_t cells =
      static_cast<uint64_t>(2 * across + 1) * static_cast<uint64_t>(2 * across + 1);
  if (Cells_.empty() || cells > Cells_.size()) {
    for (size_t which = 0; which < Nodes_.size(); ++which) {
      if (ApartM({.LongitudeDeg = of.LongitudeDeg, .LatitudeDeg = of.LatitudeDeg},
                 {.LongitudeDeg = Nodes_[which].LongitudeDeg,
                  .LatitudeDeg = Nodes_[which].LatitudeDeg},
                 Sphere{.RadiusM = RadiusM_}) <= reachM) {
        nodes.push_back(which);
      }
    }
    return;
  }
  const int64_t rowHere = RowOf(of.LatitudeDeg);
  const RowShape mine = ShapeRow(rowHere);
  for (int64_t row = rowHere - across; row <= rowHere + across; ++row) {
    const RowShape shape = ShapeRow(row);
    const int64_t reachCols = static_cast<int64_t>(std::ceil(static_cast<double>(across) *
                                                             mine.LonCellDeg / shape.LonCellDeg)) +
                              1;
    const int64_t span = shape.Columns < 2 * reachCols + 1 ? shape.Columns : 2 * reachCols + 1;
    const int64_t centre = ColumnIn(shape, of.LongitudeDeg);
    for (int64_t step = 0; step < span; ++step) {
      const int64_t column =
          ((centre + step - reachCols) % shape.Columns + shape.Columns) % shape.Columns;
      const auto seen = Cells_.find(KeyAt({.Row = row, .Column = column}));
      if (seen == Cells_.end()) { continue; }
      for (const size_t candidate : seen->second) {
        if (ApartM({.LongitudeDeg = of.LongitudeDeg, .LatitudeDeg = of.LatitudeDeg},
                   {.LongitudeDeg = Nodes_[candidate].LongitudeDeg,
                    .LatitudeDeg = Nodes_[candidate].LatitudeDeg},
                   Sphere{.RadiusM = RadiusM_}) <= reachM) {
          nodes.push_back(candidate);
        }
      }
    }
  }
}

size_t Network::Reaches(std::span<const size_t> from) const {
  std::vector<uint8_t> seen(Nodes_.size(), 0u);
  std::vector<size_t> walk;
  walk.reserve(Nodes_.size());
  size_t joined = 0;
  for (const size_t one : from) {
    if (one >= Nodes_.size() || seen[one] != 0u) { continue; }
    seen[one] = 1u;
    ++joined;
    walk.push_back(one);
  }
  for (size_t at = 0; at < walk.size(); ++at) {
    const Node &here = Nodes_[walk[at]];
    for (size_t which = 0; which < here.EdgeCount; ++which) {
      const size_t to = Edges_[here.FirstEdge + which].To;
      if (seen[to] != 0u) { continue; }
      seen[to] = 1u;
      ++joined;
      walk.push_back(to);
    }
  }
  return joined;
}

Network::Pieces Network::InPieces() const {
  Pieces out;
  std::vector<uint8_t> seen(Nodes_.size(), 0u);
  std::vector<size_t> walk;
  walk.reserve(Nodes_.size());
  for (size_t from = 0; from < Nodes_.size(); ++from) {
    if (seen[from] != 0u) { continue; }
    walk.clear();
    walk.push_back(from);
    seen[from] = 1u;
    size_t held = 1;
    for (size_t at = 0; at < walk.size(); ++at) {
      const Node &here = Nodes_[walk[at]];
      for (size_t which = 0; which < here.EdgeCount; ++which) {
        const size_t to = Edges_[here.FirstEdge + which].To;
        if (seen[to] != 0u) { continue; }
        seen[to] = 1u;
        ++held;
        walk.push_back(to);
      }
    }
    ++out.Count;
    out.Largest = std::max(held, out.Largest);
    if (held < 4) {
      ++out.UnderFour;
      out.InUnderFour += held;
    }
  }
  return out;
}

Route Network::Plan(LongitudeLatitude from, LongitudeLatitude to, double tightestM) const {
  Route out;
  out.StraightM = ApartM({.LongitudeDeg = from.LongitudeDeg, .LatitudeDeg = from.LatitudeDeg},
                         {.LongitudeDeg = to.LongitudeDeg, .LatitudeDeg = to.LatitudeDeg},
                         Sphere{.RadiusM = RadiusM_});
  if (!Woven_) {
    out.Error = "a network is planned over only after it is woven";
    return out;
  }

  const std::optional<Found> started = Nearest(from);
  const std::optional<Found> finished = Nearest(to);
  if (!started || !finished) {
    out.Error = "a network with no nodes has nothing to start from";
    return out;
  }
  const size_t start = started->Node;
  size_t finish = finished->Node;
  const double startAwayM = started->AwayM;
  const double finishAwayM = finished->AwayM;

  const double never = kBeyondAnyCoordinate;
  const size_t edges = Edges_.size();
  const auto kNoState = static_cast<size_t>(-1);
  std::vector<size_t> nearStart;
  Within(from, startAwayM + kStartReachM, nearStart);
  if (nearStart.empty()) { nearStart.push_back(start); }
  out.StartedFrom = nearStart.size();

  std::vector<size_t> nearFinish;
  const double arriveM = Nodes_[finish].HalfWidthM > 0.0 ? 2.0 * Nodes_[finish].HalfWidthM : SnapM_;
  Within(to, finishAwayM + arriveM, nearFinish);
  if (nearFinish.empty()) { nearFinish.push_back(finish); }
  std::vector<bool> arriving(Nodes_.size(), false);
  for (const size_t which : nearFinish) { arriving[which] = true; }
  out.ArrivedAt = nearFinish.size();

  const size_t states = edges + nearStart.size();
  std::vector<double> best(states, never);
  std::vector<size_t> came(states, kNoState);
  std::vector<bool> settled(states, false);
  std::vector<size_t> leaves(edges, 0);
  for (size_t node = 0; node < Nodes_.size(); ++node) {
    for (size_t which = 0; which < Nodes_[node].EdgeCount; ++which) {
      leaves[Nodes_[node].FirstEdge + which] = node;
    }
  }
  const auto standsAt = [&](size_t state) {
    return state < edges ? Edges_[state].To : nearStart[state - edges];
  };
  const auto goalM = [&](size_t node) {
    return ApartM(
        {.LongitudeDeg = Nodes_[node].LongitudeDeg, .LatitudeDeg = Nodes_[node].LatitudeDeg},
        {.LongitudeDeg = Nodes_[finish].LongitudeDeg, .LatitudeDeg = Nodes_[finish].LatitudeDeg},
        Sphere{.RadiusM = RadiusM_});
  };

  using Step = std::pair<double, size_t>;
  std::priority_queue<Step, std::vector<Step>, std::greater<>> open;
  for (size_t which = 0; which < nearStart.size(); ++which) {
    const size_t seed = nearStart[which];
    const double awayM =
        ApartM({.LongitudeDeg = from.LongitudeDeg, .LatitudeDeg = from.LatitudeDeg},
               {.LongitudeDeg = Nodes_[seed].LongitudeDeg, .LatitudeDeg = Nodes_[seed].LatitudeDeg},
               Sphere{.RadiusM = RadiusM_});
    const size_t state = edges + which;
    if (awayM >= best[state]) { continue; }
    best[state] = awayM;
    open.emplace(awayM + goalM(seed), state);
  }

  size_t reached = 0;
  std::vector<bool> nodeSeen(Nodes_.size(), false);
  size_t arrived = kNoState;
  while (!open.empty()) {
    const size_t state = open.top().second;
    open.pop();
    if (settled[state]) { continue; }
    settled[state] = true;
    const size_t node = standsAt(state);
    if (!nodeSeen[node]) {
      nodeSeen[node] = true;
      ++reached;
    }
    if (arriving[node]) {
      arrived = state;
      finish = node;
      break;
    }

    const Node &here = Nodes_[node];
    const bool hasBack = state < edges;
    double backEast = 0.0;
    double backNorth = 0.0;
    if (hasBack) {
      const Node &was = Nodes_[leaves[state]];
      backEast =
          LonApartDeg(here.LongitudeDeg, was.LongitudeDeg) * std::cos(here.LatitudeDeg * kDegToRad);
      backNorth = here.LatitudeDeg - was.LatitudeDeg;
    }
    for (size_t which = 0; which < here.EdgeCount; ++which) {
      const size_t next = here.FirstEdge + which;
      const Edge &edge = Edges_[next];
      if (hasBack && edge.LengthM > 0.0) {
        const Node &there = Nodes_[edge.To];
        const double onEast = LonApartDeg(there.LongitudeDeg, here.LongitudeDeg) *
                              std::cos(here.LatitudeDeg * kDegToRad);
        const double onNorth = there.LatitudeDeg - here.LatitudeDeg;
        const double wasLength = std::sqrt(backEast * backEast + backNorth * backNorth);
        const double onLength = std::sqrt(onEast * onEast + onNorth * onNorth);
        if (wasLength > 0.0 && onLength > 0.0) {
          const double turned = (backEast * onEast + backNorth * onNorth) / (wasLength * onLength);
          const double turnRad = std::acos(std::clamp(turned, -1.0, 1.0));
          const double half = 0.5 * turnRad;
          if (std::tan(half) > 0.0 && tightestM > 0.0 && edge.LengthM > 0.0 &&
              std::fabs(turnRad) > kLeastTurnRad) {
            const double shorter =
                edge.LengthM < Edges_[state].LengthM ? edge.LengthM : Edges_[state].LengthM;
            const double room = 0.5 * shorter / std::tan(half);
            if (room < tightestM) {
              ++out.TurnsRefused;
              continue;
            }
          }
        }
      }
      const double through = best[state] + edge.LengthM;
      if (through >= best[next]) { continue; }
      best[next] = through;
      came[next] = state;
      open.emplace(through + goalM(edge.To), next);
    }
  }

  out.Reached = reached;
  if (arrived == kNoState) {
    const size_t joined = Reaches(std::span<const size_t>(nearStart));
    const size_t joinedToEnd = Reaches(std::span<const size_t>(nearFinish));
    out.Component = joined;
    out.EndComponent = joinedToEnd;
    out.Error = "no chain of ways joins the two ends -- " + std::to_string(joined) + " nodes of " +
                std::to_string(Nodes_.size()) +
                " are joined to the start by ANY edge, and the search " + "settled " +
                std::to_string(reached) + " of those, while " + std::to_string(joinedToEnd) +
                " nodes are joined to the DESTINATION, so what separates the ends is " +
                (joined + 1 < Nodes_.size() ? std::string("the graph itself")
                                            : std::string("this search, not the graph"));
    return out;
  }

  std::vector<size_t> back;
  for (size_t state = arrived; state != kNoState; state = came[state]) {
    back.push_back(standsAt(state));
    if (back.size() > kMaxRouteLegs) {
      out.Error = "a route of more than " + std::to_string(kMaxRouteLegs) + " legs";
      return out;
    }
  }
  std::ranges::reverse(back);

  out.Legs.reserve(back.size());
  double alongM = 0.0;
  for (size_t which = 0; which < back.size(); ++which) {
    const Node &node = Nodes_[back[which]];
    if (which > 0) {
      const Node &was = Nodes_[back[which - 1]];
      alongM += ApartM({.LongitudeDeg = was.LongitudeDeg, .LatitudeDeg = was.LatitudeDeg},
                       {.LongitudeDeg = node.LongitudeDeg, .LatitudeDeg = node.LatitudeDeg},
                       Sphere{.RadiusM = RadiusM_});
    }
    Leg leg;
    leg.At.LatitudeDeg = node.LatitudeDeg;
    leg.At.LongitudeDeg = node.LongitudeDeg;
    leg.AlongM = alongM;
    leg.HalfWidthM = node.HalfWidthM;
    leg.Friction = node.Friction;
    leg.MaxGradient = node.MaxGradient;
    leg.MinRadiusM = node.MinRadiusM;
    leg.Lanes = node.Lanes;
    out.Legs.push_back(leg);
  }
  out.LengthM = alongM;
  out.Found = true;
  return out;
}

} // namespace outshine::Path
