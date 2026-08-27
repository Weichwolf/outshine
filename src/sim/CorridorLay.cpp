#include "CorridorLay.h"

#include "Pilot.h"

#include <numbers>
#include <chrono>
#include <cmath>

#include "Alignment.h"
#include "Angle.h"
#include <cstdio>

#include "Carriageway.h"
#include "Fit.h"
#include "Ribbon.h"
#include "SpeedProfile.h"

namespace outshine::Sim {


namespace {
constexpr double kPatienceS = 900.0;
}

bool LayCorridor(const Path::Route &route, const GroundQuery &ground, const Body &car,
                 const Rigged &stood, double quantumM, double tightestM, double middleLat, double sphereRadiusM,
                 Sink &say, Corridor &out, std::string &error) {
  const double carWidthM = car.WidthM;
  auto &corridor = out.Line;
  auto &fitted = out.Fitted;
  auto &profile = out.Profile;
  std::vector<double> roadM, halfWidthM, laneHalfM, asideM, frictionM;
  auto &stations = out.Fine;
  constexpr double fineM = Corridor::kFineM;
  auto &spanM = out.SpanM;
  auto &narrowestLaneM = out.NarrowestLaneM;
  auto &budgetM = out.BudgetM;
  auto &holdWithinM = out.HoldWithinM;

  const double frameLat = route.Legs.front().At.LatDeg;
  const double frameLon = route.Legs.front().At.LonDeg;
  const double perLatM = Path::ApartM(frameLat, frameLon, frameLat + 1.0, frameLon, sphereRadiusM);
  const double perLonM = Path::ApartM(frameLat, frameLon, frameLat, frameLon + 1.0, sphereRadiusM);
  out.FrameLat = frameLat;
  out.FrameLon = frameLon;
  out.PerLatM = perLatM;
  out.PerLonM = perLonM;
  std::vector<double> eastNorthM;
  eastNorthM.reserve(route.Legs.size() * 2);
  for (const auto &leg : route.Legs) {
    eastNorthM.push_back((leg.At.LonDeg - frameLon) * perLonM);
    eastNorthM.push_back((leg.At.LatDeg - frameLat) * perLatM);
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
    classTightestM[at] = mine <= 0.0 || before <= 0.0 ? 0.0 : (mine < before ? mine : before);
    if (classTightestM[at] > 0.0) { ++designed; }
    const double half = route.Legs[keptAt[at]].HalfWidthM;
    if (half > roadWithinM) { roadWithinM = half; }
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
    if (!(turn > 1.0e-9) || turn >= std::numbers::pi - 1.0e-9) { continue; }
    const double before = route.Legs[keptAt[at - 1]].HalfWidthM;
    const double other = before > 0.0 ? before : half;
    const double intoM = std::sqrt(eastIn * eastIn + northIn * northIn);
    const double outOfM = std::sqrt(eastOut * eastOut + northOut * northOut);
    const double heldM =
        JunctionKerbM(half, other, turn, intoM < outOfM ? intoM : outOfM);
    if (heldM > withinAtM[at]) { withinAtM[at] = heldM; }
    if (withinAtM[at] > widestJunctionM) { widestJunctionM = withinAtM[at]; }
  }
  say.Number("the widest a junction lets an arc leave its corner", widestJunctionM, "m");
  say.Number("vertices the route offered before simplifying", (double)(eastNorthM.size() / 2),
       "vertices");
  say.Number("vertices left after removing what the data cannot resolve",
       (double)(keptM.size() / 2), "vertices");
  say.Number("the share removed", 1.0 - (double)keptM.size() / (double)eastNorthM.size(), "of them");

  say.Number("how far the built road may leave the polyline, being its own half width",
             roadWithinM, "m");
  fitted = Fit(keptM, roadWithinM, tightestM, classTightestM, corridor, withinAtM);
  if (!fitted.Laid) { say.Refuse(Line("%s", fitted.Error.c_str())); }
  say.Number("vertices the route offered", (double)fitted.Vertices, "vertices");
  say.Number("corners the fit needed", (double)fitted.Corners, "corners");
  say.Number("runs of same-sign turns among them", (double)fitted.Runs, "runs");
  say.Number("the longest such run", (double)fitted.LongestRunVertices, "vertices");
  say.Number("vertices with a same-sign turn on BOTH sides", (double)fitted.SheltredVertices,
       "vertices");
  say.Number("straights between them", (double)fitted.Straights, "straights");
  say.Number("the corridor it laid", fitted.LengthM / 1000.0, "km");
  say.Number("the polyline it came from", route.LengthM / 1000.0, "km");
  say.Number("the tightest radius on it", fitted.TightestRadiusM, "m");
  say.Number("at which vertex that is", (double)fitted.TightestAtVertex,
       Line("of %s", std::to_string(fitted.Vertices).c_str()).c_str());
  say.Number("the tightest radius any vertex DEMANDED, drivable or not", fitted.TightestDemandedM,
       "m");
  say.Number("vertices whose road class declares a design minimum radius", (double)designed,
       Line("of %s", std::to_string(keptAt.size()).c_str()).c_str());
  size_t declaredLegs = 0;
  for (const auto &leg : route.Legs) {
    if (leg.MinRadiusM > 0.0) { ++declaredLegs; }
  }
  say.Number("legs of the route whose road class declares one", (double)declaredLegs,
       Line("of %s", std::to_string(route.Legs.size()).c_str()).c_str());
  say.Number("corners the fit laid tighter than their class allows", (double)fitted.UnderClass,
       "corners");
  if (fitted.UnderClass > 0) {
    say.Number("the worst of them", fitted.UnderClassRadiusM, "m");
    say.Number("where its class allows", fitted.UnderClassMinimumM, "m");
    say.Number("that vertex", (double)fitted.UnderClassAtVertex, "");
  }
  say.Number("the sharpest turn it carried", fitted.SharpestTurnRad * 180.0 / std::numbers::pi, "deg");
  say.Number("at which vertex", fitted.SharpestTurnAtM, "");
  say.Number("turns past a right angle", (double)fitted.TurnsPastRightAngle, "of 2480");
  say.Number("turns past 135 degrees", (double)fitted.TurnsPastHalfCircle, "of 2480");
  say.Number("vertices too sharp for the car to drive at all", (double)fitted.Undrivable, "vertices");
  say.Number("how far it leaves a vertex at worst", fitted.WorstOffsetM, "m");
  say.Number("bends the accuracy bound had to split", (double)fitted.SplitByAccuracy, "bends");
  say.Number("how far the laid line drifts from the polyline beyond any corner's own doing",
       fitted.DriftM, "m");
  say.Number("per corner that is", fitted.DriftPerCornerM * 1000.0, "mm");
  say.Number("the worst vertex", fitted.WorstVertex, "");
  say.Number("its incoming leg", fitted.WorstLegInM, "m");
  say.Number("its outgoing leg", fitted.WorstLegOutM, "m");
  say.Number("the turn there", fitted.WorstTurnRad * 180.0 / std::numbers::pi, "deg");
  say.Number("the station the resection found", fitted.WorstStationM, "m");
  say.Number("where that happens", fitted.WorstOffsetAtM / 1000.0, "km");
  say.Number("how far it is allowed to", quantumM, "m");

  if (!fitted.Laid) { say.Refuse(Line("%s", fitted.Error.c_str())); }
  if (!fitted.Laid && fitted.Undrivable > 0) {
    const size_t at = (size_t)fitted.UndrivableAtM;
    for (size_t which = at > 1 ? at - 1 : 0; which <= at + 1 && which < route.Legs.size(); ++which) {
      char atLine[96];
      std::snprintf(atLine, sizeof atLine, "AT %zu  %.7f %.7f", which,
                    route.Legs[which].At.LatDeg, route.Legs[which].At.LonDeg);
      say.Say(atLine);
    }
    if (at >= 1 && at + 1 < route.Legs.size()) {
      char legs[96];
      std::snprintf(legs, sizeof legs, "LEGS in %.2f m  out %.2f m", Path::ApartM(route.Legs[at - 1].At.LatDeg, route.Legs[at - 1].At.LonDeg,
                         route.Legs[at].At.LatDeg, route.Legs[at].At.LonDeg, sphereRadiusM),
                  Path::ApartM(route.Legs[at].At.LatDeg, route.Legs[at].At.LonDeg,
                         route.Legs[at + 1].At.LatDeg, route.Legs[at + 1].At.LonDeg, sphereRadiusM));
      say.Say(legs);
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
       fitted.LengthM / route.LengthM - 1.0, "of it");
  say.Number("the speed the tightest radius allows at 0.95 g",
       std::sqrt(0.95 * stood.Envelope.GravityMs2 * fitted.TightestRadiusM) * 3.6, "km/h");

  const double postM = ground.PostM(middleLat);
  const long posts = (long)std::ceil(fitted.LengthM / postM);
  say.Number("the elevation source's own post spacing here", postM, "m");
  say.Number("stations the corridor is sampled at", (double)(posts + 1), "stations");

  std::vector<double> heightM((size_t)posts + 1, 0.0);
  std::vector<bool> known((size_t)posts + 1, false);
  long holes = 0, waited = 0;
  const auto sampling = std::chrono::steady_clock::now();
  for (long post = 0; post <= posts; ++post) {
    const double atM = (double)post * fitted.LengthM / (double)posts;
    outshine::Placed on;
    if (!corridor.At(atM, on)) { continue; }
    const double latDeg = frameLat + on.NorthM / perLatM;
    const double lonDeg = frameLon + on.EastM / perLonM;
    for (;;) {
      const outshine::GroundSample sample = ground.At(latDeg, lonDeg);
      double aslM = 0.0;
      if (sample.TryAslM(&aslM)) {
        heightM[(size_t)post] = aslM;
        known[(size_t)post] = true;
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
  double lowestM = 0.0, highestM = 0.0;
  bool haveAny = false;
  for (size_t post = 0; post < heightM.size(); ++post) {
    if (!known[post]) { continue; }
    ++resolved;
    if (!haveAny || heightM[post] < lowestM) { lowestM = heightM[post]; }
    if (!haveAny || heightM[post] > highestM) { highestM = heightM[post]; }
    haveAny = true;
  }
  say.Number("stations the elevation source answered", (double)resolved, "stations");
  say.Number("stations it said were a hole", (double)holes, "stations");
  say.Number("times a sample had to wait for a tile", (double)waited, "waits");
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

  spanM = fitted.LengthM / (double)posts;
  roadM = heightM;
  std::vector<double> gradeLimit(roadM.size(), 0.0);
  {
    size_t leg = 0;
    for (size_t post = 0; post < roadM.size(); ++post) {
      const double atM = (double)post * spanM * route.LengthM / fitted.LengthM;
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
      const double atM = (double)post * spanM * route.LengthM / fitted.LengthM;
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
      const double atM = (double)post * spanM * route.LengthM / fitted.LengthM;
      while (leg + 1 < route.Legs.size() && route.Legs[leg + 1].AlongM < atM) { ++leg; }
      const int lanes = route.Legs[leg].Lanes;
      if (lanes <= 0) {
        ++laneless;
        laneHalfM[post] = halfWidthM[post];
        continue;
      }
      const double laneM = 2.0 * halfWidthM[post] / (double)lanes;
      laneHalfM[post] = 0.5 * laneM;
      asideM[post] = -0.5 * (double)(lanes - 1) * laneM;
    }
  }
  say.Number("stations whose road kind declares no lane count", (double)laneless, "stations");
  out.Made.LanelessKinds = laneless;

  double steppedM = 0.0, steppedAtM = 0.0;
  for (size_t post = 1; post < asideM.size(); ++post) {
    const double step = std::fabs(asideM[post] - asideM[post - 1]);
    if (step > steppedM) {
      steppedM = step;
      steppedAtM = (double)post * spanM;
    }
  }
  say.Number("the largest step the lane centre takes where the road changes width", steppedM, "m");
  say.Number("where that is", steppedAtM / 1000.0, "km");

  double narrowestLaneHereM = 1.0e9;
  for (const double half : laneHalfM) {
    narrowestLaneHereM = 2.0 * half < narrowestLaneHereM ? 2.0 * half : narrowestLaneHereM;
  }
  budgetM = 0.5 * narrowestLaneHereM - 0.5 * carWidthM;

  out.Bake(fitted.LengthM);
  for (size_t at = 0; at < stations.size(); ++at) {
    const size_t post = (size_t)((double)at * fineM / spanM);
    const size_t band = post < asideM.size() ? post : asideM.size() - 1;
    stations[at].AsideM = asideM[band];
    stations[at].EdgeM = halfWidthM[band];
    stations[at].LaneHalfM = laneHalfM[band < laneHalfM.size() ? band : laneHalfM.size() - 1];
    stations[at].Friction = frictionM[band];
  }
  {
    long gripless = 0;
    double leastGrip = 1.0e9, mostGrip = 0.0;
    for (const Station &one : stations) {
      if (!(one.Friction > 0.0)) { ++gripless; continue; }
      leastGrip = one.Friction < leastGrip ? one.Friction : leastGrip;
      mostGrip = one.Friction > mostGrip ? one.Friction : mostGrip;
    }
    say.Number("the least grip the route's surface offers", gripless < (long)stations.size() ? leastGrip : 0.0, "x");
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
    say.Number("the fastest the plan view holds", fastestMs * 3.6, "km/h");
    say.Number("the top speed the declaration would allow", stood.Envelope.TopMs() * 3.6, "km/h");
    say.Number("the look-ahead time the pilot settles over", outshine::Pilot::kSettleS, "s");
    say.Number("the reach that buys at the fastest the plan holds", reachM, "m");
    say.Number("the fastest the lane centre may move sideways", mostPerM * 1000.0, "mm per metre");
    say.Number("so a full-budget shift is taken over", budgetM / mostPerM, "m of road");
    const double most = mostPerM * fineM;

    std::vector<double> roomM(stations.size(), 0.0);
    long insideTight = 0;
    double worstDrivenM = 1.0e9;
    for (size_t fine = 0; fine < roomM.size(); ++fine) {
      double room = stations[fine].EdgeM - 0.5 * carWidthM - out.BudgetM;
      outshine::Placed on;
      if (corridor.At((double)fine * fineM, on) && on.CurvaturePerM != 0.0) {
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
         (double)insideTight, "stations");
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
      if (roomM[fine] > reachable) { roomM[fine] = reachable; }
    }
    say.Number("the tracking error the lane centre keeps clear of the edge", budgetM, "m");
    say.Number("the most a narrowing pulled the lane centre in ahead of itself", leadM, "m");
    long led = 0;
    for (size_t fine = 0; fine < stations.size(); ++fine) {
      if (stations[fine].AsideM > roomM[fine]) { stations[fine].AsideM = roomM[fine]; ++led; }
      if (stations[fine].AsideM < -roomM[fine]) { stations[fine].AsideM = -roomM[fine]; ++led; }
    }
    say.Number("stations where a narrowing ahead moved the car in early", (double)led, "stations");

    for (int sweep = 0; sweep < 400; ++sweep) {
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
      if (stations[fine].AsideM > roomM[fine]) { stations[fine].AsideM = roomM[fine]; ++clamped; }
      if (stations[fine].AsideM < -roomM[fine]) { stations[fine].AsideM = -roomM[fine]; ++clamped; }
    }
    say.Number("stations where the road edge overruled the taper", (double)clamped, "stations");

    double leftM = 0.0, worstOverM = 0.0;
    for (size_t fine = 1; fine < stations.size(); ++fine) {
      leftM = std::fmax(leftM, std::fabs(stations[fine].AsideM - stations[fine - 1].AsideM));
      const double asideReachM = std::fabs(stations[fine].AsideM) + 0.5 * carWidthM;
      if (asideReachM > stations[fine].EdgeM) {
        worstOverM = std::fmax(worstOverM, asideReachM - stations[fine].EdgeM);
      }
    }
    say.Number("the largest step left after tapering", leftM, "m");
    say.Number("the furthest the tapered lane centre pushes the car past a road edge", worstOverM, "m");
  }

  narrowestLaneM = 1.0e9;
  double widestLaneM = 0.0, mostAsideM = 0.0;
  for (size_t post = 0; post < laneHalfM.size(); ++post) {
    narrowestLaneM = 2.0 * laneHalfM[post] < narrowestLaneM ? 2.0 * laneHalfM[post] : narrowestLaneM;
    widestLaneM = 2.0 * laneHalfM[post] > widestLaneM ? 2.0 * laneHalfM[post] : widestLaneM;
    mostAsideM = std::fabs(asideM[post]) > mostAsideM ? std::fabs(asideM[post]) : mostAsideM;
  }
  say.Number("the narrowest LANE on the route", narrowestLaneM, "m");
  say.Number("the widest lane", widestLaneM, "m");
  say.Number("the furthest the car sits from the centreline", mostAsideM, "m");
  say.Number("what it leaves either side of itself in the narrowest lane",
       0.5 * narrowestLaneM - 0.5 * carWidthM, "m");

  double narrowestHalfM = 1.0e9, widestHalfM = 0.0;
  for (const double half : halfWidthM) {
    narrowestHalfM = half < narrowestHalfM ? half : narrowestHalfM;
    widestHalfM = half > widestHalfM ? half : widestHalfM;
  }
  say.Number("the narrowest the carriageway gets", 2.0 * narrowestHalfM, "m");
  say.Number("the widest", 2.0 * widestHalfM, "m");
  say.Number("the car's own width", carWidthM, "m");
  out.Made.NarrowestHalfM = narrowestHalfM;

  long undeclared = 0;
  double gentlestLimit = 1.0, gentlestAtM = 0.0;
  for (size_t post = 0; post < gradeLimit.size(); ++post) {
    if (!(gradeLimit[post] > 0.0)) {
      ++undeclared;
      continue;
    }
    if (gradeLimit[post] < gentlestLimit) {
      gentlestLimit = gradeLimit[post];
      gentlestAtM = (double)post * spanM;
    }
  }
  say.Number("stations whose road kind declares no maximum grade", (double)undeclared, "stations");
  out.Made.GradelessKinds = undeclared;
  say.Number("the gentlest grade any road class on this route declares", gentlestLimit * 100.0, "%");
  const double weightN = stood.Envelope.MassKg * stood.Envelope.GravityMs2;
  const double fromRest = stood.Envelope.DriveN / weightN;
  const double topMs = stood.Envelope.TopMs();
  const double dragAtTopN = std::isfinite(topMs)
      ? 0.5 * stood.Envelope.AirDensity * stood.Envelope.DragArea * topMs * topMs
      : 0.0;
  say.Number("the steepest the standing rig could climb from rest", fromRest * 100.0, "%");
  say.Number("the steepest it could hold at its own top speed",
       (stood.Envelope.DriveN - dragAtTopN) / weightN * 100.0, "%");
  say.Number("where that is", gentlestAtM / 1000.0, "km");

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
      if (roadM[post] > ceiling) { roadM[post] = ceiling; ++moved; }
      if (roadM[post] < floorM) { roadM[post] = floorM; ++moved; }
    }
    for (size_t post = roadM.size() - 1; post > 0; --post) {
      const double most =
          (gradeLimit[post] < gradeLimit[post - 1] ? gradeLimit[post] : gradeLimit[post - 1]) *
          spanM;
      const double ceiling = roadM[post] + most;
      const double floorM = roadM[post] - most;
      if (roadM[post - 1] > ceiling) { roadM[post - 1] = ceiling; ++moved; }
      if (roadM[post - 1] < floorM) { roadM[post - 1] = floorM; ++moved; }
    }
    if (moved == 0) { break; }
    shaped = moved;
  }
  say.Number("sweeps the shaping needed", (double)shapingPasses, "sweeps");

  double cutM = 0.0, fillM = 0.0, cutAtM = 0.0, fillAtM = 0.0, movedM = 0.0;
  for (size_t post = 0; post < roadM.size(); ++post) {
    const double byM = roadM[post] - heightM[post];
    movedM += std::fabs(byM);
    if (byM < cutM) { cutM = byM; cutAtM = (double)post * spanM; }
    if (byM > fillM) { fillM = byM; fillAtM = (double)post * spanM; }
  }
  say.Number("the deepest the road cuts into the ground", -cutM, "m");
  say.Number("where that is", cutAtM / 1000.0, "km");
  say.Number("the highest it fills above it", fillM, "m");
  say.Number("where that is", fillAtM / 1000.0, "km");
  say.Number("the mean earth moved per station", movedM / (double)roadM.size(), "m");
  say.Number("stations still being shaped when the passes ran out", (double)shaped, "stations");

  outshine::Envelope planning = stood.Envelope;
  holdWithinM = 0.5 * narrowestLaneM - 0.5 * carWidthM;
  planning.ReserveMs2 = 2.0 * holdWithinM / (1.0 * 1.0);
  out.ReserveMs2 = planning.ReserveMs2;
  const double floorRatio = 1.409 / 0.477;
  planning.HoldWithinM = holdWithinM / floorRatio;
  say.Number("what the negative control measured the closed loop to cost over the first-order lag",
       floorRatio, "x");
  say.Number("so the budget the plan is given", planning.HoldWithinM, "m");
  planning.SettleS = outshine::Pilot::kSettleS;
  planning.CorneringNPerRad = car.Contacts.empty() ? 0.0 : car.Contacts.front().Touches.CorneringNPerRad;
  say.Number("what the car leaves either side of itself there", holdWithinM, "m");
  say.Number("the lateral acceleration reserved for holding the line", planning.ReserveMs2, "m/s2");
  say.Number("what is left for the path", planning.HoldingMs2(), "m/s2");

  std::vector<outshine::Knot> rise;
  rise.reserve(roadM.size());
  double worstGradeM = 0.0, worstGradeAtM = 0.0;
  for (size_t post = 0; post < roadM.size(); ++post) {
    const double atM = (double)post * spanM;
    const size_t before = post > 0 ? post - 1 : post;
    const size_t after = post + 1 < roadM.size() ? post + 1 : post;
    const double overM = ((double)after - (double)before) * spanM;
    const double slope = overM > 0.0 ? (roadM[after] - roadM[before]) / overM : 0.0;
    if (std::fabs(slope) > std::fabs(worstGradeM)) {
      worstGradeM = slope;
      worstGradeAtM = atM;
    }
    rise.push_back(outshine::Knot{atM, roadM[post], slope});
  }
  const bool rose = corridor.Rise(rise, error);
  if (!rose) { say.Refuse(Line("%s", error.c_str())); }
  say.Number("height knots fastened to the corridor", (double)rise.size(), "knots");
  say.Number("the steepest gradient anywhere on it", worstGradeM, "m/m");
  say.Number("as a percentage", worstGradeM * 100.0, "%");
  say.Number("where that is", worstGradeAtM / 1000.0, "km");

  const double climbLimit = stood.Envelope.DriveN / (stood.Envelope.MassKg * stood.Envelope.GravityMs2);
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
  say.Number("the speed the plan holds at p01", profile.Quantile(0.01) * 3.6, "km/h");
  say.Number("at p50", meanMs * 3.6, "km/h");
  say.Number("at p95", profile.Quantile(0.95) * 3.6, "km/h");
  say.Number("at p99", profile.Quantile(0.99) * 3.6, "km/h");
  say.Number("stations the plan holds under 30 km/h",
       (double)profile.StationsUnder(30.0 / 3.6), "stations");
  say.Number("stations in all", (double)profile.SampleCount(), "stations");
  for (size_t term = 0; term < (size_t)SpeedProfile::Held::kCount; ++term) {
    const SpeedProfile::Held which = (SpeedProfile::Held)term;
    say.Number(SpeedProfile::NameOf(which), (double)profile.BoundBy(which), "stations");
  }
  const SpeedProfile::Bound bound = profile.SlowestBound();
  say.Number("the slowest station the road itself holds", bound.Ms * 3.6, "km/h");
  say.Number("where that station is", bound.AtM / 1000.0, "km");
  say.Number(SpeedProfile::NameOf(bound.By), bound.Ms * 3.6, "km/h at that station");
  say.Number("the drive time that implies", fitted.LengthM / (meanMs > 0.0 ? meanMs : 1.0) / 3600.0, "h");

  return true;
}

}
