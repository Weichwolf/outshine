#include "Corridors.h"

#include <generation/Generate.h>

#include <algorithm>
#include <expected>
#include <array>
#include <chrono>
#include <memory>
#include <ratio>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <functional>
#include <iterator>
#include <numbers>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "Capacity.h"
#include "Fit.h"
#include "Heap.h"
#include "Log.h"
#include "ReferenceLine.h"
#include "TangentFrame.h"
#include "geo/PlaceKey.h"
#include "math/Quantile.h"
#include "math/Units.h"
#include "math/Vec2.h"
#include "math/Vec3.h"
#include "spatial/Census.h"
#include "spatial/Drape.h"
#include "spatial/Refine.h"

namespace outshine::Generators {

namespace Says {
constexpr auto kInvalidTransportPointRange =
    "transport way point range exceeds the supplied coordinate stream";
constexpr auto kStaleCorridorInput = "corridor input changed during construction";
constexpr auto kPavingCreationFailed = "could not publish road geometry";
}

namespace {

constexpr double kLeastSineBetween = 1.0e-3;
constexpr double kLeastCapM = 0.01;
constexpr double kLeastSpanM = 0.05;
constexpr float kUnlitTint = 0.65f;
constexpr double kUnraisedDeckM = -1.0e29;
constexpr double kRoseLeast = 0.05;
constexpr double kRoadStepM = 16.0;
constexpr double kNodeSnapM = 2.0;
constexpr double kCrossCellM = 32.0;
constexpr double kMeetsWithinM = 10.0;
constexpr int kRampPasses = 12;
constexpr int kChordPasses = 4;
constexpr double kChordWithinM = 0.20;
constexpr double kVergeM = 1.5;
constexpr double kFitWithinM = 0.5;
constexpr double kFitTightestM = 5.5;
constexpr double kLeastRoadM = 2.0;
constexpr double kJunctionRadiusM = 4.0;
constexpr double kBoundaryReachM = 100.0;
constexpr double kAngleLookaheadM = 10.0;
constexpr double kContinuesPastDeg = 160.0;
constexpr double kParallelWithinDeg = 22.5;
constexpr double kSteepestApproach = 0.10;
constexpr double kSteepestJunction = 0.35;

constexpr auto ByBearing = [](const auto &a, const auto &b) {
  if (a.AngleRad != b.AngleRad) { return a.AngleRad < b.AngleRad; }
  return a.Edge != b.Edge ? a.Edge < b.Edge : a.End < b.End;
};

}

double Corridors::LeastSeen(double held, double seen) {
  if (!(seen > 0.0)) { return held; }
  return held > 0.0 ? std::min(held, seen) : seen;
}

void Corridors::NotesFit(Paved &into, const Fitted &got) {
  ++into.FitLaid;
  ++into.FitsMeasured;
  into.FitRadiusTightestM = LeastSeen(into.FitRadiusTightestM, got.TightestRadiusM);
}

void Corridors::FitAlongLane(Paved &into) {
  size_t from = 0;
  size_t cuts = 0;
  bool wholeWay = true;
  while (from * 2u + 6u <= into.FitEastNorth.size()) {
    ReferenceLine fitted;
    const Fitted got = Fit(std::span<const double>(into.FitEastNorth.data() + from * 2u,
                                                   into.FitEastNorth.size() - from * 2u),
                           kFitWithinM,
                           kFitTightestM,
                           fitted);
    if (got.Laid) {
      NotesFit(into, got);
      into.FitUndrivable += got.Undrivable;
      break;
    }
    if (got.Undrivable == 0 || got.TightestDemandedAtVertex == 0) {
      if (wholeWay) { ++into.FitRefused; }
      ++into.FitUnsplittable;
      break;
    }

    const size_t upTo = got.TightestDemandedAtVertex;
    if (upTo + 1u >= 3u) {
      ReferenceLine piece;
      const Fitted head =
          Fit(std::span<const double>(into.FitEastNorth.data() + from * 2u, (upTo + 1u) * 2u),
              kFitWithinM,
              kFitTightestM,
              piece);
      if (head.Laid) {
        NotesFit(into, head);
      } else {
        ++into.FitUnsplittable;
      }
    }
    ++into.FitTooTight;
    ++cuts;
    into.TightestDemandM = LeastSeen(into.TightestDemandM, got.TightestDemandedM);
    from += upTo + 1u;
    wholeWay = false;
  }
  into.FitCuts += cuts;
}

void Corridors::TrimLaneEnds(const Edge &edge, Paved &into) {
  const std::vector<double> reached = ReachedAlong(into.Along);
  const double wholeM = reached.back();
  const double fromM = edge.CutM[0];
  const double toM = wholeM - edge.CutM[1];
  if (toM - fromM < kLeastRoadM || (fromM <= kLeastCapM && toM >= wholeM - kLeastCapM)) { return; }

  const auto standAt = [&](double alongM) {
    size_t at = 1;
    while (at + 1 < reached.size() && reached[at] < alongM) { ++at; }
    const double span = reached[at] - reached[at - 1];
    const double part = span > kLeastTurnRad ? (alongM - reached[at - 1]) / span : 0.0;
    const RoadStation &from = into.Along[at - 1];
    const RoadStation &to = into.Along[at];
    return RoadStation{.EastM = from.EastM + (to.EastM - from.EastM) * part,
                       .NorthM = from.NorthM + (to.NorthM - from.NorthM) * part,
                       .GradeM = from.GradeM + (to.GradeM - from.GradeM) * part};
  };
  std::vector<RoadStation> kept;
  kept.push_back(standAt(fromM));
  for (size_t at = 0; at < into.Along.size(); ++at) {
    if (reached[at] > fromM && reached[at] < toM) { kept.push_back(into.Along[at]); }
  }
  kept.push_back(standAt(toM));
  into.Along.swap(kept);
}

void Corridors::FitLane(const Edge &edge, Paved &into) {
  into.FitEastNorth.clear();
  into.FitEastNorth.reserve(into.Along.size() * 2u);
  for (const RoadStation &one : into.Along) {
    into.FitEastNorth.push_back(one.EastM);
    into.FitEastNorth.push_back(one.NorthM);
  }
  FitAlongLane(into);
  TrimLaneEnds(edge, into);
}

void Corridors::RefineChords(const Paving &on, Paved &into) {
  for (int pass = 0; pass < kChordPasses; ++pass) {
    size_t added = 0;
    into.Finer.clear();
    into.Finer.reserve(into.Along.size() * 2u);
    for (size_t at = 1; at < into.Along.size(); ++at) {
      into.Finer.push_back(into.Along[at - 1u]);
      const double midE = 0.5 * (into.Along[at - 1u].EastM + into.Along[at].EastM);
      const double midN = 0.5 * (into.Along[at - 1u].NorthM + into.Along[at].NorthM);
      const double chord = 0.5 * (into.Along[at - 1u].GradeM + into.Along[at].GradeM);
      const double overM = on.Draped.At({.EastM = midE, .NorthM = midN}, chord);
      if (std::fabs(overM - chord) <= kChordWithinM) { continue; }
      into.Finer.push_back(RoadStation{.EastM = midE, .NorthM = midN, .GradeM = overM});
      ++added;
    }
    into.Finer.push_back(into.Along.back());
    into.Along.swap(into.Finer);
    into.ChordAdded += added;
    if (added == 0) { break; }
  }
}

size_t Corridors::StepsAcross(const Paving &on, Spanning between) {
  const double perLon = kMPerDegLon * std::cos(on.Points[between.Here] * kDeg2Rad);
  const double spanE = (on.Points[between.Next + 1] - on.Points[between.Here + 1]) * perLon;
  const double spanN = (on.Points[between.Next] - on.Points[between.Here]) * kMPerDegLat;
  return static_cast<size_t>(1.0 + std::sqrt(spanE * spanE + spanN * spanN) / kRoadStepM);
}

double Corridors::AwayM(const Paving &on, const outshine::Ground::StreetField::Way &lane) {
  double lowLat = kBeyondAnyCoordinate;
  double highLat = -kBeyondAnyCoordinate;
  double lowLon = kBeyondAnyCoordinate;
  double highLon = -kBeyondAnyCoordinate;
  const size_t first = static_cast<size_t>(lane.FirstPoint) * 2u;
  for (uint32_t step = 0; step < lane.PointCount; ++step) {
    const size_t at = first + static_cast<size_t>(step) * 2u;
    if (at + 1 >= on.Points.size()) { break; }
    lowLat = std::min(lowLat, on.Points[at]);
    highLat = std::max(highLat, on.Points[at]);
    lowLon = std::min(lowLon, on.Points[at + 1]);
    highLon = std::max(highLon, on.Points[at + 1]);
  }
  if (lowLat > highLat) { return 0.0; }
  const double nearLat = std::clamp(on.EyeLatDeg, lowLat, highLat);
  const double nearLon = std::clamp(on.EyeLonDeg, lowLon, highLon);
  const double northM = (nearLat - on.EyeLatDeg) * kMPerDegLat;
  const double eastM = (nearLon - on.EyeLonDeg) * kMPerDegLon * std::cos(on.EyeLatDeg * kDeg2Rad);
  return std::sqrt(northM * northM + eastM * eastM);
}

uint64_t Corridors::SharedNodeAt(const Paving &on, double latDeg, double lonDeg) {
  const uint64_t key = PlaceKey({.LongitudeDeg = lonDeg, .LatitudeDeg = latDeg});
  const auto seen = on.SharedNodes.find(key);
  return seen != on.SharedNodes.end() && seen->second > 1u ? key : 0u;
}

bool Corridors::StationsAlong(const Paving &on,
                              const outshine::Ground::StreetField::Way &lane,
                              const std::function<bool(LongitudeLatitude, uint64_t)> &station) {
  for (uint32_t step = 0; step + 1 < lane.PointCount; ++step) {
    const size_t here = (static_cast<size_t>(lane.FirstPoint) + step) * 2;
    const size_t next = here + 2;
    if (next + 1 >= on.Points.size()) { return false; }
    const size_t pieces = StepsAcross(on, {.Here = here, .Next = next});
    for (size_t piece = 0; piece < pieces; ++piece) {
      const double at = static_cast<double>(piece) / static_cast<double>(pieces);
      const double onLat = on.Points[here] + (on.Points[next] - on.Points[here]) * at;
      const double onLon = on.Points[here + 1] + (on.Points[next + 1] - on.Points[here + 1]) * at;
      if (!station({.LongitudeDeg = onLon, .LatitudeDeg = onLat},
                   piece == 0 ? SharedNodeAt(on, onLat, onLon) : 0u)) {
        return false;
      }
    }
  }
  const size_t last = (static_cast<size_t>(lane.FirstPoint) + lane.PointCount - 1u) * 2;
  return last + 1 < on.Points.size() &&
         station({.LongitudeDeg = on.Points[last + 1], .LatitudeDeg = on.Points[last]},
                 SharedNodeAt(on, on.Points[last], on.Points[last + 1]));
}

void Corridors::DesignLane(const Paving &on,
                           const outshine::Ground::StreetField::Way &lane,
                           size_t laneAt,
                           Paved &into) {
  into.Along.clear();
  bool whole = true;
  const auto station = [&](LongitudeLatitude over, uint64_t node) {
    const std::optional<Grounded> stood = GroundUnder(on, over);
    if (!stood) { return false; }
    into.Along.push_back(
        {.EastM = stood->EastM, .NorthM = stood->NorthM, .GradeM = stood->GradeM, .Node = node});
    return true;
  };
  whole = StationsAlong(on, lane, station);
  if (!whole || into.Along.size() < 2) {
    ++into.RefusedWays;
    return;
  }
  RefineChords(on, into);

  for (RoadStation &one : into.Along) {
    if (one.Node != 0u || into.AtCrossing.empty()) { continue; }
    const auto east = static_cast<int64_t>(std::floor(one.EastM / kCrossCellM));
    const auto north = static_cast<int64_t>(std::floor(one.NorthM / kCrossCellM));
    const auto atE = static_cast<uint64_t>(east + 0x20000000LL);
    const auto atS = static_cast<uint64_t>(north + 0x20000000LL);
    const auto near = into.AtCrossing.find((atE << 32U) | atS);
    if (near == into.AtCrossing.end()) { continue; }
    for (const auto &met : near->second) {
      const double offE = one.EastM - met.EastM;
      const double offN = one.NorthM - met.NorthM;
      if (offE * offE + offN * offN <= kMeetsWithinM * kMeetsWithinM) {
        one.Node = met.Named;
        break;
      }
    }
  }
  into.Designed[laneAt] = into.Along;
}

double Corridors::StepAlongM(std::span<const RoadStation> along, size_t at) {
  if (at == 0 || at >= along.size()) { return 0.0; }
  const double spanE = along[at].EastM - along[at - 1].EastM;
  const double spanN = along[at].NorthM - along[at - 1].NorthM;
  return std::sqrt(spanE * spanE + spanN * spanN);
}

std::vector<double> Corridors::ReachedAlong(std::span<const RoadStation> along) {
  std::vector<double> reached(along.size(), 0.0);
  for (size_t at = 1; at < along.size(); ++at) {
    reached[at] = reached[at - 1] + StepAlongM(along, at);
  }
  return reached;
}

void Corridors::MarksWaterCrossing(const Paving &on, size_t laneAt, Paved &into) {
  double overWaterM = 0.0;
  for (size_t at = 1; at < into.Along.size(); ++at) {
    double lat = 0.0;
    double lon = 0.0;
    const double midE = 0.5 * (into.Along[at - 1].EastM + into.Along[at].EastM);
    const double midN = 0.5 * (into.Along[at - 1].NorthM + into.Along[at].NorthM);
    const LongitudeLatitude midAt = on.Standing.Geo({.EastM = midE, .NorthM = midN});
    lat = midAt.LatitudeDeg;
    lon = midAt.LongitudeDeg;
    double edgeM = 0.0;
    int second = -1;
    const int which = on.Stack.Classes().ClassAt(
        *on.Classes, {.LongitudeDeg = lon, .LatitudeDeg = lat}, &edgeM, &second);
    ++into.AskedOverBridge;
    if (which < 0 || static_cast<size_t>(which) >= on.Stack.Vegetation().TemplateCount()) {
      continue;
    }
    ++into.NamedOverBridge;
    if (on.Stack.Vegetation().Rows()[static_cast<size_t>(which)].GroundClass != on.WaterRow) {
      continue;
    }
    ++into.WetOverBridge;
    overWaterM += StepAlongM(into.Along, at);
  }
  if (overWaterM > 0.0) {
    double clear = 0.0;
    for (const outshine::Ground::VegetationTemplates::WaterBand &band :
         on.Stack.Vegetation().WaterBands()) {
      clear = static_cast<double>(band.ClearanceM);
      if (overWaterM <= static_cast<double>(band.RunM)) { break; }
    }
    if (clear > 0.0) {
      double stood = -kBeyondAnyCoordinate;
      for (const RoadStation &one : into.Along) { stood = std::max(stood, one.GradeM); }
      into.DeckM[laneAt] = std::max(into.DeckM[laneAt], stood + clear);
      ++into.DecksOverWater;
      into.MostOverWaterM = std::max(into.MostOverWaterM, clear);
    }
  }
}

void Corridors::PaveLane(const Paving &on,
                         Pass pass,
                         size_t laneAt,
                         Paved &into,
                         std::vector<Yields> &corridor,
                         RoadRaised &pavement) const {
  const outshine::Ground::StreetField::Way &lane = on.Ways.Ways()[laneAt];
  if (lane.Form != outshine::Ground::StreetField::Shape::Ribbon || lane.PointCount < 2 ||
      !(lane.HalfWidthM > 0.0f)) {
    into.RefusedWays += pass == Pass::Designing ? 1u : 0u;
    return;
  }
  if (pass == Pass::Designing) {
    if (Unseen(2.0 * static_cast<double>(lane.HalfWidthM), on.FocalPx, AwayM(on, lane))) {
      ++into.UnseenWays;
      return;
    }
    DesignLane(on, lane, laneAt, into);
    return;
  }
  if (into.Designed[laneAt].size() < 2) { return; }
  IslandOf(on, lane, into.Designed[laneAt], corridor);
  const std::pair<uint32_t, uint32_t> &edges = into.EdgesOf[laneAt];
  for (uint32_t edgeAt = edges.first; edgeAt < edges.first + edges.second; ++edgeAt) {
    PaveEdge(on, edgeAt, into, corridor, pavement);
  }
}

void Corridors::PaveEdge(const Paving &on,
                         size_t edgeAt,
                         Paved &into,
                         std::vector<Yields> &corridor,
                         RoadRaised &pavement) const {
  const Edge &edge = into.Edges[edgeAt];
  const size_t laneAt = edge.Lane;
  const outshine::Ground::StreetField::Way &lane = on.Ways.Ways()[laneAt];
  const RoadStation *const first = into.Designed[laneAt].data() + edge.First;
  into.Along.assign(first, first + edge.Count);
  if (into.Along.size() < 2) { return; }

  auto tookFrom = std::chrono::steady_clock::now();
  const auto since = [&tookFrom] {
    const auto was = tookFrom;
    tookFrom = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(tookFrom - was).count();
  };
  {
    static const Heap::Tag kFittingTag("road-fit");
    const Heap::Tagged fitting(kFittingTag);
    FitLane(edge, into);
  }
  into.FitMs += since();
  if (into.Along.size() < 2) {
    ++into.RefusedWays;
    return;
  }
  if (lane.Bridge && on.WaterRow >= 0 && on.Classes) { MarksWaterCrossing(on, laneAt, into); }
  DeckOrRamp(lane, edge, into);
  into.LaidWays += lane.Bridge ? 1u : 0u;
  into.GroundWays += lane.Bridge ? 0u : 1u;
  const bool sealed =
      lane.CoverRow >= 0 &&
      static_cast<size_t>(lane.CoverRow) < on.Stack.Vegetation().TemplateCount() &&
      on.Stack.Vegetation().Rows()[static_cast<size_t>(lane.CoverRow)].Mix[2] >= 1.0f;
  RoadProfile profile = RoadProfile::Rounded;
  if (sealed) { profile = lane.Lanes >= 2 ? RoadProfile::Kerbed : RoadProfile::Simple; }
  Vec3f wears = {{0.5f, 0.5f, 0.5f}};
  if (lane.CoverRow >= 0 &&
      static_cast<size_t>(lane.CoverRow) < on.Stack.Vegetation().TemplateCount()) {
    const Vec4f &cover = on.Stack.Vegetation().Rows()[static_cast<size_t>(lane.CoverRow)].Ground;
    wears = {{cover[0], cover[1], cover[2]}};
  }
  into.WaterMs += since();
  if (lane.Bridge) {
    into.Swept += Sweeper_.Sweep(std::span<const RoadStation>(into.Along.data(), into.Along.size()),
                                 {.HalfWidthM = static_cast<double>(lane.HalfWidthM),
                                  .Profile = profile,
                                  .WearsLinear = wears,
                                  .Crossfall = std::atan(kCrossfall)},
                                 pavement);
  }
  into.SweepMs += since();
  YieldsOf(on, lane, into, corridor);
}

void Corridors::YieldsOf(const Paving &on,
                         const outshine::Ground::StreetField::Way &lane,
                         Paved &into,
                         std::vector<Yields> &corridor) {
  const auto yieldsAt = std::chrono::steady_clock::now();
  for (size_t at = 1; at < into.Along.size(); ++at) {
    const double runE = into.Along[at].EastM - into.Along[at - 1u].EastM;
    const double runN = into.Along[at].NorthM - into.Along[at - 1u].NorthM;
    const double runM = std::sqrt(runE * runE + runN * runN);
    if (!(runM > kLeastSpanM)) { continue; }
    const double groundAt = on.Draped.At(
        {.EastM = into.Along[at].EastM, .NorthM = into.Along[at].NorthM}, into.Along[at].GradeM);
    const double groundBefore =
        on.Draped.At({.EastM = into.Along[at - 1u].EastM, .NorthM = into.Along[at - 1u].NorthM},
                     into.Along[at - 1u].GradeM);
    const double yieldM = std::max(std::fabs(into.Along[at].GradeM - groundAt),
                                   std::fabs(into.Along[at - 1u].GradeM - groundBefore));
    const double outE = runN / runM;
    const double outN = -runE / runM;
    const double half = static_cast<double>(lane.HalfWidthM) + kVergeM;
    double reliefM = std::fabs(groundAt - groundBefore);
    for (const double hand : {1.0, -1.0}) {
      for (int end = 0; end < 2; ++end) {
        const RoadStation &one = end == 0 ? into.Along[at - 1u] : into.Along[at];
        const double sideE = one.EastM + outE * half * hand;
        const double sideN = one.NorthM + outN * half * hand;
        reliefM = std::max(
            reliefM, on.Draped.At({.EastM = sideE, .NorthM = sideN}, one.GradeM) - one.GradeM);
      }
    }
    if (yieldM < kStampWorthM && reliefM < kBrokenGroundM) { continue; }
    Yields made;
    made.RingEastNorthM = {into.Along[at - 1u].EastM + outE * half,
                           into.Along[at - 1u].NorthM + outN * half,
                           into.Along[at].EastM + outE * half,
                           into.Along[at].NorthM + outN * half,
                           into.Along[at].EastM - outE * half,
                           into.Along[at].NorthM - outN * half,
                           into.Along[at - 1u].EastM - outE * half,
                           into.Along[at - 1u].NorthM - outN * half};
    made.LowE = made.HighE = made.RingEastNorthM[0];
    made.LowN = made.HighN = made.RingEastNorthM[1];
    for (size_t k = 2; k + 1 < made.RingEastNorthM.size(); k += 2) {
      made.LowE = std::min(made.LowE, made.RingEastNorthM[k]);
      made.HighE = std::max(made.HighE, made.RingEastNorthM[k]);
      made.LowN = std::min(made.LowN, made.RingEastNorthM[k + 1]);
      made.HighN = std::max(made.HighN, made.RingEastNorthM[k + 1]);
    }
    made.AtE = into.Along[at - 1u].EastM;
    made.AtN = into.Along[at - 1u].NorthM;
    made.PlateauM = into.Along[at - 1u].GradeM;
    const double rise = (into.Along[at].GradeM - into.Along[at - 1u].GradeM) / runM;
    made.SlopeE = rise * runE / runM;
    made.SlopeN = rise * runN / runM;
    made.ApronM = std::clamp(kBatterRun * yieldM, kLeastApronM, kMostApronM);
    made.YieldM = yieldM;
    const bool rests = !lane.Bridge || at == 1u || at + 1u == into.Along.size();
    if (rests) {
      made.SeamEastNorthM = {
          into.Along[at - 1u].EastM + outE * static_cast<double>(lane.HalfWidthM),
          into.Along[at - 1u].NorthM + outN * static_cast<double>(lane.HalfWidthM),
          into.Along[at].EastM + outE * static_cast<double>(lane.HalfWidthM),
          into.Along[at].NorthM + outN * static_cast<double>(lane.HalfWidthM),
          into.Along[at].EastM - outE * static_cast<double>(lane.HalfWidthM),
          into.Along[at].NorthM - outN * static_cast<double>(lane.HalfWidthM),
          into.Along[at - 1u].EastM - outE * static_cast<double>(lane.HalfWidthM),
          into.Along[at - 1u].NorthM - outN * static_cast<double>(lane.HalfWidthM)};
    }
    made.Fills = !lane.Bridge;
    made.Kind = outshine::Stamp::Corridor;
    corridor.push_back(std::move(made));
  }
  into.YieldsMs +=
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - yieldsAt)
          .count();
  into.EdgeStations += into.Along.size();
}

void Corridors::IslandOf(const Paving &on,
                         const outshine::Ground::StreetField::Way &lane,
                         std::span<const RoadStation> along,
                         std::vector<Yields> &corridor) {
  if (!lane.Bridge && along.size() > 3) {
    const size_t shutFrom = static_cast<size_t>(lane.FirstPoint) * 2u;
    const size_t shutTo = shutFrom + (static_cast<size_t>(lane.PointCount) - 1u) * 2u;
    const bool shut = shutTo + 1 < on.Points.size() &&
                      std::fabs(on.Points[shutFrom] - on.Points[shutTo]) < 1.0e-7 &&
                      std::fabs(on.Points[shutFrom + 1] - on.Points[shutTo + 1]) < 1.0e-7;
    if (shut) {
      Yields island;
      island.RingEastNorthM.reserve(along.size() * 2u);
      island.LowE = island.HighE = along.front().EastM;
      island.LowN = island.HighN = along.front().NorthM;
      double summed = 0.0;
      for (const RoadStation &one : along) {
        island.RingEastNorthM.push_back(one.EastM);
        island.RingEastNorthM.push_back(one.NorthM);
        island.LowE = std::min(island.LowE, one.EastM);
        island.HighE = std::max(island.HighE, one.EastM);
        island.LowN = std::min(island.LowN, one.NorthM);
        island.HighN = std::max(island.HighN, one.NorthM);
        summed += one.GradeM;
      }
      island.AtE = along.front().EastM;
      island.AtN = along.front().NorthM;
      island.PlateauM = summed / static_cast<double>(along.size());
      island.ApronM = kLeastApronM;
      island.YieldM = kBrokenGroundM;
      island.Fills = true;
      island.Kind = outshine::Stamp::Corridor;
      corridor.push_back(std::move(island));
    }
  }
}

Corridors::Mapped Corridors::MapOf(const outshine::Ground::GroundStack &stack) {
  Mapped made;
  const outshine::Ground::OsmField *const vectors = stack.Vectors();
  if (vectors == nullptr) { return made; }
  auto created =
      Path::Network::Create(Path::Snap{.CellM = kNodeSnapM}, Path::Sphere{.RadiusM = kWgs84A});
  if (!created) {
    made.Refusal = created.error();
    return made;
  }
  auto net = std::make_shared<Path::Network>(std::move(*created));
  auto phaseBegan = std::chrono::steady_clock::now();
  const auto phaseMs = [&phaseBegan] {
    const auto now = std::chrono::steady_clock::now();
    const double ms = std::chrono::duration<double, std::milli>(now - phaseBegan).count();
    phaseBegan = now;
    return ms;
  };
  if (const auto laid = LayLanesIntoNetwork(stack.Ways(), vectors->Points(), *net); !laid) {
    made.Refusal = laid.error();
    return made;
  }
  made.LayMs = phaseMs();
  made.Ways = net->WayCount();
  if (made.Ways > 0 && !net->Weave(made.Refusal, &made.WeavePhases)) { return made; }
  made.WeaveMs = phaseMs();
  std::vector<Path::Network::Crossing> crossings;
  if (const auto swept = net->Crossings(crossings); !swept) {
    made.Refusal = swept.error();
    return made;
  }
  made.CrossingsMs = phaseMs();
  made.Nodes = net->NodeCount();
  made.Edges = net->EdgeCount();
  made.Junctions = net->JunctionCount();
  made.Elevated =
      net->Elevate([&stack](LongitudeLatitude at) { return stack.Ground().At(at).AslM(); });
  made.ElevateMs = phaseMs();
  made.Network = std::move(net);
  return made;
}

std::expected<void, std::string_view> Corridors::LayLanesIntoNetwork(
    const outshine::Ground::StreetField &ways, std::span<const double> points, Path::Network &net) {
  for (size_t at = 0; at < ways.Ways().size(); ++at) {
    const outshine::Ground::StreetField::Way &lane = ways.Ways()[at];
    if (lane.Form != outshine::Ground::StreetField::Shape::Ribbon || lane.PointCount < 2) {
      continue;
    }
    const size_t first = static_cast<size_t>(lane.FirstPoint) * 2;
    if (first > points.size() || lane.PointCount > (points.size() - first) / 2) {
      return std::unexpected(Says::kInvalidTransportPointRange);
    }
    const auto laid = net.Lay(points.subspan(first, static_cast<size_t>(lane.PointCount) * 2),
                              Path::WayClass{.HalfWidthM = static_cast<double>(lane.HalfWidthM),
                                             .MaxGradient = 0.0,
                                             .MinRadiusM = 0.0,
                                             .Friction = 0.0,
                                             .SpeedMps = static_cast<double>(lane.SpeedMps),
                                             .Lanes = lane.Lanes,
                                             .Priority = lane.Priority,
                                             .Oneway = lane.Oneway,
                                             .Sealed = lane.Sealed,
                                             .Spans = lane.Bridge,
                                             .Tag = at});
    if (!laid) { return laid; }
  }
  return {};
}

void Corridors::FileCrossing(const Path::Network::Crossing &one,
                             const TangentFrame &standing,
                             Paved &into) {
  const EastNorthUp crossedAt = standing.Place(
      {.LongitudeDeg = one.LongitudeDeg, .LatitudeDeg = one.LatitudeDeg, .HeightM = 0.0});
  const uint64_t named =
      PlaceKey({.LongitudeDeg = one.LongitudeDeg, .LatitudeDeg = one.LatitudeDeg}) | 1ULL;
  const auto east = static_cast<int64_t>(std::floor(crossedAt.EastM / kCrossCellM));
  const auto north = static_cast<int64_t>(std::floor(crossedAt.NorthM / kCrossCellM));
  for (int64_t stepE = -1; stepE <= 1; ++stepE) {
    for (int64_t stepS = -1; stepS <= 1; ++stepS) {
      const auto atE = static_cast<uint64_t>(east + stepE + 0x20000000LL);
      const auto atS = static_cast<uint64_t>(north + stepS + 0x20000000LL);
      into.AtCrossing[(atE << 32U) | atS].push_back(
          Meets{.EastM = crossedAt.EastM, .NorthM = crossedAt.NorthM, .Named = named});
    }
  }
}

void Corridors::RaiseDeckOver(const Path::Network::Crossing &one,
                              const Paving &on,
                              const Path::Network &net,
                              Paved &into) {
  const outshine::Ground::StreetField &ways = on.Ways;

  if (one.OverWay >= net.WayCount() || one.UnderWay >= net.WayCount()) { return; }
  const size_t a = net.TagOf(one.OverWay);
  const size_t b = net.TagOf(one.UnderWay);
  const outshine::Ground::StreetField::Way &first = ways.Ways()[a];
  const outshine::Ground::StreetField::Way &second = ways.Ways()[b];
  if (first.Bridge == second.Bridge) { return; }
  const size_t spans = first.Bridge ? a : b;
  const outshine::Ground::StreetField::Way &below = first.Bridge ? second : first;
  const std::optional<Grounded> under =
      GroundUnder(on, {.LongitudeDeg = one.LongitudeDeg, .LatitudeDeg = one.LatitudeDeg});
  if (!under) { return; }
  const double onDrawn = under->GradeM;
  const double need = onDrawn + static_cast<double>(below.ClearanceM);
  if (need <= into.DeckM[spans]) { return; }
  if (into.DeckM[spans] < kUnraisedDeckM) { ++into.DecksRaised; }
  into.DeckM[spans] = need;
  into.MostRaisedM = std::max(into.MostRaisedM, need - onDrawn);
}

void Corridors::Crosses(const Paving &on, Paved &into) {
  const TangentFrame &standing = on.Standing;

  auto partAt = std::chrono::steady_clock::now();
  const auto part = [&partAt] {
    const auto was = partAt;
    partAt = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(partAt - was).count();
  };

  if (on.Network == nullptr) { return; }
  const Path::Network &net = *on.Network;
  into.CrossNetworkMs = part();

  std::vector<Path::Network::Crossing> crossed;
  const auto swept = net.Crossings(crossed);
  if (!swept) { return; }
  std::ranges::sort(crossed,
                    [&net](const Path::Network::Crossing &a, const Path::Network::Crossing &b) {
                      const std::array<size_t, 4> ka = {
                          net.TagOf(a.OverWay), net.TagOf(a.UnderWay), a.OverAt, a.UnderAt};
                      const std::array<size_t, 4> kb = {
                          net.TagOf(b.OverWay), net.TagOf(b.UnderWay), b.OverAt, b.UnderAt};
                      return ka < kb;
                    });
  into.CrossSweepMs = part();
  into.CrossingsSeen = crossed.size();
  into.PairsTested = swept->PairsTested;
  into.PairsPruned = swept->PairsPruned;
  into.FullestCell = swept->FullestCell;
  for (const Path::Network::Crossing &one : crossed) { FileCrossing(one, standing, into); }
  into.CrossFilingMs = part();
  for (const Path::Network::Crossing &one : crossed) { RaiseDeckOver(one, on, net, into); }
  into.CrossDecksMs = part();
}

std::optional<Corridors::Ends> Corridors::EndsOf(const outshine::Ground::OsmField &vectors,
                                                 const outshine::Ground::StreetField::Way &lane) {
  const std::span<const double> points = vectors.Points();
  const size_t first = static_cast<size_t>(lane.FirstPoint) * 2u;
  const size_t last = first + (static_cast<size_t>(lane.PointCount) - 1u) * 2u;
  if (last + 1 >= points.size()) { return std::nullopt; }
  Ends out;
  out.At = {{points[first], points[first + 1], points[last], points[last + 1]}};
  out.Key = {{PlaceKey({.LongitudeDeg = out.At[1], .LatitudeDeg = out.At[0]}),
              PlaceKey({.LongitudeDeg = out.At[3], .LatitudeDeg = out.At[2]})}};
  return out;
}

std::optional<Corridors::Grounded> Corridors::GroundUnder(const Paving &on, LongitudeLatitude at) {
  const TangentFrame &standing = on.Standing;
  const Drape &drapedOver = on.Draped;

  const EastNorthUp flat = standing.Place(
      {.LongitudeDeg = at.LongitudeDeg, .LatitudeDeg = at.LatitudeDeg, .HeightM = 0.0});
  const Drape::EastNorth flatHere = {.EastM = flat.EastM, .NorthM = flat.NorthM};
  if (const std::optional<double> grade = drapedOver.Sample(flatHere)) {
    return Grounded{.EastM = flatHere.EastM, .NorthM = flatHere.NorthM, .GradeM = *grade};
  }

  const std::optional<double> stood = on.Stack.Ground().At(at).AslM();
  if (!stood) { return std::nullopt; }
  const EastNorthUp enu = standing.Place(
      {.LongitudeDeg = at.LongitudeDeg, .LatitudeDeg = at.LatitudeDeg, .HeightM = *stood});
  const Drape::EastNorth here = {.EastM = enu.EastM, .NorthM = enu.NorthM};
  return Grounded{
      .EastM = here.EastM, .NorthM = here.NorthM, .GradeM = drapedOver.At(here, enu.UpM)};
}

void Corridors::RaisesEnds(std::span<const uint64_t> key, double deckM, Paved &into) {
  for (const uint64_t one : key) {
    const auto found = into.EndM.find(one);
    if (found == into.EndM.end()) {
      into.EndM.emplace(one, deckM);
      continue;
    }
    found->second = std::max(found->second, deckM);
  }
}

Corridors::BridgeTopology Corridors::BridgeTopologyOf(const Paving &on) {
  const outshine::Ground::StreetField &ways = on.Ways;
  BridgeTopology topology{.EndsOfWay = std::vector<Ends>(ways.Ways().size()),
                          .HasEnds = std::vector<uint8_t>(ways.Ways().size()),
                          .WaysAt = {},
                          .PlaceOf = {}};
  for (size_t at = 0; at < ways.Ways().size(); ++at) { AppendBridgeTopology(on, at, topology); }
  return topology;
}

void Corridors::AppendBridgeTopology(const Paving &on, size_t laneAt, BridgeTopology &topology) {
  const outshine::Ground::StreetField::Way &lane = on.Ways.Ways()[laneAt];
  if (lane.Form != outshine::Ground::StreetField::Shape::Ribbon || lane.PointCount < 2) { return; }
  const std::optional<Ends> ends = EndsOf(on.Vectors, lane);
  if (!ends) { return; }
  topology.EndsOfWay[laneAt] = *ends;
  topology.HasEnds[laneAt] = 1;
  for (size_t side = 0; side < 2; ++side) {
    const uint64_t key = ends->Key[side];
    const size_t axis = side * 2u;
    topology.WaysAt[key].push_back(laneAt);
    topology.PlaceOf.try_emplace(
        key, LongitudeLatitude{.LongitudeDeg = ends->At[axis + 1u], .LatitudeDeg = ends->At[axis]});
  }
}

std::vector<uint64_t>
Corridors::RelevantBridgeEnds(const Paving &on, const BridgeTopology &topology, const Paved &into) {
  std::unordered_set<uint64_t> relevant;
  std::vector<uint64_t> frontier;
  std::vector<uint64_t> ordered;
  const auto admits = [&](uint64_t key, std::vector<uint64_t> &intoFrontier) {
    if (relevant.insert(key).second) {
      intoFrontier.push_back(key);
      ordered.push_back(key);
    }
  };
  for (size_t at = 0; at < on.Ways.Ways().size(); ++at) {
    if (topology.HasEnds[at] == 0 || !on.Ways.Ways()[at].Bridge ||
        into.DeckM[at] <= kUnraisedDeckM) {
      continue;
    }
    admits(topology.EndsOfWay[at].Key[0], frontier);
    admits(topology.EndsOfWay[at].Key[1], frontier);
  }
  for (int pass = 0; pass < kRampPasses && !frontier.empty(); ++pass) {
    std::vector<uint64_t> next;
    for (const uint64_t key : frontier) {
      const auto found = topology.WaysAt.find(key);
      if (found == topology.WaysAt.end()) { continue; }
      for (const size_t lane : found->second) {
        if (topology.HasEnds[lane] == 0) { continue; }
        admits(topology.EndsOfWay[lane].Key[0], next);
        admits(topology.EndsOfWay[lane].Key[1], next);
      }
    }
    frontier = std::move(next);
  }
  return ordered;
}

void Corridors::SeedsBridgeEnds(const Paving &on, Paved &into) {
  const BridgeTopology topology = BridgeTopologyOf(on);
  const std::vector<uint64_t> ordered = RelevantBridgeEnds(on, topology, into);
  for (const uint64_t key : ordered) {
    const auto place = topology.PlaceOf.find(key);
    if (place == topology.PlaceOf.end()) { continue; }
    const std::optional<Grounded> under = GroundUnder(on, place->second);
    if (!under) { continue; }
    into.EndM.insert_or_assign(key, under->GradeM);
    into.GroundEndM.insert_or_assign(key, under->GradeM);
  }
  for (size_t at = 0; at < on.Ways.Ways().size(); ++at) {
    if (topology.HasEnds[at] != 0 && on.Ways.Ways()[at].Bridge && into.DeckM[at] > kUnraisedDeckM) {
      RaisesEnds(topology.EndsOfWay[at].Key, into.DeckM[at], into);
    }
  }
}

double Corridors::HighestDeckM(const Paved &over) {
  double mostDeckM = 0.0;
  for (const auto &one : over.EndM) {
    const auto seeded = over.GroundEndM.find(one.first);
    if (seeded == over.GroundEndM.end()) { continue; }
    mostDeckM = std::max(mostDeckM, one.second - seeded->second);
  }
  return mostDeckM;
}

void Corridors::EasesRamps(const outshine::Ground::StreetField &ways,
                           const outshine::Ground::OsmField &vectors,
                           double mostDeckM,
                           Paved &into) {
  for (int pass = 0; pass < kRampPasses; ++pass) { EaseRampPass(ways, vectors, mostDeckM, into); }
}

void Corridors::EaseRampPass(const outshine::Ground::StreetField &ways,
                             const outshine::Ground::OsmField &vectors,
                             double mostDeckM,
                             Paved &into) {
  for (const outshine::Ground::StreetField::Way &lane : ways.Ways()) {
    if (lane.Form != outshine::Ground::StreetField::Shape::Ribbon || lane.PointCount < 2) {
      continue;
    }
    if (!(lane.MaxGradient > 0.0f)) { continue; }
    const std::optional<Ends> ends = EndsOf(vectors, lane);
    if (!ends) { continue; }
    const std::array<uint64_t, 2> &key = ends->Key;
    const auto low = into.EndM.find(key[0]);
    const auto high = into.EndM.find(key[1]);
    if (low == into.EndM.end() || high == into.EndM.end()) { continue; }
    const double perLon = kMPerDegLon * std::cos(ends->At[0] * kDeg2Rad);
    const double runE = (ends->At[3] - ends->At[1]) * perLon;
    const double runN = (ends->At[2] - ends->At[0]) * kMPerDegLat;
    const double runM = std::sqrt(runE * runE + runN * runN);
    const double mostM = runM * static_cast<double>(lane.MaxGradient);
    const double apartM = high->second - low->second;
    const auto capped = [&](uint64_t at, double toM) {
      const auto seeded = into.GroundEndM.find(at);
      return seeded == into.GroundEndM.end() ? toM : std::min(toM, seeded->second + mostDeckM);
    };
    if (apartM > mostM) {
      low->second = capped(low->first, high->second - mostM);
    } else if (-apartM > mostM) {
      high->second = capped(high->first, low->second - mostM);
    }
  }
}

void Corridors::GradesApproaches(const Paving &on, Paved &into) {
  const outshine::Ground::StreetField &ways = on.Ways;

  for (const outshine::Ground::StreetField::Way &lane : ways.Ways()) {
    if (lane.Bridge || lane.Form != outshine::Ground::StreetField::Shape::Ribbon) { continue; }
    if (lane.PointCount < 2) { continue; }
    const std::optional<Ends> ends = EndsOf(on.Vectors, lane);
    if (!ends) { continue; }
    const std::array<uint64_t, 2> &key = ends->Key;
    double rose = 0.0;
    for (size_t side = 0; side < 2; ++side) {
      const auto found = into.EndM.find(key[side]);
      if (found == into.EndM.end()) { continue; }
      const size_t axis = side * 2u;
      const std::optional<Grounded> under =
          GroundUnder(on, {.LongitudeDeg = ends->At[axis + 1u], .LatitudeDeg = ends->At[axis]});
      if (under) { rose = std::max(rose, found->second - under->GradeM); }
    }
    if (rose > kRoseLeast) {
      ++into.RampsRaised;
      into.SteepestRamp = std::max(into.SteepestRamp, rose);
    }
  }
}

void Corridors::Bridges(const Paving &on, Paved &into) {
  auto began = std::chrono::steady_clock::now();
  const auto elapsed = [&began] {
    const auto previous = began;
    began = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(began - previous).count();
  };
  SeedsBridgeEnds(on, into);
  Notes(into, "streets: of raising decks, seeding bridge ends", elapsed(), "ms");
  Notes(into, "streets: the highest deck a ramp must reach", HighestDeckM(into), "m");
  EasesRamps(on.Ways, on.Vectors, HighestDeckM(into), into);
  Notes(into, "streets: of raising decks, easing ramps", elapsed(), "ms");
  GradesApproaches(on, into);
  Notes(into, "streets: of raising decks, grading approaches", elapsed(), "ms");
}

void Corridors::SplitsEdges(Paved &into) {
  into.Edges.clear();
  into.EdgesOf.assign(into.Designed.size(), {0u, 0u});
  for (uint32_t lane = 0; lane < static_cast<uint32_t>(into.Designed.size()); ++lane) {
    const std::vector<RoadStation> &along = into.Designed[lane];
    if (along.size() < 2) { continue; }
    into.EdgesOf[lane].first = static_cast<uint32_t>(into.Edges.size());
    uint32_t first = 0;
    for (uint32_t at = 1; at < static_cast<uint32_t>(along.size()); ++at) {
      const bool last = at + 1u == along.size();
      if (along[at].Node == 0u && !last) { continue; }
      into.Edges.push_back({.Lane = lane,
                            .First = first,
                            .Count = at - first + 1u,
                            .NodeAt = {along[first].Node, along[at].Node}});
      first = at;
    }
    into.EdgesOf[lane].second = static_cast<uint32_t>(into.Edges.size()) - into.EdgesOf[lane].first;
  }
}

namespace {

struct Bound {
  std::vector<double> EastM;
  std::vector<double> NorthM;
  std::vector<double> AlongM;
};

struct Origin {
  double EastM = 0.0;
  double NorthM = 0.0;
  bool Stands = false;
};

struct Reach {
  double SideM = 0.0;
  double ReachM = 0.0;
};

struct Heading {
  double EastM = 0.0;
  double NorthM = 0.0;
};

Heading HeadingAt(std::span<const RoadStation> along, size_t at, bool backward) {
  const size_t n = along.size();
  size_t next = at;
  size_t prev = at;
  if (backward) {
    if (at > 0) { next = at - 1u; }
    if (at + 1u < n) { prev = at + 1u; }
  } else {
    if (at + 1u < n) { next = at + 1u; }
    if (at > 0) { prev = at - 1u; }
  }
  Heading out{.EastM = along[next].EastM - along[prev].EastM,
              .NorthM = along[next].NorthM - along[prev].NorthM};
  const double run = std::sqrt(out.EastM * out.EastM + out.NorthM * out.NorthM);
  if (run > kLeastRunM) {
    out.EastM /= run;
    out.NorthM /= run;
  }
  return out;
}

void StartsAt(std::span<const RoadStation> along,
              bool backward,
              double sideM,
              Origin from,
              Bound *out,
              double *reached) {
  const size_t n = along.size();
  const RoadStation &root = backward ? along.back() : along.front();
  const RoadStation &ahead = backward ? along[n - 2u] : along[1];
  Heading dir{.EastM = ahead.EastM - from.EastM, .NorthM = ahead.NorthM - from.NorthM};
  const double run = std::sqrt(dir.EastM * dir.EastM + dir.NorthM * dir.NorthM);
  if (run > kLeastRunM) {
    dir.EastM /= run;
    dir.NorthM /= run;
  }
  out->EastM.push_back(from.EastM - dir.NorthM * sideM);
  out->NorthM.push_back(from.NorthM + dir.EastM * sideM);
  out->AlongM.push_back(0.0);
  const double offE = root.EastM - from.EastM;
  const double offN = root.NorthM - from.NorthM;
  *reached = std::sqrt(offE * offE + offN * offN);
}

Bound BoundaryOf(std::span<const RoadStation> along, bool backward, Reach reach, Origin from = {}) {
  Bound out;
  const size_t n = along.size();
  double reached = 0.0;
  if (from.Stands) { StartsAt(along, backward, reach.SideM, from, &out, &reached); }
  for (size_t step = 0; step < n; ++step) {
    const size_t at = backward ? n - 1u - step : step;
    const Heading dir = HeadingAt(along, at, backward);
    if (step > 0) {
      const size_t was = backward ? at + 1u : at - 1u;
      const double sE = along[at].EastM - along[was].EastM;
      const double sN = along[at].NorthM - along[was].NorthM;
      reached += std::sqrt(sE * sE + sN * sN);
    }
    out.EastM.push_back(along[at].EastM - dir.NorthM * reach.SideM);
    out.NorthM.push_back(along[at].NorthM + dir.EastM * reach.SideM);
    out.AlongM.push_back(reached);
    if (reached > reach.ReachM) { break; }
  }
  return out;
}

struct Cut {
  double AlongA = 0.0;
  double AlongB = 0.0;
};

std::optional<Cut> NearestCut(const Bound &a, const Bound &b) {
  std::optional<Cut> best;
  for (size_t i = 1; i < a.EastM.size(); ++i) {
    const double ax = a.EastM[i - 1];
    const double ay = a.NorthM[i - 1];
    const double rx = a.EastM[i] - ax;
    const double ry = a.NorthM[i] - ay;
    for (size_t j = 1; j < b.EastM.size(); ++j) {
      const double bx = b.EastM[j - 1];
      const double by = b.NorthM[j - 1];
      const double sx = b.EastM[j] - bx;
      const double sy = b.NorthM[j] - by;
      const double denom = rx * sy - ry * sx;
      if (std::fabs(denom) < kLeastSineBetween) { continue; }
      const double t = ((bx - ax) * sy - (by - ay) * sx) / denom;
      const double u = ((bx - ax) * ry - (by - ay) * rx) / denom;
      if (t < 0.0 || t > 1.0 || u < 0.0 || u > 1.0) { continue; }
      const Cut here{.AlongA = a.AlongM[i - 1] + t * (a.AlongM[i] - a.AlongM[i - 1]),
                     .AlongB = b.AlongM[j - 1] + u * (b.AlongM[j] - b.AlongM[j - 1])};
      if (!best || here.AlongA < best->AlongA) { best = here; }
    }
  }
  return best;
}

Bound RayOf(const Bound &along) {
  Bound ray;
  size_t last = 1;
  while (last + 1u < along.EastM.size() && along.AlongM[last] < kAngleLookaheadM) { ++last; }
  double dE = along.EastM[last] - along.EastM[0];
  double dN = along.NorthM[last] - along.NorthM[0];
  const double run = std::sqrt(dE * dE + dN * dN);
  if (run > kLeastRunM) {
    dE /= run;
    dN /= run;
  }
  ray.EastM = {along.EastM[0], along.EastM[0] + dE * kBoundaryReachM};
  ray.NorthM = {along.NorthM[0], along.NorthM[0] + dN * kBoundaryReachM};
  ray.AlongM = {0.0, kBoundaryReachM};
  return ray;
}

}

std::unordered_map<uint64_t, std::vector<Corridors::Leg>> Corridors::LegsOf(const Paving &on,
                                                                            const Paved &into) {
  std::unordered_map<uint64_t, std::vector<Leg>> legsAt;
  for (uint32_t edgeAt = 0; edgeAt < static_cast<uint32_t>(into.Edges.size()); ++edgeAt) {
    AppendLeg(on, into, edgeAt, legsAt);
  }
  return legsAt;
}

void Corridors::AppendLeg(const Paving &on,
                          const Paved &into,
                          uint32_t edgeAt,
                          std::unordered_map<uint64_t, std::vector<Leg>> &legsAt) {
  const Edge &edge = into.Edges[edgeAt];
  const std::vector<RoadStation> &along = into.Designed[edge.Lane];
  const auto halfM = static_cast<double>(on.Ways.Ways()[edge.Lane].HalfWidthM);
  for (uint8_t end = 0; end < 2; ++end) {
    if (edge.NodeAt[end] == 0u) { continue; }
    const std::span<const RoadStation> stations(along.data() + edge.First, edge.Count);
    const Bound centre = BoundaryOf(stations, end == 1, {.SideM = 0.0, .ReachM = kAngleLookaheadM});
    const size_t look = centre.EastM.size() - 1u;
    const double dE = centre.EastM[look] - centre.EastM[0];
    const double dN = centre.NorthM[look] - centre.NorthM[0];
    legsAt[edge.NodeAt[end]].push_back(
        {.Edge = edgeAt, .End = end, .AngleRad = std::atan2(dN, dE), .HalfM = halfM, .CutM = 0.0});
  }
}

void Corridors::GatesOf(std::span<const Leg> legs, const Paved &into, Junction &made) {
  for (const Leg &leg : legs) {
    const Edge &edge = into.Edges[leg.Edge];
    const std::span<const RoadStation> stations(into.Designed[edge.Lane].data() + edge.First,
                                                edge.Count);
    const Bound line = BoundaryOf(stations,
                                  leg.End == 1,
                                  {.SideM = 0.0, .ReachM = leg.CutM},
                                  {.EastM = made.EastM, .NorthM = made.NorthM, .Stands = true});
    const size_t rim = line.EastM.size() - 1u;
    const size_t before = rim == 0 ? 0 : rim - 1u;
    double outE = line.EastM[rim] - line.EastM[before];
    double outN = line.NorthM[rim] - line.NorthM[before];
    const double run = std::sqrt(outE * outE + outN * outN);
    if (run > kLeastRunM) {
      outE /= run;
      outN /= run;
    }
    const double along =
        line.AlongM[rim] > kLeastRunM ? std::min(1.0, leg.CutM / line.AlongM[rim]) : 0.0;
    made.Gates.push_back(
        RoadGate{.EastM = line.EastM[before] + (line.EastM[rim] - line.EastM[before]) * along,
                 .NorthM = line.NorthM[before] + (line.NorthM[rim] - line.NorthM[before]) * along,
                 .GradeM = made.GradeM,
                 .OutE = outE,
                 .OutN = outN,
                 .HalfWidthM = leg.HalfM});
  }
}

void Corridors::ShapeOf(const Paving &on, uint64_t node, std::vector<Leg> &legs, Paved &into) {
  Junction made{.Node = node, .Legs = {}, .Gates = {}};
  const size_t n = legs.size();
  for (const Leg &leg : legs) {
    const Edge &edge = into.Edges[leg.Edge];
    const std::span<const RoadStation> stations(into.Designed[edge.Lane].data() + edge.First,
                                                edge.Count);
    const RoadStation &root = leg.End == 1 ? stations.back() : stations.front();
    made.EastM += root.EastM / static_cast<double>(n);
    made.NorthM += root.NorthM / static_cast<double>(n);
    made.GradeM += root.GradeM / static_cast<double>(n);
  }
  const Origin centre{.EastM = made.EastM, .NorthM = made.NorthM, .Stands = true};
  for (Leg &leg : legs) {
    const Edge &edge = into.Edges[leg.Edge];
    const std::span<const RoadStation> stations(into.Designed[edge.Lane].data() + edge.First,
                                                edge.Count);
    const Bound ray = RayOf(
        BoundaryOf(stations, leg.End == 1, {.SideM = 0.0, .ReachM = kAngleLookaheadM}, centre));
    leg.AngleRad = std::atan2(ray.NorthM[1] - ray.NorthM[0], ray.EastM[1] - ray.EastM[0]);
  }
  std::ranges::sort(legs, ByBearing);
  std::vector<Bound> left(n);
  std::vector<Bound> right(n);
  for (size_t i = 0; i < n; ++i) {
    const Edge &edge = into.Edges[legs[i].Edge];
    const std::span<const RoadStation> stations(into.Designed[edge.Lane].data() + edge.First,
                                                edge.Count);
    left[i] = BoundaryOf(
        stations, legs[i].End == 1, {.SideM = legs[i].HalfM, .ReachM = kBoundaryReachM}, centre);
    right[i] = BoundaryOf(
        stations, legs[i].End == 1, {.SideM = -legs[i].HalfM, .ReachM = kBoundaryReachM}, centre);
  }
  bool decked = false;
  for (const Leg &leg : legs) {
    decked = decked || on.Ways.Ways()[into.Edges[leg.Edge].Lane].Bridge;
  }
  const double rootsM = made.GradeM;
  made.GradeM = on.Draped.At({.EastM = made.EastM, .NorthM = made.NorthM}, made.GradeM);
  const auto seeded = into.EndM.find(node);
  if (decked && seeded != into.EndM.end()) { made.GradeM = seeded->second; }
  for (size_t i = 0; i < n; ++i) {
    const size_t next = (i + 1u) % n;
    double apartDeg = (legs[next].AngleRad - legs[i].AngleRad) * kRad2Deg;
    if (apartDeg < 0.0) { apartDeg += 2.0 * kDegPerHalfTurn; }
    if (apartDeg < kParallelWithinDeg || apartDeg > kContinuesPastDeg) { continue; }
    const std::optional<Cut> cut = NearestCut(RayOf(left[i]), RayOf(right[next]));
    if (!cut) { continue; }
    legs[i].CutM = std::max(legs[i].CutM, cut->AlongA);
    legs[next].CutM = std::max(legs[next].CutM, cut->AlongB);
  }
  for (Leg &leg : legs) {
    leg.CutM += kJunctionRadiusM;
    leg.CutM =
        std::min(leg.CutM, std::max(0.0, left[&leg - legs.data()].AlongM.back() - kLeastRoadM));
    into.Edges[leg.Edge].CutM[leg.End] = leg.CutM;
    into.Edges[leg.Edge].GradeAtM[leg.End] = made.GradeM;
    into.Edges[leg.Edge].Joined[leg.End] = true;
    into.DeepestCutM = std::max(into.DeepestCutM, leg.CutM);
    ++into.LegsCut;
  }
  GatesOf(legs, into, made);
  if (!(decked && seeded != into.EndM.end())) { LiesOnItsPlane(on, made, into); }
  for (const Leg &leg : legs) {
    const RoadGate &gate = made.Gates[static_cast<size_t>(&leg - legs.data())];
    into.Edges[leg.Edge].GradeAtM[leg.End] = gate.GradeM;
  }
  PressesUnder(made, rootsM, into);
  made.Legs = std::move(legs);
  into.MostOffGroundM = std::max(into.MostOffGroundM, std::fabs(made.GradeM - rootsM));
  into.Junctions.push_back(std::move(made));
}

void Corridors::LiesOnItsPlane(const Paving &on, Junction &made, Paved &into) {
  const auto drape = [&](double eastM, double southM) {
    return on.Draped.At({.EastM = eastM, .NorthM = southM}, made.GradeM);
  };
  const double east = drape(made.EastM + kJunctionRadiusM, made.NorthM);
  const double west = drape(made.EastM - kJunctionRadiusM, made.NorthM);
  const double north = drape(made.EastM, made.NorthM + kJunctionRadiusM);
  const double south = drape(made.EastM, made.NorthM - kJunctionRadiusM);
  made.SlopeE = (east - west) / (2.0 * kJunctionRadiusM);
  made.SlopeN = (north - south) / (2.0 * kJunctionRadiusM);
  const double steep = std::sqrt(made.SlopeE * made.SlopeE + made.SlopeN * made.SlopeN);
  if (steep > kSteepestJunction) {
    made.SlopeE *= kSteepestJunction / steep;
    made.SlopeN *= kSteepestJunction / steep;
    ++into.JunctionsLevelled;
  }
  for (RoadGate &gate : made.Gates) {
    gate.GradeM = made.GradeM + made.SlopeE * (gate.EastM - made.EastM) +
                  made.SlopeN * (gate.NorthM - made.NorthM);
  }
  into.SteepestJunction = std::max(
      into.SteepestJunction, std::sqrt(made.SlopeE * made.SlopeE + made.SlopeN * made.SlopeN));
}

void Corridors::PressesUnder(const Junction &made, double rootsM, Paved &into) {
  struct Corner {
    double AroundRad = 0.0;
    double EastM = 0.0;
    double NorthM = 0.0;
    size_t Gate = 0;
  };

  std::vector<Corner> around;
  for (size_t at = 0; at < made.Gates.size(); ++at) {
    const RoadGate &gate = made.Gates[at];
    for (const double hand : {1.0, -1.0}) {
      const double e = gate.EastM + gate.OutN * gate.HalfWidthM * hand;
      const double n = gate.NorthM - gate.OutE * gate.HalfWidthM * hand;
      around.push_back({.AroundRad = std::atan2(made.NorthM - n, e - made.EastM),
                        .EastM = e,
                        .NorthM = n,
                        .Gate = at * 2u + (hand > 0.0 ? 0u : 1u)});
    }
  }
  if (around.size() < 3) { return; }
  std::ranges::sort(around, [](const Corner &a, const Corner &b) {
    return a.AroundRad != b.AroundRad ? a.AroundRad < b.AroundRad : a.Gate < b.Gate;
  });
  Yields under;
  under.RingEastNorthM.reserve(around.size() * 2u);
  under.LowE = under.HighE = around.front().EastM;
  under.LowN = under.HighN = around.front().NorthM;
  for (const Corner &one : around) {
    under.RingEastNorthM.push_back(one.EastM);
    under.RingEastNorthM.push_back(one.NorthM);
    under.LowE = std::min(under.LowE, one.EastM);
    under.HighE = std::max(under.HighE, one.EastM);
    under.LowN = std::min(under.LowN, one.NorthM);
    under.HighN = std::max(under.HighN, one.NorthM);
  }
  under.AtE = made.EastM;
  under.AtN = made.NorthM;
  under.PlateauM = made.GradeM - kPavementLipM;
  under.SlopeE = made.SlopeE;
  under.SlopeN = made.SlopeN;
  under.YieldM = std::max(std::fabs(made.GradeM - rootsM), kBrokenGroundM);
  under.ApronM = std::clamp(kBatterRun * under.YieldM, kLeastApronM, kMostApronM);
  under.SeamEastNorthM = under.RingEastNorthM;
  under.Fills = true;
  under.Kind = outshine::Stamp::Corridor;
  into.UnderJunctions.push_back(std::move(under));
}

void Corridors::ShapesJunctions(const Paving &on, Paved &into) {
  std::unordered_map<uint64_t, std::vector<Leg>> legsAt = LegsOf(on, into);
  std::vector<uint64_t> nodes;
  for (const auto &one : legsAt) {
    if (one.second.size() >= 2) { nodes.push_back(one.first); }
  }
  std::ranges::sort(nodes);
  for (const uint64_t node : nodes) {
    std::vector<Leg> &legs = legsAt[node];
    std::ranges::sort(legs, ByBearing);
    if (legs.size() == 2) {
      ++into.Continuations;
      continue;
    }
    ShapeOf(on, node, legs, into);
  }
}

void Corridors::DeckOrRamp(const outshine::Ground::StreetField::Way &lane,
                           const Edge &edge,
                           Paved &into) {
  if (lane.Bridge) {
    double deck = into.DeckM[edge.Lane];
    for (const RoadStation &one : into.Along) { deck = std::max(deck, one.GradeM); }
    for (RoadStation &one : into.Along) { one.GradeM = deck; }
    return;
  }
  const double gradient =
      lane.MaxGradient > 0.0f ? static_cast<double>(lane.MaxGradient) : kSteepestApproach;
  const std::vector<double> reached = ReachedAlong(into.Along);
  for (int end = 0; end < 2; ++end) {
    if (!edge.Joined[end]) { continue; }
    const double atM = end == 0 ? 0.0 : reached.back();
    const RoadStation &rim = end == 0 ? into.Along.front() : into.Along.back();
    const double liftM = edge.GradeAtM[end] - rim.GradeM;
    if (std::fabs(liftM) < kLeastCapM) { continue; }
    const double overM = std::fabs(liftM) / gradient;
    for (size_t at = 0; at < into.Along.size(); ++at) {
      const double s = std::fabs(reached[at] - atM);
      if (s >= overM) { continue; }
      into.Along[at].GradeM += liftM * (1.0 - s / overM);
      ++into.RampStations;
      into.LongestRampM = std::max(into.LongestRampM, s);
    }
    into.MostLiftedM = std::max(into.MostLiftedM, std::fabs(liftM));
  }
}

size_t Corridors::RaisesTheJunctionBodies(const outshine::Ground::GroundMaterials &wearing,
                                          Paved &into,
                                          RoadRaised &pavement) const {
  const int asphalt = wearing.Find("asphalt");
  Vec3f wears = {{0.5f, 0.5f, 0.5f}};
  if (asphalt >= 0) { wears = wearing.At(static_cast<size_t>(asphalt)).Albedo; }
  for (const Junction &one : into.Junctions) {
    Sweeper_.Junction(std::span<const RoadGate>(one.Gates.data(), one.Gates.size()),
                      {.SlopeE = one.SlopeE, .SlopeN = one.SlopeN},
                      wears,
                      pavement);
  }
  return into.Junctions.size();
}

void Corridors::TellsWhatTheFitFound(Paved &into) {
  if (into.FitsMeasured == 0) { return; }
  Notes(into,
        "streets: ways a reference line was fitted to",
        static_cast<double>(into.FitLaid),
        "ways");
  Notes(into, "streets: and ways the fit refused", static_cast<double>(into.FitRefused), "ways");
  Notes(into,
        "streets: corners too tight to drive, cut instead",
        static_cast<double>(into.FitTooTight),
        "corners");
  Notes(into, "streets: cuts the split made", static_cast<double>(into.FitCuts), "cuts");
  Notes(into,
        "streets: stations a chord asked for",
        static_cast<double>(into.ChordAdded),
        "stations");
  Notes(into,
        "streets: pieces the sweep laid on a line",
        static_cast<double>(into.Swept.Pieces),
        "pieces");
  Notes(into, "streets: cuts the sweep made", static_cast<double>(into.Swept.Cuts), "cuts");
  Notes(into,
        "streets: pieces the sweep could not lay",
        static_cast<double>(into.Swept.Refused),
        "pieces");
  Notes(into,
        "streets: of those, the fit refused",
        static_cast<double>(into.Swept.Why.Fit),
        "pieces");
  Notes(into,
        "streets: of those, the rise refused",
        static_cast<double>(into.Swept.Why.Rise),
        "pieces");
  Notes(into,
        "streets: of those, the bank refused",
        static_cast<double>(into.Swept.Why.Bank),
        "pieces");
  Notes(into,
        "streets: of those, the sweep refused",
        static_cast<double>(into.Swept.Why.Sweep),
        "pieces");
  Notes(into,
        "streets: of those, too short to lay",
        static_cast<double>(into.Swept.Why.TooShort),
        "pieces");
  Notes(into,
        "streets: pieces the split still could not lay",
        static_cast<double>(into.FitUnsplittable),
        "pieces");
  Notes(into, "streets: the tightest radius a corner demanded", into.TightestDemandM, "m");
  Notes(into, "streets: the radius a fitted line found, tightest", into.FitRadiusTightestM, "m");
  Notes(into, "streets: reference lines measured", static_cast<double>(into.FitsMeasured), "ways");
  Notes(into,
        "streets: stations the fit calls undrivable",
        static_cast<double>(into.FitUndrivable),
        "stations");
}

bool Corridors::HandsThePavingOver(const outshine::Ground::GroundMaterials &wearing,
                                   const RoadRaised &pavement,
                                   Paved &into,
                                   Geometry &ground) {

  if (pavement.Index.size() < 3) { return true; }
  Material tarmac;
  for (int channel = 0; channel < 3; ++channel) { tarmac.BaseColour[channel] = 1.0f; }
  {
    const int asphalt = wearing.Find("asphalt");
    tarmac.Roughness =
        asphalt >= 0 ? wearing.At(static_cast<size_t>(asphalt)).Roughness : kUnlitTint;
  }
  const auto paved = ground.addSurface("streets", tarmac);
  if (!paved) { return false; }
  const auto createdPart = ground.addPart("streets", *paved);
  if (!createdPart) { return false; }
  const int pavedPart = *createdPart;
  const bool tookPaving =
      pavedPart >= 0 &&
      ground.setPositions(
          pavedPart,
          std::span<const float>(pavement.PositionM.data(), pavement.PositionM.size())) &&
      ground.setNormals(pavedPart,
                        std::span<const float>(pavement.NormalM.data(), pavement.NormalM.size())) &&
      ground.setColours(
          pavedPart,
          std::span<const float>(pavement.ColourRgba.data(), pavement.ColourRgba.size())) &&
      ground.setTriangles(pavedPart,
                          std::span<const uint32_t>(pavement.Index.data(), pavement.Index.size()));
  Notes(into, "streets: the surface they were given", static_cast<double>(paved->index()), "index");
  Notes(into, "streets: the part they were given", static_cast<double>(pavedPart), "index");
  Notes(into, "streets: the geometry took them", tookPaving ? 1.0 : 0.0, "yes/no");
  Notes(into,
        "streets: triangles wound against their normals",
        static_cast<double>(ground.windingAgainstNormals(pavedPart)),
        "triangles");
  Notes(
      into, "streets: parts the geometry now holds", static_cast<double>(ground.parts()), "parts");
  return tookPaving;
}

std::unordered_map<uint64_t, uint32_t>
Corridors::SharedNodesOf(const outshine::Ground::StreetField &ways,
                         std::span<const double> points) {
  std::unordered_map<uint64_t, uint32_t> shared;
  for (const outshine::Ground::StreetField::Way &one : ways.Ways()) {
    if (one.Form != outshine::Ground::StreetField::Shape::Ribbon) { continue; }
    for (uint32_t step = 0; step < one.PointCount; ++step) {
      const size_t at = (static_cast<size_t>(one.FirstPoint) + step) * 2u;
      if (at + 1 >= points.size()) { break; }
      ++shared[PlaceKey({.LongitudeDeg = points[at + 1], .LatitudeDeg = points[at]})];
    }
  }
  return shared;
}

bool Corridors::Lay(const Site &site,
                    Geometry &ground,
                    std::vector<Yields> *corridorOut,
                    std::vector<DiagnosticSample> *notes) const {
  const TangentFrame &standing = site.Standing;
  const std::shared_ptr<const ClassStructure> &classStructure = site.Classes;
  const Drape &drapedOver = site.Draped;
  std::vector<Yields> &corridor = *corridorOut;
  RoadRaised pavement;
  Paved into;
  Notes(into,
        "rebuild: of that, the drape the buildings stand on",
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - site.CensusAt)
            .count(),
        "ms");
  const outshine::Ground::StreetField &ways = site.Stack.Ways();
  const outshine::Ground::OsmField *const vectors = site.Stack.Vectors();
  into.DeckM.assign(ways.Ways().size(), -kBeyondAnyCoordinate);
  into.Designed.resize(ways.Ways().size());
  const int waterRow = site.Stack.Materials().Find("water");
  std::unordered_map<uint64_t, uint32_t> sharedNodes;
  std::optional<Paving> paving;
  if (vectors != nullptr) {
    sharedNodes = SharedNodesOf(ways, vectors->Points());
    paving.emplace(Paving{.Stack = site.Stack,
                          .Network = site.Network,
                          .Ways = ways,
                          .Vectors = *vectors,
                          .Points = vectors->Points(),
                          .SharedNodes = sharedNodes,
                          .Draped = drapedOver,
                          .Standing = standing,
                          .Classes = classStructure,
                          .WaterRow = waterRow,
                          .EyeLatDeg = site.EyeLatDeg,
                          .EyeLonDeg = site.EyeLonDeg,
                          .FocalPx = site.FocalPx});
  }

  auto tookFrom = std::chrono::steady_clock::now();
  const auto since = [&tookFrom] {
    const auto was = tookFrom;
    tookFrom = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(tookFrom - was).count();
  };
  const auto pavesAt = std::chrono::steady_clock::now();
  if (paving) { Crosses(*paving, into); }
  Notes(into, "streets: of that, finding the crossings", since(), "ms");
  Notes(
      into, "streets: of finding them, laying the lanes into a network", into.CrossNetworkMs, "ms");
  Notes(into, "streets: of finding them, the sweep itself", into.CrossSweepMs, "ms");
  Notes(into, "streets: of finding them, filing each one", into.CrossFilingMs, "ms");
  Notes(into, "streets: of finding them, raising a deck over each", into.CrossDecksMs, "ms");
  Notes(into,
        "streets: of finding them, pairs the sweep tested",
        static_cast<double>(into.PairsTested),
        "pairs");
  Notes(into,
        "streets: and pairs its boxes threw out first",
        static_cast<double>(into.PairsPruned),
        "pairs");
  Notes(into,
        "streets: segments in the fullest square",
        static_cast<double>(into.FullestCell),
        "segments");
  if (paving && into.DecksRaised > 0) { Bridges(*paving, into); }
  Notes(into,
        "streets: ways a ramp lifted off the ground",
        static_cast<double>(into.RampsRaised),
        "ways");
  Notes(into, "streets: and the most one was lifted", into.SteepestRamp, "m");
  Notes(into,
        "streets: crossings the plan found",
        static_cast<double>(into.CrossingsSeen),
        "crossings");
  Notes(into, "streets: decks a crossing raised", static_cast<double>(into.DecksRaised), "decks");
  Notes(into, "streets: and the most one stands over what it crosses", into.MostRaisedM, "m");
  Notes(into, "streets: of that, raising the decks", since(), "ms");
  if (paving) {
    const Paving &on = *paving;
    static const Heap::Tag kPavingHeapTag("road-pave");
    const Heap::Tagged pavingHeap(kPavingHeapTag);
    for (const Pass pass : {Pass::Designing, Pass::Paving}) {
      const std::string_view doing = Doing(pass);
      for (size_t laneAt = 0; laneAt < ways.Ways().size(); ++laneAt) {
        PaveLane(on, pass, laneAt, into, corridor, pavement);
      }
      Notes(into, std::format("streets: of that, {} every lane", doing), since(), "ms");
      Notes(into, std::format("streets: of {}, the fit", doing), into.FitMs, "ms");
      Notes(into, std::format("streets: of {}, the water", doing), into.WaterMs, "ms");
      Notes(into, std::format("streets: of {}, the sweep", doing), into.SweepMs, "ms");
      Notes(into, std::format("streets: of {}, the yields", doing), into.YieldsMs, "ms");
      Notes(into,
            std::format("streets: of {}, stations paved", doing),
            static_cast<double>(into.EdgeStations),
            "stations");
      into.FitMs = 0.0;
      into.WaterMs = 0.0;
      into.SweepMs = 0.0;
      if (pass == Pass::Designing) {
        SplitsEdges(into);
        ShapesJunctions(on, into);
        corridor.insert(corridor.end(),
                        std::make_move_iterator(into.UnderJunctions.begin()),
                        std::make_move_iterator(into.UnderJunctions.end()));
        into.UnderJunctions.clear();
        Notes(into,
              "streets: edges the ways split into",
              static_cast<double>(into.Edges.size()),
              "edges");
        Notes(into,
              "streets: junctions shaped",
              static_cast<double>(into.Junctions.size()),
              "junctions");
        Notes(into,
              "streets: nodes where a way continues",
              static_cast<double>(into.Continuations),
              "nodes");
        Notes(into,
              "streets: ways under a pixel wide, left to the ground",
              static_cast<double>(into.UnseenWays),
              "ways");
        Notes(into,
              "streets: legs cut back to a junction's rim",
              static_cast<double>(into.LegsCut),
              "legs");
        Notes(into, "streets: and the deepest cut", into.DeepestCutM, "m");
        Notes(into, "streets: and the steepest junction plane", into.SteepestJunction, "m/m");
        Notes(into,
              "streets: junctions held to the steepest paved grade",
              static_cast<double>(into.JunctionsLevelled),
              "junctions");
        Notes(into, "streets: of that, shaping the junctions", since(), "ms");
      }
    }
  }
  Notes(into,
        "streets: stations under a bridge asked",
        static_cast<double>(into.AskedOverBridge),
        "stations");
  Notes(into,
        "streets: of those a class named",
        static_cast<double>(into.NamedOverBridge),
        "stations");
  Notes(into, "streets: and of those, water", static_cast<double>(into.WetOverBridge), "stations");
  Notes(into, "streets: the water class the table names", static_cast<double>(waterRow), "index");
  Notes(into, "streets: a class structure stood", classStructure ? 1.0 : 0.0, "yes/no");
  Notes(
      into, "streets: decks a WATERWAY raised", static_cast<double>(into.DecksOverWater), "decks");
  Notes(into, "streets: and the clearance the widest one took", into.MostOverWaterM, "m");
  Notes(into,
        "streets: stations an approach ramp moved",
        static_cast<double>(into.RampStations),
        "stations");
  Notes(into, "streets: and the longest approach", into.LongestRampM, "m");
  Notes(into, "streets: and the most a rim lifted a road", into.MostLiftedM, "m");
  const auto junctionsAt = std::chrono::steady_clock::now();
  Notes(into,
        "streets: junction bodies raised",
        static_cast<double>(RaisesTheJunctionBodies(site.Stack.Materials(), into, pavement)),
        "junctions");
  Notes(into,
        "streets: of that, raising the junction bodies",
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - junctionsAt)
            .count(),
        "ms");
  TellsWhatTheFitFound(into);
  Notes(into,
        "streets: ways laid as ribbons, all of them FLOATING",
        static_cast<double>(into.LaidWays),
        "ways");
  Notes(into,
        "streets: ways the GROUND carries instead",
        static_cast<double>(into.GroundWays),
        "ways");
  Notes(into, "streets: ways the field holds", static_cast<double>(ways.Ways().size()), "ways");
  Notes(into,
        "streets: features it walked at all",
        static_cast<double>(ways.LookedCount()),
        "features");
  Notes(into,
        "streets: features no rule named",
        static_cast<double>(ways.UnruledCount()),
        "features");
  Notes(into,
        "streets: features a rule gave no width",
        static_cast<double>(ways.UnwidthedCount()),
        "features");
  Notes(into,
        "streets: features that are tunnels",
        static_cast<double>(ways.TunnelCount()),
        "features");
  Notes(into, "streets: ways OSM calls a bridge", static_cast<double>(ways.BridgeCount()), "ways");
  Notes(into, "streets: ways that state a layer", static_cast<double>(ways.LayeredCount()), "ways");
  Notes(into,
        "streets: ways whose layer is a STRING",
        static_cast<double>(ways.LayerSaidCount()),
        "ways");
  Notes(into, "streets: ways it refused", static_cast<double>(into.RefusedWays), "ways");
  const size_t pavedTriangles = pavement.Index.size() / 3;
  Notes(into, "streets: triangles", static_cast<double>(pavedTriangles), "triangles");
  const auto handingAt = std::chrono::steady_clock::now();
  if (!HandsThePavingOver(site.Stack.Materials(), pavement, into, ground)) { return false; }
  Notes(into,
        "streets: of that, handing the paving over",
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - handingAt)
            .count(),
        "ms");
  Notes(
      into,
      "streets: everything Paves did",
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - pavesAt).count(),
      "ms");
  *notes = std::move(into.Notes);
  return true;
}

std::unique_ptr<Corridors::Job> Corridors::Begin(const Site &site) {
  return std::unique_ptr<Job>(new Job(site));
}

struct Corridors::JobSlice {
  const Site &site;
  const outshine::Ground::StreetField &ways;
  const outshine::Ground::OsmField *vectors;
  const Paving *paving;
  Geometry &ground;
  std::vector<Yields> *corridor;
  std::vector<DiagnosticSample> *notes;
  std::chrono::steady_clock::time_point began;
  size_t lanesMost;
  size_t nodesMost;
  int waterRow;

  [[nodiscard]] double Elapsed() const {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began)
        .count();
  }
};

std::expected<bool, std::string_view>
Corridors::Advance(Job &job,
                   const Site &site,
                   size_t lanesMost,
                   size_t nodesMost,
                   Geometry &ground,
                   std::vector<Yields> *corridor,
                   std::vector<DiagnosticSample> *notes) const {
  const auto *vectors = site.Stack.Vectors();
  const auto &ways = site.Stack.Ways();
  if ((vectors != nullptr ? vectors->Generation() : 0) != job.VectorGeneration ||
      ways.Ways().size() != job.WayCount) {
    return std::unexpected(Says::kStaleCorridorInput);
  }
  if (job.Phase == Job::Stage::Done) { return true; }
  const auto began = std::chrono::steady_clock::now();
  const int waterRow = site.Stack.Materials().Find("water");
  if (job.Phase == Job::Stage::Prepare && vectors != nullptr) {
    job.SharedNodes = SharedNodesOf(ways, vectors->Points());
  }
  std::optional<Paving> paving;
  if (vectors != nullptr) {
    paving.emplace(Paving{.Stack = site.Stack,
                          .Network = site.Network,
                          .Ways = ways,
                          .Vectors = *vectors,
                          .Points = vectors->Points(),
                          .SharedNodes = job.SharedNodes,
                          .Draped = site.Draped,
                          .Standing = site.Standing,
                          .Classes = site.Classes,
                          .WaterRow = waterRow,
                          .EyeLatDeg = site.EyeLatDeg,
                          .EyeLonDeg = site.EyeLonDeg,
                          .FocalPx = site.FocalPx});
  }
  const JobSlice slice{.site = site,
                       .ways = ways,
                       .vectors = vectors,
                       .paving = paving ? &*paving : nullptr,
                       .ground = ground,
                       .corridor = corridor,
                       .notes = notes,
                       .began = began,
                       .lanesMost = lanesMost,
                       .nodesMost = nodesMost,
                       .waterRow = waterRow};
  const auto entered = job.Phase;
  std::expected<bool, std::string_view> result = std::unexpected(Says::kStaleCorridorInput);
  if (entered <= Job::Stage::CrossDecks) {
    result = AdvanceCrossings(job, slice);
  } else if (entered <= Job::Stage::BridgeRelevant) {
    result = AdvanceBridgeTopology(job, slice);
  } else if (entered <= Job::Stage::BridgeRaise) {
    result = AdvanceBridgeDecks(job, slice);
  } else if (entered == Job::Stage::BridgeCleanup) {
    result = AdvanceBridgeCleanup(job, slice);
  } else if (entered <= Job::Stage::BridgeGrades) {
    result = AdvanceBridgeGrades(job, slice);
  } else if (entered <= Job::Stage::Legs) {
    result = AdvanceRoadDesign(job, slice);
  } else if (entered <= Job::Stage::Pave) {
    result = AdvanceRoadJunctions(job, slice);
  } else if (entered <= Job::Stage::Bodies) {
    result = AdvanceRoadBodies(job, slice);
  } else if (entered == Job::Stage::FinishNotes) {
    result = AdvanceFinish(job, slice);
  } else if (entered == Job::Stage::Transfer) {
    result = BeginTransfer(job, slice);
  } else if (entered == Job::Stage::TransferValidate) {
    result = AdvanceTransferValidation(job, slice);
  } else {
    result = AdvanceTransfer(job, slice);
  }
  const auto index = static_cast<size_t>(entered);
  job.LongestSliceMs[index] = std::max(job.LongestSliceMs[index], slice.Elapsed());
  return result;
}

std::expected<bool, std::string_view> Corridors::AdvanceCrossings(Job &job, const JobSlice &slice) {
  const auto &site = slice.site;
  const auto *paving = slice.paving;
  const size_t lanesMost = slice.lanesMost;
  const auto began = slice.began;
  const auto elapsed = [&slice] { return slice.Elapsed(); };
  Paved &into = job.Work;
  switch (job.Phase) {
    case Job::Stage::Prepare:
      Notes(into,
            "rebuild: of that, the drape the buildings stand on",
            std::chrono::duration<double, std::milli>(began - site.CensusAt).count(),
            "ms");
      into.DeckM.assign(job.WayCount, -kBeyondAnyCoordinate);
      into.Designed.resize(job.WayCount);
      job.TotalMs += elapsed();
      job.Phase = Job::Stage::Crossings;
      return false;
    case Job::Stage::Crossings:
      if (site.Network != nullptr) {
        const Path::Network &network = *site.Network;
        const auto swept = network.Crossings(job.Crossed);
        if (swept) {
          std::ranges::sort(
              job.Crossed,
              [&network](const Path::Network::Crossing &a, const Path::Network::Crossing &b) {
                const std::array<size_t, 4> ka = {
                    network.TagOf(a.OverWay), network.TagOf(a.UnderWay), a.OverAt, a.UnderAt};
                const std::array<size_t, 4> kb = {
                    network.TagOf(b.OverWay), network.TagOf(b.UnderWay), b.OverAt, b.UnderAt};
                return ka < kb;
              });
          into.CrossingsSeen = job.Crossed.size();
          into.PairsTested = swept->PairsTested;
          into.PairsPruned = swept->PairsPruned;
          into.FullestCell = swept->FullestCell;
        }
      }
      into.CrossSweepMs = elapsed();
      job.TotalMs += elapsed();
      job.Phase = Job::Stage::CrossFile;
      return false;
    case Job::Stage::CrossFile: {
      const size_t end =
          std::min(job.NextCrossing + 2u * std::max(size_t{1}, lanesMost), job.Crossed.size());
      for (; job.NextCrossing < end; ++job.NextCrossing) {
        FileCrossing(job.Crossed[job.NextCrossing], site.Standing, into);
      }
      job.StageMs += elapsed();
      job.TotalMs += elapsed();
      if (job.NextCrossing < job.Crossed.size()) { return false; }
      into.CrossFilingMs = job.StageMs;
      job.NextCrossing = 0;
      job.StageMs = 0.0;
      job.Phase = Job::Stage::CrossDecks;
      return false;
    }
    case Job::Stage::CrossDecks: {
      const size_t end =
          std::min(job.NextCrossing + 2u * std::max(size_t{1}, lanesMost), job.Crossed.size());
      if (paving != nullptr && site.Network != nullptr) {
        for (; job.NextCrossing < end; ++job.NextCrossing) {
          RaiseDeckOver(job.Crossed[job.NextCrossing], *paving, *site.Network, into);
        }
      } else {
        job.NextCrossing = end;
      }
      job.StageMs += elapsed();
      job.TotalMs += elapsed();
      if (job.NextCrossing < job.Crossed.size()) { return false; }
      into.CrossDecksMs = job.StageMs;
      job.Crossed.clear();
      Notes(into,
            "streets: of that, finding the crossings",
            into.CrossSweepMs + into.CrossFilingMs + into.CrossDecksMs,
            "ms");
      Notes(into,
            "streets: of finding them, laying the lanes into a network",
            into.CrossNetworkMs,
            "ms");
      Notes(into, "streets: of finding them, the sweep itself", into.CrossSweepMs, "ms");
      Notes(into, "streets: of finding them, filing each one", into.CrossFilingMs, "ms");
      Notes(into, "streets: of finding them, raising a deck over each", into.CrossDecksMs, "ms");
      Notes(into,
            "streets: of finding them, pairs the sweep tested",
            static_cast<double>(into.PairsTested),
            "pairs");
      Notes(into,
            "streets: and pairs its boxes threw out first",
            static_cast<double>(into.PairsPruned),
            "pairs");
      Notes(into,
            "streets: segments in the fullest square",
            static_cast<double>(into.FullestCell),
            "segments");
      job.Phase = Job::Stage::BridgeTopology;
      return false;
    }
    default: return std::unexpected(Says::kStaleCorridorInput);
  }
}

std::expected<bool, std::string_view> Corridors::AdvanceBridgeTopology(Job &job,
                                                                       const JobSlice &slice) {
  const auto *paving = slice.paving;
  const size_t lanesMost = slice.lanesMost;
  const auto elapsed = [&slice] { return slice.Elapsed(); };
  const Paved &into = job.Work;
  switch (job.Phase) {
    case Job::Stage::BridgeTopology: {
      if (paving == nullptr || into.DecksRaised == 0) {
        job.Phase = Job::Stage::BridgeGrades;
        return false;
      }
      if (!job.BridgeTopologyStarted) {
        job.Topology.EndsOfWay.resize(job.WayCount);
        job.Topology.HasEnds.resize(job.WayCount);
        job.BridgeTopologyStarted = true;
      }
      const size_t end = std::min(job.NextLane + std::max(size_t{1}, lanesMost), job.WayCount);
      for (; job.NextLane < end; ++job.NextLane) {
        AppendBridgeTopology(*paving, job.NextLane, job.Topology);
      }
      job.BridgeSeedMs += elapsed();
      job.TotalMs += elapsed();
      if (job.NextLane < job.WayCount) { return false; }
      job.NextLane = 0;
      job.Phase = Job::Stage::BridgeRelevant;
      return false;
    }
    case Job::Stage::BridgeRelevant:
      if (paving == nullptr) { return std::unexpected(Says::kStaleCorridorInput); }
      job.BridgeEnds = RelevantBridgeEnds(*paving, job.Topology, into);
      job.BridgeSeedMs += elapsed();
      job.TotalMs += elapsed();
      job.Phase = Job::Stage::BridgeSample;
      return false;
    default: return std::unexpected(Says::kStaleCorridorInput);
  }
}

std::expected<bool, std::string_view> Corridors::AdvanceBridgeDecks(Job &job,
                                                                    const JobSlice &slice) {
  const auto &ways = slice.ways;
  const auto *paving = slice.paving;
  const size_t lanesMost = slice.lanesMost;
  const auto elapsed = [&slice] { return slice.Elapsed(); };
  Paved &into = job.Work;
  switch (job.Phase) {
    case Job::Stage::BridgeSample: {
      if (paving == nullptr) { return std::unexpected(Says::kStaleCorridorInput); }
      const size_t end =
          std::min(job.NextBridgeEnd + 2u * std::max(size_t{1}, lanesMost), job.BridgeEnds.size());
      for (; job.NextBridgeEnd < end; ++job.NextBridgeEnd) {
        const uint64_t key = job.BridgeEnds[job.NextBridgeEnd];
        const auto place = job.Topology.PlaceOf.find(key);
        if (place == job.Topology.PlaceOf.end()) { continue; }
        const std::optional<Grounded> under = GroundUnder(*paving, place->second);
        if (!under) { continue; }
        into.EndM.insert_or_assign(key, under->GradeM);
        into.GroundEndM.insert_or_assign(key, under->GradeM);
      }
      job.BridgeSeedMs += elapsed();
      job.TotalMs += elapsed();
      if (job.NextBridgeEnd < job.BridgeEnds.size()) { return false; }
      job.Phase = Job::Stage::BridgeRaise;
      return false;
    }
    case Job::Stage::BridgeRaise: {
      const size_t end = std::min(job.NextLane + std::max(size_t{1}, lanesMost), job.WayCount);
      for (; job.NextLane < end; ++job.NextLane) {
        if (job.Topology.HasEnds[job.NextLane] != 0 && ways.Ways()[job.NextLane].Bridge &&
            into.DeckM[job.NextLane] > kUnraisedDeckM) {
          RaisesEnds(job.Topology.EndsOfWay[job.NextLane].Key, into.DeckM[job.NextLane], into);
        }
      }
      job.BridgeSeedMs += elapsed();
      job.TotalMs += elapsed();
      if (job.NextLane < job.WayCount) { return false; }
      job.NextLane = 0;
      Notes(into, "streets: of raising decks, seeding bridge ends", job.BridgeSeedMs, "ms");
      Notes(into, "streets: the highest deck a ramp must reach", HighestDeckM(into), "m");
      job.Phase = Job::Stage::BridgeCleanup;
      return false;
    }
    default: return std::unexpected(Says::kStaleCorridorInput);
  }
}

std::expected<bool, std::string_view> Corridors::AdvanceBridgeCleanup(Job &job,
                                                                      const JobSlice &slice) {
  if (job.Phase != Job::Stage::BridgeCleanup) { return std::unexpected(Says::kStaleCorridorInput); }
  constexpr size_t kEntriesPerSlice = 4096;
  constexpr double kCleanupBudgetMs = 2.0;
  for (size_t removed = 0; removed < kEntriesPerSlice && !job.Topology.WaysAt.empty() &&
                           slice.Elapsed() < kCleanupBudgetMs;
       ++removed) {
    job.Topology.WaysAt.erase(job.Topology.WaysAt.begin());
  }
  for (size_t removed = 0; removed < kEntriesPerSlice && !job.Topology.PlaceOf.empty() &&
                           slice.Elapsed() < kCleanupBudgetMs;
       ++removed) {
    job.Topology.PlaceOf.erase(job.Topology.PlaceOf.begin());
  }
  job.TotalMs += slice.Elapsed();
  if (!job.Topology.WaysAt.empty() || !job.Topology.PlaceOf.empty()) { return false; }
  job.RampCapM = HighestDeckM(job.Work);
  job.Topology.EndsOfWay.clear();
  job.Topology.HasEnds.clear();
  job.BridgeEnds.clear();
  job.Phase = Job::Stage::BridgeRamps;
  return false;
}

std::expected<bool, std::string_view> Corridors::AdvanceBridgeGrades(Job &job,
                                                                     const JobSlice &slice) {
  const auto &ways = slice.ways;
  const auto *vectors = slice.vectors;
  const auto *paving = slice.paving;
  const auto elapsed = [&slice] { return slice.Elapsed(); };
  Paved &into = job.Work;
  switch (job.Phase) {
    case Job::Stage::BridgeRamps:
      if (paving != nullptr && into.DecksRaised > 0) {
        EaseRampPass(ways, *vectors, job.RampCapM, into);
        job.RampMs += elapsed();
        job.TotalMs += elapsed();
        if (++job.RampPass < kRampPasses) { return false; }
        Notes(into, "streets: of raising decks, easing ramps", job.RampMs, "ms");
      } else {
        job.TotalMs += elapsed();
      }
      job.Phase = Job::Stage::BridgeGrades;
      return false;
    case Job::Stage::BridgeGrades:
      if (paving != nullptr && into.DecksRaised > 0) {
        GradesApproaches(*paving, into);
        Notes(into, "streets: of raising decks, grading approaches", elapsed(), "ms");
      }
      Notes(into,
            "streets: ways a ramp lifted off the ground",
            static_cast<double>(into.RampsRaised),
            "ways");
      Notes(into, "streets: and the most one was lifted", into.SteepestRamp, "m");
      Notes(into,
            "streets: crossings the plan found",
            static_cast<double>(into.CrossingsSeen),
            "crossings");
      Notes(
          into, "streets: decks a crossing raised", static_cast<double>(into.DecksRaised), "decks");
      Notes(into, "streets: and the most one stands over what it crosses", into.MostRaisedM, "m");
      Notes(into, "streets: of that, raising the decks", elapsed(), "ms");
      job.TotalMs += elapsed();
      job.Phase = Job::Stage::Design;
      return false;
    default: return std::unexpected(Says::kStaleCorridorInput);
  }
}

std::expected<bool, std::string_view> Corridors::AdvanceRoadDesign(Job &job,
                                                                   const JobSlice &slice) const {
  const auto *paving = slice.paving;
  const size_t lanesMost = slice.lanesMost;
  const auto elapsed = [&slice] { return slice.Elapsed(); };
  Paved &into = job.Work;
  switch (job.Phase) {
    case Job::Stage::Design: {
      const size_t end = std::min(job.NextLane + std::max(size_t{1}, lanesMost), job.WayCount);
      if (paving != nullptr) {
        for (; job.NextLane < end; ++job.NextLane) {
          PaveLane(*paving, Pass::Designing, job.NextLane, into, job.Corridor, job.Pavement);
        }
      } else {
        job.NextLane = end;
      }
      job.StageMs += elapsed();
      job.TotalMs += elapsed();
      if (job.NextLane < job.WayCount) { return false; }
      Notes(into, "streets: of that, designing every lane", job.StageMs, "ms");
      Notes(into, "streets: of designing, the fit", into.FitMs, "ms");
      Notes(into, "streets: of designing, the water", into.WaterMs, "ms");
      Notes(into, "streets: of designing, the sweep", into.SweepMs, "ms");
      Notes(into, "streets: of designing, the yields", into.YieldsMs, "ms");
      Notes(into,
            "streets: of designing, stations paved",
            static_cast<double>(into.EdgeStations),
            "stations");
      into.FitMs = into.WaterMs = into.SweepMs = 0.0;
      job.NextLane = 0;
      job.StageMs = 0.0;
      job.Phase = Job::Stage::Edges;
      return false;
    }
    case Job::Stage::Edges:
      SplitsEdges(into);
      job.StageMs += elapsed();
      job.TotalMs += elapsed();
      job.Phase = Job::Stage::Legs;
      return false;
    case Job::Stage::Legs: {
      const size_t end =
          std::min(job.NextEdge + 2u * std::max(size_t{1}, lanesMost), into.Edges.size());
      if (paving != nullptr) {
        for (; job.NextEdge < end; ++job.NextEdge) {
          AppendLeg(*paving, into, static_cast<uint32_t>(job.NextEdge), job.LegsAt);
        }
      } else {
        job.NextEdge = end;
      }
      job.StageMs += elapsed();
      job.TotalMs += elapsed();
      if (job.NextEdge < into.Edges.size()) { return false; }
      for (const auto &one : job.LegsAt) {
        if (one.second.size() >= 2) { job.Nodes.push_back(one.first); }
      }
      std::ranges::sort(job.Nodes);
      job.Phase = Job::Stage::Junctions;
      return false;
    }
    default: return std::unexpected(Says::kStaleCorridorInput);
  }
}

std::expected<bool, std::string_view> Corridors::AdvanceRoadJunctions(Job &job,
                                                                      const JobSlice &slice) const {
  const auto *paving = slice.paving;
  const size_t lanesMost = slice.lanesMost;
  const size_t nodesMost = slice.nodesMost;
  const auto elapsed = [&slice] { return slice.Elapsed(); };
  Paved &into = job.Work;
  switch (job.Phase) {
    case Job::Stage::Junctions: {
      const size_t end = std::min(job.NextNode + std::max(size_t{1}, nodesMost), job.Nodes.size());
      for (; job.NextNode < end; ++job.NextNode) {
        const uint64_t node = job.Nodes[job.NextNode];
        std::vector<Leg> &legs = job.LegsAt.at(node);
        std::ranges::sort(legs, ByBearing);
        if (legs.size() == 2) {
          ++into.Continuations;
        } else if (paving != nullptr) {
          ShapeOf(*paving, node, legs, into);
        }
      }
      job.StageMs += elapsed();
      job.TotalMs += elapsed();
      if (job.NextNode < job.Nodes.size()) { return false; }
      job.Corridor.insert(job.Corridor.end(),
                          std::make_move_iterator(into.UnderJunctions.begin()),
                          std::make_move_iterator(into.UnderJunctions.end()));
      into.UnderJunctions.clear();
      Notes(into,
            "streets: edges the ways split into",
            static_cast<double>(into.Edges.size()),
            "edges");
      Notes(into,
            "streets: junctions shaped",
            static_cast<double>(into.Junctions.size()),
            "junctions");
      Notes(into,
            "streets: nodes where a way continues",
            static_cast<double>(into.Continuations),
            "nodes");
      Notes(into,
            "streets: ways under a pixel wide, left to the ground",
            static_cast<double>(into.UnseenWays),
            "ways");
      Notes(into,
            "streets: legs cut back to a junction's rim",
            static_cast<double>(into.LegsCut),
            "legs");
      Notes(into, "streets: and the deepest cut", into.DeepestCutM, "m");
      Notes(into, "streets: and the steepest junction plane", into.SteepestJunction, "m/m");
      Notes(into,
            "streets: junctions held to the steepest paved grade",
            static_cast<double>(into.JunctionsLevelled),
            "junctions");
      Notes(into, "streets: of that, shaping the junctions", job.StageMs, "ms");
      job.NextLane = 0;
      job.StageMs = 0.0;
      job.Phase = Job::Stage::Pave;
      return false;
    }
    case Job::Stage::Pave: {
      const size_t end = std::min(job.NextLane + std::max(size_t{1}, lanesMost), job.WayCount);
      if (paving != nullptr) {
        for (; job.NextLane < end; ++job.NextLane) {
          PaveLane(*paving, Pass::Paving, job.NextLane, into, job.Corridor, job.Pavement);
        }
      } else {
        job.NextLane = end;
      }
      job.StageMs += elapsed();
      job.TotalMs += elapsed();
      if (job.NextLane < job.WayCount) { return false; }
      Notes(into, "streets: of that, paving every lane", job.StageMs, "ms");
      Notes(into, "streets: of paving, the fit", into.FitMs, "ms");
      Notes(into, "streets: of paving, the water", into.WaterMs, "ms");
      Notes(into, "streets: of paving, the sweep", into.SweepMs, "ms");
      Notes(into, "streets: of paving, the yields", into.YieldsMs, "ms");
      Notes(into,
            "streets: of paving, stations paved",
            static_cast<double>(into.EdgeStations),
            "stations");
      job.StageMs = 0.0;
      job.Phase = Job::Stage::Bodies;
      return false;
    }
    default: return std::unexpected(Says::kStaleCorridorInput);
  }
}

std::expected<bool, std::string_view> Corridors::AdvanceRoadBodies(Job &job,
                                                                   const JobSlice &slice) const {
  const auto &site = slice.site;
  const size_t nodesMost = slice.nodesMost;
  const auto elapsed = [&slice] { return slice.Elapsed(); };
  Paved &into = job.Work;
  if (job.Phase != Job::Stage::Bodies) { return std::unexpected(Says::kStaleCorridorInput); }
  {
    const auto &wearing = site.Stack.Materials();
    const int asphalt = wearing.Find("asphalt");
    Vec3f wears = {{0.5f, 0.5f, 0.5f}};
    if (asphalt >= 0) { wears = wearing.At(static_cast<size_t>(asphalt)).Albedo; }
    const size_t end =
        std::min(job.NextBody + std::max(size_t{1}, nodesMost), into.Junctions.size());
    for (; job.NextBody < end; ++job.NextBody) {
      const Junction &one = into.Junctions[job.NextBody];
      Sweeper_.Junction(std::span<const RoadGate>(one.Gates.data(), one.Gates.size()),
                        {.SlopeE = one.SlopeE, .SlopeN = one.SlopeN},
                        wears,
                        job.Pavement);
    }
    job.StageMs += elapsed();
    job.TotalMs += elapsed();
    if (job.NextBody < into.Junctions.size()) { return false; }
    Notes(into,
          "streets: junction bodies raised",
          static_cast<double>(into.Junctions.size()),
          "junctions");
    Notes(into, "streets: of that, raising the junction bodies", job.StageMs, "ms");
    job.Phase = Job::Stage::FinishNotes;
    return false;
  }
}

std::expected<bool, std::string_view> Corridors::AdvanceFinish(Job &job, const JobSlice &slice) {
  const auto &site = slice.site;
  const auto &ways = slice.ways;
  const int waterRow = slice.waterRow;
  const auto elapsed = [&slice] { return slice.Elapsed(); };
  Paved &into = job.Work;
  if (job.Phase == Job::Stage::FinishNotes) {
    Notes(into,
          "streets: stations under a bridge asked",
          static_cast<double>(into.AskedOverBridge),
          "stations");
    Notes(into,
          "streets: of those a class named",
          static_cast<double>(into.NamedOverBridge),
          "stations");
    Notes(
        into, "streets: and of those, water", static_cast<double>(into.WetOverBridge), "stations");
    Notes(into, "streets: the water class the table names", static_cast<double>(waterRow), "index");
    Notes(into, "streets: a class structure stood", site.Classes ? 1.0 : 0.0, "yes/no");
    Notes(into,
          "streets: decks a WATERWAY raised",
          static_cast<double>(into.DecksOverWater),
          "decks");
    Notes(into, "streets: and the clearance the widest one took", into.MostOverWaterM, "m");
    Notes(into,
          "streets: stations an approach ramp moved",
          static_cast<double>(into.RampStations),
          "stations");
    Notes(into, "streets: and the longest approach", into.LongestRampM, "m");
    Notes(into, "streets: and the most a rim lifted a road", into.MostLiftedM, "m");
    TellsWhatTheFitFound(into);
    Notes(into,
          "streets: ways laid as ribbons, all of them FLOATING",
          static_cast<double>(into.LaidWays),
          "ways");
    Notes(into,
          "streets: ways the GROUND carries instead",
          static_cast<double>(into.GroundWays),
          "ways");
    Notes(into, "streets: ways the field holds", static_cast<double>(job.WayCount), "ways");
    Notes(into,
          "streets: features it walked at all",
          static_cast<double>(ways.LookedCount()),
          "features");
    Notes(into,
          "streets: features no rule named",
          static_cast<double>(ways.UnruledCount()),
          "features");
    Notes(into,
          "streets: features a rule gave no width",
          static_cast<double>(ways.UnwidthedCount()),
          "features");
    Notes(into,
          "streets: features that are tunnels",
          static_cast<double>(ways.TunnelCount()),
          "features");
    Notes(
        into, "streets: ways OSM calls a bridge", static_cast<double>(ways.BridgeCount()), "ways");
    Notes(
        into, "streets: ways that state a layer", static_cast<double>(ways.LayeredCount()), "ways");
    Notes(into,
          "streets: ways whose layer is a STRING",
          static_cast<double>(ways.LayerSaidCount()),
          "ways");
    Notes(into, "streets: ways it refused", static_cast<double>(into.RefusedWays), "ways");
    Notes(into,
          "streets: triangles",
          static_cast<double>(job.Pavement.Index.size()) / 3.0,
          "triangles");
    job.TotalMs += elapsed();
    job.Phase = Job::Stage::Transfer;
    return false;
  }
  return std::unexpected(Says::kStaleCorridorInput);
}

std::expected<bool, std::string_view> Corridors::AdvanceTransfer(Job &job, const JobSlice &slice) {
  auto &ground = slice.ground;
  const auto elapsed = [&slice] { return slice.Elapsed(); };
  switch (job.Phase) {
    case Job::Stage::TransferPositions:
      if (job.TransferPart >= 0 && !ground.setPositions(job.TransferPart, job.Pavement.PositionM)) {
        return std::unexpected(Says::kPavingCreationFailed);
      }
      job.TotalMs += elapsed();
      job.Phase = Job::Stage::TransferNormals;
      return false;
    case Job::Stage::TransferNormals:
      if (job.TransferPart >= 0 && !ground.setNormals(job.TransferPart, job.Pavement.NormalM)) {
        return std::unexpected(Says::kPavingCreationFailed);
      }
      job.TotalMs += elapsed();
      job.Phase = Job::Stage::TransferColours;
      return false;
    case Job::Stage::TransferColours:
      if (job.TransferPart >= 0 && !ground.setColours(job.TransferPart, job.Pavement.ColourRgba)) {
        return std::unexpected(Says::kPavingCreationFailed);
      }
      job.TotalMs += elapsed();
      job.Phase = Job::Stage::TransferTriangles;
      return false;
    case Job::Stage::TransferTriangles:
      if (job.TransferPart >= 0 && !ground.setTriangles(job.TransferPart, job.Pavement.Index)) {
        return std::unexpected(Says::kPavingCreationFailed);
      }
      job.TotalMs += elapsed();
      job.Phase = Job::Stage::TransferValidate;
      return false;
    default: return std::unexpected(Says::kStaleCorridorInput);
  }
}

std::expected<bool, std::string_view> Corridors::BeginTransfer(Job &job, const JobSlice &slice) {
  if (job.Phase != Job::Stage::Transfer) { return std::unexpected(Says::kStaleCorridorInput); }
  if (job.Pavement.Index.size() >= 3) {
    Material tarmac;
    for (int channel = 0; channel < 3; ++channel) { tarmac.BaseColour[channel] = 1.0f; }
    const auto &wearing = slice.site.Stack.Materials();
    const int asphalt = wearing.Find("asphalt");
    tarmac.Roughness =
        asphalt >= 0 ? wearing.At(static_cast<size_t>(asphalt)).Roughness : kUnlitTint;
    const auto paved = slice.ground.addSurface("streets", tarmac);
    if (!paved) { return std::unexpected(Says::kPavingCreationFailed); }
    const auto part = slice.ground.addPart("streets", *paved);
    if (!part) { return std::unexpected(Says::kPavingCreationFailed); }
    job.TransferPart = *part;
    Notes(job.Work,
          "streets: the surface they were given",
          static_cast<double>(paved->index()),
          "index");
    Notes(job.Work, "streets: the part they were given", static_cast<double>(*part), "index");
  }
  job.TotalMs += slice.Elapsed();
  job.Phase = Job::Stage::TransferPositions;
  return false;
}

std::expected<bool, std::string_view> Corridors::AdvanceTransferValidation(Job &job,
                                                                           const JobSlice &slice) {
  if (job.Phase != Job::Stage::TransferValidate) {
    return std::unexpected(Says::kStaleCorridorInput);
  }
  const auto &ground = slice.ground;
  auto *corridor = slice.corridor;
  auto *notes = slice.notes;
  const auto elapsed = [&slice] { return slice.Elapsed(); };
  Paved &into = job.Work;
  if (job.TransferPart >= 0) {
    Notes(into, "streets: the geometry took them", 1.0, "yes/no");
    Notes(into,
          "streets: triangles wound against their normals",
          static_cast<double>(ground.windingAgainstNormals(job.TransferPart)),
          "triangles");
    Notes(into,
          "streets: parts the geometry now holds",
          static_cast<double>(ground.parts()),
          "parts");
  }
  Notes(into, "streets: of that, handing the paving over", elapsed(), "ms");
  job.TotalMs += elapsed();
  Notes(into, "streets: everything Paves did", job.TotalMs, "ms");
  job.LongestSliceMs[static_cast<size_t>(Job::Stage::TransferValidate)] = elapsed();
  constexpr std::array<std::string_view, 24> stages{"prepare",
                                                    "crossings",
                                                    "cross-file",
                                                    "cross-decks",
                                                    "bridge-topology",
                                                    "bridge-relevant",
                                                    "bridge-sample",
                                                    "bridge-raise",
                                                    "bridge-cleanup",
                                                    "bridge-ramps",
                                                    "bridge-grades",
                                                    "design",
                                                    "edges",
                                                    "legs",
                                                    "junctions",
                                                    "pave",
                                                    "bodies",
                                                    "notes",
                                                    "transfer",
                                                    "transfer-positions",
                                                    "transfer-normals",
                                                    "transfer-colours",
                                                    "transfer-triangles",
                                                    "transfer-validate"};
  for (size_t stage = 0; stage < stages.size(); ++stage) {
    Notes(into,
          std::format("streets: longest {} slice", stages[stage]),
          job.LongestSliceMs[stage],
          "ms");
  }
  *corridor = std::move(job.Corridor);
  *notes = std::move(into.Notes);
  job.Phase = Job::Stage::Done;
  return true;
}

}
