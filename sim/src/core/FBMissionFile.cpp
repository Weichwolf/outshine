#include "FBMissionFile.h"
#include "FBCivilTime.h"
#include "FBEphemeris.h"
#include <sstream>

namespace FlightBox {

namespace {

std::string Trim(const std::string &s) {
  size_t a = s.find_first_not_of(" \t\r");
  if (a == std::string::npos) return "";
  size_t b = s.find_last_not_of(" \t\r");
  return s.substr(a, b - a + 1);
}

/* A callsign becomes a filename AND a log field value, so the alphabet is the intersection of both. */
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
  bool inNet = false;
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

    /* Two scopes, one file: both directions are hard errors — a late mission keyword would read as
     * "mission-wide but declared late", and an actor keyword before any `unit` has no owner. */
    const bool inBlock = !out.Units.empty();
    FBMissionUnit *unit = inBlock ? &out.Units.back() : nullptr;

    /* THE THIRD SCOPE, and it is a sub-scope of the first: `net <name>` opens a block whose own
     * keywords are legal only inside it, so `link` or `member` outside one is a parse error rather
     * than a keyword that quietly attaches to the last net declared far above. */
    if (inNet && (kw == "control" || kw == "link" || kw == "period" || kw == "hold" ||
                  kw == "wcs" || kw == "member")) {
      FBNetSpec &nb = out.Nets.back();
      if (kw == "control") {
        if (!nb.Control.empty()) return fail("net '" + nb.Name + "' already has a 'control'");
        if (!(ls >> nb.Control)) return fail("'control' needs the node's callsign");
      } else if (kw == "link") {
        std::string kind;
        if (!(ls >> kind)) return fail("'link' needs wire|radio");
        if (kind == "wire") {
          nb.Wire = true;
        } else if (kind == "radio") {
          nb.Wire = false;
          if (!(ls >> nb.RangeM) || nb.RangeM <= 0.0) return fail("'link radio' needs a positive rangeM");
        } else {
          return fail("'link' needs wire|radio");
        }
        std::string mastKw;
        if (ls >> mastKw) {
          if (mastKw != "mast") return fail("'link' takes only an optional 'mast <m>' after it");
          if (!(ls >> nb.MastM) || nb.MastM < 0.0) return fail("'link ... mast' needs a non-negative height");
        }
      } else if (kw == "period") {
        if (!(ls >> nb.PeriodS) || nb.PeriodS <= 0.0) return fail("'period' needs positive seconds");
      } else if (kw == "hold") {
        if (!(ls >> nb.HoldCycles) || nb.HoldCycles <= 0.0) return fail("'hold' needs a positive cycle count");
      } else if (kw == "wcs") {
        std::string w;
        if (!(ls >> w) || !FBWeaponsControlFromString(w.c_str(), nb.Wcs))
          return fail("'wcs' needs free|tight|hold");
      } else {
        FBNetMember m;
        if (!(ls >> m.Id)) return fail("'member' needs a unit callsign");
        std::string tok;
        while (ls >> tok) {
          if (tok == "sector") {
            if (!(ls >> m.SectorCentreDeg >> m.SectorHalfDeg))
              return fail("'member ... sector' needs <centreDeg> <halfDeg>");
            if (m.SectorCentreDeg < 0.0 || m.SectorCentreDeg > 360.0 || m.SectorHalfDeg <= 0.0 ||
                m.SectorHalfDeg > 180.0)
              return fail("'member ... sector' wants centre 0..360 and half 0..180");
            m.HaveSector = true;
          } else if (tok == "autonomy") {
            std::string a;
            if (!(ls >> a) || !FBWeaponsControlFromString(a.c_str(), m.Autonomy))
              return fail("'member ... autonomy' needs free|tight|hold");
          } else {
            return fail("unknown 'member' token '" + tok + "'");
          }
        }
        for (const auto &net : out.Nets)
          for (const auto &had : net.Members)
            if (had.Id == m.Id) return fail("unit '" + m.Id + "' is already a member of a net");
        nb.Members.push_back(std::move(m));
      }
      continue;
    }
    inNet = false;

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

    if (kw == "zone" || kw == "net") {
      if (inBlock) return fail("'" + kw + "' is mission-wide and must come before the first 'unit' block");
      if (kw == "zone") {
        FBZone z;
        if (!(ls >> z.Name >> z.LatDeg >> z.LonDeg >> z.RadiusM >> z.AltMinM >> z.AltMaxM))
          return fail("'zone' needs <name> <lat> <lon> <radiusM> <altMinM> <altMaxM>");
        if (!CallsignOk(z.Name)) return fail("zone name '" + z.Name + "' must be 1-24 chars of "
                                             "[A-Za-z0-9_-] (it names a telemetry column)");
        if (z.RadiusM <= 0.0) return fail("zone '" + z.Name + "' needs a positive radius");
        if (z.AltMinM >= z.AltMaxM)
          return fail("zone '" + z.Name + "' needs altMin < altMax (a band, not a plane)");
        for (const auto &had : out.Zones)
          if (had.Name == z.Name) return fail("duplicate zone name '" + z.Name + "'");
        out.Zones.push_back(std::move(z));
      } else {
        FBNetSpec n;
        std::string extra;
        if (!(ls >> n.Name)) return fail("'net' needs a name (e.g. 'net iads')");
        if (ls >> extra) return fail("'net' takes exactly one name");
        if (!CallsignOk(n.Name)) return fail("net name '" + n.Name + "' must be 1-24 chars of [A-Za-z0-9_-]");
        for (const auto &had : out.Nets)
          if (had.Name == n.Name) return fail("duplicate net name '" + n.Name + "'");
        out.Nets.push_back(std::move(n));
        inNet = true;
      }
      continue;
    }

    if (kw == "name" || kw == "runway" || kw == "timeout" || kw == "wx" || kw == "time") {
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
      } else if (kw == "wx") {
        /* An explicit KIND word first, never a guess from what follows: `wx 270 20` would read as a
         * shorthand today and as something else the day a third kind appears. */
        if (out.HaveWeather) return fail("mission already has a 'wx' line");
        std::string kind;
        if (!(ls >> kind)) return fail("'wx' needs calm|fixture|wind");
        if (kind == "calm") {
          out.Weather = FBWeatherSpec{};
        } else if (kind == "fixture") {
          std::string rest;
          std::getline(ls, rest);
          out.Weather.Kind = FBWeatherKind::Fixture;
          out.Weather.Fixture = Trim(rest);
          if (out.Weather.Fixture.empty()) return fail("'wx fixture' needs an asset name or a path");
        } else if (kind == "wind") {
          double dir, kt;
          if (!(ls >> dir >> kt)) return fail("'wx wind' needs dirDegFROM speedKt");
          if (dir < 0.0 || dir > 360.0) return fail("'wx wind' direction must be 0..360 (the bearing it blows FROM)");
          if (kt < 0.0 || kt > 300.0) return fail("'wx wind' speed must be 0..300 kt");
          out.Weather.Kind = FBWeatherKind::Wind;
          out.Weather.WindFromDeg = dir;
          out.Weather.WindSpeedKt = kt;
        } else {
          return fail("'wx' needs calm|fixture|wind");
        }
        out.HaveWeather = true;
      } else if (kw == "time") {
        /* ZULU ONLY, exact shape, no best-effort read: the consumers are functions of (lat, lon, utc),
         * and a local spelling would need an offset FlightBox cannot source. */
        if (out.HaveTime) return fail("mission already has a 'time' line");
        std::string tok, extra;
        if (!(ls >> tok)) return fail("'time' needs a UTC instant, e.g. 'time 1999-03-24T22:00:00Z'");
        if (ls >> extra) return fail("'time' takes exactly one token (UTC only, no offset)");
        if (!FBParseIsoUtc(tok.c_str(), out.UtcT0S))
          return fail("'time " + tok + "' is not YYYY-MM-DDThh:mm:ssZ (Zulu only, seconds mandatory, "
                      "no fractional seconds)");
        const int64_t lo = FBDaysFromCivil(kEphemerisMinYear, 1, 1) * 86400;
        const int64_t hi = FBDaysFromCivil(kEphemerisMaxYear + 1, 1, 1) * 86400;
        if (out.UtcT0S < lo || out.UtcT0S >= hi)
          return fail("'time " + tok + "' is outside " + std::to_string(kEphemerisMinYear) + ".." +
                      std::to_string(kEphemerisMaxYear) + ", where the ephemeris holds");
        out.HaveTime = true;
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
    } else if (kw == "flight") {
      /* The flight is IDENTITY, declared beside the team and for the same reason: the cooperative net
       * reads both off the registry. Position 1 is the lead — which is what makes the datalink's
       * flight-lead filter a statement instead of a mission-file ordinal. */
      if (unit->Flight.Declared()) return fail("unit '" + unit->Id + "' already has a 'flight'");
      std::string fname;
      int pos = 0;
      if (!(ls >> fname)) return fail("'flight' needs a name and a position (e.g. 'flight viper 2')");
      if (!(ls >> pos)) return fail("'flight' needs a position after the name (1 = lead)");
      if (!CallsignOk(fname) || fname.size() >= (size_t)kFlightNameLen)
        return fail("flight name '" + fname + "' must be 1-" + std::to_string(kFlightNameLen - 1) +
                    " chars of [A-Za-z0-9_-] (it travels in a PPLI)");
      if (pos < 1 || pos > kMaxFlightPosition)
        return fail("'flight' position must be 1.." + std::to_string(kMaxFlightPosition) + " (1 = lead)");
      for (const auto &u : out.Units)
        if (&u != unit && u.Flight.Name == fname && u.Flight.Position == pos)
          return fail("flight '" + fname + "' already has a unit at position " + std::to_string(pos));
      unit->Flight.Name = fname;
      unit->Flight.Position = pos;
    } else if (kw == "spawn") {
      /* spawn <lat lon | threshold> <altM | ground> <hdgDeg> <speedKt> — the declarative IC. Neither
       * keyword is a second syntax or a second code path (doc/missions/syntax.md). */
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
    } else if (kw == "objective") {
      /* `kill` takes an EXPLICIT unit/team discriminator rather than guessing from the next word: a
       * callsign is allowed to be "hostile", and that must not silently change what the line means. */
      const std::string kinds =
          "'objective' needs survive|waypoints|kill|identify|protect|no_fire|deny|avoid|suppress";
      FBObjective o;
      /* THE DECLARED SPAN is a modifier on EVERY kind, so it is taken off the tail before the kind is
       * read — otherwise each kind's own optional trailing token (`exposure`, `emitting`, `hold`) would
       * have to know a word that has nothing to do with it. doc/missions/verdict.md `E17`. */
      {
        std::string rest, w;
        std::getline(ls, rest);
        std::vector<std::string> tok;
        std::istringstream ts(rest);
        while (ts >> w) tok.push_back(w);
        if (tok.size() >= 2 && tok[tok.size() - 2] == "until") {
          std::istringstream vs(tok.back());
          if (!(vs >> o.UntilS) || !vs.eof() || !(o.UntilS > 0.0))
            return fail("'objective ... until' needs seconds greater than zero");
          if (haveTimeout && o.UntilS >= out.TimeoutS)
            return fail("'objective ... until " + tok.back() + "' is at or past the mission timeout (" +
                        std::to_string(out.TimeoutS) + " s): a span that cannot close is the old "
                        "end-of-run rule wearing a new word");
          tok.resize(tok.size() - 2);
        }
        std::string body;
        for (const std::string &t : tok) body += t + " ";
        ls.clear();
        ls.str(body);
      }
      std::string what;
      if (!(ls >> what)) return fail(kinds);
      /* The unit/team discriminator of every kind that offers both, read the same way for all of them:
       * a callsign may legally be spelled `hostile`, so the scope word is never guessed. */
      auto readTarget = [&](const std::string &line) -> bool {
        std::string scope, target;
        if (!(ls >> scope) || !(ls >> target)) return fail(line);
        if (scope == "unit") {
          o.Scope = FBObjectiveScope::Unit;
          o.TargetId = target;
          if (target == unit->Id) return fail("unit '" + unit->Id + "' cannot have itself as a target");
        } else if (scope == "team") {
          o.Scope = FBObjectiveScope::Team;
          if (!FBUnitTeamFromString(target.c_str(), o.TargetTeam))
            return fail("'" + what + "' team needs friendly|hostile|neutral");
        } else {
          return fail(line);
        }
        return true;
      };
      if (what == "survive") {
        o.Kind = FBObjectiveKind::Survive;
      } else if (what == "waypoints") {
        o.Kind = FBObjectiveKind::Waypoints;
        if (unit->Plan.Empty()) return fail("'objective waypoints' needs 'wp'/'land' lines above it");
      } else if (what == "kill") {
        if (!readTarget("'objective kill' needs 'unit <callsign>' or 'team <faction>'")) return false;
        o.Kind = o.Scope == FBObjectiveScope::Unit ? FBObjectiveKind::KillUnit : FBObjectiveKind::KillTeam;
      } else if (what == "identify") {
        /* No default box: it is [SET] per mission and has to be visible in the file. */
        o.Kind = FBObjectiveKind::Identify;
        std::string scope, target, rangeKw, holdKw;
        if (!(ls >> scope) || !(ls >> target) || scope != "unit")
          return fail("'objective identify' needs 'unit <callsign> range <m> hold <s>'");
        o.TargetId = target;
        if (target == unit->Id) return fail("unit '" + unit->Id + "' cannot have itself as a target");
        if (!(ls >> rangeKw >> o.RangeM) || rangeKw != "range" || o.RangeM <= 0.0)
          return fail("'objective identify' needs 'range <metres>' with a positive value");
        if (!(ls >> holdKw >> o.HoldS) || holdKw != "hold" || o.HoldS < 0.0)
          return fail("'objective identify' needs 'hold <seconds>' with a non-negative value");
      } else if (what == "protect") {
        o.Kind = FBObjectiveKind::Protect;
        if (!readTarget("'objective protect' needs 'unit <callsign>' or 'team <faction>'")) return false;
      } else if (what == "no_fire") {
        o.Kind = FBObjectiveKind::NoFire;
      } else if (what == "deny") {
        /* `release` is a word of its own so that a later `deny <something-else>` is a new spelling and
         * not a re-reading of this one. */
        std::string sub;
        if (!(ls >> sub) || sub != "release")
          return fail("'objective deny' needs 'release unit <callsign>' or 'release team <faction>'");
        o.Kind = FBObjectiveKind::DenyRelease;
        if (!readTarget("'objective deny release' needs 'unit <callsign>' or 'team <faction>'")) return false;
      } else if (what == "suppress") {
        /* `emitting <s>` is a cumulative ALLOWANCE and not a time window: a window would be a modifier
         * on every kind and would multiply the grammar (doc/missions/verdict.md's own reason). Absent
         * means zero — the position must never have been on the air at all. */
        o.Kind = FBObjectiveKind::Suppress;
        if (!readTarget("'objective suppress' needs 'unit <callsign>' or 'team <faction>'")) return false;
        std::string emKw;
        if (ls >> emKw) {
          if (emKw != "emitting")
            return fail("'objective suppress' takes only 'emitting <s>' after the target");
          if (!(ls >> o.HoldS) || o.HoldS < 0.0)
            return fail("'objective suppress ... emitting' needs non-negative seconds");
        }
      } else if (what == "avoid") {
        /* `zone` is a word of its own for the same reason `release` is: a later `avoid <something-else>`
         * must be a new spelling, not a re-reading of this one. */
        std::string sub;
        if (!(ls >> sub) || sub != "zone")
          return fail("'objective avoid' needs 'zone <name> [exposure <s>]'");
        o.Kind = FBObjectiveKind::AvoidZone;
        if (!(ls >> o.TargetId)) return fail("'objective avoid zone' needs a zone name");
        std::string expKw;
        if (ls >> expKw) {
          if (expKw != "exposure") return fail("'objective avoid zone' takes only 'exposure <s>' after the name");
          if (!(ls >> o.HoldS) || o.HoldS < 0.0)
            return fail("'objective avoid zone ... exposure' needs non-negative seconds");
        }
      } else {
        return fail(kinds);
      }
      for (const auto &have : unit->Objectives)
        if (have.Kind == o.Kind && have.Scope == o.Scope && have.TargetId == o.TargetId &&
            have.TargetTeam == o.TargetTeam && have.RangeM == o.RangeM && have.HoldS == o.HoldS &&
            have.UntilS == o.UntilS)
          return fail("unit '" + unit->Id + "' declares '" + FBObjectiveStr(o) + "' twice");
      unit->Objectives.push_back(std::move(o));
    } else if (kw == "set") {
      /* Raw KV only: the PARSER never interprets a key, the MODULE does. An unknown key is a
       * Runner-level FAIL, not a parse error. */
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
    /* Resolved against the WHOLE cast, not the blocks seen so far, or a duel would be an ordering
     * puzzle. A target that does not exist is a mission that can never be won: a parse error. */
    for (const auto &o : u.Objectives) {
      if (o.Kind == FBObjectiveKind::AvoidZone) {
        bool haveZone = false;
        for (const auto &z : out.Zones) haveZone = haveZone || z.Name == o.TargetId;
        if (!haveZone)
          return failFile("unit '" + u.Id + "': 'objective " + FBObjectiveStr(o) + "' names no 'zone' "
                          "in this mission");
        continue;
      }
      bool byName = o.Kind == FBObjectiveKind::KillUnit || o.Kind == FBObjectiveKind::Identify ||
                    ((o.Kind == FBObjectiveKind::Protect || o.Kind == FBObjectiveKind::DenyRelease ||
                      o.Kind == FBObjectiveKind::Suppress) &&
                     o.Scope == FBObjectiveScope::Unit);
      if (!byName) continue;
      bool found = false;
      for (const auto &other : out.Units) found = found || other.Id == o.TargetId;
      if (!found)
        return failFile("unit '" + u.Id + "': 'objective " + FBObjectiveStr(o) + "' names no unit "
                        "in this mission");
    }
    /* A flight without a lead is not a flight: every piece of formation behaviour — the station a
     * wingman holds, the datalink's `fl` filter — is defined against position 1. */
    if (!u.Flight.Declared()) continue;
    bool haveLead = false;
    for (const auto &other : out.Units)
      haveLead = haveLead || (other.Flight.Name == u.Flight.Name && other.Flight.IsLead());
    if (!haveLead)
      return failFile("flight '" + u.Flight.Name + "' has no unit at position 1 (the lead)");
  }

  /* ---- THE NETS, resolved against the WHOLE cast for the same reason the objectives are ---- */
  for (const FBNetSpec &n : out.Nets) {
    if (n.Members.empty()) return failFile("net '" + n.Name + "' has no 'member'");
    for (const FBNetMember &m : n.Members) {
      bool found = false;
      for (const auto &u : out.Units) found = found || u.Id == m.Id;
      if (!found)
        return failFile("net '" + n.Name + "': 'member " + m.Id + "' names no unit in this mission");
    }
    if (n.Control.empty()) continue;
    bool isMember = false;
    for (const FBNetMember &m : n.Members) isMember = isMember || m.Id == n.Control;
    if (!isMember)
      return failFile("net '" + n.Name + "': 'control " + n.Control + "' is not also a 'member' of it");
  }

  /* ---- The one interface consequence, GENERATED rather than smuggled ----
   * A net block is doctrine ABOUT positions and not a property OF one, so the author writes it once in
   * one place; the only mission-data->module path is ApplySetup, so the keys a member's link slot needs
   * are produced here. They are never author-written — an author's `set net_*` line would simply be
   * overwritten by the block's own, since these are appended last. doc/air-defence-network.md §8. */
  for (const FBNetSpec &n : out.Nets) {
    for (const FBNetMember &m : n.Members) {
      for (auto &u : out.Units) {
        if (u.Id != m.Id) continue;
        std::ostringstream link;
        if (n.Wire) link << "wire " << n.MastM;
        else link << "radio " << n.RangeM << " " << n.MastM;
        u.SetKV.emplace_back("net_link", link.str());
        u.SetKV.emplace_back("net_period_s", std::to_string(n.PeriodS));
        u.SetKV.emplace_back("net_hold", std::to_string(n.HoldCycles));
        if (m.HaveSector)
          u.SetKV.emplace_back("net_sector",
                               std::to_string(m.SectorCentreDeg) + " " + std::to_string(m.SectorHalfDeg));
        u.SetKV.emplace_back("net_autonomy", FBWeaponsControlStr(m.Autonomy));
        /* A member subscribes to the node; the NODE transmits a weapons control state and subscribes to
         * nobody. Exactly one of the two keys is generated per member, which is also how the position
         * learns which of the two it is without ever being told its own callsign. */
        if (!n.Control.empty() && n.Control == m.Id) u.SetKV.emplace_back("net_wcs", FBWeaponsControlStr(n.Wcs));
        else if (!n.Control.empty()) u.SetKV.emplace_back("net_control", n.Control);
      }
    }
  }
  return true;
}

} // namespace FlightBox
