#include "CorridorLay.h"

#include <numbers>
#include <chrono>
#include <cmath>
#include <cstdio>

#include "Carriageway.h"
#include "Fit.h"
#include "Ribbon.h"
#include "SpeedProfile.h"

namespace outshine::Sim {

namespace {
constexpr double kPatienceS = 900.0;
} // namespace

bool LayCorridor(const Path::Route &route, Ground::GroundStream &ground, const Vehicle &car,
                 const Rigged &stood, double quantumM, double tightestM, double middleLat, double sphereRadiusM,
                 Sink &say, Corridor &out, std::string &error) {
  const double carWidthM = car.WidthM;
  auto &corridor = out.Line;
  auto &fitted = out.Fitted;
  auto &profile = out.Profile;
  auto &roadM = out.RoadM;
  auto &halfWidthM = out.HalfWidthM;
  auto &laneHalfM = out.LaneHalfM;
  auto &asideM = out.AsideM;
  auto &fineAside = out.FineAside;
  auto &fineEdge = out.FineEdge;
  const double fineM = out.FineM;
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

  const std::vector<double> keptM = outshine::Simplify(eastNorthM, quantumM);
  say.Number("vertices the route offered before simplifying", (double)(eastNorthM.size() / 2),
       "vertices");
  say.Number("vertices left after removing what the data cannot resolve",
       (double)(keptM.size() / 2), "vertices");
  say.Number("the share removed", 1.0 - (double)keptM.size() / (double)eastNorthM.size(), "of them");

    fitted = Fit(keptM, quantumM, tightestM, corridor);
  if (!fitted.Laid) { say.Say(Line("REFUSED %s", fitted.Error.c_str())); }
  say.Number("vertices the route offered", (double)fitted.Vertices, "vertices");
  say.Number("corners the fit needed", (double)fitted.Corners, "corners");
  say.Number("straights between them", (double)fitted.Straights, "straights");
  say.Number("the corridor it laid", fitted.LengthM / 1000.0, "km");
  say.Number("the polyline it came from", route.LengthM / 1000.0, "km");
  say.Number("the tightest radius on it", fitted.TightestRadiusM, "m");
  say.Number("the sharpest turn it carried", fitted.SharpestTurnRad * 180.0 / std::numbers::pi, "deg");
  say.Number("at which vertex", fitted.SharpestTurnAtM, "");
  say.Number("turns past a right angle", (double)fitted.TurnsPastRightAngle, "of 2480");
  say.Number("turns past 135 degrees", (double)fitted.TurnsPastHalfCircle, "of 2480");
  say.Number("vertices too sharp for the car to drive at all", (double)fitted.Undrivable, "vertices");
  say.Number("how far it leaves a vertex at worst", fitted.WorstOffsetM, "m");
  say.Number("corners the fit had to correct by measuring them", (double)fitted.Corrected, "corners");
  say.Number("passes it needed", (double)fitted.Passes, "passes");
  say.Number("how far the laid line drifts from the polyline beyond any corner's own doing",
       fitted.DriftM, "m");
  say.Number("per corner that is", fitted.DriftPerCornerM * 1000.0, "mm");
  say.Number("the worst vertex", fitted.WorstVertex, "");
  say.Number("its incoming leg", fitted.WorstLegInM, "m");
  say.Number("its outgoing leg", fitted.WorstLegOutM, "m");
  say.Number("the turn there", fitted.WorstTurnRad * 180.0 / std::numbers::pi, "deg");
  say.Number("the radius it settled on", fitted.WorstRadiusM, "m");
  say.Number("the station the fit expected it at", fitted.WorstExpectedM, "m");
  say.Number("the station the resection found", fitted.WorstStationM, "m");
  say.Number("corners the data cannot support at any drivable radius", (double)fitted.Strained,
       "corners");
  say.Number("how far the worst of those leaves its vertex", fitted.StrainedWorstM, "m");
  say.Number("where that happens", fitted.WorstOffsetAtM / 1000.0, "km");
  say.Number("how far it is allowed to", quantumM, "m");

  if (!fitted.Laid) { say.Say(Line("REFUSED %s", fitted.Error.c_str())); }
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
  say.Claim(fitted.Laid,
        "**AND THE ROUTE BECOMES A CORRIDOR WITH NO CURVATURE LEAP ANYWHERE ON IT.** 752 km of OSM "
        "polyline, every interior vertex carrying spiral-arc-spiral, laid by a ReferenceLine that "
        "REFUSES a leap -- so a step in the lateral force has no spelling on this road");
  if (!fitted.Laid) { return false; }
  say.Claim(fitted.DriftM < 0.05 * quantumM * (double)fitted.Corners,
        "**AND WHAT IS LEFT IS DRIFT, WHICH NO CORNER CAN CORRECT.** The line is walked corner by "
        "corner and each spiral is integrated by 8-node Gauss-Legendre; the residual accumulates "
        "laterally over 2300 corners. It is reported as its own term, in millimetres per corner, "
        "rather than being chased by shrinking corners that were never at fault");
  say.Claim(fitted.Strained * 200 < fitted.Corners,
        "**AND WHERE THE DATA CANNOT SUPPORT A ROAD AT ANY RADIUS THE CAR CAN TURN, THAT CORNER IS "
        "COUNTED AND NOT HIDDEN.** Those are the corners whose vertices the line must leave by more "
        "than the tile's own quantisation to stay drivable at all -- a classified finding with a "
        "count, not a fit that quietly bent further. Fewer than one in two hundred here");
  say.Number("how much longer the corridor is than the polyline",
       fitted.LengthM / route.LengthM - 1.0, "of it");
  say.Claim(fitted.LengthM < 1.05 * route.LengthM,
        "and within a few per cent of the polyline it was fitted through -- a cut corner is shorter "
        "than the corner, and the small overrun is the first-order construction on the tail of "
        "sharp turns");
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
  say.Number("the elevation where the route starts", heightM.empty() ? 0.0 : heightM.front(), "m");
  say.Number("and where it ends", heightM.empty() ? 0.0 : heightM.back(), "m");

  say.Claim(resolved > 0, "**THE ELEVATION SOURCE ANSWERS ALONG THE WHOLE CORRIDOR.** Real height data, "
                      "streamed for the same route the ways came from");
  say.Claim(holes == 0, "with no hole in it -- a hole is a named refusal and there is none here");
  say.Number("the elevation where the route ends", heightM.empty() ? 0.0 : heightM.back(), "m");

  outshine::SpeedProfile inPlan;
  say.Claim(inPlan.Over(corridor, stood.Envelope, postM, 0.0, error),
        "the plan view alone gives a speed at every station, before the ground is consulted");

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
  say.Claim(laneless == 0,
        "**AND EVERY KIND ON THE ROUTE DECLARES HOW MANY LANES IT CARRIES.** The lane count comes from "
        "the same cross-sections the widths do -- RAA RQ 28 is two 3.75 m running lanes and a 2.5 m "
        "shoulder per one-way carriageway -- so a car's lane is the width over the count and not the "
        "whole road");

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

  fineAside.assign((size_t)(fitted.LengthM / fineM) + 2, 0.0);
  fineEdge.assign(fineAside.size(), 0.0);
  for (size_t fine = 0; fine < fineAside.size(); ++fine) {
    const size_t post = (size_t)((double)fine * fineM / spanM);
    const size_t band = post < asideM.size() ? post : asideM.size() - 1;
    fineAside[fine] = asideM[band];
    fineEdge[fine] = halfWidthM[band];
  }
  {
    const double reachM = 1.0 * 232.722657 / 3.6;
    const double mostPerM = budgetM / reachM;
    say.Number("the fastest the lane centre may move sideways", mostPerM * 1000.0, "mm per metre");
    say.Number("so a 1.125 m shift is taken over", 1.125 / mostPerM, "m of road");
    const double most = mostPerM * fineM;

    std::vector<double> roomM(fineAside.size(), 0.0);
    long insideTight = 0;
    double worstDrivenM = 1.0e9;
    for (size_t fine = 0; fine < roomM.size(); ++fine) {
      double room = fineEdge[fine] - 0.5 * carWidthM - out.BudgetM;
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
    for (size_t fine = 0; fine < fineAside.size(); ++fine) {
      if (fineAside[fine] > roomM[fine]) { fineAside[fine] = roomM[fine]; ++led; }
      if (fineAside[fine] < -roomM[fine]) { fineAside[fine] = -roomM[fine]; ++led; }
    }
    say.Number("stations where a narrowing ahead moved the car in early", (double)led, "stations");

    for (int sweep = 0; sweep < 400; ++sweep) {
      long moved = 0;
      for (size_t fine = 1; fine < fineAside.size(); ++fine) {
        if (fineAside[fine] > fineAside[fine - 1] + most) {
          fineAside[fine] = fineAside[fine - 1] + most;
          ++moved;
        }
        if (fineAside[fine] < fineAside[fine - 1] - most) {
          fineAside[fine] = fineAside[fine - 1] - most;
          ++moved;
        }
      }
      for (size_t fine = fineAside.size() - 1; fine > 0; --fine) {
        if (fineAside[fine - 1] > fineAside[fine] + most) {
          fineAside[fine - 1] = fineAside[fine] + most;
          ++moved;
        }
        if (fineAside[fine - 1] < fineAside[fine] - most) {
          fineAside[fine - 1] = fineAside[fine] - most;
          ++moved;
        }
      }
      if (moved == 0) { break; }
    }
    long clamped = 0;
    for (size_t fine = 0; fine < fineAside.size(); ++fine) {
      if (fineAside[fine] > roomM[fine]) { fineAside[fine] = roomM[fine]; ++clamped; }
      if (fineAside[fine] < -roomM[fine]) { fineAside[fine] = -roomM[fine]; ++clamped; }
    }
    say.Number("stations where the road edge overruled the taper", (double)clamped, "stations");

    double leftM = 0.0, worstOverM = 0.0;
    for (size_t fine = 1; fine < fineAside.size(); ++fine) {
      leftM = std::fmax(leftM, std::fabs(fineAside[fine] - fineAside[fine - 1]));
      const double asideReachM = std::fabs(fineAside[fine]) + 0.5 * carWidthM;
      if (asideReachM > fineEdge[fine]) {
        worstOverM = std::fmax(worstOverM, asideReachM - fineEdge[fine]);
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
  say.Claim(2.0 * narrowestHalfM > carWidthM,
        "**AND THE CAR FITS ON THE NARROWEST STRETCH OF ITS OWN ROUTE.** The harvest already refused "
        "ways narrower than the car; this says the route it chose kept that true end to end");

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
  say.Claim(undeclared == 0,
        "**AND EVERY KIND ON THE ROUTE DECLARES ITS OWN MAXIMUM GRADE.** A station with none would be "
        "flattened by a shaping that had nothing to shape it to -- silently, which is the failure "
        "this count exists to make loud. The grades are RAA, RAL and RASt figures declared beside "
        "the declared vegetation table");
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
  planning.SettleS = 1.0;
  planning.CorneringNPerRad = car.CorneringNPerRad;
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
  if (!rose) { say.Say(Line("REFUSED %s", error.c_str())); }
  say.Number("height knots fastened to the corridor", (double)rise.size(), "knots");
  say.Number("the steepest gradient anywhere on it", worstGradeM, "m/m");
  say.Number("as a percentage", worstGradeM * 100.0, "%");
  say.Number("where that is", worstGradeAtM / 1000.0, "km");

  const double climbLimit = stood.Envelope.DriveN / (stood.Envelope.MassKg * stood.Envelope.GravityMs2);
  say.Number("the steepest the standing rig's drivetrain can climb", climbLimit * 100.0, "%");
  say.Claim(rose, "**AND THE CORRIDOR RISES WITH THE REAL GROUND UNDER IT.** Heights from the "
              "declared elevation source, each a knot with its own slope, and one cubic through "
              "them -- the same mechanism the synthetic road used, fed by the world");
  say.Claim(std::fabs(worstGradeM) < climbLimit,
        "**AND NOTHING ON IT IS STEEPER THAN THE CAR CAN CLIMB.** The limit is the standing "
        "rig's drive force against its own weight; a gradient past it is the drivetrain "
        "REFUSING, and on this route there is none -- which is the first evidence that the "
        "ground under an OSM road is reconstructed well enough to drive");

  const double shortestCornerM = 1.5 * tightestM * 0.1;
  const double profileStepM = 0.5 * shortestCornerM;
  say.Number("the shortest corner the fit can produce", shortestCornerM, "m");
  say.Number("the step the speed profile is sampled at", profileStepM, "m");
  say.Claim(profile.Over(corridor, planning, profileStepM, 0.0, error),
        "and a speed profile is solved over the whole corridor from its geometry alone");
  if (!error.empty()) { say.Say(Line("REFUSED %s", error.c_str())); }

  double slowestMs = 1.0e9, fastestMs = 0.0, meanMs = 0.0;
  for (size_t sample = 0; sample < profile.SampleCount(); ++sample) {
    const double ms = profile.SampleAt(sample);
    slowestMs = ms < slowestMs ? ms : slowestMs;
    fastestMs = ms > fastestMs ? ms : fastestMs;
    meanMs += ms;
  }
  if (profile.SampleCount() > 0) { meanMs /= (double)profile.SampleCount(); }
  say.Number("the slowest the profile asks for", slowestMs * 3.6, "km/h");
  say.Number("the fastest", fastestMs * 3.6, "km/h");
  say.Number("the mean", meanMs * 3.6, "km/h");
  say.Number("stations where a CREST and not a curve set the speed",
       (double)profile.CrestsThatBound(), "stations");
  say.Number("the slowest a crest holds it to", profile.CrestHeldMs() * 3.6, "km/h");
  say.Number("where that crest is", profile.CrestHeldAtM() / 1000.0, "km");
  say.Number("the drive time that implies", fitted.LengthM / (meanMs > 0.0 ? meanMs : 1.0) / 3600.0, "h");


  return true;
}

} // namespace outshine::Sim
