#include "Corridors.h"

#include <algorithm>
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
constexpr int kLevelPasses = 24;
constexpr double kLevelledM = 0.01;
constexpr int kRampPasses = 12;
constexpr int kChordPasses = 4;
constexpr double kChordWithinM = 0.20;
constexpr double kLeastCrestK = 10.0;
constexpr double kVergeM = 1.5;
constexpr double kTrimMostWidths = 4.0;
constexpr double kFitWithinM = 0.5;
constexpr double kFitTightestM = 5.5;
constexpr double kLeastRoadM = 2.0;

} // namespace

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

void Corridors::TrimLaneEnds(size_t laneAt, Paved &into) {
  const std::vector<double> reached = ReachedAlong(into.Along);
  const double wholeM = reached.back();
  const double fromM = into.TrimM[laneAt * 2u];
  const double toM = wholeM - into.TrimM[laneAt * 2u + 1u];
  if (toM - fromM < kLeastRoadM || (fromM <= kLeastCapM && toM >= wholeM - kLeastCapM)) { return; }

  const auto standAt = [&](double alongM) {
    size_t at = 1;
    while (at + 1 < reached.size() && reached[at] < alongM) { ++at; }
    const double span = reached[at] - reached[at - 1];
    const double part = span > kLeastTurnRad ? (alongM - reached[at - 1]) / span : 0.0;
    const RoadStation &from = into.Along[at - 1];
    const RoadStation &to = into.Along[at];
    return RoadStation{.EastM = from.EastM + (to.EastM - from.EastM) * part,
                       .SouthM = from.SouthM + (to.SouthM - from.SouthM) * part,
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

void Corridors::FitLane(size_t laneAt, Paved &into) {
  into.FitEastNorth.clear();
  into.FitEastNorth.reserve(into.Along.size() * 2u);
  for (const RoadStation &one : into.Along) {
    into.FitEastNorth.push_back(one.EastM);
    into.FitEastNorth.push_back(-one.SouthM);
  }
  FitAlongLane(into);
  TrimLaneEnds(laneAt, into);
}

void Corridors::RefineChords(const Paving &on, Paved &into) {
  for (int pass = 0; pass < kChordPasses; ++pass) {
    size_t added = 0;
    into.Finer.clear();
    into.Finer.reserve(into.Along.size() * 2u);
    for (size_t at = 1; at < into.Along.size(); ++at) {
      into.Finer.push_back(into.Along[at - 1u]);
      const double midE = 0.5 * (into.Along[at - 1u].EastM + into.Along[at].EastM);
      const double midS = 0.5 * (into.Along[at - 1u].SouthM + into.Along[at].SouthM);
      const double chord = 0.5 * (into.Along[at - 1u].GradeM + into.Along[at].GradeM);
      const double overM = on.Draped.At({.EastM = midE, .SouthM = midS}, chord);
      if (std::fabs(overM - chord) <= kChordWithinM) { continue; }
      into.Finer.push_back(RoadStation{.EastM = midE, .SouthM = midS, .GradeM = overM});
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
                           Paved &into) const {
  into.Along.clear();
  bool whole = true;
  const auto station = [&](LongitudeLatitude over, uint64_t node) {
    const std::optional<Grounded> stood = GroundUnder(on, over);
    if (!stood) { return false; }
    into.Along.push_back(
        {.EastM = stood->EastM, .SouthM = stood->SouthM, .GradeM = stood->GradeM, .Node = node});
    return true;
  };
  whole = StationsAlong(on, lane, station);
  if (!whole || into.Along.size() < 2) {
    ++into.RefusedWays;
    return;
  }
  RefineChords(on, into);

  if (lane.MaxGradient > 0.0f) {
    Sweeper_.Design(std::span<RoadStation>(into.Along.data(), into.Along.size()),
                    static_cast<double>(lane.MaxGradient),
                    kLeastCrestK);
  }
  for (RoadStation &one : into.Along) {
    if (one.Node != 0u || into.AtCrossing.empty()) { continue; }
    const auto east = static_cast<int64_t>(std::floor(one.EastM / kCrossCellM));
    const auto south = static_cast<int64_t>(std::floor(one.SouthM / kCrossCellM));
    const auto atE = static_cast<uint64_t>(east + 0x20000000LL);
    const auto atS = static_cast<uint64_t>(south + 0x20000000LL);
    const auto near = into.AtCrossing.find((atE << 32U) | atS);
    if (near == into.AtCrossing.end()) { continue; }
    for (const auto &met : near->second) {
      const double offE = one.EastM - met.EastM;
      const double offS = one.SouthM - met.SouthM;
      if (offE * offE + offS * offS <= kMeetsWithinM * kMeetsWithinM) {
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
  const double spanS = along[at].SouthM - along[at - 1].SouthM;
  return std::sqrt(spanE * spanE + spanS * spanS);
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
    const double midS = 0.5 * (into.Along[at - 1].SouthM + into.Along[at].SouthM);
    const LongitudeLatitude midAt = on.Standing.Geo({.EastM = midE, .NorthM = -midS});
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

void Corridors::LevelsDeckOrApproach(const Paving &on,
                                     const outshine::Ground::StreetField::Way &lane,
                                     size_t laneAt,
                                     Paved &into) {
  if (lane.Bridge) {
    double deck = into.DeckM[laneAt];
    for (const RoadStation &one : into.Along) { deck = std::max(deck, one.GradeM); }
    for (RoadStation &one : into.Along) { one.GradeM = deck; }
  } else if (!into.EndM.empty()) {
    const size_t first = static_cast<size_t>(lane.FirstPoint) * 2;
    const size_t last = first + (static_cast<size_t>(lane.PointCount) - 1u) * 2;
    if (last + 1 < on.Points.size()) {
      const auto from = into.EndM.find(
          PlaceKey({.LongitudeDeg = on.Points[first + 1], .LatitudeDeg = on.Points[first]}));
      const auto to = into.EndM.find(
          PlaceKey({.LongitudeDeg = on.Points[last + 1], .LatitudeDeg = on.Points[last]}));
      if (from != into.EndM.end() && to != into.EndM.end()) {
        const std::vector<double> reached = ReachedAlong(into.Along);
        const double runM = reached.back();
        for (size_t at = 0; at < into.Along.size(); ++at) {
          const double along01 = runM > kLeastRunM ? reached[at] / runM : 0.0;
          const double wanted = from->second + (to->second - from->second) * along01;
          into.Along[at].GradeM = std::max(into.Along[at].GradeM, wanted);
        }
      }
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
  if (pass == Pass::Paving) {
    into.Along = into.Designed[laneAt];
    if (into.Along.size() < 2) { return; }
  } else {
    DesignLane(on, lane, laneAt, into);
    return;
  }

  auto tookFrom = std::chrono::steady_clock::now();
  const auto since = [&tookFrom] {
    const auto was = tookFrom;
    tookFrom = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(tookFrom - was).count();
  };
  {
    const Heap::Tagged fitting("road-fit");
    FitLane(laneAt, into);
  }
  into.FitMs += since();
  if (into.Along.size() < 2) {
    ++into.RefusedWays;
    return;
  }
  if (lane.Bridge && on.WaterRow >= 0 && on.Classes) { MarksWaterCrossing(on, laneAt, into); }
  LevelsDeckOrApproach(on, lane, laneAt, into);
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
  for (size_t at = 1; at < into.Along.size(); ++at) {
    const double runE = into.Along[at].EastM - into.Along[at - 1u].EastM;
    const double runS = into.Along[at].SouthM - into.Along[at - 1u].SouthM;
    const double runM = std::sqrt(runE * runE + runS * runS);
    if (!(runM > kLeastSpanM)) { continue; }
    const double groundAt = on.Draped.At(
        {.EastM = into.Along[at].EastM, .SouthM = -into.Along[at].SouthM}, into.Along[at].GradeM);
    const double groundBefore =
        on.Draped.At({.EastM = into.Along[at - 1u].EastM, .SouthM = -into.Along[at - 1u].SouthM},
                     into.Along[at - 1u].GradeM);
    const double yieldM = std::max(std::fabs(into.Along[at].GradeM - groundAt),
                                   std::fabs(into.Along[at - 1u].GradeM - groundBefore));
    const double outE = -runS / runM;
    const double outS = runE / runM;
    const double half = static_cast<double>(lane.HalfWidthM) + kVergeM;
    double reliefM = std::fabs(groundAt - groundBefore);
    for (const double hand : {1.0, -1.0}) {
      for (int end = 0; end < 2; ++end) {
        const RoadStation &one = end == 0 ? into.Along[at - 1u] : into.Along[at];
        const double sideE = one.EastM + outE * half * hand;
        const double sideS = one.SouthM + outS * half * hand;
        reliefM = std::max(
            reliefM, on.Draped.At({.EastM = sideE, .SouthM = -sideS}, one.GradeM) - one.GradeM);
      }
    }
    if (yieldM < kStampWorthM && reliefM < kBrokenGroundM) { continue; }
    Yields made;
    made.RingEastSouthM = {into.Along[at - 1u].EastM + outE * half,
                           into.Along[at - 1u].SouthM + outS * half,
                           into.Along[at].EastM + outE * half,
                           into.Along[at].SouthM + outS * half,
                           into.Along[at].EastM - outE * half,
                           into.Along[at].SouthM - outS * half,
                           into.Along[at - 1u].EastM - outE * half,
                           into.Along[at - 1u].SouthM - outS * half};
    made.LowE = made.HighE = made.RingEastSouthM[0];
    made.LowS = made.HighS = made.RingEastSouthM[1];
    for (size_t k = 2; k + 1 < made.RingEastSouthM.size(); k += 2) {
      made.LowE = std::min(made.LowE, made.RingEastSouthM[k]);
      made.HighE = std::max(made.HighE, made.RingEastSouthM[k]);
      made.LowS = std::min(made.LowS, made.RingEastSouthM[k + 1]);
      made.HighS = std::max(made.HighS, made.RingEastSouthM[k + 1]);
    }
    made.AtE = into.Along[at - 1u].EastM;
    made.AtS = into.Along[at - 1u].SouthM;
    made.PlateauM = into.Along[at - 1u].GradeM;
    const double rise = (into.Along[at].GradeM - into.Along[at - 1u].GradeM) / runM;
    made.SlopeE = rise * runE / runM;
    made.SlopeS = rise * runS / runM;
    made.ApronM = std::clamp(kBatterRun * yieldM, kLeastApronM, kMostApronM);
    made.YieldM = yieldM;
    const bool rests = !lane.Bridge || at == 1u || at + 1u == into.Along.size();
    if (rests) {
      made.SeamEastSouthM = {
          into.Along[at - 1u].EastM + outE * static_cast<double>(lane.HalfWidthM),
          into.Along[at - 1u].SouthM + outS * static_cast<double>(lane.HalfWidthM),
          into.Along[at].EastM + outE * static_cast<double>(lane.HalfWidthM),
          into.Along[at].SouthM + outS * static_cast<double>(lane.HalfWidthM),
          into.Along[at].EastM - outE * static_cast<double>(lane.HalfWidthM),
          into.Along[at].SouthM - outS * static_cast<double>(lane.HalfWidthM),
          into.Along[at - 1u].EastM - outE * static_cast<double>(lane.HalfWidthM),
          into.Along[at - 1u].SouthM - outS * static_cast<double>(lane.HalfWidthM)};
    }
    made.Fills = !lane.Bridge;
    made.Kind = outshine::Stamp::Corridor;
    corridor.push_back(std::move(made));
  }
  if (!lane.Bridge && into.Along.size() > 3) {
    const size_t shutFrom = static_cast<size_t>(lane.FirstPoint) * 2u;
    const size_t shutTo = shutFrom + (static_cast<size_t>(lane.PointCount) - 1u) * 2u;
    const bool shut = shutTo + 1 < on.Points.size() &&
                      std::fabs(on.Points[shutFrom] - on.Points[shutTo]) < 1.0e-7 &&
                      std::fabs(on.Points[shutFrom + 1] - on.Points[shutTo + 1]) < 1.0e-7;
    if (shut) {
      Yields island;
      island.RingEastSouthM.reserve(into.Along.size() * 2u);
      island.LowE = island.HighE = into.Along.front().EastM;
      island.LowS = island.HighS = into.Along.front().SouthM;
      double summed = 0.0;
      for (const RoadStation &one : into.Along) {
        island.RingEastSouthM.push_back(one.EastM);
        island.RingEastSouthM.push_back(one.SouthM);
        island.LowE = std::min(island.LowE, one.EastM);
        island.HighE = std::max(island.HighE, one.EastM);
        island.LowS = std::min(island.LowS, one.SouthM);
        island.HighS = std::max(island.HighS, one.SouthM);
        summed += one.GradeM;
      }
      island.AtE = into.Along.front().EastM;
      island.AtS = into.Along.front().SouthM;
      island.PlateauM = summed / static_cast<double>(into.Along.size());
      island.ApronM = kLeastApronM;
      island.YieldM = kBrokenGroundM;
      island.Fills = true;
      island.Kind = outshine::Stamp::Corridor;
      corridor.push_back(std::move(island));
    }
  }
  {
    const size_t first = static_cast<size_t>(lane.FirstPoint) * 2u;
    const size_t last = first + (static_cast<size_t>(lane.PointCount) - 1u) * 2u;
    if (last + 1 < on.Points.size()) {
      const std::array<uint64_t, 2> key = {
          {PlaceKey({.LongitudeDeg = on.Points[first + 1], .LatitudeDeg = on.Points[first]}),
           PlaceKey({.LongitudeDeg = on.Points[last + 1], .LatitudeDeg = on.Points[last]})}};
      for (int side = 0; side < 2; ++side) {
        const RoadStation &at = side == 0 ? into.Along.front() : into.Along.back();
        const RoadStation &to = side == 0 ? into.Along[1] : into.Along[into.Along.size() - 2u];
        double outE = to.EastM - at.EastM;
        double outS = to.SouthM - at.SouthM;
        const double run = std::sqrt(outE * outE + outS * outS);
        if (!(run > kLeastRunM)) { continue; }
        outE /= run;
        outS /= run;
        into.Gates[key[side]].push_back(
            RoadGate{.EastM = at.EastM,
                     .SouthM = at.SouthM,
                     .GradeM = at.GradeM,
                     .OutE = outE,
                     .OutS = outS,
                     .HalfWidthM = static_cast<double>(lane.HalfWidthM)});
      }
    }
  }
}

void Corridors::LayLanesIntoNetwork(const outshine::Ground::StreetField &ways,
                                    std::span<const double> points,
                                    Path::Network &net,
                                    std::vector<size_t> &netToLane) {
  netToLane.reserve(ways.Ways().size());
  for (size_t at = 0; at < ways.Ways().size(); ++at) {
    const outshine::Ground::StreetField::Way &lane = ways.Ways()[at];
    if (lane.Form != outshine::Ground::StreetField::Shape::Ribbon || lane.PointCount < 2) {
      continue;
    }
    const size_t first = static_cast<size_t>(lane.FirstPoint) * 2;
    if (first + static_cast<size_t>(lane.PointCount) * 2 > points.size()) { continue; }
    net.Lay(points.subspan(first, static_cast<size_t>(lane.PointCount) * 2),
            Path::WayClass{.HalfWidthM = static_cast<double>(lane.HalfWidthM),
                           .MaxGradient = 0.0,
                           .MinRadiusM = 0.0,
                           .Friction = 0.0,
                           .Lanes = lane.Lanes,
                           .Spans = lane.Bridge});
    netToLane.push_back(at);
  }
}

void Corridors::FileCrossing(const Path::Network::Crossing &one,
                             const TangentFrame &standing,
                             Paved &into) {
  const EastNorthUp crossedAt = standing.Place(
      {.LongitudeDeg = one.LongitudeDeg, .LatitudeDeg = one.LatitudeDeg, .HeightM = 0.0});
  const uint64_t named =
      PlaceKey({.LongitudeDeg = one.LongitudeDeg, .LatitudeDeg = one.LatitudeDeg}) | 1ULL;
  const auto east = static_cast<int64_t>(std::floor(crossedAt.EastM / kCrossCellM));
  const auto south = static_cast<int64_t>(std::floor(-crossedAt.NorthM / kCrossCellM));
  for (int64_t stepE = -1; stepE <= 1; ++stepE) {
    for (int64_t stepS = -1; stepS <= 1; ++stepS) {
      const auto atE = static_cast<uint64_t>(east + stepE + 0x20000000LL);
      const auto atS = static_cast<uint64_t>(south + stepS + 0x20000000LL);
      into.AtCrossing[(atE << 32U) | atS].push_back(
          Meets{.EastM = crossedAt.EastM, .SouthM = -crossedAt.NorthM, .Named = named});
    }
  }
}

void Corridors::RaiseDeckOver(const Path::Network::Crossing &one,
                              const Paving &on,
                              std::span<const size_t> netToLane,
                              Paved &into) {
  const outshine::Ground::StreetField &ways = on.Ways;
  const TangentFrame &standing = on.Standing;
  const Drape &drapedOver = on.Draped;

  if (one.OverWay >= netToLane.size() || one.UnderWay >= netToLane.size()) { return; }
  const size_t a = netToLane[one.OverWay];
  const size_t b = netToLane[one.UnderWay];
  const outshine::Ground::StreetField::Way &first = ways.Ways()[a];
  const outshine::Ground::StreetField::Way &second = ways.Ways()[b];
  if (first.Bridge == second.Bridge) { return; }
  const size_t spans = first.Bridge ? a : b;
  const outshine::Ground::StreetField::Way &below = first.Bridge ? second : first;
  const std::optional<double> stood =
      on.Stack.Ground()
          .At({.LongitudeDeg = one.LongitudeDeg, .LatitudeDeg = one.LatitudeDeg})
          .AslM();
  if (!stood) { return; }
  const EastNorthUp at = standing.Place(
      {.LongitudeDeg = one.LongitudeDeg, .LatitudeDeg = one.LatitudeDeg, .HeightM = *stood});
  const double onDrawn = drapedOver.At({.EastM = at.EastM, .SouthM = -at.NorthM}, at.UpM);
  const double need = onDrawn + static_cast<double>(below.ClearanceM);
  if (need <= into.DeckM[spans]) { return; }
  if (into.DeckM[spans] < kUnraisedDeckM) { ++into.DecksRaised; }
  into.DeckM[spans] = need;
  into.MostRaisedM = std::max(into.MostRaisedM, need - onDrawn);
}

void Corridors::Crosses(const Paving &on, Paved &into) {
  const outshine::Ground::StreetField &ways = on.Ways;
  const outshine::Ground::OsmField &vectors = on.Vectors;
  const TangentFrame &standing = on.Standing;

  auto partAt = std::chrono::steady_clock::now();
  const auto part = [&partAt] {
    const auto was = partAt;
    partAt = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(partAt - was).count();
  };

  Path::Network net(Path::Snap{.CellM = kNodeSnapM}, Path::Sphere{.RadiusM = Data::kWgs84A});
  std::vector<size_t> netToLane;
  LayLanesIntoNetwork(ways, vectors.Points(), net, netToLane);
  into.CrossNetworkMs = part();

  std::vector<Path::Network::Crossing> crossed;
  const auto swept = net.Crossings(crossed);
  if (!swept) { return; }
  into.CrossSweepMs = part();
  into.CrossingsSeen = crossed.size();
  into.PairsTested = swept->PairsTested;
  into.PairsPruned = swept->PairsPruned;
  into.FullestCell = swept->FullestCell;
  for (const Path::Network::Crossing &one : crossed) { FileCrossing(one, standing, into); }
  into.CrossFilingMs = part();
  for (const Path::Network::Crossing &one : crossed) { RaiseDeckOver(one, on, netToLane, into); }
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

  const std::optional<double> stood = on.Stack.Ground().At(at).AslM();
  if (!stood) { return std::nullopt; }
  const EastNorthUp enu = standing.Place(
      {.LongitudeDeg = at.LongitudeDeg, .LatitudeDeg = at.LatitudeDeg, .HeightM = *stood});
  const Drape::EastSouth here = {.EastM = enu.EastM, .SouthM = -enu.NorthM};
  return Grounded{
      .EastM = here.EastM, .SouthM = here.SouthM, .GradeM = drapedOver.At(here, enu.UpM)};
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

void Corridors::SeedsBridgeEnds(const Paving &on, Paved &into) {
  const outshine::Ground::StreetField &ways = on.Ways;

  for (size_t at = 0; at < ways.Ways().size(); ++at) {
    const outshine::Ground::StreetField::Way &lane = ways.Ways()[at];
    if (lane.Form != outshine::Ground::StreetField::Shape::Ribbon || lane.PointCount < 2) {
      continue;
    }
    const std::optional<Ends> ends = EndsOf(on.Vectors, lane);
    if (!ends) { continue; }
    const std::array<uint64_t, 2> &key = ends->Key;
    for (int side = 0; side < 2; ++side) {
      const size_t axis = static_cast<size_t>(side) * 2u;
      const std::optional<Grounded> under =
          GroundUnder(on, {.LongitudeDeg = ends->At[axis + 1u], .LatitudeDeg = ends->At[axis]});
      if (!under) { continue; }
      const double stood = under->GradeM;
      const auto found = into.EndM.find(key[side]);
      if (found == into.EndM.end()) {
        into.EndM.emplace(key[side], stood);
        into.GroundEndM.emplace(key[side], stood);
      } else {
        found->second = std::max(found->second, stood);
        const auto seeded = into.GroundEndM.find(key[side]);
        if (seeded != into.GroundEndM.end()) { seeded->second = std::max(seeded->second, stood); }
      }
    }
    if (lane.Bridge && into.DeckM[at] > kUnraisedDeckM) { RaisesEnds(key, into.DeckM[at], into); }
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
  for (int pass = 0; pass < kRampPasses; ++pass) {
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
      const double perLon = 111320.0 * std::cos(ends->At[0] * kDeg2Rad);
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
}

void Corridors::GradesApproaches(const Paving &on, Paved &into) {
  const outshine::Ground::StreetField &ways = on.Ways;

  for (const outshine::Ground::StreetField::Way &lane : ways.Ways()) {
    if (lane.Bridge || lane.Form != outshine::Ground::StreetField::Shape::Ribbon) { continue; }
    if (lane.PointCount < 2) { continue; }
    const std::optional<Ends> ends = EndsOf(on.Vectors, lane);
    if (!ends) { continue; }
    const std::array<uint64_t, 2> &key = ends->Key;
    const std::optional<Grounded> low =
        GroundUnder(on, {.LongitudeDeg = ends->At[1], .LatitudeDeg = ends->At[0]});
    const std::optional<Grounded> high =
        GroundUnder(on, {.LongitudeDeg = ends->At[3], .LatitudeDeg = ends->At[2]});
    if (!low || !high) { continue; }
    const Vec2 stood = {{low->GradeM, high->GradeM}};
    double rose = 0.0;
    for (int side = 0; side < 2; ++side) {
      const auto found = into.EndM.find(key[side]);
      if (found == into.EndM.end()) { continue; }
      rose = std::max(rose, found->second - stood[side]);
    }
    if (rose > kRoseLeast) {
      ++into.RampsRaised;
      into.SteepestRamp = std::max(into.SteepestRamp, rose);
    }
  }
}

void Corridors::Bridges(const Paving &on, Paved &into) {

  SeedsBridgeEnds(on, into);
  Notes(into, "streets: the highest deck a ramp must reach", HighestDeckM(into), "m");
  EasesRamps(on.Ways, on.Vectors, HighestDeckM(into), into);
  GradesApproaches(on, into);
}

namespace {

struct Leaving {
  uint32_t Way = 0;
  uint8_t Side = 0;
  float DirE = 0.0f;
  float DirN = 0.0f;
  float HalfM = 0.0f;
};

void EndsMeetingAt(const outshine::Ground::StreetField &ways,
                   std::span<const double> points,
                   std::unordered_map<uint64_t, std::vector<Leaving>> &meeting) {
  for (size_t at = 0; at < ways.Ways().size(); ++at) {
    const outshine::Ground::StreetField::Way &lane = ways.Ways()[at];
    if (lane.Form != outshine::Ground::StreetField::Shape::Ribbon || lane.PointCount < 2) {
      continue;
    }
    if (!(lane.HalfWidthM > 0.0f)) { continue; }
    const size_t first = static_cast<size_t>(lane.FirstPoint) * 2u;
    const size_t last = first + (static_cast<size_t>(lane.PointCount) - 1u) * 2u;
    if (last + 1 >= points.size()) { continue; }
    for (int side = 0; side < 2; ++side) {
      const size_t here = side == 0 ? first : last;
      const size_t next = side == 0 ? first + 2u : last - 2u;
      const double perLon = 111320.0 * std::cos(points[here] * kDeg2Rad);
      double outE = (points[next + 1] - points[here + 1]) * perLon;
      double outN = (points[next] - points[here]) * kMPerDegLat;
      const double run = std::sqrt(outE * outE + outN * outN);
      if (!(run > kLeastRunM)) { continue; }
      outE /= run;
      outN /= run;
      meeting[PlaceKey({.LongitudeDeg = points[here + 1], .LatitudeDeg = points[here]})].push_back(
          Leaving{.Way = static_cast<uint32_t>(at),
                  .Side = static_cast<uint8_t>(side),
                  .DirE = static_cast<float>(outE),
                  .DirN = static_cast<float>(outN),
                  .HalfM = lane.HalfWidthM});
    }
  }
}

double BackOffFor(const Leaving &mine, std::span<const Leaving> leaving) {
  double back = 0.0;
  for (const Leaving &other : leaving) {
    if (other.Way == mine.Way && other.Side == mine.Side) { continue; }
    const double cosBetween =
        static_cast<double>(mine.DirE) * other.DirE + static_cast<double>(mine.DirN) * other.DirN;
    const double sinBetween = std::fabs(static_cast<double>(mine.DirE) * other.DirN -
                                        static_cast<double>(mine.DirN) * other.DirE);
    if (sinBetween < kLeastSineBetween) { continue; }
    back =
        std::max(back,
                 (static_cast<double>(other.HalfM) + static_cast<double>(mine.HalfM) * cosBetween) /
                     sinBetween);
  }
  return back;
}

double SharpestForkFor(const Leaving &mine, std::span<const Leaving> leaving) {
  double sharpest = kDegPerHalfTurn;
  for (const Leaving &other : leaving) {
    if (other.Way == mine.Way && other.Side == mine.Side) { continue; }
    const double between = std::acos(std::clamp(static_cast<double>(mine.DirE) * other.DirE +
                                                    static_cast<double>(mine.DirN) * other.DirN,
                                                -1.0,
                                                1.0));
    sharpest = std::min(sharpest, kDegPerHalfTurn - between * kRad2Deg);
  }
  return sharpest;
}

} // namespace

void Corridors::Shortens(const outshine::Ground::StreetField &ways,
                         const outshine::Ground::OsmField &vectors,
                         Paved &into) {
  std::unordered_map<uint64_t, std::vector<Leaving>> meeting;
  EndsMeetingAt(ways, vectors.Points(), meeting);

  std::vector<uint64_t> met;
  met.reserve(meeting.size());
  for (const auto &one : meeting) {
    if (one.second.size() >= 2) { met.push_back(one.first); }
  }
  std::ranges::sort(met);

  for (const uint64_t at : met) {
    const std::vector<Leaving> &leaving = meeting[at];
    for (const Leaving &mine : leaving) {
      const double back = BackOffFor(mine, leaving);
      const double capped = std::min(back, static_cast<double>(mine.HalfM) * kTrimMostWidths);
      into.TrimM[static_cast<size_t>(mine.Way) * 2u + mine.Side] = capped;
      if (capped > kLeastCapM) {
        ++into.EndsTrimmed;
        into.DeepestTrimM = std::max(into.DeepestTrimM, capped);
      }
      if (back > capped + kLeastCapM) {
        ++into.EndsStillCrossing;
        const double shortBy = back - capped;
        into.LongestShortM = std::max(into.LongestShortM, shortBy);
        into.ShortestByM = into.CapsBit == 0 ? shortBy : std::min(into.ShortestByM, shortBy);
        const double fork = SharpestForkFor(mine, leaving);
        into.SharpestForkDeg = into.CapsBit == 0 ? fork : std::min(into.SharpestForkDeg, fork);
        ++into.CapsBit;
      }
    }
  }
}

namespace {

struct Meeting {
  uint64_t Node = 0;
  uint32_t Lane = 0;
  double GradeM = 0.0;
};

struct MetAt {
  uint32_t First = 0;
  uint32_t Count = 0;
};

std::vector<Meeting> MeetingsIn(std::span<const std::vector<RoadStation>> designed) {
  std::vector<Meeting> met;
  for (uint32_t lane = 0; lane < static_cast<uint32_t>(designed.size()); ++lane) {
    for (const RoadStation &one : designed[lane]) {
      if (one.Node == 0u) { continue; }
      met.push_back({.Node = one.Node, .Lane = lane, .GradeM = one.GradeM});
    }
  }
  std::ranges::stable_sort(met, {}, &Meeting::Node);
  return met;
}

std::vector<MetAt> JunctionsIn(std::span<const Meeting> met) {
  std::vector<MetAt> meetings;
  for (uint32_t at = 0; at < static_cast<uint32_t>(met.size());) {
    uint32_t past = at;
    while (past < met.size() && met[past].Node == met[at].Node) { ++past; }
    if (past - at >= 2) { meetings.push_back({.First = at, .Count = past - at}); }
    at = past;
  }
  return meetings;
}

double RelaxMeetings(std::span<const Meeting> met,
                     std::span<const MetAt> meetings,
                     std::span<double> shiftM) {
  std::vector<double> pullM(shiftM.size(), 0.0);
  std::vector<uint32_t> pulls(shiftM.size(), 0u);
  double movedM = 0.0;
  for (int round = 0; round < kLevelPasses; ++round) {
    std::ranges::fill(pullM, 0.0);
    std::ranges::fill(pulls, 0u);
    for (const MetAt &node : meetings) {
      const std::span<const Meeting> held(met.data() + node.First, node.Count);
      double wanted = 0.0;
      for (const Meeting &one : held) { wanted += one.GradeM + shiftM[one.Lane]; }
      wanted /= static_cast<double>(node.Count);
      for (const Meeting &one : held) {
        pullM[one.Lane] += wanted - (one.GradeM + shiftM[one.Lane]);
        ++pulls[one.Lane];
      }
    }
    double most = 0.0;
    for (size_t lane = 0; lane < shiftM.size(); ++lane) {
      if (pulls[lane] == 0u) { continue; }
      const double by = pullM[lane] / static_cast<double>(pulls[lane]);
      shiftM[lane] += by;
      most = std::max(most, std::fabs(by));
    }
    movedM = most;
    if (most < kLevelledM) { break; }
  }
  return movedM;
}

} // namespace

double Corridors::LevelsWhereWaysMeet(Paved &into) {
  const std::vector<Meeting> met = MeetingsIn(into.Designed);
  const std::vector<MetAt> meetings = JunctionsIn(met);
  std::vector<double> shiftM(into.Designed.size(), 0.0);
  const double movedM = RelaxMeetings(met, meetings, shiftM);

  for (size_t lane = 0; lane < into.Designed.size(); ++lane) {
    if (shiftM[lane] == 0.0) { continue; }
    for (RoadStation &one : into.Designed[lane]) { one.GradeM += shiftM[lane]; }
  }
  return movedM;
}

size_t Corridors::RaisesTheJunctionBodies(const outshine::Ground::GroundMaterials &wearing,
                                          Paved &into,
                                          RoadRaised &pavement) const {

  std::vector<uint64_t> nodes;
  nodes.reserve(into.Gates.size());
  for (const auto &one : into.Gates) {
    if (one.second.size() >= 2) { nodes.push_back(one.first); }
  }
  std::ranges::sort(nodes);
  const int asphalt = wearing.Find("asphalt");
  Vec3f wears = {{0.5f, 0.5f, 0.5f}};
  if (asphalt >= 0) { wears = wearing.At(static_cast<size_t>(asphalt)).Albedo; }
  size_t raised = 0;
  for (const uint64_t node : nodes) {
    const std::vector<RoadGate> &met = into.Gates[node];
    Sweeper_.Junction(std::span<const RoadGate>(met.data(), met.size()), wears, pavement);
    ++raised;
  }
  return raised;
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

void Corridors::HandsThePavingOver(const outshine::Ground::GroundMaterials &wearing,
                                   const RoadRaised &pavement,
                                   Paved &into,
                                   Geometry &ground) {

  if (pavement.Index.size() < 3) { return; }
  Material tarmac;
  for (int channel = 0; channel < 3; ++channel) { tarmac.BaseColour[channel] = 1.0f; }
  {
    const int asphalt = wearing.Find("asphalt");
    tarmac.Roughness =
        asphalt >= 0 ? wearing.At(static_cast<size_t>(asphalt)).Roughness : kUnlitTint;
  }
  const MaterialInstance paved = ground.addSurface("streets", tarmac);
  const int pavedPart = ground.addPart("streets", paved);
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
  Notes(into, "streets: the surface they were given", static_cast<double>(paved.index()), "index");
  Notes(into, "streets: the part they were given", static_cast<double>(pavedPart), "index");
  Notes(into, "streets: the geometry took them", tookPaving ? 1.0 : 0.0, "yes/no");
  Notes(
      into, "streets: parts the geometry now holds", static_cast<double>(ground.parts()), "parts");
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

void Corridors::Lay(const Site &site,
                    Geometry &ground,
                    std::vector<Yields> *corridorOut,
                    std::vector<Measure> *notes) const {
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
  into.TrimM.assign(ways.Ways().size() * 2u, 0.0);
  into.Designed.resize(ways.Ways().size());
  const int waterRow = site.Stack.Materials().Find("water");
  std::unordered_map<uint64_t, uint32_t> sharedNodes;
  std::optional<Paving> paving;
  if (vectors != nullptr) {
    sharedNodes = SharedNodesOf(ways, vectors->Points());
    paving.emplace(Paving{.Stack = site.Stack,
                          .Ways = ways,
                          .Vectors = *vectors,
                          .Points = vectors->Points(),
                          .SharedNodes = sharedNodes,
                          .Draped = drapedOver,
                          .Standing = standing,
                          .Classes = classStructure,
                          .WaterRow = waterRow});
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
  if (paving) { Shortens(ways, paving->Vectors, into); }
  Notes(into, "streets: of that, shortening the ends", since(), "ms");
  Notes(into, "streets: ends a cap shortened", static_cast<double>(into.CapsBit), "ends");
  Notes(into, "streets: and the most one lost", into.LongestShortM, "m");
  Notes(into, "streets: the sharpest fork a cap bit at", into.SharpestForkDeg, "deg");
  Notes(
      into, "streets: way ends a junction trimmed", static_cast<double>(into.EndsTrimmed), "ends");
  Notes(into, "streets: and the deepest trim", into.DeepestTrimM, "m");
  Notes(into,
        "streets: ends STILL crossing, the cap bit",
        static_cast<double>(into.EndsStillCrossing),
        "ends");
  if (paving) {
    const Paving &on = *paving;
    const Heap::Tagged pavingHeap("road-pave");
    for (const Pass pass : {Pass::Designing, Pass::Paving}) {
      const std::string_view doing = Doing(pass);
      for (size_t laneAt = 0; laneAt < ways.Ways().size(); ++laneAt) {
        PaveLane(on, pass, laneAt, into, corridor, pavement);
      }
      Notes(into, std::format("streets: of that, {} every lane", doing), since(), "ms");
      Notes(into, std::format("streets: of {}, the fit", doing), into.FitMs, "ms");
      Notes(into, std::format("streets: of {}, the water", doing), into.WaterMs, "ms");
      Notes(into, std::format("streets: of {}, the sweep", doing), into.SweepMs, "ms");
      into.FitMs = 0.0;
      into.WaterMs = 0.0;
      into.SweepMs = 0.0;
      if (pass == Pass::Designing) {
        Notes(into, "streets: the levelling's last shift", LevelsWhereWaysMeet(into), "m");
        Notes(into, "streets: of that, levelling the junctions", since(), "ms");
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
  HandsThePavingOver(site.Stack.Materials(), pavement, into, ground);
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
}
} // namespace outshine::Generators
