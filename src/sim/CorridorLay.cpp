#include "math/Units.h"
#include "math/Quantile.h"
#include "CorridorLay.h"

#include "Pilot.h"

#include <array>
#include <algorithm>
#include <numbers>
#include <chrono>
#include <cmath>

#include "Alignment.h"
#include "Angle.h"
#include <cstdio>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "Carriageway.h"
#include "Fit.h"
#include "Ribbon.h"
#include "SpeedProfile.h"

namespace outshine::Sim {

namespace {

constexpr double kNoLeastYet = 1.0e9;
constexpr double kTurnLeastRad = kLeastTurnRad;
constexpr double kMmPerM = 1000.0;
constexpr double kGripMargin = 0.95;
constexpr size_t kSayLineBytes = 96;
constexpr int kSweeps = 400;
constexpr double kSlowStationKmh = 30.0;

constexpr double kPatienceS = 900.0;
} // namespace

bool LayCorridor(const Path::Route &route,
                 const GroundQuery &ground,
                 const Scenario::Body &car,
                 const Rigged &stood,
                 double quantumM,
                 double tightestM,
                 double middleLat,
                 double sphereRadiusM,
                 Sink &say,
                 Corridor &out,
                 std::string &error) {
  const double carWidthM = car.WidthM;
  auto &corridor = out.Line;
  auto &fitted = out.Fitted;
  auto &profile = out.Profile;
  std::vector<double> roadM;
  std::vector<double> halfWidthM;
  std::vector<double> laneHalfM;
  std::vector<double> asideM;
  std::vector<double> frictionM;
  auto &stations = out.Fine;
  constexpr double fineM = Corridor::kFineM;
  auto &spanM = out.SpanM;
  auto &narrowestLaneM = out.NarrowestLaneM;
  auto &budgetM = out.BudgetM;
  auto &holdWithinM = out.HoldWithinM;

  const double frameLat = route.Legs.front().At.LatitudeDeg;
  const double frameLon = route.Legs.front().At.LongitudeDeg;
  const double perLatM = Path::ApartM({.LongitudeDeg = frameLon, .LatitudeDeg = frameLat},
                                      {.LongitudeDeg = frameLon, .LatitudeDeg = frameLat + 1.0},
                                      Path::Sphere{.RadiusM = sphereRadiusM});
  const double perLonM = Path::ApartM({.LongitudeDeg = frameLon, .LatitudeDeg = frameLat},
                                      {.LongitudeDeg = frameLon + 1.0, .LatitudeDeg = frameLat},
                                      Path::Sphere{.RadiusM = sphereRadiusM});
  out.FrameLat = frameLat;
  out.FrameLon = frameLon;
  out.PerLatM = perLatM;
  out.PerLonM = perLonM;
  std::vector<double> eastNorthM;
  eastNorthM.reserve(route.Legs.size() * 2);
  for (const auto &leg : route.Legs) {
    eastNorthM.push_back((leg.At.LongitudeDeg - frameLon) * perLonM);
    eastNorthM.push_back((leg.At.LatitudeDeg - frameLat) * perLatM);
  }
  say.Number("metres per degree of latitude in the local frame", perLatM, "m");
  say.Number("metres per degree of longitude there", perLonM, "m");

  std::vector<size_t> keptAt;
  const std::vector<double> keptM = outshine::Simplify(eastNorthM, quantumM, keptAt);
  std::vector<double> classTightestM(keptAt.size(), 0.0);
  size_t designed = 0;
  double roadWithinM = 0.0;
  for (size_t at = 0; at < keptAt.size(); ++at) {
    const double mine = route.Legs[keptAt[at]].MinRadiusM;
    const double before = at > 0 ? route.Legs[keptAt[at - 1]].MinRadiusM : mine;
    classTightestM[at] = mine <= 0.0 || before <= 0.0 ? 0.0 : std::min(mine, before);
    if (classTightestM[at] > 0.0) { ++designed; }
    const double half = route.Legs[keptAt[at]].HalfWidthM;
    roadWithinM = std::max(half, roadWithinM);
  }
  if (!(roadWithinM > quantumM)) { roadWithinM = quantumM; }

  std::vector<double> withinAtM(keptAt.size(), 0.0);
  double widestJunctionM = 0.0;
  for (size_t at = 0; at < keptAt.size(); ++at) {
    const double mine = route.Legs[keptAt[at]].HalfWidthM;
    const double half = mine > 0.0 ? mine : roadWithinM;
    withinAtM[at] = half;
    if (at == 0 || at + 1 >= keptAt.size()) { continue; }
    const double eastIn = keptM[2 * at] - keptM[2 * (at - 1)];
    const double northIn = keptM[2 * at + 1] - keptM[2 * (at - 1) + 1];
    const double eastOut = keptM[2 * (at + 1)] - keptM[2 * at];
    const double northOut = keptM[2 * (at + 1) + 1] - keptM[2 * at + 1];
    const double turn =
        std::fabs(outshine::Wrapped(std::atan2(northOut, eastOut) - std::atan2(northIn, eastIn)));
    if (!(turn > kTurnLeastRad) || turn >= std::numbers::pi - kTurnLeastRad) { continue; }
    const double before = route.Legs[keptAt[at - 1]].HalfWidthM;
    const double other = before > 0.0 ? before : half;
    const double intoM = std::sqrt(eastIn * eastIn + northIn * northIn);
    const double outOfM = std::sqrt(eastOut * eastOut + northOut * northOut);
    const double heldM = JunctionKerbM({.HalfAM = half,
                                        .HalfBM = other,
                                        .DeflectionRad = turn,
                                        .ShorterLegM = intoM < outOfM ? intoM : outOfM});
    withinAtM[at] = std::max(heldM, withinAtM[at]);
    widestJunctionM = std::max(withinAtM[at], widestJunctionM);
  }
  say.Number("the widest a junction lets an arc leave its corner", widestJunctionM, "m");
  const size_t offered = eastNorthM.size() / 2;
  const size_t kept = keptM.size() / 2;
  say.Number(
      "vertices the route offered before simplifying", static_cast<double>(offered), "vertices");
  say.Number("vertices left after removing what the data cannot resolve",
             static_cast<double>(kept),
             "vertices");
  say.Number("the share removed",
             1.0 - static_cast<double>(keptM.size()) / static_cast<double>(eastNorthM.size()),
             "of them");

  say.Number(
      "how far the built road may leave the polyline, being its own half width", roadWithinM, "m");
  fitted = Fit(keptM, roadWithinM, tightestM, classTightestM, corridor, withinAtM);
  if (!fitted.Laid) { say.Refuse(Line("%s", fitted.Error.c_str())); }
  say.Number("vertices the route offered", static_cast<double>(fitted.Vertices), "vertices");
  say.Number("corners the fit needed", static_cast<double>(fitted.Corners), "corners");
  say.Number("runs of same-sign turns among them", static_cast<double>(fitted.Runs), "runs");
  say.Number("the longest such run", static_cast<double>(fitted.LongestRunVertices), "vertices");
  say.Number("vertices with a same-sign turn on BOTH sides",
             static_cast<double>(fitted.SheltredVertices),
             "vertices");
  say.Number("straights between them", static_cast<double>(fitted.Straights), "straights");
  say.Number("the corridor it laid", fitted.LengthM / kMPerKm, "km");
  say.Number("the polyline it came from", route.LengthM / kMPerKm, "km");
  say.Number("the tightest radius on it", fitted.TightestRadiusM, "m");
  say.Number("at which vertex that is",
             static_cast<double>(fitted.TightestAtVertex),
             Line("of %s", std::to_string(fitted.Vertices).c_str()).c_str());
  say.Number(
      "the tightest radius any vertex DEMANDED, drivable or not", fitted.TightestDemandedM, "m");
  say.Number("vertices whose road class declares a design minimum radius",
             static_cast<double>(designed),
             Line("of %s", std::to_string(keptAt.size()).c_str()).c_str());
  size_t declaredLegs = 0;
  for (const auto &leg : route.Legs) {
    if (leg.MinRadiusM > 0.0) { ++declaredLegs; }
  }
  say.Number("legs of the route whose road class declares one",
             static_cast<double>(declaredLegs),
             Line("of %s", std::to_string(route.Legs.size()).c_str()).c_str());
  say.Number("corners the fit laid tighter than their class allows",
             static_cast<double>(fitted.UnderClass),
             "corners");
  if (fitted.UnderClass > 0) {
    say.Number("the worst of them", fitted.UnderClassRadiusM, "m");
    say.Number("where its class allows", fitted.UnderClassMinimumM, "m");
    say.Number("that vertex", static_cast<double>(fitted.UnderClassAtVertex), "");
  }
  say.Number("the sharpest turn it carried", fitted.SharpestTurnRad * kRad2Deg, "deg");
  say.Number("at which vertex", fitted.SharpestTurnAtM, "");
  say.Number(
      "turns past a right angle", static_cast<double>(fitted.TurnsPastRightAngle), "of 2480");
  say.Number("turns past 135 degrees", static_cast<double>(fitted.TurnsPastHalfCircle), "of 2480");
  say.Number("vertices too sharp for the car to drive at all",
             static_cast<double>(fitted.Undrivable),
             "vertices");
  say.Number("how far it leaves a vertex at worst", fitted.WorstOffsetM, "m");
  say.Number("bends the accuracy bound had to split",
             static_cast<double>(fitted.SplitByAccuracy),
             "bends");
  say.Number("how far the laid line drifts from the polyline beyond any corner's own doing",
             fitted.DriftM,
             "m");
  say.Number("per corner that is", fitted.DriftPerCornerM * kMmPerM, "mm");
  say.Number("the worst vertex", fitted.WorstVertex, "");
  say.Number("its incoming leg", fitted.WorstLegInM, "m");
  say.Number("its outgoing leg", fitted.WorstLegOutM, "m");
  say.Number("the turn there", fitted.WorstTurnRad * kRad2Deg, "deg");
  say.Number("the station the resection found", fitted.WorstStationM, "m");
  say.Number("where that happens", fitted.WorstOffsetAtM / kMPerKm, "km");
  say.Number("how far it is allowed to", quantumM, "m");

  if (!fitted.Laid) { say.Refuse(Line("%s", fitted.Error.c_str())); }
  if (!fitted.Laid && fitted.Undrivable > 0) {
    const auto at = static_cast<size_t>(fitted.UndrivableAtM);
    for (size_t which = at > 1 ? at - 1 : 0; which <= at + 1 && which < route.Legs.size();
         ++which) {
      std::array<char, kSayLineBytes> atLine{};
      std::snprintf(atLine.data(),
                    atLine.size(),
                    "AT %zu  %.7f %.7f",
                    which,
                    route.Legs[which].At.LatitudeDeg,
                    route.Legs[which].At.LongitudeDeg);
      say.Say(atLine.data());
    }
    if (at >= 1 && at + 1 < route.Legs.size()) {
      std::array<char, kSayLineBytes> legs{};
      std::snprintf(legs.data(),
                    legs.size(),
                    "LEGS in %.2f m  out %.2f m",
                    Path::ApartM({.LongitudeDeg = route.Legs[at - 1].At.LongitudeDeg,
                                  .LatitudeDeg = route.Legs[at - 1].At.LatitudeDeg},
                                 {.LongitudeDeg = route.Legs[at].At.LongitudeDeg,
                                  .LatitudeDeg = route.Legs[at].At.LatitudeDeg},
                                 Path::Sphere{.RadiusM = sphereRadiusM}),
                    Path::ApartM({.LongitudeDeg = route.Legs[at].At.LongitudeDeg,
                                  .LatitudeDeg = route.Legs[at].At.LatitudeDeg},
                                 {.LongitudeDeg = route.Legs[at + 1].At.LongitudeDeg,
                                  .LatitudeDeg = route.Legs[at + 1].At.LatitudeDeg},
                                 Path::Sphere{.RadiusM = sphereRadiusM}));
      say.Say(legs.data());
    }
  }
  if (!fitted.Laid) {
    say.Refuse("the route does not lay as a corridor");
    return false;
  }
  say.Number("the tightest radius the fit produced", fitted.TightestRadiusM, "m");
  say.Number("the tightest radius the body can drive", tightestM, "m");
  say.Number("the quantisation the drift is bounded against", quantumM, "m");
  say.Number("how much longer the corridor is than the polyline",
             fitted.LengthM / route.LengthM - 1.0,
             "of it");
  say.Number("the speed the tightest radius allows at 0.95 g",
             std::sqrt(kGripMargin * stood.Envelope.GravityMs2 * fitted.TightestRadiusM) * kMsToKmh,
             "km/h");

  const double postM = ground.PostM(middleLat);
  const long posts = static_cast<long>(std::ceil(fitted.LengthM / postM));
  say.Number("the elevation source's own post spacing here", postM, "m");
  say.Number("stations the corridor is sampled at", static_cast<double>(posts + 1), "stations");

  std::vector<double> heightM(static_cast<size_t>(posts) + 1, 0.0);
  std::vector<bool> known(static_cast<size_t>(posts) + 1, false);
  long holes = 0;
  long waited = 0;
  const auto sampling = std::chrono::steady_clock::now();
  for (long post = 0; post <= posts; ++post) {
    const double atM = static_cast<double>(post) * fitted.LengthM / static_cast<double>(posts);
    outshine::Placed on;
    if (!corridor.At(atM, on)) { continue; }
    const double latDeg = frameLat + on.NorthM / perLatM;
    const double lonDeg = frameLon + on.EastM / perLonM;
    for (;;) {
      const outshine::GroundSample sample =
          ground.At({.LongitudeDeg = lonDeg, .LatitudeDeg = latDeg});
      if (const std::optional<double> aslM = sample.AslM()) {
        heightM[static_cast<size_t>(post)] = *aslM;
        known[static_cast<size_t>(post)] = true;
        break;
      }
      if (sample.Where() == outshine::GroundSample::State::Hole) {
        ++holes;
        break;
      }
      ++waited;
      if (std::chrono::duration<double>(std::chrono::steady_clock::now() - sampling).count() >
          kPatienceS) {
        break;
      }
    }
  }
  const double sampledS =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - sampling).count();

  long resolved = 0;
  double lowestM = 0.0;
  double highestM = 0.0;
  bool haveAny = false;
  for (size_t post = 0; post < heightM.size(); ++post) {
    if (!known[post]) { continue; }
    ++resolved;
    if (!haveAny || heightM[post] < lowestM) { lowestM = heightM[post]; }
    if (!haveAny || heightM[post] > highestM) { highestM = heightM[post]; }
    haveAny = true;
  }
  say.Number("stations the elevation source answered", static_cast<double>(resolved), "stations");
  say.Number("stations it said were a hole", static_cast<double>(holes), "stations");
  say.Number("times a sample had to wait for a tile", static_cast<double>(waited), "waits");
  say.Number("seconds spent sampling the ground", sampledS, "s");
  say.Number("the lowest the corridor runs", lowestM, "m");
  say.Number("the highest", highestM, "m");
  out.FrameAltM = heightM.empty() ? 0.0 : heightM.front();
  say.Number("the elevation where the route starts", out.FrameAltM, "m");
  say.Number("and where it ends", heightM.empty() ? 0.0 : heightM.back(), "m");

  out.Made.Resolved = resolved;
  out.Made.Holes = holes;
  say.Number("the elevation where the route ends", heightM.empty() ? 0.0 : heightM.back(), "m");

  outshine::SpeedProfile inPlan;
  if (!inPlan.Over(corridor, stood.Envelope, postM, 0.0, error)) {
    say.Refuse(Line("the plan view carries no speed: %s", error.c_str()));
    return false;
  }

  spanM = fitted.LengthM / static_cast<double>(posts);
  roadM = heightM;
  std::vector<double> gradeLimit(roadM.size(), 0.0);
  {
    size_t leg = 0;
    for (size_t post = 0; post < roadM.size(); ++post) {
      const double atM = static_cast<double>(post) * spanM * route.LengthM / fitted.LengthM;
      while (leg + 1 < route.Legs.size() && route.Legs[leg + 1].AlongM < atM) { ++leg; }
      double limit = route.Legs[leg].MaxGradient;
      if (leg + 1 < route.Legs.size() && route.Legs[leg + 1].MaxGradient > 0.0 &&
          route.Legs[leg + 1].MaxGradient < limit) {
        limit = route.Legs[leg + 1].MaxGradient;
      }
      gradeLimit[post] = limit;
    }
  }
  halfWidthM.assign(roadM.size(), 0.0);
  frictionM.assign(roadM.size(), 0.0);
  {
    size_t leg = 0;
    for (size_t post = 0; post < halfWidthM.size(); ++post) {
      const double atM = static_cast<double>(post) * spanM * route.LengthM / fitted.LengthM;
      while (leg + 1 < route.Legs.size() && route.Legs[leg + 1].AlongM < atM) { ++leg; }
      double half = route.Legs[leg].HalfWidthM;
      if (leg + 1 < route.Legs.size() && route.Legs[leg + 1].HalfWidthM > 0.0 &&
          route.Legs[leg + 1].HalfWidthM < half) {
        half = route.Legs[leg + 1].HalfWidthM;
      }
      halfWidthM[post] = half;
      frictionM[post] = route.Legs[leg].Friction;
    }
  }
  laneHalfM.assign(roadM.size(), 0.0);
  asideM.assign(roadM.size(), 0.0);
  long laneless = 0;
  {
    size_t leg = 0;
    for (size_t post = 0; post < laneHalfM.size(); ++post) {
      const double atM = static_cast<double>(post) * spanM * route.LengthM / fitted.LengthM;
      while (leg + 1 < route.Legs.size() && route.Legs[leg + 1].AlongM < atM) { ++leg; }
      const int lanes = route.Legs[leg].Lanes;
      if (lanes <= 0) {
        ++laneless;
        laneHalfM[post] = halfWidthM[post];
        continue;
      }
      const double laneM = 2.0 * halfWidthM[post] / static_cast<double>(lanes);
      laneHalfM[post] = 0.5 * laneM;
      asideM[post] = -0.5 * static_cast<double>(lanes - 1) * laneM;
    }
  }
  say.Number(
      "stations whose road kind declares no lane count", static_cast<double>(laneless), "stations");
  out.Made.LanelessKinds = laneless;

  double steppedM = 0.0;
  double steppedAtM = 0.0;
  for (size_t post = 1; post < asideM.size(); ++post) {
    const double step = std::fabs(asideM[post] - asideM[post - 1]);
    if (step > steppedM) {
      steppedM = step;
      steppedAtM = static_cast<double>(post) * spanM;
    }
  }
  say.Number("the largest step the lane centre takes where the road changes width", steppedM, "m");
  say.Number("where that is", steppedAtM / kMPerKm, "km");

  double narrowestLaneHereM = kNoLeastYet;
  for (const double half : laneHalfM) {
    narrowestLaneHereM = 2.0 * half < narrowestLaneHereM ? 2.0 * half : narrowestLaneHereM;
  }
  budgetM = 0.5 * narrowestLaneHereM - 0.5 * carWidthM;

  out.Bake(fitted.LengthM);
  for (size_t at = 0; at < stations.size(); ++at) {
    const auto post = static_cast<size_t>(static_cast<double>(at) * fineM / spanM);
    const size_t band = post < asideM.size() ? post : asideM.size() - 1;
    stations[at].AsideM = asideM[band];
    stations[at].EdgeM = halfWidthM[band];
    stations[at].LaneHalfM = laneHalfM[band < laneHalfM.size() ? band : laneHalfM.size() - 1];
    stations[at].Friction = frictionM[band];
  }
  {
    long gripless = 0;
    double leastGrip = kNoLeastYet;
    double mostGrip = 0.0;
    for (const Station &one : stations) {
      if (!(one.Friction > 0.0)) {
        ++gripless;
        continue;
      }
      leastGrip = one.Friction < leastGrip ? one.Friction : leastGrip;
      mostGrip = one.Friction > mostGrip ? one.Friction : mostGrip;
    }
    say.Number("the least grip the route's surface offers",
               std::cmp_less(gripless, stations.size()) ? leastGrip : 0.0,
               "x");
    say.Number("the most", mostGrip, "x");
    if (gripless > 0) {
      say.Refuse("the route crosses " + std::to_string(gripless) +
                 " station(s) whose surface declares no friction, and a wheel cannot stand on a "
                 "ground that grips with nothing");
      error = "a station on the route carries no surface friction";
      return false;
    }
  }
  {
    const double fastestMs = inPlan.Fastest().Ms;
    const double reachM = outshine::Pilot::kSettleS * fastestMs;
    const auto rate = AsideRatePerM(budgetM, fastestMs);
    if (!rate) {
      say.Refuse(std::string(rate.error()));
      return false;
    }
    const double mostPerM = *rate;
    out.AsideRatePerM = mostPerM;
    say.Number("the fastest the plan view holds", fastestMs * kMsToKmh, "km/h");
    say.Number(
        "the top speed the declaration would allow", stood.Envelope.TopMs() * kMsToKmh, "km/h");
    say.Number("the look-ahead time the pilot settles over", outshine::Pilot::kSettleS, "s");
    say.Number("the reach that buys at the fastest the plan holds", reachM, "m");
    say.Number("the fastest the lane centre may move sideways", mostPerM * kMmPerM, "mm per metre");
    say.Number("so a full-budget shift is taken over", budgetM / mostPerM, "m of road");
    const double most = mostPerM * fineM;

    std::vector<double> roomM(stations.size(), 0.0);
    long insideTight = 0;
    double worstDrivenM = kNoLeastYet;
    for (size_t fine = 0; fine < roomM.size(); ++fine) {
      double room = stations[fine].EdgeM - 0.5 * carWidthM - out.BudgetM;
      outshine::Placed on;
      if (corridor.At(static_cast<double>(fine) * fineM, on) && on.CurvaturePerM != 0.0) {
        const double radiusM = 1.0 / std::fabs(on.CurvaturePerM);
        const double inside = radiusM - tightestM;
        worstDrivenM = radiusM < worstDrivenM ? radiusM : worstDrivenM;
        if (inside < room) {
          room = inside;
          ++insideTight;
        }
      }
      roomM[fine] = room > 0.0 ? room : 0.0;
    }
    say.Number("the tightest the corridor itself turns", worstDrivenM, "m");
    say.Number("stations where the corner is too tight to hold two lanes apart",
               static_cast<double>(insideTight),
               "stations");
    double leadM = 0.0;
    for (size_t fine = roomM.size() - 1; fine > 0; --fine) {
      const double reachable = roomM[fine] + most;
      if (roomM[fine - 1] > reachable) {
        leadM = std::fmax(leadM, roomM[fine - 1] - reachable);
        roomM[fine - 1] = reachable;
      }
    }
    for (size_t fine = 1; fine < roomM.size(); ++fine) {
      const double reachable = roomM[fine - 1] + most;
      roomM[fine] = std::min(roomM[fine], reachable);
    }
    say.Number("the tracking error the lane centre keeps clear of the edge", budgetM, "m");
    say.Number("the most a narrowing pulled the lane centre in ahead of itself", leadM, "m");
    long led = 0;
    for (size_t fine = 0; fine < stations.size(); ++fine) {
      if (stations[fine].AsideM > roomM[fine]) {
        stations[fine].AsideM = roomM[fine];
        ++led;
      }
      if (stations[fine].AsideM < -roomM[fine]) {
        stations[fine].AsideM = -roomM[fine];
        ++led;
      }
    }
    say.Number("stations where a narrowing ahead moved the car in early",
               static_cast<double>(led),
               "stations");

    for (int sweep = 0; sweep < kSweeps; ++sweep) {
      long moved = 0;
      for (size_t fine = 1; fine < stations.size(); ++fine) {
        if (stations[fine].AsideM > stations[fine - 1].AsideM + most) {
          stations[fine].AsideM = stations[fine - 1].AsideM + most;
          ++moved;
        }
        if (stations[fine].AsideM < stations[fine - 1].AsideM - most) {
          stations[fine].AsideM = stations[fine - 1].AsideM - most;
          ++moved;
        }
      }
      for (size_t fine = stations.size() - 1; fine > 0; --fine) {
        if (stations[fine - 1].AsideM > stations[fine].AsideM + most) {
          stations[fine - 1].AsideM = stations[fine].AsideM + most;
          ++moved;
        }
        if (stations[fine - 1].AsideM < stations[fine].AsideM - most) {
          stations[fine - 1].AsideM = stations[fine].AsideM - most;
          ++moved;
        }
      }
      if (moved == 0) { break; }
    }
    long clamped = 0;
    for (size_t fine = 0; fine < stations.size(); ++fine) {
      if (stations[fine].AsideM > roomM[fine]) {
        stations[fine].AsideM = roomM[fine];
        ++clamped;
      }
      if (stations[fine].AsideM < -roomM[fine]) {
        stations[fine].AsideM = -roomM[fine];
        ++clamped;
      }
    }
    say.Number("stations where the road edge overruled the taper",
               static_cast<double>(clamped),
               "stations");

    double leftM = 0.0;
    double worstOverM = 0.0;
    for (size_t fine = 1; fine < stations.size(); ++fine) {
      leftM = std::fmax(leftM, std::fabs(stations[fine].AsideM - stations[fine - 1].AsideM));
      const double asideReachM = std::fabs(stations[fine].AsideM) + 0.5 * carWidthM;
      if (asideReachM > stations[fine].EdgeM) {
        worstOverM = std::fmax(worstOverM, asideReachM - stations[fine].EdgeM);
      }
    }
    say.Number("the largest step left after tapering", leftM, "m");
    say.Number(
        "the furthest the tapered lane centre pushes the car past a road edge", worstOverM, "m");
  }

  narrowestLaneM = kNoLeastYet;
  double widestLaneM = 0.0;
  double mostAsideM = 0.0;
  for (size_t post = 0; post < laneHalfM.size(); ++post) {
    narrowestLaneM =
        2.0 * laneHalfM[post] < narrowestLaneM ? 2.0 * laneHalfM[post] : narrowestLaneM;
    widestLaneM = 2.0 * laneHalfM[post] > widestLaneM ? 2.0 * laneHalfM[post] : widestLaneM;
    mostAsideM = std::fabs(asideM[post]) > mostAsideM ? std::fabs(asideM[post]) : mostAsideM;
  }
  say.Number("the narrowest LANE on the route", narrowestLaneM, "m");
  say.Number("the widest lane", widestLaneM, "m");
  say.Number("the furthest the car sits from the centreline", mostAsideM, "m");
  say.Number("what it leaves either side of itself in the narrowest lane",
             0.5 * narrowestLaneM - 0.5 * carWidthM,
             "m");

  double narrowestHalfM = kNoLeastYet;
  double widestHalfM = 0.0;
  for (const double half : halfWidthM) {
    narrowestHalfM = half < narrowestHalfM ? half : narrowestHalfM;
    widestHalfM = half > widestHalfM ? half : widestHalfM;
  }
  say.Number("the narrowest the carriageway gets", 2.0 * narrowestHalfM, "m");
  say.Number("the widest", 2.0 * widestHalfM, "m");
  say.Number("the car's own width", carWidthM, "m");
  out.Made.NarrowestHalfM = narrowestHalfM;

  long undeclared = 0;
  double gentlestLimit = 1.0;
  double gentlestAtM = 0.0;
  for (size_t post = 0; post < gradeLimit.size(); ++post) {
    if (!(gradeLimit[post] > 0.0)) {
      ++undeclared;
      continue;
    }
    if (gradeLimit[post] < gentlestLimit) {
      gentlestLimit = gradeLimit[post];
      gentlestAtM = static_cast<double>(post) * spanM;
    }
  }
  say.Number("stations whose road kind declares no maximum grade",
             static_cast<double>(undeclared),
             "stations");
  out.Made.GradelessKinds = undeclared;
  say.Number(
      "the gentlest grade any road class on this route declares", gentlestLimit * 100.0, "%");
  const double weightN = stood.Envelope.MassKg * stood.Envelope.GravityMs2;
  const double fromRest = stood.Envelope.DriveN / weightN;
  const double topMs = stood.Envelope.TopMs();
  const double dragAtTopN = std::isfinite(topMs) ? 0.5 * stood.Envelope.AirDensity *
                                                       stood.Envelope.DragArea * topMs * topMs
                                                 : 0.0;
  say.Number("the steepest the standing rig could climb from rest", fromRest * 100.0, "%");
  say.Number("the steepest it could hold at its own top speed",
             (stood.Envelope.DriveN - dragAtTopN) / weightN * 100.0,
             "%");
  say.Number("where that is", gentlestAtM / kMPerKm, "km");

  long shaped = 0;
  int shapingPasses = 0;
  for (int sweep = 0; sweep < 8; ++sweep) {
    ++shapingPasses;
    long moved = 0;
    for (size_t post = 1; post < roadM.size(); ++post) {
      const double most =
          (gradeLimit[post] < gradeLimit[post - 1] ? gradeLimit[post] : gradeLimit[post - 1]) *
          spanM;
      const double ceiling = roadM[post - 1] + most;
      const double floorM = roadM[post - 1] - most;
      if (roadM[post] > ceiling) {
        roadM[post] = ceiling;
        ++moved;
      }
      if (roadM[post] < floorM) {
        roadM[post] = floorM;
        ++moved;
      }
    }
    for (size_t post = roadM.size() - 1; post > 0; --post) {
      const double most =
          (gradeLimit[post] < gradeLimit[post - 1] ? gradeLimit[post] : gradeLimit[post - 1]) *
          spanM;
      const double ceiling = roadM[post] + most;
      const double floorM = roadM[post] - most;
      if (roadM[post - 1] > ceiling) {
        roadM[post - 1] = ceiling;
        ++moved;
      }
      if (roadM[post - 1] < floorM) {
        roadM[post - 1] = floorM;
        ++moved;
      }
    }
    if (moved == 0) { break; }
    shaped = moved;
  }
  say.Number("sweeps the shaping needed", static_cast<double>(shapingPasses), "sweeps");

  double cutM = 0.0;
  double fillM = 0.0;
  double cutAtM = 0.0;
  double fillAtM = 0.0;
  double movedM = 0.0;
  for (size_t post = 0; post < roadM.size(); ++post) {
    const double byM = roadM[post] - heightM[post];
    movedM += std::fabs(byM);
    if (byM < cutM) {
      cutM = byM;
      cutAtM = static_cast<double>(post) * spanM;
    }
    if (byM > fillM) {
      fillM = byM;
      fillAtM = static_cast<double>(post) * spanM;
    }
  }
  say.Number("the deepest the road cuts into the ground", -cutM, "m");
  say.Number("where that is", cutAtM / kMPerKm, "km");
  say.Number("the highest it fills above it", fillM, "m");
  say.Number("where that is", fillAtM / kMPerKm, "km");
  say.Number("the mean earth moved per station", movedM / static_cast<double>(roadM.size()), "m");
  say.Number("stations still being shaped when the passes ran out",
             static_cast<double>(shaped),
             "stations");

  outshine::Envelope planning = stood.Envelope;
  holdWithinM = 0.5 * narrowestLaneM - 0.5 * carWidthM;
  planning.ReserveMs2 = 2.0 * holdWithinM / (1.0 * 1.0);
  out.ReserveMs2 = planning.ReserveMs2;
  const double floorRatio = 1.409 / 0.477;
  planning.HoldWithinM = holdWithinM / floorRatio;
  say.Number("what the negative control measured the closed loop to cost over the first-order lag",
             floorRatio,
             "x");
  say.Number("so the budget the plan is given", planning.HoldWithinM, "m");
  planning.SettleS = outshine::Pilot::kSettleS;
  planning.CorneringNPerRad =
      car.Contacts.empty() ? 0.0 : car.Contacts.front().Touches.CorneringNPerRad;
  say.Number("what the car leaves either side of itself there", holdWithinM, "m");
  say.Number("the lateral acceleration reserved for holding the line", planning.ReserveMs2, "m/s2");
  say.Number("what is left for the path", planning.HoldingMs2(), "m/s2");

  std::vector<outshine::Knot> rise;
  rise.reserve(roadM.size());
  double worstGradeM = 0.0;
  double worstGradeAtM = 0.0;
  for (size_t post = 0; post < roadM.size(); ++post) {
    const double atM = static_cast<double>(post) * spanM;
    const size_t before = post > 0 ? post - 1 : post;
    const size_t after = post + 1 < roadM.size() ? post + 1 : post;
    const double overM = (static_cast<double>(after) - static_cast<double>(before)) * spanM;
    const double slope = overM > 0.0 ? (roadM[after] - roadM[before]) / overM : 0.0;
    if (std::fabs(slope) > std::fabs(worstGradeM)) {
      worstGradeM = slope;
      worstGradeAtM = atM;
    }
    rise.push_back(outshine::Knot{.AlongM = atM, .Value = roadM[post], .RatePerM = slope});
  }
  const bool rose = corridor.Rise(rise, error);
  if (!rose) { say.Refuse(Line("%s", error.c_str())); }
  say.Number("height knots fastened to the corridor", static_cast<double>(rise.size()), "knots");
  say.Number("the steepest gradient anywhere on it", worstGradeM, "m/m");
  say.Number("as a percentage", worstGradeM * 100.0, "%");
  say.Number("where that is", worstGradeAtM / kMPerKm, "km");

  const double climbLimit =
      stood.Envelope.DriveN / (stood.Envelope.MassKg * stood.Envelope.GravityMs2);
  say.Number("the steepest the standing rig's drivetrain can climb", climbLimit * 100.0, "%");
  out.Made.Rose = rose;
  out.Made.WorstGradeM = worstGradeM;
  out.Made.ClimbLimit = climbLimit;
  if (!rose) {
    say.Refuse(Line("the corridor carries no ground: %s", error.c_str()));
    return false;
  }
  if (std::fabs(worstGradeM) >= climbLimit) {
    say.Refuse("the corridor climbs steeper than the drivetrain can pull");
    return false;
  }

  const double shortestCornerM = 1.5 * tightestM * 0.1;
  const double profileStepM = 0.5 * shortestCornerM;
  say.Number("the shortest corner the fit can produce", shortestCornerM, "m");
  say.Number("the step the speed profile is sampled at", profileStepM, "m");
  if (!profile.Over(corridor, planning, profileStepM, 0.0, error)) {
    say.Refuse(Line("no speed profile solves over the corridor: %s", error.c_str()));
    return false;
  }

  const double meanMs = profile.Quantile(0.5);
  say.Number(
      "the speed the plan holds at p01", profile.Quantile(kNearestQuantile) * kMsToKmh, "km/h");
  say.Number("at p50", meanMs * kMsToKmh, "km/h");
  say.Number("at p95", profile.Quantile(kBroadQuantile) * kMsToKmh, "km/h");
  say.Number("at p99", profile.Quantile(kWidestQuantile) * kMsToKmh, "km/h");
  say.Number("stations the plan holds under 30 km/h",
             static_cast<double>(profile.StationsUnder(kSlowStationKmh / kMsToKmh)),
             "stations");
  say.Number("stations in all", static_cast<double>(profile.SampleCount()), "stations");
  for (size_t term = 0; term < static_cast<size_t>(SpeedProfile::Held::kCount); ++term) {
    const auto which = static_cast<SpeedProfile::Held>(term);
    say.Number(
        SpeedProfile::NameOf(which), static_cast<double>(profile.BoundBy(which)), "stations");
  }
  const SpeedProfile::Bound bound = profile.SlowestBound();
  say.Number("the slowest station the road itself holds", bound.Ms * kMsToKmh, "km/h");
  say.Number("where that station is", bound.AtM / kMPerKm, "km");
  say.Number(SpeedProfile::NameOf(bound.By), bound.Ms * kMsToKmh, "km/h at that station");
  say.Number("the drive time that implies",
             fitted.LengthM / (meanMs > 0.0 ? meanMs : 1.0) / kSPerHour,
             "h");

  return true;
}

} // namespace outshine::Sim
