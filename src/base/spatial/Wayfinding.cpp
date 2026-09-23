#include "math/Units.h"
#include "Wayfinding.h"

#include <cstdio>

#include <array>
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <numbers>
#include <numeric>
#include <optional>
#include <cmath>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <tuple>
#include <queue>
#include <ratio>
#include <vector>
#include <utility>
#include <unordered_map>
#include <unordered_set>

namespace outshine::Path {

namespace Says {
constexpr auto kSearchCostOverflow =
    "route search encountered costs outside the finite metre range";
constexpr auto kRouteLegBudget = "route exceeds the configured leg budget";
constexpr auto kRouteLengthOverflow = "route length exceeds finite metre range";
constexpr auto kUnbuiltNetwork = "transport network must be rebuilt after source changes";
constexpr auto kInvalidSpatialQuery =
    "spatial query requires canonical finite coordinates and a finite nonnegative radius";
constexpr auto kInvalidNetworkGrid = "transport grid requires finite positive radius and cell size "
                                     "with representable global indices";
constexpr auto kInvalidWayCoordinates = "transport way contains invalid geographic coordinates";
constexpr auto kInvalidWayPoints =
    "a transport way requires at least two complete latitude/longitude pairs";
constexpr auto kNetworkPointBudget = "transport network point budget exceeded";
constexpr auto kInvalidWayProperties =
    "transport way physical properties must be finite and nonnegative";
constexpr auto kInvalidRouteCoordinates =
    "route coordinates must be finite with longitude in [-180,180] and latitude in [-90,90]";
constexpr auto kInvalidTurnRadius = "minimum turn radius must be finite and nonnegative";
}

constexpr uint64_t kWordMost = 0xFFFFFFFFull;

namespace {

constexpr double kDegToRad = std::numbers::pi / kDegPerHalfTurn;
constexpr size_t kNoSearchState = std::numeric_limits<size_t>::max();

[[nodiscard]] bool ValidCoordinates(LongitudeLatitude at) {
  return std::isfinite(at.LongitudeDeg) && std::isfinite(at.LatitudeDeg) &&
         std::abs(at.LongitudeDeg) <= kDegPerHalfTurn &&
         std::abs(at.LatitudeDeg) <= kDegPerHalfTurn / 2.0;
}

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

}

uint64_t Network::PhysicalEdgeKey(size_t from, size_t to) {
  static_assert(kMaxNetworkPoints <= std::numeric_limits<uint32_t>::max());
  return (static_cast<uint64_t>(std::min(from, to))
          << static_cast<unsigned>(std::numeric_limits<uint32_t>::digits)) |
         static_cast<uint64_t>(std::max(from, to));
}

void Network::PhysicalAdjacency::Adopt(size_t nodes, std::unordered_set<uint64_t> &&edges) {
  Degree_.assign(nodes, 0);
  Edges_ = std::move(edges);
  DegreeCursor_ = Edges_.cbegin();
}

void Network::PhysicalAdjacency::Connect(size_t from, size_t to) {
  if (Edges_.insert(Network::PhysicalEdgeKey(from, to)).second) {
    ++Degree_[from];
    ++Degree_[to];
  }
}

void Network::PhysicalAdjacency::Disconnect(size_t from, size_t to) {
  if (Edges_.erase(Network::PhysicalEdgeKey(from, to)) != 0) {
    --Degree_[from];
    --Degree_[to];
  }
}

bool Network::PhysicalAdjacency::AccumulateDegrees(size_t itemsMost) {
  if (!DegreeCursor_) { return true; }
  size_t visited = 0;
  while (*DegreeCursor_ != Edges_.cend() && visited < itemsMost) {
    const uint64_t key = **DegreeCursor_;
    ++*DegreeCursor_;
    ++Degree_[static_cast<size_t>(key >> 32u)];
    ++Degree_[static_cast<size_t>(static_cast<uint32_t>(key))];
    ++visited;
  }
  if (*DegreeCursor_ != Edges_.cend()) { return false; }
  DegreeCursor_.reset();
  return true;
}

bool Network::PhysicalAdjacency::Release(size_t itemsMost) {
  size_t released = 0;
  while (!Edges_.empty() && released < itemsMost) {
    Edges_.erase(Edges_.begin());
    ++released;
  }
  if (!Edges_.empty()) { return false; }
  Degree_.clear();
  return true;
}

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

std::expected<Network, std::string_view> Network::Create(Snap snap, Sphere on) {
  if (!std::isfinite(snap.CellM) || snap.CellM <= 0.0 || !std::isfinite(on.RadiusM) ||
      on.RadiusM <= 0.0) {
    return std::unexpected(Says::kInvalidNetworkGrid);
  }
  const double metresPerDegree = MetresPerDegreeLat(on.RadiusM);
  const double circumferenceM = kDegPerTurn * metresPerDegree;
  const double latitudeCellDeg = snap.CellM / metresPerDegree;
  const double columns = std::ceil(kDegPerTurn / latitudeCellDeg);
  if (!std::isfinite(circumferenceM) || circumferenceM <= 0.0 || !std::isfinite(latitudeCellDeg) ||
      latitudeCellDeg <= 0.0 || latitudeCellDeg > kDegPerTurn || !std::isfinite(columns) ||
      columns > static_cast<double>(std::numeric_limits<uint32_t>::max()) ||
      metresPerDegree * kLeastRunM <= 0.0) {
    return std::unexpected(Says::kInvalidNetworkGrid);
  }
  return Network(snap, on);
}

std::expected<void, std::string_view> Network::Lay(std::span<const double> latLonPairs,
                                                   const WayClass &of) {
  if (latLonPairs.size() < 4 || latLonPairs.size() % 2 != 0) {
    return std::unexpected(Says::kInvalidWayPoints);
  }
  const size_t points = latLonPairs.size() / 2;
  if (points > kMaxNetworkPoints || Points_.size() / 2 > kMaxNetworkPoints - points) {
    return std::unexpected(Says::kNetworkPointBudget);
  }
  for (size_t at = 0; at < points; ++at) {
    if (!ValidCoordinates(
            {.LongitudeDeg = latLonPairs[2 * at + 1], .LatitudeDeg = latLonPairs[2 * at]})) {
      return std::unexpected(Says::kInvalidWayCoordinates);
    }
  }
  for (const double value :
       {of.HalfWidthM, of.MaxGradient, of.MinRadiusM, of.Friction, of.SpeedMps}) {
    if (!std::isfinite(value) || value < 0.0) {
      return std::unexpected(Says::kInvalidWayProperties);
    }
  }
  if (of.Lanes < 0 || !std::isfinite(2.0 * of.HalfWidthM)) {
    return std::unexpected(Says::kInvalidWayProperties);
  }
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
  CachedCrossings_.clear();
  CachedSweep_.reset();

  for (size_t which = 0; which < points; ++which) {
    Points_.push_back(latLonPairs[2 * which]);
    Points_.push_back(latLonPairs[2 * which + 1]);
    WayOf_.push_back(mine);
  }
  Woven_ = false;
  return {};
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
  CachedCrossings_.clear();
  CachedSweep_.reset();
  Woven_ = false;
  return Joined_;
}

bool Network::WayLess(size_t a, size_t b) const {
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
  if (wa.Lanes != wb.Lanes) { return wa.Lanes < wb.Lanes; }
  const auto left =
      std::tie(wa.Friction, wa.SpeedMps, wa.Priority, wa.Oneway, wa.Sealed, wa.Spans, wa.Tag);
  const auto right =
      std::tie(wb.Friction, wb.SpeedMps, wb.Priority, wb.Oneway, wb.Sealed, wb.Spans, wb.Tag);
  return left == right ? a < b : left < right;
}

void Network::SortWaysIntoDeclaredOrder() {
  std::vector<size_t> order(Ways_.size());
  for (size_t at = 0; at < order.size(); ++at) { order[at] = at; }
  std::ranges::sort(order, [this](size_t a, size_t b) { return WayLess(a, b); });
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

void Network::SnapOnePoint(size_t point, std::vector<size_t> &nodeOf, CellsByKey &byCell) {
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

void Network::SnapPointsIntoNodes(std::vector<size_t> &nodeOf, CellsByKey &byCell) {
  for (size_t point = 0; point < Points_.size() / 2; ++point) {
    SnapOnePoint(point, nodeOf, byCell);
  }
}

void Network::AppendWayEdge(const Way &way,
                            size_t step,
                            std::span<const size_t> nodeOf,
                            OutgoingEdges &outgoing) const {
  const size_t from = nodeOf[way.First + step - 1];
  const size_t to = nodeOf[way.First + step];
  if (from == to) { return; }
  const double lengthM =
      ApartM({.LongitudeDeg = Nodes_[from].LongitudeDeg, .LatitudeDeg = Nodes_[from].LatitudeDeg},
             {.LongitudeDeg = Nodes_[to].LongitudeDeg, .LatitudeDeg = Nodes_[to].LatitudeDeg},
             Sphere{.RadiusM = RadiusM_});
  outgoing[from].push_back(Edge{.To = to, .LengthM = lengthM});
  if (!way.Oneway) { outgoing[to].push_back(Edge{.To = from, .LengthM = lengthM}); }
}

void Network::EdgesFromWays(std::span<const size_t> nodeOf, OutgoingEdges &outgoing) const {
  for (const Way &way : Ways_) {
    for (size_t step = 1; step < way.Count; ++step) { AppendWayEdge(way, step, nodeOf, outgoing); }
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
      if (!IndexEdgeCell(
              ends,
              {.Row = row, .Column = ((column % shape.Columns) + shape.Columns) % shape.Columns},
              byEdgeCell,
              error)) {
        return false;
      }
    }
  }
  return true;
}

size_t Network::EdgesByCell::ShardOf(int64_t key) noexcept {
  const auto bits = static_cast<uint64_t>(key);
  return static_cast<size_t>((bits ^ (bits >> 32u)) & (kShards - 1u));
}

Network::EdgesByCell::Entries &Network::EdgesByCell::Cell(int64_t key) {
  return Shards_[ShardOf(key)][key];
}

const Network::EdgesByCell::Entries *Network::EdgesByCell::Find(int64_t key) const {
  const auto &shard = Shards_[ShardOf(key)];
  const auto found = shard.find(key);
  return found == shard.end() ? nullptr : &found->second;
}

bool Network::EdgesByCell::Release(size_t itemsMost) {
  size_t released = 0;
  while (NextReleaseShard_ < kShards && released < itemsMost) {
    auto &shard = Shards_[NextReleaseShard_];
    if (shard.empty()) {
      ++NextReleaseShard_;
      continue;
    }
    shard.erase(shard.begin());
    ++released;
  }
  while (NextReleaseShard_ < kShards && Shards_[NextReleaseShard_].empty()) { ++NextReleaseShard_; }
  return NextReleaseShard_ == kShards;
}

bool Network::IndexEdgeCell(EdgeEnds ends,
                            RowColumn at,
                            EdgesByCell &byEdgeCell,
                            std::string &error) {
  if (IndexedCells_ >= kMaxNetworkPoints) {
    error = "the tie index would hold more than " + std::to_string(kMaxNetworkPoints) +
            " cells for " + std::to_string(Nodes_.size()) +
            " nodes, which is a graph this network cannot weave at a snap of " +
            std::to_string(SnapM_) + " m";
    return false;
  }
  ++IndexedCells_;
  byEdgeCell.Cell(KeyAt(at)).emplace_back(static_cast<uint32_t>(ends.From),
                                          static_cast<uint32_t>(ends.To));
  return true;
}

bool Network::IndexEdgesByCell(const OutgoingEdges &outgoing,
                               double tieReachM,
                               EdgesByCell &byEdgeCell,
                               std::string &error) {
  std::unordered_set<uint64_t> indexed;
  for (size_t from = 0; from < Nodes_.size(); ++from) {
    for (const Edge &edge : outgoing[from]) {
      if (!indexed.insert(PhysicalEdgeKey(from, edge.To)).second) { continue; }
      if (!IndexOneEdge({.From = std::min(from, edge.To), .To = std::max(from, edge.To)},
                        tieReachM,
                        byEdgeCell,
                        error)) {
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
      std::vector<std::pair<uint32_t, uint32_t>> &cell = byEdgeCell.Cell(key);
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
      const auto *const seen = byEdgeCell.Find(KeyAt({.Row = row, .Column = column}));
      if (seen == nullptr) { continue; }
      for (const auto &held : *seen) {
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
  const auto count = [&outgoing](size_t from, size_t to) {
    return std::ranges::count(outgoing[from], to, &Edge::To);
  };
  const auto forward = count(best.From, best.To);
  const auto reverse = count(best.To, best.From);
  if (forward == 0 && reverse == 0) { return false; }
  const auto unlink = [&outgoing](size_t from, size_t to) {
    std::erase_if(outgoing[from], [to](const Edge &edge) { return edge.To == to; });
  };
  unlink(best.From, best.To);
  unlink(best.To, best.From);
  MarkEdgeOverCells({.From = best.From, .To = best.To}, false, tieReachM, byEdgeCell);

  const auto link = [this, &outgoing](size_t from, size_t to) {
    const double lengthM =
        ApartM({.LongitudeDeg = Nodes_[from].LongitudeDeg, .LatitudeDeg = Nodes_[from].LatitudeDeg},
               {.LongitudeDeg = Nodes_[to].LongitudeDeg, .LatitudeDeg = Nodes_[to].LatitudeDeg},
               Sphere{.RadiusM = RadiusM_});
    outgoing[from].push_back(Edge{.To = to, .LengthM = lengthM});
  };
  for (std::ptrdiff_t at = 0; at < forward; ++at) {
    link(best.From, loose);
    link(loose, best.To);
  }
  for (std::ptrdiff_t at = 0; at < reverse; ++at) {
    link(best.To, loose);
    link(loose, best.From);
  }
  MarkEdgeOverCells({.From = best.From, .To = loose}, true, tieReachM, byEdgeCell);
  MarkEdgeOverCells({.From = loose, .To = best.To}, true, tieReachM, byEdgeCell);
  return true;
}

bool Network::TieLooseEnds(OutgoingEdges &outgoing, std::string &error) {
  const double tieReachM = TieReachM();
  EdgesByCell byEdgeCell;
  if (!IndexEdgesByCell(outgoing, tieReachM, byEdgeCell, error)) { return false; }

  PhysicalAdjacency adjacency(Nodes_.size());
  for (size_t from = 0; from < outgoing.size(); ++from) {
    for (const Edge &edge : outgoing[from]) { adjacency.Connect(from, edge.To); }
  }
  Tied_ = 0;
  for (size_t loose = 0; loose < Nodes_.size(); ++loose) {
    if (adjacency.Degree(loose) != 1) { continue; }
    const NearestEdge best = NearestEdgeTo(loose, byEdgeCell, tieReachM);
    if (best.From == Nodes_.size()) { continue; }
    if (SpliceInto(loose, best, tieReachM, outgoing, byEdgeCell)) {
      adjacency.Disconnect(best.From, best.To);
      adjacency.Connect(best.From, loose);
      adjacency.Connect(loose, best.To);
      ++Tied_;
    }
  }
  return true;
}

bool Network::PrepareWeave(std::string &error) {
  CachedCrossings_.clear();
  CachedSweep_.reset();
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
  return true;
}

bool Network::Weave(std::string &error, WeaveTimings *timings) {
  if (!PrepareWeave(error)) { return false; }

  auto phaseBegan = std::chrono::steady_clock::now();
  const auto phaseMs = [&phaseBegan] {
    const auto now = std::chrono::steady_clock::now();
    const double ms = std::chrono::duration<double, std::milli>(now - phaseBegan).count();
    phaseBegan = now;
    return ms;
  };
  SortWaysIntoDeclaredOrder();
  if (timings != nullptr) { timings->SortMs = phaseMs(); }

  std::vector<size_t> nodeOf(Points_.size() / 2, 0);
  CellsByKey byCell;
  SnapPointsIntoNodes(nodeOf, byCell);
  if (timings != nullptr) { timings->SnapMs = phaseMs(); }

  std::vector<std::vector<Edge>> outgoing(Nodes_.size());
  EdgesFromWays(nodeOf, outgoing);
  if (timings != nullptr) { timings->EdgesMs = phaseMs(); }
  if (!TieLooseEnds(outgoing, error)) { return false; }
  if (timings != nullptr) { timings->TieMs = phaseMs(); }

  for (size_t node = 0; node < Nodes_.size(); ++node) {
    Nodes_[node].FirstEdge = Edges_.size();
    Nodes_[node].EdgeCount = outgoing[node].size();
    for (const Edge &edge : outgoing[node]) { Edges_.push_back(edge); }
  }

  NodeOfPoint_.resize(nodeOf.size());
  for (size_t at = 0; at < nodeOf.size(); ++at) {
    NodeOfPoint_[at] = static_cast<uint32_t>(nodeOf[at]);
  }
  Cells_ = std::move(byCell);
  Woven_ = true;
  if (timings != nullptr) { timings->PackMs = phaseMs(); }
  return true;
}

std::optional<LongitudeLatitude> Network::CrossingOf(const Filed &one, const Filed &two) {
  const double ax = one.Ax;
  const double ay = one.Ay;
  const double rx = one.Bx - ax;
  const double ry = one.By - ay;
  const double cx = two.Ax;
  const double cy = two.Ay;
  const double sx = two.Bx - cx;
  const double sy = two.By - cy;
  const double denominator = rx * sy - ry * sx;
  if (denominator == 0.0) { return std::nullopt; }
  const double along = ((cx - ax) * sy - (cy - ay) * sx) / denominator;
  const double across = ((cx - ax) * ry - (cy - ay) * rx) / denominator;
  if (along <= 0.0 || along >= 1.0 || across <= 0.0 || across >= 1.0) { return std::nullopt; }
  double longitudeDeg = ax + along * rx;
  while (longitudeDeg > kDegPerHalfTurn) { longitudeDeg -= kDegPerTurn; }
  while (longitudeDeg < -kDegPerHalfTurn) { longitudeDeg += kDegPerTurn; }
  return LongitudeLatitude{.LongitudeDeg = longitudeDeg, .LatitudeDeg = ay + along * ry};
}

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
      const std::optional<LongitudeLatitude> met = CrossingOf(ours, yours);
      if (!met || SquareIn(grid, box, *met) != span.Square) { continue; }
      into.push_back(Crossing{.OverWay = ours.Way,
                              .UnderWay = yours.Way,
                              .LatitudeDeg = met->LatitudeDeg,
                              .LongitudeDeg = met->LongitudeDeg,
                              .OverAt = static_cast<uint32_t>(filed.SegAt[ours.Seg]),
                              .UnderAt = static_cast<uint32_t>(filed.SegAt[yours.Seg])});
    }
  }
}

std::expected<Network::Swept, std::string_view>
Network::Crossings(std::vector<Crossing> &into) const {
  into.clear();
  if (CachedSweep_) {
    into = CachedCrossings_;
    return *CachedSweep_;
  }
  Swept swept;
  auto phaseBegan = std::chrono::steady_clock::now();
  const auto phaseMs = [&phaseBegan] {
    const auto ended = std::chrono::steady_clock::now();
    const double elapsed = std::chrono::duration<double, std::milli>(ended - phaseBegan).count();
    phaseBegan = ended;
    return elapsed;
  };
  const size_t points = Points_.size() / 2;
  if (Ways_.size() < 2 || points < 4) { return swept; }

  std::vector<double> lon(points, 0.0);
  const Spanned over = SpanOfPoints(lon);
  swept.SpanMs = phaseMs();
  std::vector<uint32_t> segWay;
  std::vector<uint32_t> segAt;
  const size_t segments = SegmentsOfWays(segWay, segAt);
  swept.SegmentsMs = phaseMs();
  if (segments < 2) { return swept; }

  const std::expected<Gridded, std::string_view> grid =
      GridOver({.Lon = lon, .SegAt = segAt, .Segments = segments}, over);
  if (!grid) { return std::unexpected(grid.error()); }
  swept.GridMs = phaseMs();
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
    const size_t held = holds[cell + 1u] - holds[cell];
    swept.FullestCell = std::max(held, swept.FullestCell);
    swept.CandidatePairs += held * (held - static_cast<size_t>(held > 0)) / 2;
  }
  swept.FilingMs = phaseMs();

  for (size_t cell = 0; cell < cells; ++cell) {
    CrossingsInCell(
        {.SegAt = segAt, .InCell = inCell},
        {.Square = static_cast<uint32_t>(cell), .From = holds[cell], .To = holds[cell + 1u]},
        *grid,
        over,
        into,
        swept);
  }
  swept.TestMs = phaseMs();
  swept.Found = into.size();
  CachedCrossings_ = into;
  CachedSweep_ = swept;
  swept.CacheMs = phaseMs();
  CachedSweep_ = swept;
  return swept;
}

size_t Network::JunctionCount() const {
  size_t junctions = 0;
  for (const Node &node : Nodes_) {
    if (node.EdgeCount > 2) { ++junctions; }
  }
  return junctions;
}

std::expected<std::optional<Network::Found>, std::string_view>
Network::Nearest(LongitudeLatitude to) const {
  if (!ValidCoordinates(to)) { return std::unexpected(Says::kInvalidSpatialQuery); }
  if (!Woven_ && !Ways_.empty()) { return std::unexpected(Says::kUnbuiltNetwork); }
  if (Nodes_.empty()) { return std::nullopt; }
  std::vector<size_t> found;
  for (int widening = 0;; ++widening) {
    const double reachM = 4.0 * SnapM_ * std::pow(4.0, widening);
    if (!(reachM < std::numbers::pi * RadiusM_)) { break; }
    if (const auto result = Within(to, reachM, found); !result) {
      return std::unexpected(result.error());
    }
    if (!found.empty()) { break; }
  }
  if (found.empty()) {
    if (const auto result = Within(to, std::numbers::pi * RadiusM_, found); !result) {
      return std::unexpected(result.error());
    }
  }
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

std::expected<void, std::string_view>
Network::Within(LongitudeLatitude of, double reachM, std::vector<size_t> &nodes) const {
  if (!ValidCoordinates(of) || !std::isfinite(reachM) || reachM < 0.0) {
    return std::unexpected(Says::kInvalidSpatialQuery);
  }
  if (!Woven_ && !Ways_.empty()) { return std::unexpected(Says::kUnbuiltNetwork); }
  nodes.clear();
  const auto appendWithin = [&](size_t which) {
    const Node &node = Nodes_[which];
    if (ApartM(of,
               {.LongitudeDeg = node.LongitudeDeg, .LatitudeDeg = node.LatitudeDeg},
               Sphere{.RadiusM = RadiusM_}) <= reachM) {
      nodes.push_back(which);
    }
  };
  const double acrossCells = std::ceil(reachM / SnapM_) + 1.0;
  if (Cells_.empty() || 2.0 * acrossCells + 1.0 > std::sqrt(static_cast<double>(Cells_.size()))) {
    for (size_t which = 0; which < Nodes_.size(); ++which) { appendWithin(which); }
    return {};
  }
  const auto across = static_cast<int64_t>(acrossCells);
  const int64_t rowHere = RowOf(of.LatitudeDeg);
  const RowShape mine = ShapeRow(rowHere);
  for (int64_t row = rowHere - across; row <= rowHere + across; ++row) {
    const RowShape shape = ShapeRow(row);
    const auto reachCols = static_cast<int64_t>(std::min(
        static_cast<double>(shape.Columns),
        shape.Columns == 1
            ? 1.0
            : std::ceil(static_cast<double>(across) * mine.LonCellDeg / shape.LonCellDeg) + 1.0));
    const int64_t span = shape.Columns < 2 * reachCols + 1 ? shape.Columns : 2 * reachCols + 1;
    const int64_t centre = ColumnIn(shape, of.LongitudeDeg);
    for (int64_t step = 0; step < span; ++step) {
      const int64_t column =
          ((centre + step - reachCols) % shape.Columns + shape.Columns) % shape.Columns;
      const auto seen = Cells_.find(KeyAt({.Row = row, .Column = column}));
      if (seen == Cells_.end()) { continue; }
      for (const size_t candidate : seen->second) { appendWithin(candidate); }
    }
  }
  return {};
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

Network::ComponentStatistics Network::WeakComponents() const {
  ComponentStatistics out;
  std::vector<size_t> parent(Nodes_.size());
  std::ranges::iota(parent, size_t{0});
  std::vector<size_t> sizes(Nodes_.size(), 1);
  const auto root = [&parent](size_t node) {
    while (parent[node] != node) {
      parent[node] = parent[parent[node]];
      node = parent[node];
    }
    return node;
  };
  for (size_t from = 0; from < Nodes_.size(); ++from) {
    const Node &node = Nodes_[from];
    for (const Edge &edge : std::span<const Edge>(Edges_).subspan(node.FirstEdge, node.EdgeCount)) {
      size_t a = root(from);
      size_t b = root(edge.To);
      if (a == b) { continue; }
      if (sizes[a] < sizes[b]) { std::swap(a, b); }
      parent[b] = a;
      sizes[a] += sizes[b];
      sizes[b] = 0;
    }
  }
  for (const size_t size : sizes) {
    if (size == 0) { continue; }
    ++out.Count;
    out.Largest = std::max(size, out.Largest);
    if (size < 4) {
      ++out.UnderFour;
      out.InUnderFour += size;
    }
  }
  return out;
}

bool Network::LocalTurnAllowsRadius(const Edge &incoming,
                                    size_t previousNode,
                                    const Edge &outgoing,
                                    double minimumRadiusM) const {
  if (minimumRadiusM == 0.0) { return true; }
  if (incoming.LengthM <= 0.0 || outgoing.LengthM <= 0.0) { return false; }
  const Node &was = Nodes_[previousNode];
  const Node &here = Nodes_[incoming.To];
  const Node &there = Nodes_[outgoing.To];
  const double longitudeScale = std::cos(here.LatitudeDeg * kDegToRad);
  const double backEast = LonApartDeg(here.LongitudeDeg, was.LongitudeDeg) * longitudeScale;
  const double backNorth = here.LatitudeDeg - was.LatitudeDeg;
  const double onEast = LonApartDeg(there.LongitudeDeg, here.LongitudeDeg) * longitudeScale;
  const double onNorth = there.LatitudeDeg - here.LatitudeDeg;
  const double cross = std::abs(backEast * onNorth - backNorth * onEast);
  const double dot = backEast * onEast + backNorth * onNorth;
  if (cross == 0.0) { return dot > 0.0; }
  const double turnRad = std::atan2(cross, dot);
  const double availableM = 0.5 * std::min(incoming.LengthM, outgoing.LengthM);
  return minimumRadiusM <= availableM / std::tan(0.5 * turnRad);
}

class Network::RouteSearch {
public:
  struct Input {
    LongitudeLatitude From;
    LongitudeLatitude To;
    std::span<const size_t> Starts;
    std::span<const size_t> Goals;
    double GoalRadiusM = 0.0;
    double MinimumRadiusM = 0.0;
  };

  RouteSearch(const Network &network, Input input)
      : Network_(network),
        Input_(input),
        Best_(network.Edges_.size() + input.Starts.size(), std::numeric_limits<double>::infinity()),
        Previous_(Best_.size(), kNoSearchState),
        Settled_(Best_.size(), false),
        Seen_(network.Nodes_.size(), false),
        Sources_(network.Edges_.size(), 0),
        Goals_(network.Nodes_.size(), false) {
    for (const size_t goal : input.Goals) { Goals_[goal] = true; }
    for (size_t node = 0; node < network.Nodes_.size(); ++node) {
      const Node &here = network.Nodes_[node];
      for (size_t edge = 0; edge < here.EdgeCount; ++edge) {
        Sources_[here.FirstEdge + edge] = node;
      }
    }
  }

  [[nodiscard]] std::expected<bool, std::string_view> Run(Route &out) {
    Seed();
    while (!Open_.empty()) {
      const size_t state = Open_.top().second;
      Open_.pop();
      if (Settled_[state]) { continue; }
      Settled_[state] = true;
      const size_t node = NodeForState(state);
      if (!Seen_[node]) {
        Seen_[node] = true;
        ++out.Reached;
      }
      if (Goals_[node]) {
        Arrived_ = state;
        return true;
      }
      Expand(state, out);
    }
    if (CostOverflow_) { return std::unexpected(Says::kSearchCostOverflow); }
    return false;
  }

  [[nodiscard]] RouteTrace Trace() const {
    return {.Arrived = Arrived_, .Predecessors = Previous_, .Starts = Input_.Starts};
  }

private:
  [[nodiscard]] size_t NodeForState(size_t state) const {
    return state < Network_.Edges_.size() ? Network_.Edges_[state].To
                                          : Input_.Starts[state - Network_.Edges_.size()];
  }

  [[nodiscard]] double Heuristic(size_t node) const {
    const Node &here = Network_.Nodes_[node];
    const double distance =
        ApartM({.LongitudeDeg = here.LongitudeDeg, .LatitudeDeg = here.LatitudeDeg},
               Input_.To,
               Sphere{.RadiusM = Network_.RadiusM_});
    return std::max(0.0, distance - Input_.GoalRadiusM);
  }

  struct Candidate {
    size_t State = kNoSearchState;
    double DistanceM = 0.0;
    size_t Predecessor = kNoSearchState;
  };

  void Offer(Candidate candidate) {
    if (!std::isfinite(candidate.DistanceM)) {
      CostOverflow_ = true;
      return;
    }
    if (candidate.DistanceM >= Best_[candidate.State]) { return; }
    const double priority = candidate.DistanceM + Heuristic(NodeForState(candidate.State));
    if (!std::isfinite(priority)) {
      CostOverflow_ = true;
      return;
    }
    Best_[candidate.State] = candidate.DistanceM;
    Previous_[candidate.State] = candidate.Predecessor;
    Open_.emplace(priority, candidate.State);
  }

  void Seed() {
    for (size_t which = 0; which < Input_.Starts.size(); ++which) {
      const Node &node = Network_.Nodes_[Input_.Starts[which]];
      const double awayM =
          ApartM(Input_.From,
                 {.LongitudeDeg = node.LongitudeDeg, .LatitudeDeg = node.LatitudeDeg},
                 Sphere{.RadiusM = Network_.RadiusM_});
      Offer({.State = Network_.Edges_.size() + which, .DistanceM = awayM});
    }
  }

  void Expand(size_t state, Route &out) {
    const Node &here = Network_.Nodes_[NodeForState(state)];
    for (size_t which = 0; which < here.EdgeCount; ++which) {
      const size_t next = here.FirstEdge + which;
      const Edge &edge = Network_.Edges_[next];
      if (state < Network_.Edges_.size() &&
          !Network_.LocalTurnAllowsRadius(
              Network_.Edges_[state], Sources_[state], edge, Input_.MinimumRadiusM)) {
        ++out.TurnsRefused;
        continue;
      }
      Offer({.State = next, .DistanceM = Best_[state] + edge.LengthM, .Predecessor = state});
    }
  }

  const Network &Network_;
  Input Input_;
  std::vector<double> Best_;
  std::vector<size_t> Previous_;
  std::vector<bool> Settled_;
  std::vector<bool> Seen_;
  std::vector<size_t> Sources_;
  std::vector<bool> Goals_;
  using Step = std::pair<double, size_t>;
  std::priority_queue<Step, std::vector<Step>, std::greater<>> Open_;
  size_t Arrived_ = kNoSearchState;
  bool CostOverflow_ = false;
};

Route Network::Plan(LongitudeLatitude from, LongitudeLatitude to, double tightestM) const {
  Route out;
  if (!ValidCoordinates(from) || !ValidCoordinates(to)) {
    out.Error = Says::kInvalidRouteCoordinates;
    return out;
  }
  if (!std::isfinite(tightestM) || tightestM < 0.0) {
    out.Error = Says::kInvalidTurnRadius;
    return out;
  }
  out.StraightM = ApartM({.LongitudeDeg = from.LongitudeDeg, .LatitudeDeg = from.LatitudeDeg},
                         {.LongitudeDeg = to.LongitudeDeg, .LatitudeDeg = to.LatitudeDeg},
                         Sphere{.RadiusM = RadiusM_});
  if (!Woven_) {
    out.Error = "a network is planned over only after it is woven";
    return out;
  }

  const auto started = Nearest(from);
  const auto finished = Nearest(to);
  if (!started || !finished) {
    out.Error = !started ? started.error() : finished.error();
    return out;
  }
  if (!*started || !*finished) {
    out.Error = "a network with no nodes has nothing to start from";
    return out;
  }
  const size_t start = (*started)->Node;
  const size_t finish = (*finished)->Node;
  const double startAwayM = (*started)->AwayM;
  const double finishAwayM = (*finished)->AwayM;

  std::vector<size_t> nearStart;
  if (const auto result = Within(from, startAwayM + kStartReachM, nearStart); !result) {
    out.Error = result.error();
    return out;
  }
  if (nearStart.empty()) { nearStart.push_back(start); }
  out.StartedFrom = nearStart.size();

  std::vector<size_t> nearFinish;
  const double arriveM = Nodes_[finish].HalfWidthM > 0.0 ? 2.0 * Nodes_[finish].HalfWidthM : SnapM_;
  const double goalRadiusM = finishAwayM + arriveM;
  if (const auto result = Within(to, goalRadiusM, nearFinish); !result) {
    out.Error = result.error();
    return out;
  }
  if (nearFinish.empty()) { nearFinish.push_back(finish); }
  out.ArrivedAt = nearFinish.size();
  RouteSearch search(*this,
                     {.From = from,
                      .To = to,
                      .Starts = nearStart,
                      .Goals = nearFinish,
                      .GoalRadiusM = goalRadiusM,
                      .MinimumRadiusM = tightestM});
  const auto found = search.Run(out);
  if (!found) {
    out.Error = found.error();
    return out;
  }
  if (!*found) {
    const size_t joined = Reaches(std::span<const size_t>(nearStart));
    const size_t joinedToEnd = Reaches(std::span<const size_t>(nearFinish));
    out.Component = joined;
    out.EndComponent = joinedToEnd;
    out.Error = "no chain of ways joins the two ends -- " + std::to_string(joined) + " nodes of " +
                std::to_string(Nodes_.size()) +
                " are joined to the start by ANY edge, and the search " + "settled " +
                std::to_string(out.Reached) + " of those, while " + std::to_string(joinedToEnd) +
                " nodes are joined to the DESTINATION, so what separates the ends is " +
                (joined + 1 < Nodes_.size() ? std::string("the graph itself")
                                            : std::string("this search, not the graph"));
    return out;
  }

  if (const auto result = ReconstructRoute(search.Trace(), out); !result) {
    out.Error = result.error();
  }
  return out;
}

std::expected<void, std::string_view> Network::ReconstructRoute(RouteTrace trace,
                                                                Route &out) const {
  size_t count = 0;
  for (size_t state = trace.Arrived; state != kNoSearchState; state = trace.Predecessors[state]) {
    if (count == kMaxRouteLegs) { return std::unexpected(Says::kRouteLegBudget); }
    ++count;
  }
  std::vector<Leg> legs(count);
  size_t remaining = count;
  for (size_t state = trace.Arrived; state != kNoSearchState; state = trace.Predecessors[state]) {
    const size_t nodeIndex =
        state < Edges_.size() ? Edges_[state].To : trace.Starts[state - Edges_.size()];
    const Node &node = Nodes_[nodeIndex];
    legs[--remaining] = {.At = {.LongitudeDeg = node.LongitudeDeg, .LatitudeDeg = node.LatitudeDeg},
                         .HalfWidthM = node.HalfWidthM,
                         .MaxGradient = node.MaxGradient,
                         .MinRadiusM = node.MinRadiusM,
                         .Friction = node.Friction,
                         .Lanes = node.Lanes};
  }
  double alongM = 0.0;
  for (size_t which = 1; which < legs.size(); ++which) {
    alongM += ApartM(legs[which - 1].At, legs[which].At, Sphere{.RadiusM = RadiusM_});
    if (!std::isfinite(alongM)) { return std::unexpected(Says::kRouteLengthOverflow); }
    legs[which].AlongM = alongM;
  }
  out.Legs = std::move(legs);
  out.LengthM = alongM;
  out.Found = true;
  return {};
}

std::optional<double> Network::SampleFiniteHeight(const HeightSource &source,
                                                  LongitudeLatitude at) {
  if (!source) { return std::nullopt; }
  const auto height = source(at);
  return height && std::isfinite(*height) ? height : std::nullopt;
}

Network::Elevated Network::Elevate(const HeightSource &heightOf) {
  Elevated made;
  const size_t points = Points_.size() / 2;
  HeightsM_.assign(points, 0.0);
  std::vector<std::optional<double>> atNode;
  if (Woven_) {
    atNode.reserve(Nodes_.size());
    for (const auto &node : Nodes_) {
      atNode.push_back(SampleFiniteHeight(
          heightOf, {.LongitudeDeg = node.LongitudeDeg, .LatitudeDeg = node.LatitudeDeg}));
    }
  }
  for (size_t at = 0; at < points; ++at) {
    const size_t node = Woven_ && at < NodeOfPoint_.size() ? NodeOfPoint_[at] : Nodes_.size();
    std::optional<double> height;
    if (node < Nodes_.size()) {
      height = atNode[node];
    } else {
      height = SampleFiniteHeight(
          heightOf, {.LongitudeDeg = Points_[2 * at + 1], .LatitudeDeg = Points_[2 * at]});
    }
    if (height) {
      HeightsM_[at] = *height;
      ++made.Points;
    } else {
      HeightsM_[at] = at > 0 && WayOf_[at] == WayOf_[at - 1] ? HeightsM_[at - 1] : 0.0;
      ++made.Refused;
    }
  }
  StationsOfWays();
  SlopesOfWays();
  CountGrades(made);
  return made;
}

void Network::CountGrades(Elevated &made) const {
  for (size_t at = 0; at < SlopeM_.size(); ++at) {
    const double grade = std::fabs(SlopeM_[at]);
    if (grade > Network::kTenPercent) { ++made.OverTenPercent; }
    if (grade > Network::kThirtyPercent) { ++made.OverThirtyPercent; }
    made.SteepestGrade = std::max(made.SteepestGrade, grade);
    if (!Ways_[WayOf_[at]].Sealed) { continue; }
    if (grade > Network::kTenPercent) { ++made.SealedOverTenPercent; }
    made.SteepestSealedGrade = std::max(made.SteepestSealedGrade, grade);
  }
}

void Network::StationsOfWays() {
  StationM_.assign(Points_.size() / 2, 0.0);
  const Sphere on{.RadiusM = RadiusM_};
  for (const Way &way : Ways_) {
    for (size_t at = way.First + 1; at < way.First + way.Count; ++at) {
      const LongitudeLatitude from{.LongitudeDeg = Points_[2 * at - 1],
                                   .LatitudeDeg = Points_[2 * at - 2]};
      const LongitudeLatitude to{.LongitudeDeg = Points_[2 * at + 1],
                                 .LatitudeDeg = Points_[2 * at]};
      StationM_[at] = StationM_[at - 1] + ApartM(from, to, on);
    }
  }
}

void Network::SlopesOfWays() {
  SlopeM_.assign(Points_.size() / 2, 0.0);
  for (const Way &way : Ways_) {
    if (way.Count < 2) { continue; }
    const size_t last = way.First + way.Count - 1;
    for (size_t at = way.First; at <= last; ++at) {
      const size_t before = at == way.First ? at : at - 1;
      const size_t after = at == last ? at : at + 1;
      const double run = StationM_[after] - StationM_[before];
      SlopeM_[at] = run > 0.0 ? (HeightsM_[after] - HeightsM_[before]) / run : 0.0;
    }
  }
}

double Network::LengthM(size_t way) const {
  if (way >= Ways_.size() || Ways_[way].Count == 0 || StationM_.size() < Points_.size() / 2) {
    return 0.0;
  }
  return StationM_[Ways_[way].First + Ways_[way].Count - 1];
}

std::optional<Network::Station> Network::Profile(Along at) const {
  const size_t way = at.Way;
  const double stationM = at.StationM;
  if (way >= Ways_.size() || Ways_[way].Count == 0 || HeightsM_.size() < Points_.size() / 2) {
    return std::nullopt;
  }
  const Way &held = Ways_[way];
  const size_t first = held.First;
  const size_t last = first + held.Count - 1;
  if (held.Count == 1) { return Station{.HeightM = HeightsM_[first], .Grade = 0.0}; }
  const double s = std::clamp(stationM, 0.0, StationM_[last]);
  size_t a = first;
  while (a + 1 < last && StationM_[a + 1] <= s) { ++a; }
  const size_t b = a + 1;
  const double run = StationM_[b] - StationM_[a];
  if (run <= 0.0) { return Station{.HeightM = HeightsM_[a], .Grade = SlopeM_[a]}; }
  const double t = (s - StationM_[a]) / run;
  const double t2 = t * t;
  const double t3 = t2 * t;
  const double h0 = HeightsM_[a];
  const double h1 = HeightsM_[b];
  const double m0 = SlopeM_[a] * run;
  const double m1 = SlopeM_[b] * run;
  const double height = (2.0 * t3 - 3.0 * t2 + 1.0) * h0 + (t3 - 2.0 * t2 + t) * m0 +
                        (-2.0 * t3 + 3.0 * t2) * h1 + (t3 - t2) * m1;
  const double slope = ((6.0 * t2 - 6.0 * t) * h0 + (3.0 * t2 - 4.0 * t + 1.0) * m0 +
                        (-6.0 * t2 + 6.0 * t) * h1 + (3.0 * t2 - 2.0 * t) * m1) /
                       run;
  return Station{.HeightM = height, .Grade = slope};
}

}
