#include "FBMissionFile.h"
#include <sstream>

namespace FlightBox {

namespace {

std::string Trim(const std::string &s) {
  size_t a = s.find_first_not_of(" \t\r");
  if (a == std::string::npos) return "";
  size_t b = s.find_last_not_of(" \t\r");
  return s.substr(a, b - a + 1);
}

} // namespace

bool FBParseMissionFile(const std::string &text, FBMission &out, std::string *err) {
  out = FBMission{};
  int lineNo = 0;
  bool haveTimeout = false;
  auto fail = [&](const std::string &msg) {
    if (err) *err = "line " + std::to_string(lineNo) + ": " + msg;
    return false;
  };

  std::istringstream lines(text);
  std::string raw;
  while (std::getline(lines, raw)) {
    lineNo++;
    std::string line = raw;
    size_t hash = line.find('#');
    if (hash != std::string::npos) line = line.substr(0, hash);
    line = Trim(line);
    if (line.empty()) continue;

    std::istringstream ls(line);
    std::string kw;
    ls >> kw;

    if (kw == "name") {
      std::string rest;
      std::getline(ls, rest);
      out.Name = Trim(rest);
      if (out.Name.empty()) return fail("'name' needs a value");
    } else if (kw == "runway") {
      double lat, lon, elev, hdg, len;
      if (!(ls >> lat >> lon >> elev >> hdg >> len)) return fail("'runway' needs lat lon elevM hdgDeg lengthM");
      out.Runway = FBRunway{lat, lon, elev, hdg, len, 0.0};
      out.HaveRunway = true;
    } else if (kw == "takeoff") {
      if (!out.HaveRunway) return fail("'takeoff' before 'runway'");
      out.Plan.AddWaypoint(FBWaypoint{out.Runway.ThresholdLatDeg, out.Runway.ThresholdLonDeg,
                                      out.Runway.ThresholdElevM, 0.0, FBWaypointType::Takeoff});
    } else if (kw == "wp") {
      double lat, lon, alt, spd;
      if (!(ls >> lat >> lon >> alt >> spd)) return fail("'wp' needs lat lon altM speedKt");
      out.Plan.AddWaypoint(FBWaypoint{lat, lon, alt, spd, FBWaypointType::Enroute});
    } else if (kw == "land") {
      if (!out.HaveRunway) return fail("'land' before 'runway'");
      out.Plan.AddWaypoint(FBWaypoint{out.Runway.ThresholdLatDeg, out.Runway.ThresholdLonDeg,
                                      out.Runway.ThresholdElevM, 0.0, FBWaypointType::Land});
    } else if (kw == "timeout") {
      double t;
      if (!(ls >> t) || t <= 0.0) return fail("'timeout' needs a positive seconds value");
      out.TimeoutS = t;
      haveTimeout = true;
    } else {
      return fail("unknown keyword '" + kw + "'");
    }
  }

  if (out.Name.empty()) return fail("mission has no 'name'");
  if (!out.HaveRunway) return fail("mission has no 'runway'");
  if (!haveTimeout) return fail("mission has no 'timeout'");
  return true;
}

} // namespace FlightBox
