#include "FBHudDecl.h"

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include "FBDisplaySystem.h"
#include "FBState.h"
#include "FBStore.h"
#include "FBUnits.h"

namespace FlightBox::Systems {
namespace {

template <class E> struct Word { const char *Name; E Id; };

constexpr Word<FBHudKind> kKinds[] = {
    {"text", FBHudKind::Text},         {"line", FBHudKind::Line},
    {"box", FBHudKind::Box},           {"circle", FBHudKind::Circle},
    {"bar", FBHudKind::Bar},           {"cross", FBHudKind::Cross},
    {"rose", FBHudKind::Rose},         {"scope", FBHudKind::Scope},
    {"vector", FBHudKind::Vector},     {"compass", FBHudKind::Compass},
    {"tape", FBHudKind::Tape},         {"ladder", FBHudKind::Ladder},
    {"horizon", FBHudKind::Horizon},   {"ils", FBHudKind::Ils},
    {"fpm", FBHudKind::Fpm},           {"contacts", FBHudKind::Contacts},
    {"ccip", FBHudKind::Ccip},         {"funnel", FBHudKind::Funnel},
    {"dlz", FBHudKind::Dlz},
};

constexpr Word<FBHudNum> kNums[] = {
    {"cas_kt", FBHudNum::CasKt},               {"mach", FBHudNum::Mach},
    {"tas_kt", FBHudNum::TasKt},               {"gs_kt", FBHudNum::GsKt},
    {"gs_ms", FBHudNum::GsMs},                 {"alt_ft", FBHudNum::AltFt},
    {"alt_m", FBHudNum::AltM},                 {"agl_ft", FBHudNum::AglFt},
    {"agl_m", FBHudNum::AglM},                 {"vs_fpm", FBHudNum::VsFpm},
    {"vs_ms", FBHudNum::VsMs},                 {"heading", FBHudNum::Heading},
    {"track", FBHudNum::Track},                {"fpa", FBHudNum::Fpa},
    {"pitch", FBHudNum::Pitch},                {"roll", FBHudNum::Roll},
    {"g", FBHudNum::GLoad},                    {"g_peak", FBHudNum::GPeak},
    {"fuel_lb", FBHudNum::FuelLb},             {"fuel_pct", FBHudNum::FuelPct},
    {"gear", FBHudNum::Gear},                  {"speedbrake", FBHudNum::Speedbrake},
    {"gun_rounds", FBHudNum::GunRounds},       {"gun_fired", FBHudNum::GunFired},
    {"station", FBHudNum::Station},            {"stores_loaded", FBHudNum::StoresLoaded},
    {"stores_released", FBHudNum::StoresReleased}, {"chaff", FBHudNum::Chaff},
    {"flare", FBHudNum::Flare},                {"steer_num", FBHudNum::SteerNum},
    {"steer_brg", FBHudNum::SteerBrg},         {"steer_rel_brg", FBHudNum::SteerRelBrg},
    {"steer_dist_nm", FBHudNum::SteerDistNm},  {"steer_ttg_s", FBHudNum::SteerTtgS},
    {"steer_el", FBHudNum::SteerElDeg},        {"home_dist_nm", FBHudNum::HomeDistNm},
    {"home_rel_brg", FBHudNum::HomeRelBrg},    {"contacts", FBHudNum::Contacts},
    {"tgt_range_nm", FBHudNum::TgtRangeNm},    {"tgt_closure_kt", FBHudNum::TgtClosureKt},
    {"tgt_tti_s", FBHudNum::TgtTtiS},          {"tgt_az", FBHudNum::TgtAz},
    {"tgt_el", FBHudNum::TgtEl},               {"tgt_aspect", FBHudNum::TgtAspect},
    {"rmin_nm", FBHudNum::RminNm},             {"rtr_nm", FBHudNum::RtrNm},
    {"raero_nm", FBHudNum::RaeroNm},           {"ag_range_nm", FBHudNum::AgRangeNm},
    {"ag_ttr_s", FBHudNum::AgTtrS},            {"ag_miss_m", FBHudNum::AgMissM},
    {"gun_lead_az", FBHudNum::GunLeadAz},      {"gun_lead_el", FBHudNum::GunLeadEl},
    {"gun_span_mr", FBHudNum::GunSpanMr},      {"weapon_count", FBHudNum::WeaponCount},
    {"sun_el", FBHudNum::SunElDeg},
    {"cloud_cover", FBHudNum::CloudCover},     {"sim_t", FBHudNum::SimTimeS},
    {"friendly", FBHudNum::WatchFriendly},     {"hostile", FBHudNum::WatchHostile},
    {"event_age_s", FBHudNum::WatchEventAgeS}, {"watch_t", FBHudNum::WatchSimT},
};

constexpr Word<FBHudStr> kStrs[] = {
    {"mode", FBHudStr::Mode},                 {"range_provider", FBHudStr::RangeProvider},
    {"weapon", FBHudStr::Weapon},
    {"title", FBHudStr::WatchTitle},          {"mission", FBHudStr::WatchMission},
    {"subject", FBHudStr::WatchSubject},      {"subject_team", FBHudStr::WatchSubjectTeam},
    {"subject_kind", FBHudStr::WatchSubjectKind}, {"shot", FBHudStr::WatchShot},
    {"event", FBHudStr::WatchEvent},          {"event_team", FBHudStr::WatchEventTeam},
};

constexpr Word<FBHudFlag> kFlags[] = {
    {"always", FBHudFlag::Always},           {"telemetry", FBHudFlag::Telemetry},
    {"airborne", FBHudFlag::Airborne},       {"wow", FBHudFlag::WeightOnWheels},
    {"gear_down", FBHudFlag::GearDown},      {"speedbrake", FBHudFlag::Speedbrake},
    {"engine", FBHudFlag::EngineRunning},    {"radar_on", FBHudFlag::RadarOn},
    {"radar_contact", FBHudFlag::RadarContact}, {"locked", FBHudFlag::Locked},
    {"iff_friendly", FBHudFlag::IffFriendly}, {"gun_ready", FBHudFlag::GunReady},
    {"gun_firing", FBHudFlag::GunFiring},    {"gun_valid", FBHudFlag::GunValid},
    {"gun_in_range", FBHudFlag::GunInRange}, {"gun_in_funnel", FBHudFlag::GunInFunnel},
    {"dlz_valid", FBHudFlag::DlzValid},      {"in_zone", FBHudFlag::InZone},
    {"ag_valid", FBHudFlag::AgValid},        {"ag_in_range", FBHudFlag::AgInRange},
    {"ag_release", FBHudFlag::AgRelease},    {"armed", FBHudFlag::Armed},
    {"designating", FBHudFlag::Designating}, {"station_selected", FBHudFlag::StoresSelected},
    {"supersonic", FBHudFlag::Supersonic},
    {"bingo", FBHudFlag::Bingo},             {"alow", FBHudFlag::Alow},
    {"gear_unsafe", FBHudFlag::GearUnsafe},  {"nav_valid", FBHudFlag::NavValid},
    {"ils_window", FBHudFlag::IlsWindow},    {"held", FBHudFlag::WatchHeld},
    {"event", FBHudFlag::WatchEvent},
};

template <class E, size_t N> bool Lookup(const Word<E> (&table)[N], const std::string &w, E &out) {
  for (const Word<E> &e : table)
    if (w == e.Name) { out = e.Id; return true; }
  return false;
}

/* Every conversion this file is allowed to print: a printf whose conversion does not match the double
 * it is handed is undefined behaviour, and a mod is data — so the check is here and not in review. */
int CountConversions(const std::string &f, bool &bad) {
  int n = 0;
  bad = false;
  for (size_t i = 0; i < f.size(); i++) {
    if (f[i] != '%') continue;
    if (i + 1 < f.size() && f[i + 1] == '%') { i++; continue; }
    size_t j = i + 1;
    while (j < f.size() && (std::strchr("-+ #0", f[j]) != nullptr)) j++;
    while (j < f.size() && (std::isdigit((unsigned char)f[j]) || f[j] == '.')) j++;
    if (j >= f.size() || std::strchr("fFeEgG", f[j]) == nullptr) { bad = true; return n; }
    n++;
    i = j;
  }
  return n;
}

std::string Trim(const std::string &s) {
  size_t a = s.find_first_not_of(" \t\r\n");
  if (a == std::string::npos) return "";
  return s.substr(a, s.find_last_not_of(" \t\r\n") - a + 1);
}

/* Whitespace-separated, except inside double quotes — the one place a HUD needs a space in a value. */
bool Tokenise(const std::string &line, std::vector<std::string> &out, std::vector<bool> &quoted) {
  out.clear();
  quoted.clear();
  size_t i = 0;
  while (i < line.size()) {
    while (i < line.size() && std::isspace((unsigned char)line[i])) i++;
    if (i >= line.size()) break;
    if (line[i] == '"') {
      size_t e = line.find('"', i + 1);
      if (e == std::string::npos) return false;
      out.push_back(line.substr(i + 1, e - i - 1));
      quoted.push_back(true);
      i = e + 1;
      continue;
    }
    size_t e = i;
    while (e < line.size() && !std::isspace((unsigned char)line[e])) e++;
    out.push_back(line.substr(i, e - i));
    quoted.push_back(false);
    i = e;
  }
  return true;
}

float Num(const std::string &s) { return (float)std::atof(s.c_str()); }

float Wrap180(float d) {
  while (d > 180.0f) d -= 360.0f;
  while (d < -180.0f) d += 360.0f;
  return d;
}

/* WHAT IS ON THE SELECTED RAIL, and how many more of it hang elsewhere — the two things a weapon
 * line prints. Derived from the published station table; nothing new is measured. */
FBStoreKind SelectedStoreKind(const FBState &s) {
  if (!s.Stores.H.Readable()) return FBStoreKind::None;
  int st = s.Stores.SelectedStation;
  if (st < 1 || st > kMaxStoreStations) return FBStoreKind::None;
  return (FBStoreKind)s.Stores.Station[st - 1];
}

int SelectedStoreCount(const FBState &s) {
  FBStoreKind k = SelectedStoreKind(s);
  if (k == FBStoreKind::None) return 0;
  int n = 0;
  for (int i = 0; i < s.Stores.StationCount && i < kMaxStoreStations; i++)
    if ((FBStoreKind)s.Stores.Station[i] == k) n++;
  return n;
}

const FBRadarContact *Locked(const FBState &s) {
  if (!s.Radar.H.Readable() || s.Radar.LockIndex < 0 || s.Radar.LockIndex >= s.Radar.ContactCount) return nullptr;
  return &s.Radar.Contacts[s.Radar.LockIndex];
}

} // namespace

bool FBParseHud(const std::string &text, FBHudDeck &out, std::string *err) {
  out = FBHudDeck{};
  std::vector<std::string> t;
  std::vector<bool> q;
  size_t pos = 0;
  int lineNo = 0;
  auto fail = [&](const std::string &why) {
    if (err) *err = "line " + std::to_string(lineNo) + ": " + why;
    return false;
  };

  while (pos <= text.size()) {
    size_t nl = text.find('\n', pos);
    std::string line = Trim(text.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos));
    pos = nl == std::string::npos ? text.size() + 1 : nl + 1;
    lineNo++;
    size_t hash = line.find('#');
    if (hash != std::string::npos) line = Trim(line.substr(0, hash));
    if (line.empty()) continue;
    if (!Tokenise(line, t, q)) return fail("unterminated quote");

    const std::string &head = t[0];
    if (head == "hud") {
      if (t.size() < 2) return fail("hud needs a name");
      out.Name = t[1];
      continue;
    }
    if (head == "scale" || head == "inset" || head == "tfov") {
      if (t.size() < 2) return fail(head + " needs a value");
      float v = Num(t[1]);
      if (head == "scale") out.Scale = v;
      else if (head == "inset") out.InsetPx = v;
      else out.TfovDeg = v;
      continue;
    }
    if (head == "color") {
      if (t.size() < 4) return fail("color needs r g b");
      out.R = Num(t[1]); out.G = Num(t[2]); out.B = Num(t[3]);
      continue;
    }

    FBHudElement e;
    if (!Lookup(kKinds, head, e.Kind)) return fail("unknown element '" + head + "'");
    e.R = out.R; e.G = out.G; e.B = out.B;

    for (size_t i = 1; i < t.size();) {
      const std::string &k = t[i];
      auto need = [&](size_t n) { return i + n < t.size(); };
      if (k == "color") {
        if (!need(3)) return fail("color needs r g b");
        e.R = Num(t[i + 1]); e.G = Num(t[i + 2]); e.B = Num(t[i + 3]);
        e.HaveColour = true;
        i += 4;
        continue;
      }
      if (!need(1)) return fail("'" + k + "' without a value");
      const std::string &v = t[i + 1];
      bool vq = q[i + 1];
      i += 2;
      if (k == "x") { e.X = Num(v); continue; }
      if (k == "y") { e.Y = Num(v); continue; }
      if (k == "dx") { e.Dx = Num(v); continue; }
      if (k == "dy") { e.Dy = Num(v); continue; }
      if (k == "w") { e.W = Num(v); continue; }
      if (k == "h") { e.H = Num(v); continue; }
      if (k == "size") { e.Size = Num(v); continue; }
      if (k == "size2") { e.Size2 = Num(v); continue; }
      if (k == "span") { e.Span = Num(v); continue; }
      if (k == "step") { e.Step = Num(v); continue; }
      if (k == "gap") { e.Gap = Num(v); continue; }
      if (k == "len") { e.Len = Num(v); continue; }
      if (k == "count") { e.Count = (int)Num(v); continue; }
      if (k == "gate") { if (!Lookup(kNums, v, e.Gate)) return fail("unknown source '" + v + "'"); continue; }
      if (k == "lo") { e.Lo = Num(v); e.HaveLo = true; continue; }
      if (k == "hi") { e.Hi = Num(v); e.HaveHi = true; continue; }
      if (k == "src") { if (!Lookup(kNums, v, e.Src)) return fail("unknown source '" + v + "'"); continue; }
      if (k == "src2") { if (!Lookup(kNums, v, e.Src2)) return fail("unknown source '" + v + "'"); continue; }
      if (k == "frame") {
        e.Frame = v == "world" ? FBHudFrame::World : v == "body" ? FBHudFrame::Body : FBHudFrame::Screen;
        if (v != "world" && v != "body" && v != "screen") return fail("frame is world|body|screen");
        continue;
      }
      if (k == "align") {
        e.Align = v == "c" ? FBHudAlign::Centre : v == "r" ? FBHudAlign::Right : FBHudAlign::Left;
        if (v != "l" && v != "c" && v != "r") return fail("align is l|c|r");
        continue;
      }
      if (k == "when") {
        std::string f = v;
        if (!f.empty() && f[0] == '!') { e.Invert = true; f = f.substr(1); }
        if (!Lookup(kFlags, f, e.When)) return fail("unknown condition '" + f + "'");
        continue;
      }
      if (k == "fmt") {
        bool bad = false;
        int n = CountConversions(v, bad);
        if (bad) return fail("fmt takes only %f-class conversions");
        if (n > 2) return fail("fmt takes at most two conversions");
        e.Fmt = v;
        continue;
      }
      if (k == "text") {
        if (vq) e.Literal = v;
        else if (!Lookup(kStrs, v, e.Str)) return fail("unknown string source '" + v + "'");
        continue;
      }
      return fail("unknown key '" + k + "'");
    }
    out.Elements.push_back(std::move(e));
  }
  return true;
}

float FBHudNumber(FBHudNum id, const FBState &s, const FBHudEnv &env) {
  const FBRadarContact *lk = Locked(s);
  switch (id) {
    case FBHudNum::None: return 0.0f;
    case FBHudNum::CasKt: return s.AirData.CasKt;
    case FBHudNum::Mach: return s.AirData.Mach;
    case FBHudNum::TasKt: return s.Platform.TasMs * (float)kMsToKt;
    case FBHudNum::GsKt: return s.Platform.GsMs * (float)kMsToKt;
    case FBHudNum::GsMs: return s.Platform.GsMs;
    case FBHudNum::AltFt: return s.Platform.AltM * (float)kMToFt;
    case FBHudNum::AltM: return s.Platform.AltM;
    case FBHudNum::AglFt: return s.RadarAlt.H.Readable() ? s.RadarAlt.AglFt : env.Agl * (float)kMToFt;
    case FBHudNum::AglM: return env.Agl;
    case FBHudNum::VsFpm: return s.Platform.VsMs * (float)kMToFt * 60.0f;
    case FBHudNum::VsMs: return s.Platform.VsMs;
    case FBHudNum::Heading: return s.Platform.YawDeg < 0 ? s.Platform.YawDeg + 360.0f : s.Platform.YawDeg;
    case FBHudNum::Track: return s.AirData.TrackDeg;
    case FBHudNum::Fpa: return s.AirData.FpaDeg;
    case FBHudNum::Pitch: return s.Platform.PitchDeg;
    case FBHudNum::Roll: return s.Platform.RollDeg;
    case FBHudNum::GLoad: return s.AirData.GLoad;
    case FBHudNum::GPeak: return s.AirData.GLoadPeak;
    case FBHudNum::FuelLb: return s.Airframe.FuelLbs;
    case FBHudNum::FuelPct: return s.Airframe.FuelPct;
    case FBHudNum::Gear: return s.Airframe.GearPosition;
    case FBHudNum::Speedbrake: return s.Airframe.SpeedbrakeNorm;
    case FBHudNum::GunRounds: return (float)s.Gun.RoundsRemaining;
    case FBHudNum::GunFired: return (float)s.Gun.RoundsFired;
    case FBHudNum::Station: return (float)s.Stores.SelectedStation;
    case FBHudNum::StoresLoaded: return (float)s.Stores.LoadedCount;
    case FBHudNum::StoresReleased: return (float)s.Stores.ReleasedCount;
    case FBHudNum::Chaff: return (float)s.Cmds.ChaffRemaining;
    case FBHudNum::Flare: return (float)s.Cmds.FlareRemaining;
    case FBHudNum::SteerNum: return (float)s.Ufc.SteerNum;
    case FBHudNum::SteerBrg: return s.Nav.SteerBearingDeg;
    case FBHudNum::SteerRelBrg: return Wrap180(s.Nav.SteerBearingDeg - s.Platform.YawDeg);
    case FBHudNum::SteerDistNm: return s.Nav.SteerDistNm;
    case FBHudNum::SteerTtgS: return s.Cruise.SteerTtgS;
    case FBHudNum::SteerElDeg: return s.Nav.SteerElevAngleDeg;
    case FBHudNum::HomeDistNm: return s.Platform.HomeDistM * (float)kMToNm;
    case FBHudNum::HomeRelBrg: return s.Platform.HomeBearingDeg;
    case FBHudNum::Contacts: return s.Radar.H.Readable() ? (float)s.Radar.ContactCount : 0.0f;
    case FBHudNum::TgtRangeNm: return s.FireControl.TargetRangeM * (float)kMToNm;
    case FBHudNum::TgtClosureKt: return s.FireControl.ClosureMs * (float)kMsToKt;
    case FBHudNum::TgtTtiS: return s.FireControl.TimeToImpactS;
    case FBHudNum::TgtAz: return lk ? lk->BearingDeg : 0.0f;
    case FBHudNum::TgtEl: return lk ? lk->ElevAngleDeg : 0.0f;
    case FBHudNum::TgtAspect: return s.Bfm.H.Readable() ? (float)s.Bfm.AspectDeg : 0.0f;
    case FBHudNum::RminNm: return s.FireControl.RminM * (float)kMToNm;
    case FBHudNum::RtrNm: return s.FireControl.RtrM * (float)kMToNm;
    case FBHudNum::RaeroNm: return s.FireControl.RaeroM * (float)kMToNm;
    case FBHudNum::AgRangeNm: return s.FireControl.AgRangeM * (float)kMToNm;
    case FBHudNum::AgTtrS: return s.FireControl.AgTimeToReleaseS;
    case FBHudNum::AgMissM: return s.FireControl.AgMissM;
    case FBHudNum::GunLeadAz: return s.FireControl.GunLeadAzDeg;
    case FBHudNum::GunLeadEl: return s.FireControl.GunLeadElDeg;
    case FBHudNum::GunSpanMr: return s.FireControl.GunSpanMr;
    case FBHudNum::WeaponCount: return (float)SelectedStoreCount(s);
    case FBHudNum::SunElDeg: return s.Env.SunElDeg;
    case FBHudNum::CloudCover: return s.Env.CloudCover;
    case FBHudNum::SimTimeS: return (float)s.NowS;
    case FBHudNum::WatchFriendly: return env.Watch ? env.Watch->Friendly : 0.0f;
    case FBHudNum::WatchHostile: return env.Watch ? env.Watch->Hostile : 0.0f;
    case FBHudNum::WatchEventAgeS: return env.Watch ? env.Watch->EventAgeS : -1.0f;
    case FBHudNum::WatchSimT: return env.Watch ? env.Watch->SimT : 0.0f;
  }
  return 0.0f;
}

const char *FBHudString(FBHudStr id, const FBState &s, const FBHudEnv &env) {
  static const char *const kModes[] = {"MANUAL", "DIRECT", "COURSE"};
  static char provider[2] = {'B', 0};
  const FBHudWatch *w = env.Watch;
  switch (id) {
    case FBHudStr::None: return "";
    case FBHudStr::Mode: return kModes[(int)s.Platform.Mode <= 2 ? (int)s.Platform.Mode : 0];
    case FBHudStr::RangeProvider: provider[0] = s.FireControl.RangeProvider; return provider;
    case FBHudStr::Weapon: {
      const FBStoreSpec *sp = FBStoreSpecOf(SelectedStoreKind(s));
      return sp ? sp->Key : "";
    }
    case FBHudStr::WatchTitle: return w ? w->Title : "";
    case FBHudStr::WatchMission: return w ? w->Mission : "";
    case FBHudStr::WatchSubject: return w ? w->Subject : "";
    case FBHudStr::WatchSubjectTeam: return w ? w->SubjectTeam : "";
    case FBHudStr::WatchSubjectKind: return w ? w->SubjectKind : "";
    case FBHudStr::WatchShot: return w ? w->Shot : "";
    case FBHudStr::WatchEvent: return w ? w->Event : "";
    case FBHudStr::WatchEventTeam: return w ? w->EventTeam : "";
  }
  return "";
}

bool FBHudFlagOn(FBHudFlag id, const FBState &s, const FBHudEnv &env) {
  const FBRadarContact *lk = Locked(s);
  switch (id) {
    case FBHudFlag::Always: return true;
    case FBHudFlag::Telemetry: return env.Have;
    case FBHudFlag::Airborne: return s.Airframe.H.Readable() && !s.Airframe.WeightOnWheels;
    case FBHudFlag::WeightOnWheels: return s.Airframe.H.Readable() && s.Airframe.WeightOnWheels;
    case FBHudFlag::GearDown: return s.Airframe.H.Readable() && s.Airframe.GearPosition > 0.95f;
    case FBHudFlag::Speedbrake: return s.Airframe.H.Readable() && s.Airframe.SpeedbrakeNorm > 0.05f;
    case FBHudFlag::EngineRunning: return s.Airframe.H.Readable() && s.Airframe.EngineRunning;
    case FBHudFlag::RadarOn: return s.Radar.Powered && s.Radar.Radiating;
    case FBHudFlag::RadarContact: return s.Radar.H.Readable() && s.Radar.ContactCount > 0;
    case FBHudFlag::Locked: return lk != nullptr;
    case FBHudFlag::IffFriendly: return lk && lk->Iff == FBIffReply::Friendly;
    case FBHudFlag::GunReady: return s.Gun.H.Readable() && s.Gun.Ready;
    case FBHudFlag::GunFiring: return s.Gun.H.Readable() && s.Gun.Firing;
    case FBHudFlag::GunValid: return s.FireControl.H.Readable() && s.FireControl.GunValid;
    case FBHudFlag::GunInRange: return s.FireControl.H.Readable() && s.FireControl.GunInRange;
    case FBHudFlag::GunInFunnel: return s.FireControl.H.Readable() && s.FireControl.GunInFunnel;
    case FBHudFlag::DlzValid: return s.FireControl.H.Readable() && s.FireControl.DlzValid;
    case FBHudFlag::InZone: return s.FireControl.H.Readable() && s.FireControl.InZone;
    case FBHudFlag::AgValid: return s.FireControl.H.Readable() && s.FireControl.AgValid;
    case FBHudFlag::AgInRange: return s.FireControl.H.Readable() && s.FireControl.AgInRange;
    case FBHudFlag::AgRelease:
      return s.FireControl.H.Readable() && s.FireControl.AgInRange && s.FireControl.AgTimeToReleaseS <= 0.0f;
    case FBHudFlag::Armed: return s.Stores.H.Readable() && s.Stores.Arm == FBArmState::Arm;
    case FBHudFlag::Designating: return s.Stores.H.Readable() && s.Stores.Designating;
    case FBHudFlag::StoresSelected: return s.Stores.H.Readable() && s.Stores.SelectedStation > 0;
    case FBHudFlag::Supersonic: return s.AirData.H.Readable() && s.AirData.Mach >= 1.0f;
    case FBHudFlag::Bingo: return (s.Warnings.Active & FBWarnBingo) != 0;
    case FBHudFlag::Alow: return (s.Warnings.Active & FBWarnAlow) != 0;
    case FBHudFlag::GearUnsafe: return (s.Warnings.Active & FBWarnGearUnsafe) != 0;
    case FBHudFlag::NavValid: return s.Nav.H.Readable();
    /* THE ILS WINDOW IS THE ORIGINAL'S, quoted: 6 miles from the field and below 5 000 ft AGL
     * [mods/f22/doc/hud.md §5, MAN p.31]. The field is the active steerpoint, which is the closest
     * thing on this bus to a runway — the engine publishes no localiser. */
    case FBHudFlag::IlsWindow:
      return s.Nav.H.Readable() && s.Nav.SteerDistNm <= 6.0f &&
             (s.RadarAlt.H.Readable() ? s.RadarAlt.AglFt : env.Agl * (float)kMToFt) <= 5000.0f;
    case FBHudFlag::WatchHeld: return env.Watch && env.Watch->Held;
    case FBHudFlag::WatchEvent: return env.Watch && env.Watch->EventAgeS >= 0.0f;
  }
  return false;
}

} // namespace FlightBox::Systems
