#include "Snapshot.h"

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <fstream>
#include <sstream>

#include "Json.h"
#include "Scene.h"

namespace outshine::Clients {
namespace {

/* ONE LINE, so the log is a sequence a reader can tail and cut. */
void Put(std::string &s, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
void Put(std::string &s, const char *fmt, ...) {
  char buf[512];
  va_list ap;
  va_start(ap, fmt);
  const int n = vsnprintf(buf, sizeof buf, fmt, ap);
  va_end(ap);
  if (n > 0) s.append(buf, (size_t)(n < (int)sizeof buf ? n : (int)sizeof buf - 1));
}

std::string Quote(const std::string &in) {
  std::string out;
  for (const char c : in) {
    if (c == '"' || c == '\\') { out += '\\'; out += c; }
    else if ((unsigned char)c >= 0x20) out += c;
  }
  return out;
}

bool Num(const Render::Json::Ref &r, const char *key, double &out, std::string &err) {
  const Render::Json::Ref v = r[key];
  if (v.GetKind() != Render::Json::Kind::Number) {
    err = std::string("missing or non-numeric field: ") + key;
    return false;
  }
  out = v.Num();
  return true;
}

bool Near(double a, double b, double tol) { return std::fabs(a - b) <= tol; }

}  // namespace

void Snapshot::SetName(const char *name) { Name_ = name ? name : ""; }

void Snapshot::SetScene(const Scene &s) {
  ScenePath_ = s.Id();
  SceneLat_ = s.Lat();
  SceneLon_ = s.Lon();
  SceneEyeM_ = s.EyeM();
  SceneYawDeg_ = s.YawDeg();
  ScenePitchDeg_ = s.PitchDeg();
  SceneFovDeg_ = s.FovDeg();
  SceneUtcS_ = s.UtcS();
}

void Snapshot::SetCamera(double lat, double lon, double yawDeg, double pitchDeg) {
  Lat_ = lat;
  Lon_ = lon;
  YawDeg_ = yawDeg;
  PitchDeg_ = pitchDeg;
}

void Snapshot::SetDerived(double groundM, double altAslM, double sunElDeg, double sunAzDeg) {
  GroundM_ = groundM;
  AltAslM_ = altAslM;
  SunElDeg_ = sunElDeg;
  SunAzDeg_ = sunAzDeg;
}

void Snapshot::SetClient(const char *name, double clockMs) {
  Client_ = name ? name : "";
  ClockMs_ = clockMs;
}

/* 9 decimals of degree = 0.11 mm — past anything the DEM or the eye can tell apart, and short enough
 * that the line stays readable. */
std::string Snapshot::Text() const {
  std::string s;
  s.reserve(768);
  Put(s, "{\"format\":\"outshine-snapshot\",\"version\":1");
  Put(s, ",\"name\":\"%s\"", Quote(Name_).c_str());
  Put(s, ",\"png\":\"%s.png\"", Quote(Name_).c_str());
  Put(s, ",\"client\":\"%s\",\"clockMs\":%.1f", Quote(Client_).c_str(), ClockMs_);
  Put(s, ",\"camera\":{\"lat\":%.9f,\"lon\":%.9f,\"yawDeg\":%.6f,\"pitchDeg\":%.6f}",
      Lat_, Lon_, YawDeg_, PitchDeg_);
  Put(s, ",\"scene\":{\"path\":\"%s\",\"lat\":%.9f,\"lon\":%.9f,\"eyeM\":%.4f,\"yawDeg\":%.6f,"
         "\"pitchDeg\":%.6f,\"fovDeg\":%.6f,\"utcS\":%lld}",
      Quote(ScenePath_).c_str(), SceneLat_, SceneLon_, SceneEyeM_, SceneYawDeg_, ScenePitchDeg_,
      SceneFovDeg_, (long long)SceneUtcS_);
  Put(s, ",\"derived\":{\"groundM\":%.4f,\"altAslM\":%.4f,\"sunElDeg\":%.4f,\"sunAzDeg\":%.4f}",
      GroundM_, AltAslM_, SunElDeg_, SunAzDeg_);
  Put(s, "}");
  return s;
}

bool Snapshot::Load(const char *path) {
  std::ifstream f(path ? path : "", std::ios::binary);
  if (!f) {
    Error_ = std::string("cannot open ") + (path ? path : "");
    return false;
  }
  std::ostringstream buf;
  buf << f.rdbuf();
  const std::string text = buf.str();
  return LoadText(text.c_str(), text.size());
}

bool Snapshot::LoadText(const char *text, size_t len) {
  Render::Json doc;
  if (!doc.Parse(text, len)) {
    Error_ = "not valid JSON";
    return false;
  }
  const Render::Json::Ref root = doc.Root();
  if (root.GetKind() != Render::Json::Kind::Object) {
    Error_ = "root is not an object";
    return false;
  }
  if (!root["format"].StrEquals("outshine-snapshot")) {
    Error_ = "not an outshine snapshot";
    return false;
  }
  Name_ = root["name"].Str();
  Client_ = root["client"].Str();
  ClockMs_ = root["clockMs"].Num();

  const Render::Json::Ref cam = root["camera"];
  if (cam.GetKind() != Render::Json::Kind::Object) {
    Error_ = "missing camera object";
    return false;
  }
  if (!Num(cam, "lat", Lat_, Error_)) return false;
  if (!Num(cam, "lon", Lon_, Error_)) return false;
  if (!Num(cam, "yawDeg", YawDeg_, Error_)) return false;
  if (!Num(cam, "pitchDeg", PitchDeg_, Error_)) return false;

  const Render::Json::Ref sc = root["scene"];
  if (sc.GetKind() != Render::Json::Kind::Object) {
    Error_ = "missing scene object";
    return false;
  }
  ScenePath_ = sc["path"].Str();
  if (!Num(sc, "lat", SceneLat_, Error_)) return false;
  if (!Num(sc, "lon", SceneLon_, Error_)) return false;
  if (!Num(sc, "eyeM", SceneEyeM_, Error_)) return false;
  if (!Num(sc, "yawDeg", SceneYawDeg_, Error_)) return false;
  if (!Num(sc, "pitchDeg", ScenePitchDeg_, Error_)) return false;
  if (!Num(sc, "fovDeg", SceneFovDeg_, Error_)) return false;
  SceneUtcS_ = (int64_t)sc["utcS"].Num();

  const Render::Json::Ref d = root["derived"];
  GroundM_ = d["groundM"].Num();
  AltAslM_ = d["altAslM"].Num();
  SunElDeg_ = d["sunElDeg"].Num();
  SunAzDeg_ = d["sunAzDeg"].Num();
  return true;
}

/* THE GUARD THAT KEEPS THIS FROM BECOMING A SECOND DECLARATION: the snapshot names the scene it was
 * taken in, and re-rendering it against a different one is refused rather than approximated. The
 * tolerances are the JSON round-trip's last digit, not a similarity measure — any real divergence is
 * orders of magnitude larger than these. */
bool Snapshot::Matches(const Scene &s) {
  const char *bad = nullptr;
  if (!Near(SceneLat_, s.Lat(), 1e-7)) bad = "lat";
  else if (!Near(SceneLon_, s.Lon(), 1e-7)) bad = "lon";
  else if (!Near(SceneEyeM_, s.EyeM(), 1e-3)) bad = "eyeM";
  else if (!Near(SceneFovDeg_, s.FovDeg(), 1e-4)) bad = "fovDeg";
  else if (SceneUtcS_ != s.UtcS()) bad = "utcS";
  if (!bad) return true;
  Error_ = std::string("snapshot was taken in a different scene: ") + bad;
  return false;
}

} // namespace outshine::Clients
