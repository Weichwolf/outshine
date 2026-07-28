#include "FBCampaignFile.h"
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

} // namespace

std::string FBCarryMaskStr(uint8_t mask) {
  std::string s;
  if (mask & FBCarryUnits) s += "units";
  if (mask & FBCarryGround) s += s.empty() ? "ground" : "+ground";
  if (mask & FBCarryStores) s += s.empty() ? "stores" : "+stores";
  return s.empty() ? "none" : s;
}

const char *FBCampaignStopStr(FBCampaignStop s) {
  switch (s) {
    case FBCampaignStop::Never: return "never";
    case FBCampaignStop::Fail: return "fail";
    case FBCampaignStop::Crash: return "crash";
  }
  return "?";
}

bool FBParseCampaignFile(const std::string &text, FBCampaign &out, std::string *err) {
  out = FBCampaign{};
  int lineNo = 0;
  bool haveCarry = false, haveStop = false;
  auto fail = [&](const std::string &msg) {
    if (err) *err = "line " + std::to_string(lineNo) + ": " + msg;
    return false;
  };
  auto failFile = [&](const std::string &msg) {
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

    if (kw == "name") {
      if (!out.Name.empty()) return fail("campaign already has a 'name' line");
      std::string rest = Trim(line.substr(kw.size()));
      if (rest.empty()) return fail("'name' needs a value");
      /* It names the output directory, so it lives under the same alphabet a callsign does. */
      for (char c : rest)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
              c == '_' || c == '-'))
          return fail("campaign name '" + rest + "' must be [A-Za-z0-9_-] (it also names a directory)");
      out.Name = rest;
      continue;
    }

    if (kw == "time") {
      std::string tok, extra;
      if (out.HaveTime) return fail("campaign already has a 'time' line");
      if (!(ls >> tok)) return fail("'time' needs a UTC instant, e.g. 'time 1999-03-24T22:00:00Z'");
      if (ls >> extra) return fail("'time' takes exactly one token (UTC only, no offset)");
      if (!FBParseIsoUtc(tok.c_str(), out.UtcT0S))
        return fail("'time " + tok + "' is not YYYY-MM-DDThh:mm:ssZ (Zulu only, seconds mandatory)");
      const int64_t lo = FBDaysFromCivil(kEphemerisMinYear, 1, 1) * 86400;
      const int64_t hi = FBDaysFromCivil(kEphemerisMaxYear + 1, 1, 1) * 86400;
      if (out.UtcT0S < lo || out.UtcT0S >= hi)
        return fail("'time " + tok + "' is outside " + std::to_string(kEphemerisMinYear) + ".." +
                    std::to_string(kEphemerisMaxYear));
      out.HaveTime = true;
      continue;
    }

    if (kw == "carry") {
      if (haveCarry) return fail("campaign already has a 'carry' line");
      uint8_t mask = 0;
      std::string tok;
      while (ls >> tok) {
        if (tok == "units") mask |= FBCarryUnits;
        else if (tok == "ground") mask |= FBCarryGround;
        else if (tok == "stores") mask |= FBCarryStores;
        else return fail("'carry' takes units|ground|stores, not '" + tok + "'");
      }
      if (mask == 0) return fail("'carry' needs at least one of units|ground|stores");
      out.Carry = mask;
      haveCarry = true;
      continue;
    }

    if (kw == "stop_on") {
      std::string tok, extra;
      if (haveStop) return fail("campaign already has a 'stop_on' line");
      if (!(ls >> tok)) return fail("'stop_on' needs never|fail|crash");
      if (ls >> extra) return fail("'stop_on' takes exactly one token");
      if (tok == "never") out.StopOn = FBCampaignStop::Never;
      else if (tok == "fail") out.StopOn = FBCampaignStop::Fail;
      else if (tok == "crash") out.StopOn = FBCampaignStop::Crash;
      else return fail("'stop_on' takes never|fail|crash, not '" + tok + "'");
      haveStop = true;
      continue;
    }

    if (kw == "mission") {
      std::string path = Trim(line.substr(kw.size()));
      if (path.empty()) return fail("'mission' needs a path to a .fbm file");
      out.Missions.push_back(path);
      continue;
    }

    return fail("unknown keyword '" + kw + "'");
  }

  if (out.Name.empty()) return failFile("campaign has no 'name'");
  if (out.Missions.empty()) return failFile("campaign has no 'mission' line");
  return true;
}

} // namespace FlightBox
