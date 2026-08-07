#include "Scene.h"

#include <fstream>
#include <sstream>

#include "CivilTime.h"
#include "Json.h"

namespace outshine::Clients {
namespace {

bool Field(const Render::Json::Ref &root, const char *key, double lo, double hi, double &out,
           std::string &err) {
  const Render::Json::Ref v = root[key];
  if (v.GetKind() != Render::Json::Kind::Number) {
    err = std::string("missing or non-numeric field: ") + key;
    return false;
  }
  out = v.Num();
  if (out < lo || out > hi) {
    err = std::string(key) + " out of range [" + std::to_string(lo) + "," + std::to_string(hi) + "]";
    return false;
  }
  return true;
}

}  // namespace

bool Scene::Load(const char *path) {
  Path_ = path ? path : "";
  std::ifstream f(Path_, std::ios::binary);
  if (!f) {
    Error_ = "cannot open " + Path_;
    return false;
  }
  std::ostringstream buf;
  buf << f.rdbuf();
  const std::string text = buf.str();

  Render::Json doc;
  if (!doc.Parse(text.c_str(), text.size())) {
    Error_ = "not valid JSON";
    return false;
  }
  const Render::Json::Ref root = doc.Root();
  if (root.GetKind() != Render::Json::Kind::Object) {
    Error_ = "root is not an object";
    return false;
  }

  if (!Field(root, "lat", -90.0, 90.0, Lat_, Error_)) return false;
  if (!Field(root, "lon", -180.0, 180.0, Lon_, Error_)) return false;
  if (!Field(root, "eyeM", 0.0, 10000.0, EyeM_, Error_)) return false;
  if (!Field(root, "yawDeg", -360.0, 360.0, YawDeg_, Error_)) return false;
  if (!Field(root, "pitchDeg", -90.0, 90.0, PitchDeg_, Error_)) return false;
  if (!Field(root, "fovDeg", 1.0, 179.0, FovDeg_, Error_)) return false;
  if (!Field(root, "windDeg", 0.0, 360.0, WindDeg_, Error_)) return false;
  if (!Field(root, "windMs", 0.0, 120.0, WindMs_, Error_)) return false;
  if (!Field(root, "cloudCover", 0.0, 1.0, CloudCover_, Error_)) return false;

  const Render::Json::Ref utc = root["utc"];
  if (utc.GetKind() != Render::Json::Kind::String) {
    Error_ = "missing or non-string field: utc";
    return false;
  }
  Utc_ = utc.Str();
  if (!ParseIsoUtc(Utc_.c_str(), UtcS_)) {
    Error_ = "utc is not YYYY-MM-DDThh:mm:ssZ: " + Utc_;
    return false;
  }
  return ParseExposure(root);
}

/* "exposure": { "mode": "auto"|"manual", "keyEv"|"compEv": <stops> }
 * Absent -> auto, no compensation. Present -> the mode's OWN stop field is required, because a mode
 * declared without its number is a scene that did not finish saying what it wanted. */
bool Scene::ParseExposure(const Render::Json::Ref &root) {
  const Render::Json::Ref e = root["exposure"];
  if (e.GetKind() == Render::Json::Kind::Invalid) return true;
  if (e.GetKind() != Render::Json::Kind::Object) {
    Error_ = "exposure is not an object";
    return false;
  }
  const Render::Json::Ref mode = e["mode"];
  if (mode.GetKind() != Render::Json::Kind::String) {
    Error_ = "exposure needs a string field: mode (auto|manual)";
    return false;
  }
  const bool manual = mode.StrEquals("manual");
  if (!manual && !mode.StrEquals("auto")) {
    Error_ = "exposure.mode is neither auto nor manual: " + mode.Str();
    return false;
  }
  Exposure_.Mode = manual ? Render::ExposureMode::Manual : Render::ExposureMode::Auto;

  double stops = 0.0;
  const char *stopKey = manual ? "keyEv" : "compEv";
  if (!Field(e, stopKey, -20.0, 20.0, stops, Error_)) return false;
  if (manual) Exposure_.KeyEv = (float)stops;
  else Exposure_.CompEv = (float)stops;
  return true;
}

}  // namespace outshine::Clients
