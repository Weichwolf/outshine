#include "Wayfinding.h"


#include <numbers>
#include <algorithm>
#include <cmath>
#include <queue>

namespace outshine::Path {

namespace {

constexpr double kDegToRad = std::numbers::pi / 180.0;

double MetresPerDegreeLat(double sphereRadiusM) { return sphereRadiusM * kDegToRad; }

double MetresPerDegreeLon(double latDeg, double sphereRadiusM) {
  const double shrink = std::cos(latDeg * kDegToRad);
  // [SET] the pole clamp: within 0.2 m of the pole a parallel has no usable east, and
  // the clamp keeps the division finite instead of minting infinities
  return MetresPerDegreeLat(sphereRadiusM) * (shrink > 1.0e-6 ? shrink : 1.0e-6);
}

// a longitude difference wraps the way ApartM's does -- an edge across the antimeridian
// is a fraction of a degree, never three hundred and sixty
[[nodiscard]] double LonApartDeg(double toDeg, double fromDeg) {
  const double apart = toDeg - fromDeg;
  return apart - 360.0 * std::floor(apart / 360.0 + 0.5);
}

} // namespace

double ApartM(double fromLatDeg, double fromLonDeg, double toLatDeg, double toLonDeg,
              double sphereRadiusM) {
  const double fromLat = fromLatDeg * kDegToRad;
  const double toLat = toLatDeg * kDegToRad;
  const double byLat = (toLatDeg - fromLatDeg) * kDegToRad;
  double byLon = toLonDeg - fromLonDeg;
  while (byLon > 180.0) { byLon -= 360.0; }
  while (byLon < -180.0) { byLon += 360.0; }
  byLon *= kDegToRad;

  const double half = std::sin(0.5 * byLat) * std::sin(0.5 * byLat) +
                      std::cos(fromLat) * std::cos(toLat) * std::sin(0.5 * byLon) *
                          std::sin(0.5 * byLon);
  return 2.0 * sphereRadiusM * std::asin(std::sqrt(half < 1.0 ? half : 1.0));
}

void Network::Lay(const double *latLonPairs, size_t points, double halfWidthM,
                  double maxGradient, int lanes) {
  if (latLonPairs == nullptr || points < 2) { return; }
  Way way;
  way.First = Points_.size() / 2;
  way.Count = points;
  way.HalfWidthM = halfWidthM;
  way.MaxGradient = maxGradient;
  way.Lanes = lanes;
  Ways_.push_back(way);
  for (size_t which = 0; which < points; ++which) {
    Points_.push_back(latLonPairs[2 * which]);
    Points_.push_back(latLonPairs[2 * which + 1]);
    Widths_.push_back(halfWidthM);
    Gradients_.push_back(maxGradient);
    Lanes_.push_back(lanes);
  }
  Woven_ = false;
}

// the index lives on the sphere it indexes: a row's OWN latitude fixes the column
// modulus (every point in the row shares one lonCell), the column wraps at the row's
// circumference so the antimeridian's two sides are NEIGHBOURS, not half a billion apart
Network::RowShape Network::ShapeRow(int64_t row) const {
  RowShape shape;
  shape.Row = row;
  const double latCell = SnapM_ / MetresPerDegreeLat(RadiusM_);
  const double rowLat = ((double)row + 0.5) * latCell;
  shape.LonCellDeg = SnapM_ / MetresPerDegreeLon(rowLat, RadiusM_);
  const double columns = std::ceil(360.0 / shape.LonCellDeg);
  shape.Columns = columns > 1.0 ? (int64_t)columns : 1;
  return shape;
}

int64_t Network::RowOf(double latDeg) const {
  const double latCell = SnapM_ / MetresPerDegreeLat(RadiusM_);
  return (int64_t)std::floor(latDeg / latCell);
}

int64_t Network::ColumnIn(const RowShape &shape, double lonDeg) {
  const double wrapped = lonDeg - 360.0 * std::floor((lonDeg + 180.0) / 360.0);
  int64_t column = (int64_t)std::floor((wrapped + 180.0) / shape.LonCellDeg);
  return ((column % shape.Columns) + shape.Columns) % shape.Columns;
}

int64_t Network::KeyAt(int64_t row, int64_t column) {
  return (row << 32) ^ (column & 0xffffffffLL);
}

int64_t Network::CellOf(double latDeg, double lonDeg) const {
  const int64_t row = RowOf(latDeg);
  return KeyAt(row, ColumnIn(ShapeRow(row), lonDeg));
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
    error = "a network of " + std::to_string(Points_.size() / 2) +
            " points reaches the bound of " + std::to_string(kMaxNetworkPoints);
    return false;
  }
  if (!(SnapM_ > 0.0)) {
    error = "a snapping distance is what makes two ways meet, and this network declares " +
            std::to_string(SnapM_) + " m";
    return false;
  }

  // the weave is a function of the CONTENT, never of arrival: tiles stream in whatever order
  // the scheduler grants, so the ways are put into one canonical order before any node merges
  {
    std::vector<size_t> order(Ways_.size());
    for (size_t at = 0; at < order.size(); ++at) { order[at] = at; }
    std::sort(order.begin(), order.end(), [this](size_t a, size_t b) {
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
      return wa.Lanes < wb.Lanes;
    });
    std::vector<double> points, widths, gradients;
    std::vector<int> lanes;
    std::vector<Way> ways;
    points.reserve(Points_.size());
    widths.reserve(Widths_.size());
    gradients.reserve(Gradients_.size());
    lanes.reserve(Lanes_.size());
    ways.reserve(Ways_.size());
    for (const size_t which : order) {
      Way moved = Ways_[which];
      const size_t first = moved.First;
      moved.First = points.size() / 2;
      ways.push_back(moved);
      for (size_t at = 0; at < moved.Count; ++at) {
        points.push_back(Points_[2 * (first + at)]);
        points.push_back(Points_[2 * (first + at) + 1]);
        widths.push_back(Widths_[first + at]);
        gradients.push_back(Gradients_[first + at]);
        lanes.push_back(Lanes_[first + at]);
      }
    }
    Points_ = std::move(points);
    Widths_ = std::move(widths);
    Gradients_ = std::move(gradients);
    Lanes_ = std::move(lanes);
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
      // derived: a neighbour within one snap sits within lonCell(here) DEGREES of this
      // point; in the visited row's own columns that is the ratio of the two cell
      // widths -- the equatorward parallel is longer, so its columns are narrower and
      // one snap can cross more of them -- plus one for the floor boundary
      const int64_t reachCols =
          (int64_t)std::ceil(mine.LonCellDeg / shape.LonCellDeg) + 1;
      const int64_t centre = ColumnIn(shape, lonDeg);
      const int64_t span =
          shape.Columns < 2 * reachCols + 1 ? shape.Columns : 2 * reachCols + 1;
      for (int64_t step = 0; step < span && found == Nodes_.size(); ++step) {
        const int64_t column =
            ((centre + step - reachCols) % shape.Columns + shape.Columns) % shape.Columns;
        const auto seen = byCell.find(KeyAt(row, column));
        if (seen == byCell.end()) { continue; }
        for (const size_t candidate : seen->second) {
          if (ApartM(latDeg, lonDeg, Nodes_[candidate].LatDeg, Nodes_[candidate].LonDeg, RadiusM_) <= SnapM_) {
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
      made.HalfWidthM = Widths_[point];
      made.MaxGradient = Gradients_[point];
      made.Lanes = Lanes_[point];
      Nodes_.push_back(made);
      byCell[CellOf(latDeg, lonDeg)].push_back(found);
    } else {
      if (Widths_[point] > Nodes_[found].HalfWidthM) { Nodes_[found].HalfWidthM = Widths_[point]; }
      if (Nodes_[found].Lanes <= 0) { Nodes_[found].Lanes = Lanes_[point]; }
      if (Nodes_[found].MaxGradient <= 0.0 ||
          (Gradients_[point] > 0.0 && Gradients_[point] < Nodes_[found].MaxGradient)) {
        Nodes_[found].MaxGradient = Gradients_[point];
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
      const double lengthM = ApartM(Nodes_[from].LatDeg, Nodes_[from].LonDeg, Nodes_[to].LatDeg,
                                    Nodes_[to].LonDeg, RadiusM_);
      outgoing[from].push_back(Edge{to, lengthM});
      outgoing[to].push_back(Edge{from, lengthM});
    }
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

size_t Network::JunctionCount() const {
  size_t junctions = 0;
  for (const Node &node : Nodes_) {
    if (node.EdgeCount > 2) { ++junctions; }
  }
  return junctions;
}

bool Network::Nearest(const Waypoint &to, size_t &node, double &awayM) const {
  if (Nodes_.empty()) { return false; }
  // the woven cell index answers in a growing reach; the widening quadruples so a miss
  // costs a constant factor of the hit that ends it, capped at the half circumference
  std::vector<size_t> found;
  for (double reachM = 4.0 * SnapM_; reachM < std::numbers::pi * RadiusM_; reachM *= 4.0) {
    Within(to, reachM, found);
    if (!found.empty()) { break; }
  }
  if (found.empty()) { Within(to, std::numbers::pi * RadiusM_, found); }
  size_t best = found.empty() ? 0 : found.front();
  double bestAway = ApartM(to.LatDeg, to.LonDeg, Nodes_[best].LatDeg, Nodes_[best].LonDeg, RadiusM_);
  for (const size_t which : found) {
    const double away = ApartM(to.LatDeg, to.LonDeg, Nodes_[which].LatDeg, Nodes_[which].LonDeg, RadiusM_);
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
  // one slack cell each way: a column is SnapM wide in metres at its OWN row, so the span
  // in cells is the reach in snaps at every latitude
  const int64_t across = (int64_t)std::ceil(reachM / SnapM_) + 1;
  const uint64_t cells = (uint64_t)(2 * across + 1) * (uint64_t)(2 * across + 1);
  if (Cells_.empty() || cells > Cells_.size()) {
    // the reach box would touch more cells than exist -- the index cannot beat the scan
    for (size_t which = 0; which < Nodes_.size(); ++which) {
      if (ApartM(of.LatDeg, of.LonDeg, Nodes_[which].LatDeg, Nodes_[which].LonDeg, RadiusM_) <= reachM) {
        nodes.push_back(which);
      }
    }
    return;
  }
  // the walk enumerates (row, column) KEYS directly, columns modular in each row's own
  // circumference -- each cell is asked once, so no node answers twice
  const int64_t rowHere = RowOf(of.LatDeg);
  const RowShape mine = ShapeRow(rowHere);
  for (int64_t row = rowHere - across; row <= rowHere + across; ++row) {
    const RowShape shape = ShapeRow(row);
    // derived: the reach spans across cells of the QUERY row's width; in this row's own
    // columns that scales by the two rows' cell-width ratio, plus one for the floor
    const int64_t reachCols =
        (int64_t)std::ceil((double)across * mine.LonCellDeg / shape.LonCellDeg) + 1;
    const int64_t span =
        shape.Columns < 2 * reachCols + 1 ? shape.Columns : 2 * reachCols + 1;
    const int64_t centre = ColumnIn(shape, of.LonDeg);
    for (int64_t step = 0; step < span; ++step) {
      const int64_t column =
          ((centre + step - reachCols) % shape.Columns + shape.Columns) % shape.Columns;
      const auto seen = Cells_.find(KeyAt(row, column));
      if (seen == Cells_.end()) { continue; }
      for (const size_t candidate : seen->second) {
        if (ApartM(of.LatDeg, of.LonDeg, Nodes_[candidate].LatDeg, Nodes_[candidate].LonDeg,
                   RadiusM_) <= reachM) {
          nodes.push_back(candidate);
        }
      }
    }
  }
}

Route Network::Plan(const Waypoint &from, const Waypoint &to, double tightestM) const {
  Route out;
  out.StraightM = ApartM(from.LatDeg, from.LonDeg, to.LatDeg, to.LonDeg, RadiusM_);
  if (!Woven_) {
    out.Error = "a network is planned over only after it is woven";
    return out;
  }

  size_t start = 0, finish = 0;
  double startAwayM = 0.0, finishAwayM = 0.0;
  if (!Nearest(from, start, startAwayM) || !Nearest(to, finish, finishAwayM)) {
    out.Error = "a network with no nodes has nothing to start from";
    return out;
  }

  // the turn refusal depends on the ARRIVING direction, so the search state is the
  // directed edge, not the node -- node settling refused legal routes a different
  // approach would have taken, and judged turns against arrivals the path never used
  const double never = 1.0e300;
  const size_t edges = Edges_.size();
  const size_t kNoState = (size_t)-1;
  std::vector<size_t> nearStart;
  Within(from, startAwayM + kStartReachM, nearStart);
  if (nearStart.empty()) { nearStart.push_back(start); }
  out.StartedFrom = nearStart.size();

  // a state is an edge index, or edges+i for "standing at seed i, arrived by no edge"
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
    return ApartM(Nodes_[node].LatDeg, Nodes_[node].LonDeg, Nodes_[finish].LatDeg,
                  Nodes_[finish].LonDeg, RadiusM_);
  };

  using Step = std::pair<double, size_t>;
  std::priority_queue<Step, std::vector<Step>, std::greater<Step>> open;
  for (size_t which = 0; which < nearStart.size(); ++which) {
    const size_t seed = nearStart[which];
    const double awayM =
        ApartM(from.LatDeg, from.LonDeg, Nodes_[seed].LatDeg, Nodes_[seed].LonDeg, RadiusM_);
    const size_t state = edges + which;
    if (awayM >= best[state]) { continue; }
    best[state] = awayM;
    open.push(Step{awayM + goalM(seed), state});
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
    if (node == finish) {
      arrived = state;
      break;
    }

    const Node &here = Nodes_[node];
    const bool hasBack = state < edges;
    double backEast = 0.0, backNorth = 0.0;
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
          const double turned =
              (backEast * onEast + backNorth * onNorth) / (wasLength * onLength);
          const double turnRad = std::acos(turned < -1.0 ? -1.0 : (turned > 1.0 ? 1.0 : turned));
          const double half = 0.5 * turnRad;
          if (std::tan(half) > 0.0 && tightestM > 0.0 &&
              edge.LengthM > 0.0 && std::fabs(turnRad) > 1.0e-9) {
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
      open.push(Step{through + goalM(edge.To), next});
    }
  }

  out.Reached = reached;
  if (arrived == kNoState) {
    out.Error = "the network holds both ends but no chain of ways joins them -- " +
                std::to_string(reached) + " nodes of " + std::to_string(Nodes_.size()) +
                " were reachable from the start, so this is a network in pieces and not a "
                "search that gave up";
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
  std::reverse(back.begin(), back.end());

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
    leg.MaxGradient = node.MaxGradient;
    leg.Lanes = node.Lanes;
    out.Legs.push_back(leg);
  }
  out.LengthM = alongM;
  out.Found = true;
  return out;
}

} // namespace outshine::Path
