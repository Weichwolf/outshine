#include "FBCatalogueBoot.h"

#include <cstdlib>
#include <fstream>
#include <sstream>

namespace FlightBox::Missions {
namespace {

using Setter = void (*)(FBAircraftSpec &, double);

/* ONE LINE PER SCHEMA FIELD, and the table IS the accepted vocabulary: a manifest field with no entry
 * here does not exist. Ordered like core/FBAircraft.h so the two read as one document. */
struct NumField { const char *Name; Setter Set; };
constexpr NumField kNumFields[] = {
    {"engines", [](FBAircraftSpec &s, double v) { s.Engines = static_cast<int>(v); }},
    {"radar_search_m", [](FBAircraftSpec &s, double v) { s.Radar.SearchRangeM = v; }},
    {"radar_track_m", [](FBAircraftSpec &s, double v) { s.Radar.TrackRangeM = v; }},
    {"radar_az_half_deg", [](FBAircraftSpec &s, double v) { s.Radar.AzHalfDeg = v; }},
    {"radar_el_center_deg", [](FBAircraftSpec &s, double v) { s.Radar.ElCenterDeg = v; }},
    {"radar_el_half_deg", [](FBAircraftSpec &s, double v) { s.Radar.ElHalfDeg = v; }},
    {"radar_frame_s", [](FBAircraftSpec &s, double v) { s.Radar.FrameS = v; }},
    {"radar_lookdown_m", [](FBAircraftSpec &s, double v) { s.Radar.LookDownRangeM = v; }},
    {"radar_notch_ms", [](FBAircraftSpec &s, double v) { s.Radar.DopplerNotchMs = v; }},
    {"radar_notch_rejects", [](FBAircraftSpec &s, double v) { s.Radar.NotchRejects = v != 0.0; }},
    {"rwr", [](FBAircraftSpec &s, double v) { s.HasRwr = v != 0.0; }},
    {"irst_m", [](FBAircraftSpec &s, double v) { s.IrstRangeM = v; }},
    {"irst_high_m", [](FBAircraftSpec &s, double v) { s.IrstHighRangeM = v; }},
    {"net_node", [](FBAircraftSpec &s, double v) { s.NetNode = v != 0.0; }},
    {"net_member", [](FBAircraftSpec &s, double v) { s.NetMember = v != 0.0; }},
    {"stations", [](FBAircraftSpec &s, double v) { s.Stations = static_cast<int>(v); }},
    {"gun_rounds", [](FBAircraftSpec &s, double v) { s.GunRounds = static_cast<int>(v); }},
    {"rcs_m2", [](FBAircraftSpec &s, double v) { s.RcsM2 = v; }},
    {"corner_kt", [](FBAircraftSpec &s, double v) { s.Perf.CornerKt = v; }},
    {"corner_g", [](FBAircraftSpec &s, double v) { s.Perf.CornerG = v; }},
    {"max_g", [](FBAircraftSpec &s, double v) { s.Perf.MaxG = v; }},
    {"alpha_limit_deg", [](FBAircraftSpec &s, double v) { s.Perf.AlphaLimitDeg = v; }},
    {"min_speed_kt", [](FBAircraftSpec &s, double v) { s.Perf.MinSpeedKt = v; }},
    {"climb_speed_kt", [](FBAircraftSpec &s, double v) { s.Perf.ClimbSpeedKt = v; }},
    {"approach_kt", [](FBAircraftSpec &s, double v) { s.Perf.ApproachKt = v; }},
    {"pitch_stick_max", [](FBAircraftSpec &s, double v) { s.Perf.PitchStickMax = v; }},
    {"roll_plant_a", [](FBAircraftSpec &s, double v) { s.Perf.RollPlantA = v; }},
    {"roll_plant_k_degs", [](FBAircraftSpec &s, double v) { s.Perf.RollPlantKDegS = v; }},
    {"cruise_ms", [](FBAircraftSpec &s, double v) { s.Mover.CruiseMs = v; }},
    {"max_ms", [](FBAircraftSpec &s, double v) { s.Mover.MaxMs = v; }},
    {"ceiling_m", [](FBAircraftSpec &s, double v) { s.Mover.CeilingM = v; }},
    {"climb_ms", [](FBAircraftSpec &s, double v) { s.Mover.ClimbMs = v; }},
    {"bank_deg", [](FBAircraftSpec &s, double v) { s.Mover.BankDeg = v; }},
};

/* One row under construction. The two dimensions are NOT spec fields: they exist to derive the damage
 * layout, and carrying them twice would let a layout and its aeroplane disagree. */
struct Pending {
  bool Open = false;
  std::string Key, Name, Fdm;
  FBAircraftSpec Spec{};
  double SpanM = 0.0, LenM = 0.0;
};

std::string Trim(const std::string &s) {
  size_t a = s.find_first_not_of(" \t\r");
  if (a == std::string::npos) return "";
  return s.substr(a, s.find_last_not_of(" \t\r") - a + 1);
}

bool Number(const std::string &tok, double &out) {
  char *end = nullptr;
  out = strtod(tok.c_str(), &end);
  return end && *end == '\0' && end != tok.c_str();
}

bool Tier(const std::string &tok, FBAirTier &out) {
  for (int i = 0; i <= static_cast<int>(FBAirTier::Peer); i++) {
    const FBAirTier t = static_cast<FBAirTier>(i);
    if (tok == FBAirTierStr(t)) { out = t; return true; }
  }
  return false;
}

} // namespace

bool FBLoadAircraftCatalogue(const std::string &path, FBAircraftCatalogue &out, std::string *err) {
  std::ifstream in(path);
  if (!in) { if (err) *err = "cannot open " + path; return false; }

  Pending p;
  int line = 0;
  std::string raw;
  auto fail = [&](const std::string &what) {
    if (err) *err = path + ":" + std::to_string(line) + ": " + what;
    return false;
  };
  auto flush = [&]() {
    if (!p.Open) return true;
    if (p.Name.empty()) return fail("aircraft " + p.Key + " declares no name");
    if (p.SpanM <= 0.0 || p.LenM <= 0.0) return fail("aircraft " + p.Key + " declares no span/length");
    out.Add(p.Key, p.Name, p.Fdm, p.Spec, p.SpanM, p.LenM);
    p = Pending{};
    return true;
  };

  while (std::getline(in, raw)) {
    line++;
    const size_t hash = raw.find('#');
    const std::string text = Trim(hash == std::string::npos ? raw : raw.substr(0, hash));
    if (text.empty()) continue;

    const size_t sp = text.find_first_of(" \t");
    const std::string field = text.substr(0, sp);
    const std::string rest = sp == std::string::npos ? "" : Trim(text.substr(sp + 1));
    if (rest.empty()) return fail("field '" + field + "' has no value");

    if (field == "aircraft") {
      if (!flush()) return false;
      if (rest.find_first_of(" \t") != std::string::npos) return fail("key '" + rest + "' has a space");
      for (size_t i = 0; i < out.Size(); i++)
        if (rest == out.At(i).Key) return fail("aircraft " + rest + " is declared twice");
      p.Open = true;
      p.Key = rest;
      continue;
    }
    if (!p.Open) return fail("'" + field + "' outside an aircraft block");

    if (field == "name") {
      if (rest.size() < 2 || rest.front() != '"' || rest.back() != '"')
        return fail("name must be quoted");
      p.Name = rest.substr(1, rest.size() - 2);
      continue;
    }
    if (field == "tier") {
      if (!Tier(rest, p.Spec.Tier)) return fail("unknown tier '" + rest + "'");
      continue;
    }
    if (field == "fdm") { p.Fdm = rest; continue; }
    if (field == "gun") {
      if (rest == "none") { p.Spec.Gun = FBGunKind::None; continue; }
      const FBGunSpec *g = FBFindGun(rest.c_str());
      if (!g) return fail("unknown gun '" + rest + "'");
      p.Spec.Gun = g->Kind;
      continue;
    }

    const NumField *nf = nullptr;
    for (const NumField &f : kNumFields)
      if (field == f.Name) nf = &f;
    const bool dim = field == "span_m" || field == "length_m";
    if (!nf && !dim) return fail("unknown field '" + field + "'");
    double v = 0.0;
    if (!Number(rest, v)) return fail("field '" + field + "' takes a number, got '" + rest + "'");
    if (field == "span_m") p.SpanM = v;
    else if (dim) p.LenM = v;
    else nf->Set(p.Spec, v);
  }
  return flush();
}

} // namespace FlightBox::Missions
