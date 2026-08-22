#include "Wayfinding.h"

#include "Fit.h"

#include <algorithm>
#include <cmath>
#include <queue>

namespace outshine::World {

namespace {

constexpr double kDegToRad = 0.017453292519943295;

double MetresPerDegreeLat(double sphereRadiusM) { return sphereRadiusM * kDegToRad; }

double MetresPerDegreeLon(double latDeg, double sphereRadiusM) {
  const double shrink = std::cos(latDeg * kDegToRad);
  return MetresPerDegreeLat(sphereRadiusM) * (shrink > 1.0e-6 ? shrink : 1.0e-6);
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

int64_t Network::CellOf(double latDeg, double lonDeg) const {
  const double latCell = SnapM_ / MetresPerDegreeLat(RadiusM_);
  const double lonCell = SnapM_ / MetresPerDegreeLon(latDeg, RadiusM_);
  const int64_t row = (int64_t)std::floor(latDeg / latCell);
  const int64_t column = (int64_t)std::floor(lonDeg / lonCell);
  return (row << 32) ^ (column & 0xffffffffLL);
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

  std::vector<size_t> nodeOf(Points_.size() / 2, 0);
  std::unordered_map<int64_t, std::vector<size_t>> byCell;
  for (size_t point = 0; point < Points_.size() / 2; ++point) {
    const double latDeg = Points_[2 * point];
    const double lonDeg = Points_[2 * point + 1];

    size_t found = Nodes_.size();
    const double latCell = SnapM_ / MetresPerDegreeLat(RadiusM_);
    const double lonCell = SnapM_ / MetresPerDegreeLon(latDeg, RadiusM_);
    for (int row = -1; row <= 1 && found == Nodes_.size(); ++row) {
      for (int column = -1; column <= 1 && found == Nodes_.size(); ++column) {
        const int64_t key = CellOf(latDeg + (double)row * latCell, lonDeg + (double)column * lonCell);
        const auto seen = byCell.find(key);
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

size_t Network::JunctionCount(void) const {
  size_t junctions = 0;
  for (const Node &node : Nodes_) {
    if (node.EdgeCount > 2) { ++junctions; }
  }
  return junctions;
}

bool Network::Nearest(const Waypoint &to, size_t &node, double &awayM) const {
  if (Nodes_.empty()) { return false; }
  size_t best = 0;
  double bestAway = ApartM(to.LatDeg, to.LonDeg, Nodes_[0].LatDeg, Nodes_[0].LonDeg, RadiusM_);
  for (size_t which = 1; which < Nodes_.size(); ++which) {
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
  for (size_t which = 0; which < Nodes_.size(); ++which) {
    if (ApartM(of.LatDeg, of.LonDeg, Nodes_[which].LatDeg, Nodes_[which].LonDeg, RadiusM_) <= reachM) {
      nodes.push_back(which);
    }
  }
}

Route Network::Plan(const Waypoint &from, const Waypoint &to, double tightestM,
                    double withinM) const {
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

  const double never = 1.0e300;
  std::vector<double> best(Nodes_.size(), never);
  std::vector<size_t> came(Nodes_.size(), Nodes_.size());
  std::vector<double> cameLengthM(Nodes_.size(), 0.0);
  std::vector<bool> settled(Nodes_.size(), false);

  using Step = std::pair<double, size_t>;
  std::priority_queue<Step, std::vector<Step>, std::greater<Step>> open;
  std::vector<size_t> nearStart;
  Within(from, startAwayM + kStartReachM, nearStart);
  if (nearStart.empty()) { nearStart.push_back(start); }
  out.StartedFrom = nearStart.size();
  for (const size_t seed : nearStart) {
    const double awayM = ApartM(from.LatDeg, from.LonDeg, Nodes_[seed].LatDeg, Nodes_[seed].LonDeg, RadiusM_);
    if (awayM >= best[seed]) { continue; }
    best[seed] = awayM;
    open.push(Step{awayM + ApartM(Nodes_[seed].LatDeg, Nodes_[seed].LonDeg, Nodes_[finish].LatDeg,
                                  Nodes_[finish].LonDeg, RadiusM_),
                   seed});
  }

  size_t reached = 0;
  while (!open.empty()) {
    const size_t node = open.top().second;
    open.pop();
    if (settled[node]) { continue; }
    settled[node] = true;
    ++reached;
    if (node == finish) { break; }

    const Node &here = Nodes_[node];
    const bool hasBack = came[node] != Nodes_.size();
    double backEast = 0.0, backNorth = 0.0;
    if (hasBack) {
      const Node &was = Nodes_[came[node]];
      backEast = (here.LonDeg - was.LonDeg) * std::cos(here.LatDeg * 0.017453292519943295);
      backNorth = here.LatDeg - was.LatDeg;
    }
    for (size_t which = 0; which < here.EdgeCount; ++which) {
      const Edge &edge = Edges_[here.FirstEdge + which];
      if (hasBack && edge.LengthM > 0.0) {
        const Node &next = Nodes_[edge.To];
        const double onEast =
            (next.LonDeg - here.LonDeg) * std::cos(here.LatDeg * 0.017453292519943295);
        const double onNorth = next.LatDeg - here.LatDeg;
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
              edge.LengthM < cameLengthM[node] ? edge.LengthM : cameLengthM[node];
            const double room = 0.5 * shorter / std::tan(half);
            if (room < tightestM) {
              ++out.TurnsRefused;
              continue;
            }
          }
        }
      }
      const double through = best[node] + edge.LengthM;
      if (through >= best[edge.To]) { continue; }
      best[edge.To] = through;
      came[edge.To] = node;
      cameLengthM[edge.To] = edge.LengthM;
      open.push(Step{through + ApartM(Nodes_[edge.To].LatDeg, Nodes_[edge.To].LonDeg,
                                      Nodes_[finish].LatDeg, Nodes_[finish].LonDeg, RadiusM_),
                     edge.To});
    }
  }

  out.Reached = reached;
  if (!settled[finish]) {
    out.Error = "the network holds both ends but no chain of ways joins them -- " +
                std::to_string(reached) + " nodes of " + std::to_string(Nodes_.size()) +
                " were reachable from the start, so this is a network in pieces and not a "
                "search that gave up";
    return out;
  }

  std::vector<size_t> back;
  for (size_t node = finish; node != Nodes_.size(); node = came[node]) {
    back.push_back(node);
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

Route Plan(const Waypoint &from, const Waypoint &to, double sphereRadiusM) {
  Route out;
  out.StraightM = ApartM(from.LatDeg, from.LonDeg, to.LatDeg, to.LonDeg, sphereRadiusM);
  out.Error = "no network is woven over the streamed ways yet, so no route between " +
              std::to_string(from.LatDeg) + " " + std::to_string(from.LonDeg) + " and " +
              std::to_string(to.LatDeg) + " " + std::to_string(to.LonDeg) +
              " can be planned -- Network::Weave and Network::Plan are built and nothing feeds "
              "them OsmField's ways";
  return out;
}

} // namespace outshine::World
