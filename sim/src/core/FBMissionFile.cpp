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

/* A unit callsign also becomes a filename (outDir/telemetry_<id>.csv) and a log field value, so the
 * accepted alphabet is the intersection of "safe in both": no separators, no quoting, no spaces. */
bool CallsignOk(const std::string &s) {
  if (s.empty() || s.size() > 24) return false;
  for (char c : s)
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
          c == '_' || c == '-')) return false;
  return true;
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
  auto failFile = [&](const std::string &msg) {   /* whole-file requirement: no line to point at */
    if (err) *err = msg;
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

    /* Two scopes, one file (doc/mission-format.md): mission-wide keywords may only appear BEFORE the
     * first actor block, actor keywords only INSIDE one. Both directions are hard errors — a `runway`
     * line sitting between two units would silently read as "mission-wide but declared late", and an
     * actor keyword before any `unit` line has no owner to belong to. */
    const bool inBlock = !out.Units.empty();
    FBMissionUnit *unit = inBlock ? &out.Units.back() : nullptr;

    if (kw == "unit") {
      std::string id, extra;
      if (!(ls >> id)) return fail("'unit' needs a callsign (e.g. 'unit lead')");
      if (ls >> extra) return fail("'unit' takes exactly one callsign");
      if (!CallsignOk(id))
        return fail("unit callsign '" + id + "' must be 1-24 chars of [A-Za-z0-9_-] (it also names a file)");
      for (const auto &u : out.Units)
        if (u.Id == id) return fail("duplicate unit callsign '" + id + "'");
      out.Units.push_back(FBMissionUnit{});
      out.Units.back().Id = id;
      continue;
    }

    if (kw == "name" || kw == "runway" || kw == "timeout") {
      if (inBlock) return fail("'" + kw + "' is mission-wide and must come before the first 'unit' block");
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
      } else {
        double t;
        if (!(ls >> t) || t <= 0.0) return fail("'timeout' needs a positive seconds value");
        out.TimeoutS = t;
        haveTimeout = true;
      }
      continue;
    }

    if (!unit) return fail("'" + kw + "' belongs to an actor and needs a 'unit <callsign>' line first");

    if (kw == "module") {
      std::string rest;
      std::getline(ls, rest);
      unit->ModuleName = Trim(rest);
      if (unit->ModuleName.empty()) return fail("'module' needs a value (e.g. 'module f16')");
    } else if (kw == "team") {
      std::string t;
      if (!(ls >> t) || !FBUnitTeamFromString(t.c_str(), unit->Team))
        return fail("'team' needs friendly|hostile|neutral");
    } else if (kw == "spawn") {
      /* spawn <lat lon | threshold> <altM | ground> <hdgDeg> <speedKt> — the actor's declarative IC
       * (doc/mission-format.md): 'threshold' reuses the mission's runway lat/lon (a ground-start
       * convenience, not a second position syntax), 'ground' resolves the altitude from terrain + gear
       * clearance at spawn (FBMissionBoot.h), never a separate code path. */
      if (unit->HaveSpawn) return fail("unit '" + unit->Id + "' already has a 'spawn'");
      std::string first;
      if (!(ls >> first)) return fail("'spawn' needs lat|threshold");
      double lat, lon;
      if (first == "threshold") {
        if (!out.HaveRunway) return fail("'spawn threshold' without a mission 'runway' line");
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
      unit->Spawn = FBSpawn{lat, lon, ground, altM, hdg, spd};
      unit->HaveSpawn = true;
    } else if (kw == "wp") {
      double lat, lon, alt, spd;
      if (!(ls >> lat >> lon >> alt >> spd)) return fail("'wp' needs lat lon altM speedKt");
      unit->Plan.AddWaypoint(FBWaypoint{lat, lon, alt, spd, FBWaypointType::Enroute});
    } else if (kw == "land") {
      if (!out.HaveRunway) return fail("'land' without a mission 'runway' line");
      unit->Plan.AddWaypoint(FBWaypoint{out.Runway.ThresholdLatDeg, out.Runway.ThresholdLonDeg,
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
      unit->SetKV.emplace_back(key, rest);
    } else {
      return fail("unknown keyword '" + kw + "'");
    }
  }

  if (out.Name.empty()) return failFile("mission has no 'name'");
  if (!haveTimeout) return failFile("mission has no 'timeout'");
  if (out.Units.empty()) return failFile("mission has no 'unit' block");
  for (const auto &u : out.Units) {
    if (u.ModuleName.empty()) return failFile("unit '" + u.Id + "' has no 'module'");
    if (!u.HaveSpawn) return failFile("unit '" + u.Id + "' has no 'spawn'");
  }
  return true;
}

} // namespace FlightBox
