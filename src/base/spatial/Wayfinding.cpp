#include "Units.h"
#include "Wayfinding.h"

#include <array>
#include <algorithm>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <numbers>
#include <algorithm>
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

constexpr double kNoLeastYet = 1.0e9;

namespace {

constexpr double kDegToRad = std::numbers::pi / kDegPerHalfTurn;

double MetresPerDegreeLat(double sphereRadiusM) {
  return sphereRadiusM * kDegToRad;
}

double MetresPerDegreeLon(double latDeg, double sphereRadiusM) {
  const double shrink = std::cos(latDeg * kDegToRad);
  return MetresPerDegreeLat(sphereRadiusM) * (shrink > kLeastRunM ? shrink : kLeastRunM);
}

[[nodiscard]] double LonApartDeg(double toDeg, double fromDeg) {
  const double apart = toDeg - fromDeg;
  return apart - kDegPerTurn * std::floor(apart / kDegPerTurn + 0.5);
}

} // namespace

double ApartM(
    double fromLatDeg, double fromLonDeg, double toLatDeg, double toLonDeg, double sphereRadiusM) {
  const double fromLat = fromLatDeg * kDegToRad;
  const double toLat = toLatDeg * kDegToRad;
  const double byLat = (toLatDeg - fromLatDeg) * kDegToRad;
  double byLon = toLonDeg - fromLonDeg;
  while (byLon > kDegPerHalfTurn) { byLon -= kDegPerTurn; }
  while (byLon < -kDegPerHalfTurn) { byLon += kDegPerTurn; }
  byLon *= kDegToRad;

  const double half =
      std::sin(0.5 * byLat) * std::sin(0.5 * byLat) +
      std::cos(fromLat) * std::cos(toLat) * std::sin(0.5 * byLon) * std::sin(0.5 * byLon);
  return 2.0 * sphereRadiusM * std::asin(std::sqrt(half < 1.0 ? half : 1.0));
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
  Points_.reserve(Points_.size() + 2 * points);
  WayOf_.reserve(WayOf_.size() + points);
  for (size_t which = 0; which < points; ++which) {
    Points_.push_back(latLonPairs[2 * which]);
    Points_.push_back(latLonPairs[2 * which + 1]);
    WayOf_.push_back(mine);
  }
  Woven_ = false;
}

Network::RowShape Network::ShapeRow(int64_t row) const {
  return ShapeRowOver(row, SnapM_);
}

Network::RowShape Network::ShapeRowOver(int64_t row, double cellM) const {
  RowShape shape;
  shape.Row = row;
  const double latCell = cellM / MetresPerDegreeLat(RadiusM_);
  const double rowLat = (static_cast<double>(row) + 0.5) * latCell;
  shape.LonCellDeg = cellM / MetresPerDegreeLon(rowLat, RadiusM_);
  const double columns = std::ceil(kDegPerTurn / shape.LonCellDeg);
  shape.Columns = columns > 1.0 ? static_cast<int64_t>(columns) : 1;
  return shape;
}

int64_t Network::RowOf(double latDeg) const {
  return RowOver(latDeg, SnapM_);
}

int64_t Network::RowOver(double latDeg, double cellM) const {
  const double latCell = cellM / MetresPerDegreeLat(RadiusM_);
  return static_cast<int64_t>(std::floor(latDeg / latCell));
}

int64_t Network::ColumnIn(const RowShape &shape, double lonDeg) {
  const double wrapped =
      lonDeg - kDegPerTurn * std::floor((lonDeg + kDegPerHalfTurn) / kDegPerTurn);
  const auto column =
      static_cast<int64_t>(std::floor((wrapped + kDegPerHalfTurn) / shape.LonCellDeg));
  return ((column % shape.Columns) + shape.Columns) % shape.Columns;
}

int64_t Network::KeyAt(int64_t row, int64_t column) {
  const auto high = static_cast<uint64_t>(row) << 32u;
  const auto low = static_cast<uint64_t>(column) & 0xffffffffULL;
  return static_cast<int64_t>(high ^ low);
}

int64_t Network::CellOf(double latDeg, double lonDeg) const {
  const int64_t row = RowOf(latDeg);
  return KeyAt(row, ColumnIn(ShapeRow(row), lonDeg));
}

size_t Network::Cross() {
  Joined_ = 0;
  LeftAlone_ = 0;
  std::vector<Crossing> found;
  const auto swept = Crossings(found);
  if (!swept) { return 0; }

  struct Cut {
    uint32_t After = 0;
    double LatDeg = 0.0, LonDeg = 0.0;
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
      const double fromLat = Points_[2 * sides[side]];
      const double fromLon = Points_[2 * sides[side] + 1];
      const double toLat = Points_[2 * sides[side] + 2];
      const double toLon = Points_[2 * sides[side] + 3];
      const double runLat = toLat - fromLat;
      const double runLon = LonApartDeg(toLon, fromLon);
      const double square = runLat * runLat + runLon * runLon;
      const double along =
          square > 0.0
              ? ((held.LatDeg - fromLat) * runLat + LonApartDeg(held.LonDeg, fromLon) * runLon) /
                    square
              : 0.0;
      perWay[ways[side]].push_back(
          Cut{.After = local, .LatDeg = held.LatDeg, .LonDeg = held.LonDeg, .Along = along});
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
        laid.push_back(cuts[next].LatDeg);
        laid.push_back(cuts[next].LonDeg);
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

  {
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

  std::vector<size_t> nodeOf(Points_.size() / 2, 0);
  std::unordered_map<int64_t, std::vector<size_t>> byCell;
  for (size_t point = 0; point < Points_.size() / 2; ++point) {
    const double latDeg = Points_[2 * point];
    const double lonDeg = Points_[2 * point + 1];

    size_t found = Nodes_.size();
    const int64_t rowHere = RowOf(latDeg);
    const RowShape mine = ShapeRow(rowHere);
    for (int64_t row = rowHere - 1; row <= rowHere + 1 && found == Nodes_.size(); ++row) {
      const RowShape shape = ShapeRow(row);
      const int64_t reachCols =
          static_cast<int64_t>(std::ceil(mine.LonCellDeg / shape.LonCellDeg)) + 1;
      const int64_t centre = ColumnIn(shape, lonDeg);
      const int64_t span = shape.Columns < 2 * reachCols + 1 ? shape.Columns : 2 * reachCols + 1;
      for (int64_t step = 0; step < span && found == Nodes_.size(); ++step) {
        const int64_t column =
            ((centre + step - reachCols) % shape.Columns + shape.Columns) % shape.Columns;
        const auto seen = byCell.find(KeyAt(row, column));
        if (seen == byCell.end()) { continue; }
        for (const size_t candidate : seen->second) {
          if (ApartM(
                  latDeg, lonDeg, Nodes_[candidate].LatDeg, Nodes_[candidate].LonDeg, RadiusM_) <=
              SnapM_) {
            found = candidate;
            break;
          }
        }
      }
    }

    if (found == Nodes_.size()) {
      Node made;
      made.LatDeg = latDeg;
      made.LonDeg = lonDeg;
      const Way &from = Ways_[WayOf_[point]];
      made.HalfWidthM = from.HalfWidthM;
      made.Friction = from.Friction;
      made.MaxGradient = from.MaxGradient;
      made.MinRadiusM = from.MinRadiusM;
      made.Lanes = from.Lanes;
      Nodes_.push_back(made);
      byCell[CellOf(latDeg, lonDeg)].push_back(found);
    } else {
      const Way &from = Ways_[WayOf_[point]];
      Nodes_[found].HalfWidthM = std::max(from.HalfWidthM, Nodes_[found].HalfWidthM);
      if (from.Friction > 0.0 &&
          (Nodes_[found].Friction <= 0.0 || from.Friction < Nodes_[found].Friction)) {
        Nodes_[found].Friction = from.Friction;
      }
      if (Nodes_[found].Lanes <= 0) { Nodes_[found].Lanes = from.Lanes; }
      if (Nodes_[found].MaxGradient <= 0.0 ||
          (from.MaxGradient > 0.0 && from.MaxGradient < Nodes_[found].MaxGradient)) {
        Nodes_[found].MaxGradient = from.MaxGradient;
      }
      if (from.MinRadiusM <= 0.0 || Nodes_[found].MinRadiusM <= 0.0) {
        Nodes_[found].MinRadiusM = 0.0;
      } else if (from.MinRadiusM < Nodes_[found].MinRadiusM) {
        Nodes_[found].MinRadiusM = from.MinRadiusM;
      }
    }
    nodeOf[point] = found;
  }

  std::vector<std::vector<Edge>> outgoing(Nodes_.size());
  for (const Way &way : Ways_) {
    for (size_t step = 1; step < way.Count; ++step) {
      const size_t from = nodeOf[way.First + step - 1];
      const size_t to = nodeOf[way.First + step];
      if (from == to) { continue; }
      const double lengthM = ApartM(
          Nodes_[from].LatDeg, Nodes_[from].LonDeg, Nodes_[to].LatDeg, Nodes_[to].LonDeg, RadiusM_);
      outgoing[from].push_back(Edge{.To = to, .LengthM = lengthM});
      outgoing[to].push_back(Edge{.To = from, .LengthM = lengthM});
    }
  }

  double tieReachM = SnapM_;
  for (const Node &held : Nodes_) {
    const double reachM = 2.0 * held.HalfWidthM;
    tieReachM = std::max(reachM, tieReachM);
  }

  std::unordered_map<int64_t, std::vector<std::pair<uint32_t, uint32_t>>> byEdgeCell;
  for (uint32_t from = 0; from < static_cast<uint32_t>(Nodes_.size()); ++from) {
    for (const Edge &edge : outgoing[from]) {
      if (static_cast<uint32_t>(edge.To) < from) { continue; }
      const Node &a = Nodes_[from];
      const Node &b = Nodes_[edge.To];
      const int64_t firstRow = RowOver(a.LatDeg < b.LatDeg ? a.LatDeg : b.LatDeg, tieReachM);
      const int64_t lastRow = RowOver(a.LatDeg < b.LatDeg ? b.LatDeg : a.LatDeg, tieReachM);
      for (int64_t row = firstRow; row <= lastRow; ++row) {
        const RowShape shape = ShapeRowOver(row, tieReachM);
        const int64_t one = ColumnIn(shape, a.LonDeg);
        const int64_t two = ColumnIn(shape, b.LonDeg);
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
          byEdgeCell[KeyAt(row, ((column % shape.Columns) + shape.Columns) % shape.Columns)]
              .emplace_back(from, static_cast<uint32_t>(edge.To));
        }
      }
    }
  }

  const auto overCells = [&](size_t from, size_t to, bool holding) {
    const Node &a = Nodes_[from];
    const Node &b = Nodes_[to];
    const auto one = static_cast<uint32_t>(from < to ? from : to);
    const auto two = static_cast<uint32_t>(from < to ? to : from);
    const int64_t firstRow = RowOver(a.LatDeg < b.LatDeg ? a.LatDeg : b.LatDeg, tieReachM);
    const int64_t lastRow = RowOver(a.LatDeg < b.LatDeg ? b.LatDeg : a.LatDeg, tieReachM);
    for (int64_t row = firstRow; row <= lastRow; ++row) {
      const RowShape shape = ShapeRowOver(row, tieReachM);
      const int64_t here = ColumnIn(shape, a.LonDeg);
      const int64_t there = ColumnIn(shape, b.LonDeg);
      const int64_t firstColumn = here < there ? here : there;
      const int64_t lastColumn = here < there ? there : here;
      for (int64_t column = firstColumn; column <= lastColumn; ++column) {
        const int64_t key = KeyAt(row, ((column % shape.Columns) + shape.Columns) % shape.Columns);
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
  };

  Tied_ = 0;
  for (size_t loose = 0; loose < Nodes_.size(); ++loose) {
    if (outgoing[loose].size() != 1) { continue; }
    const Node &end = Nodes_[loose];
    const double reachM = end.HalfWidthM > 0.0 ? 2.0 * end.HalfWidthM : SnapM_;
    const double perLatM = ApartM(end.LatDeg, end.LonDeg, end.LatDeg + 1.0, end.LonDeg, RadiusM_);
    const double perLonM = ApartM(end.LatDeg, end.LonDeg, end.LatDeg, end.LonDeg + 1.0, RadiusM_);

    size_t bestFrom = Nodes_.size();
    size_t bestTo = Nodes_.size();
    double bestM = reachM;
    const int64_t rowHere = RowOver(end.LatDeg, tieReachM);
    const int64_t rowReach = static_cast<int64_t>(std::ceil(reachM / tieReachM)) + 1;
    for (int64_t row = rowHere - rowReach; row <= rowHere + rowReach; ++row) {
      const RowShape shape = ShapeRowOver(row, tieReachM);
      const int64_t centre = ColumnIn(shape, end.LonDeg);
      const int64_t colReach =
          static_cast<int64_t>(std::ceil(reachM / (shape.LonCellDeg * perLonM))) + 1;
      for (int64_t step = -colReach; step <= colReach; ++step) {
        const int64_t column = ((centre + step) % shape.Columns + shape.Columns) % shape.Columns;
        const auto seen = byEdgeCell.find(KeyAt(row, column));
        if (seen == byEdgeCell.end()) { continue; }
        for (const auto &held : seen->second) {
          const size_t near = held.first;
          const size_t over = held.second;
          if (near == loose || over == loose) { continue; }
          {
            const double ax = (Nodes_[near].LonDeg - end.LonDeg) * perLonM;
            const double ay = (Nodes_[near].LatDeg - end.LatDeg) * perLatM;
            const double bx = (Nodes_[over].LonDeg - end.LonDeg) * perLonM;
            const double by = (Nodes_[over].LatDeg - end.LatDeg) * perLatM;
            const double dx = bx - ax;
            const double dy = by - ay;
            const double span = dx * dx + dy * dy;
            double along = span > 0.0 ? -(ax * dx + ay * dy) / span : 0.0;
            along = std::clamp(along, 0.0, 1.0);
            const double cx = ax + along * dx;
            const double cy = ay + along * dy;
            const double awayM = std::sqrt(cx * cx + cy * cy);
            const double touchM = end.HalfWidthM + Nodes_[near].HalfWidthM;
            if (awayM >= bestM || awayM > touchM) { continue; }
            bestM = awayM;
            bestFrom = near;
            bestTo = over;
          }
        }
      }
    }
    if (bestFrom == Nodes_.size()) { continue; }

    const auto unlink = [&outgoing](size_t from, size_t to) {
      std::vector<Edge> &held = outgoing[from];
      for (size_t at = 0; at < held.size(); ++at) {
        if (held[at].To != to) { continue; }
        held.erase(held.begin() + static_cast<ptrdiff_t>(at));
        return true;
      }
      return false;
    };
    if (!unlink(bestFrom, bestTo) || !unlink(bestTo, bestFrom)) { continue; }
    overCells(bestFrom, bestTo, false);
    const auto link = [this, &outgoing](size_t from, size_t to) {
      const double lengthM = ApartM(
          Nodes_[from].LatDeg, Nodes_[from].LonDeg, Nodes_[to].LatDeg, Nodes_[to].LonDeg, RadiusM_);
      outgoing[from].push_back(Edge{.To = to, .LengthM = lengthM});
      outgoing[to].push_back(Edge{.To = from, .LengthM = lengthM});
    };
    link(bestFrom, loose);
    link(loose, bestTo);
    overCells(bestFrom, loose, true);
    overCells(loose, bestTo, true);
    ++Tied_;
  }

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

[[nodiscard]] bool SegmentsMeet(double ax,
                                double ay,
                                double bx,
                                double by,
                                double cx,
                                double cy,
                                double dx,
                                double dy,
                                double *atX,
                                double *atY) {
  const double rx = bx - ax;
  const double ry = by - ay;
  const double sx = dx - cx;
  const double sy = dy - cy;
  const double denominator = rx * sy - ry * sx;
  if (denominator == 0.0) { return false; }
  const double along = ((cx - ax) * sy - (cy - ay) * sx) / denominator;
  const double across = ((cx - ax) * ry - (cy - ay) * rx) / denominator;
  if (along <= 0.0 || along >= 1.0 || across <= 0.0 || across >= 1.0) { return false; }
  *atX = ax + along * rx;
  *atY = ay + along * ry;
  return true;
}

} // namespace

namespace {

struct Filed {
  uint32_t Square = 0;
  uint32_t Seg = 0;
};

static_assert(sizeof(Filed) == 8);
static_assert(std::is_trivially_copyable_v<Filed>);

} // namespace

std::expected<Network::Swept, std::string_view>
Network::Crossings(std::vector<Crossing> &into) const {
  into.clear();
  Swept swept;
  const size_t points = Points_.size() / 2;
  if (Ways_.size() < 2 || points < 4) { return swept; }

  const double aboutLon = Points_[1];
  std::vector<double> lon(points, 0.0);
  double westLon = kNoLeastYet;
  double eastLon = -kNoLeastYet;
  double southLat = kNoLeastYet;
  double northLat = -kNoLeastYet;
  for (size_t at = 0; at < points; ++at) {
    double away = Points_[2 * at + 1] - aboutLon;
    while (away > kDegPerHalfTurn) { away -= kDegPerTurn; }
    while (away < -kDegPerHalfTurn) { away += kDegPerTurn; }
    lon[at] = aboutLon + away;
    westLon = lon[at] < westLon ? lon[at] : westLon;
    eastLon = lon[at] > eastLon ? lon[at] : eastLon;
    const double lat = Points_[2 * at];
    southLat = lat < southLat ? lat : southLat;
    northLat = lat > northLat ? lat : northLat;
  }

  size_t segments = 0;
  for (const Way &way : Ways_) { segments += way.Count > 1 ? way.Count - 1 : 0; }
  if (segments < 2) { return swept; }

  std::vector<uint32_t> segWay(segments, 0);
  std::vector<uint32_t> segAt(segments, 0);
  {
    size_t made = 0;
    for (size_t which = 0; which < Ways_.size(); ++which) {
      const Way &way = Ways_[which];
      for (size_t a = 0; a + 1 < way.Count; ++a) {
        segWay[made] = static_cast<uint32_t>(which);
        segAt[made] = static_cast<uint32_t>(way.First + a);
        ++made;
      }
    }
  }

  double reachSum = 0.0;
  for (size_t seg = 0; seg < segments; ++seg) {
    const size_t first = segAt[seg];
    const double byLon = std::fabs(lon[first + 1] - lon[first]);
    const double byLat = std::fabs(Points_[2 * first + 2] - Points_[2 * first]);
    reachSum += byLon > byLat ? byLon : byLat;
  }
  const double cellDeg = reachSum > 0.0 ? 2.0 * reachSum / static_cast<double>(segments) : 1.0;
  const size_t cells = 2u * segments + 1u;

  const uint64_t wide = static_cast<uint64_t>(std::floor((eastLon - westLon) / cellDeg)) + 2u;
  const uint64_t high = static_cast<uint64_t>(std::floor((northLat - southLat) / cellDeg)) + 2u;
  if (wide > kWordMost / high) {
    return std::unexpected(
        "the network's extent over its mean segment reach needs more squares than a 32-bit "
        "square index holds");
  }

  std::vector<uint32_t> holds(cells + 1u, 0);
  const auto squareOf = [&](double atLon, double atLat) -> uint32_t {
    const double fx = std::floor((atLon - westLon) / cellDeg);
    const double fy = std::floor((atLat - southLat) / cellDeg);
    const uint64_t x = fx <= 0.0 ? 0u : static_cast<uint64_t>(fx);
    const uint64_t y = fy <= 0.0 ? 0u : static_cast<uint64_t>(fy);
    return static_cast<uint32_t>((y < high ? y : high - 1u) * wide + (x < wide ? x : wide - 1u));
  };
  const auto bucketOf = [&](uint32_t square) {
    return static_cast<size_t>((static_cast<uint64_t>(square) * 2654435761ull) %
                               static_cast<uint64_t>(cells));
  };

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

  for (size_t seg = 0; seg < segments; ++seg) {
    overSquares(seg, [&](uint32_t square) { ++holds[bucketOf(square) + 1u]; });
  }
  for (size_t cell = 0; cell < cells; ++cell) { holds[cell + 1u] += holds[cell]; }
  std::vector<uint32_t> filled(holds.begin(), holds.end() - 1);
  std::vector<Filed> inCell(holds[cells], Filed{});
  for (size_t seg = 0; seg < segments; ++seg) {
    overSquares(seg, [&](uint32_t square) {
      inCell[filled[bucketOf(square)]++] =
          Filed{.Square = square, .Seg = static_cast<uint32_t>(seg)};
    });
  }
  for (size_t cell = 0; cell < cells; ++cell) {
    const size_t held = holds[cell + 1u] - holds[cell];
    swept.FullestCell = held > swept.FullestCell ? held : swept.FullestCell;
  }

  for (size_t cell = 0; cell < cells; ++cell) {
    const uint32_t begins = holds[cell];
    const uint32_t ends = holds[cell + 1u];
    for (uint32_t one = begins; one + 1u < ends; ++one) {
      const Filed &ours = inCell[one];
      const size_t mine = ours.Seg;
      const size_t firstA = segAt[mine];
      const double ax = lon[firstA];
      const double ay = Points_[2 * firstA];
      const double bx = lon[firstA + 1];
      const double by = Points_[2 * firstA + 2];
      for (uint32_t two = one + 1u; two < ends; ++two) {
        const Filed &yours = inCell[two];
        const size_t theirs = yours.Seg;
        if (segWay[mine] == segWay[theirs]) { continue; }
        if (ours.Square != yours.Square) { continue; }
        ++swept.PairsTested;
        const size_t firstB = segAt[theirs];
        const double cx = lon[firstB];
        const double cy = Points_[2 * firstB];
        const double dx = lon[firstB + 1];
        const double dy = Points_[2 * firstB + 2];
        double atX = 0.0;
        double atY = 0.0;
        if (!SegmentsMeet(ax, ay, bx, by, cx, cy, dx, dy, &atX, &atY)) { continue; }
        if (squareOf(atX, atY) != ours.Square) { continue; }
        double back = atX;
        while (back > kDegPerHalfTurn) { back -= kDegPerTurn; }
        while (back < -kDegPerHalfTurn) { back += kDegPerTurn; }
        into.push_back(Crossing{.OverWay = static_cast<uint32_t>(segWay[mine]),
                                .UnderWay = static_cast<uint32_t>(segWay[theirs]),
                                .LatDeg = atY,
                                .LonDeg = back,
                                .OverAt = static_cast<uint32_t>(firstA),
                                .UnderAt = static_cast<uint32_t>(firstB)});
      }
    }
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

bool Network::Nearest(const Waypoint &to, size_t &node, double &awayM) const {
  if (Nodes_.empty()) { return false; }
  std::vector<size_t> found;
  for (double reachM = 4.0 * SnapM_; reachM < std::numbers::pi * RadiusM_; reachM *= 4.0) {
    Within(to, reachM, found);
    if (!found.empty()) { break; }
  }
  if (found.empty()) { Within(to, std::numbers::pi * RadiusM_, found); }
  size_t best = found.empty() ? 0 : found.front();
  double bestAway =
      ApartM(to.LatDeg, to.LonDeg, Nodes_[best].LatDeg, Nodes_[best].LonDeg, RadiusM_);
  for (const size_t which : found) {
    const double away =
        ApartM(to.LatDeg, to.LonDeg, Nodes_[which].LatDeg, Nodes_[which].LonDeg, RadiusM_);
    if (away < bestAway) {
      bestAway = away;
      best = which;
    }
  }
  node = best;
  awayM = bestAway;
  return true;
}

void Network::Within(const Waypoint &of, double reachM, std::vector<size_t> &nodes) const {
  nodes.clear();
  const int64_t across = static_cast<int64_t>(std::ceil(reachM / SnapM_)) + 1;
  const uint64_t cells =
      static_cast<uint64_t>(2 * across + 1) * static_cast<uint64_t>(2 * across + 1);
  if (Cells_.empty() || cells > Cells_.size()) {
    for (size_t which = 0; which < Nodes_.size(); ++which) {
      if (ApartM(of.LatDeg, of.LonDeg, Nodes_[which].LatDeg, Nodes_[which].LonDeg, RadiusM_) <=
          reachM) {
        nodes.push_back(which);
      }
    }
    return;
  }
  const int64_t rowHere = RowOf(of.LatDeg);
  const RowShape mine = ShapeRow(rowHere);
  for (int64_t row = rowHere - across; row <= rowHere + across; ++row) {
    const RowShape shape = ShapeRow(row);
    const int64_t reachCols = static_cast<int64_t>(std::ceil(static_cast<double>(across) *
                                                             mine.LonCellDeg / shape.LonCellDeg)) +
                              1;
    const int64_t span = shape.Columns < 2 * reachCols + 1 ? shape.Columns : 2 * reachCols + 1;
    const int64_t centre = ColumnIn(shape, of.LonDeg);
    for (int64_t step = 0; step < span; ++step) {
      const int64_t column =
          ((centre + step - reachCols) % shape.Columns + shape.Columns) % shape.Columns;
      const auto seen = Cells_.find(KeyAt(row, column));
      if (seen == Cells_.end()) { continue; }
      for (const size_t candidate : seen->second) {
        if (ApartM(of.LatDeg,
                   of.LonDeg,
                   Nodes_[candidate].LatDeg,
                   Nodes_[candidate].LonDeg,
                   RadiusM_) <= reachM) {
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

Route Network::Plan(const Waypoint &from, const Waypoint &to, double tightestM) const {
  Route out;
  out.StraightM = ApartM(from.LatDeg, from.LonDeg, to.LatDeg, to.LonDeg, RadiusM_);
  if (!Woven_) {
    out.Error = "a network is planned over only after it is woven";
    return out;
  }

  size_t start = 0;
  size_t finish = 0;
  double startAwayM = 0.0;
  double finishAwayM = 0.0;
  if (!Nearest(from, start, startAwayM) || !Nearest(to, finish, finishAwayM)) {
    out.Error = "a network with no nodes has nothing to start from";
    return out;
  }

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
    return ApartM(Nodes_[node].LatDeg,
                  Nodes_[node].LonDeg,
                  Nodes_[finish].LatDeg,
                  Nodes_[finish].LonDeg,
                  RadiusM_);
  };

  using Step = std::pair<double, size_t>;
  std::priority_queue<Step, std::vector<Step>, std::greater<>> open;
  for (size_t which = 0; which < nearStart.size(); ++which) {
    const size_t seed = nearStart[which];
    const double awayM =
        ApartM(from.LatDeg, from.LonDeg, Nodes_[seed].LatDeg, Nodes_[seed].LonDeg, RadiusM_);
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
      backEast = LonApartDeg(here.LonDeg, was.LonDeg) * std::cos(here.LatDeg * kDegToRad);
      backNorth = here.LatDeg - was.LatDeg;
    }
    for (size_t which = 0; which < here.EdgeCount; ++which) {
      const size_t next = here.FirstEdge + which;
      const Edge &edge = Edges_[next];
      if (hasBack && edge.LengthM > 0.0) {
        const Node &there = Nodes_[edge.To];
        const double onEast =
            LonApartDeg(there.LonDeg, here.LonDeg) * std::cos(here.LatDeg * kDegToRad);
        const double onNorth = there.LatDeg - here.LatDeg;
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
      alongM += ApartM(was.LatDeg, was.LonDeg, node.LatDeg, node.LonDeg, RadiusM_);
    }
    Leg leg;
    leg.At.LatDeg = node.LatDeg;
    leg.At.LonDeg = node.LonDeg;
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
