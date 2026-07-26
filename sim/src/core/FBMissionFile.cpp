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
    } else if (kw == "module") {
      std::string rest;
      std::getline(ls, rest);
      out.ModuleName = Trim(rest);
      if (out.ModuleName.empty()) return fail("'module' needs a value (e.g. 'module f16')");
    } else if (kw == "runway") {
      double lat, lon, elev, hdg, len;
      if (!(ls >> lat >> lon >> elev >> hdg >> len)) return fail("'runway' needs lat lon elevM hdgDeg lengthM");
      out.Runway = FBRunway{lat, lon, elev, hdg, len, 0.0};
      out.HaveRunway = true;
    } else if (kw == "spawn") {
      /* spawn <lat lon | threshold> <altM | ground> <hdgDeg> <speedKt> — the unit's declarative IC
       * (doc/mission-format.md): 'threshold' reuses the already-declared runway's lat/lon (a ground-start
       * convenience, not a second position syntax), 'ground' resolves the altitude from terrain + gear
       * clearance at spawn (FBMissionBoot.h), never a separate code path. */
      std::string first;
      if (!(ls >> first)) return fail("'spawn' needs lat|threshold");
      double lat, lon;
      if (first == "threshold") {
        if (!out.HaveRunway) return fail("'spawn threshold' before 'runway'");
        lat = out.Runway.ThresholdLatDeg; lon = out.Runway.ThresholdLonDeg;
      } else {
        try { lat = std::stod(first); } catch (...) { return fail("'spawn' needs a numeric lat or 'threshold'"); }
        if (!(ls >> lon)) return fail("'spawn' needs lon after lat");
      }
      std::string altTok;
      if (!(ls >> altTok)) return fail("'spawn' needs altM or 'ground' after lat/lon");
      bool ground = (altTok == "ground");
      double altM = 0.0;
      if (!ground) {
        try { altM = std::stod(altTok); } catch (...) { return fail("'spawn' altitude must be a number or 'ground'"); }
      }
      double hdg, spd;
      if (!(ls >> hdg >> spd)) return fail("'spawn' needs hdgDeg speedKt");
      out.Spawn = FBSpawn{lat, lon, ground, altM, hdg, spd};
      out.HaveSpawn = true;
    } else if (kw == "wp") {
      double lat, lon, alt, spd;
      if (!(ls >> lat >> lon >> alt >> spd)) return fail("'wp' needs lat lon altM speedKt");
      out.Plan.AddWaypoint(FBWaypoint{lat, lon, alt, spd, FBWaypointType::Enroute});
    } else if (kw == "land") {
      if (!out.HaveRunway) return fail("'land' before 'runway'");
      out.Plan.AddWaypoint(FBWaypoint{out.Runway.ThresholdLatDeg, out.Runway.ThresholdLonDeg,
                                      out.Runway.ThresholdElevM, 0.0, FBWaypointType::Land});
    } else if (kw == "set") {
      /* set <key> <value...> — raw KV data only (doc/mission-format.md); the PARSER never interprets a
       * key, only the module does (FBModule::ApplySetup) once the Runner hands these over in the spawn
       * IC window. An unknown key is a Runner-level FAIL, not a parse error. */
      std::string key, rest;
      if (!(ls >> key)) return fail("'set' needs a key");
      std::getline(ls, rest);
      rest = Trim(rest);
      if (rest.empty()) return fail("'set' needs a value");
      out.SetKV.emplace_back(key, rest);
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
  if (out.ModuleName.empty()) return fail("mission has no 'module'");
  if (!out.HaveSpawn) return fail("mission has no 'spawn'");
  if (!haveTimeout) return fail("mission has no 'timeout'");
  return true;
}

} // namespace FlightBox
