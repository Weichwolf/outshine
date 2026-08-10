#include "Scene.h"

#include "CivilTime.h"

namespace outshine::Clients {
namespace {

using Ref = Render::Json::Ref;
using JKind = Render::Json::Kind;

bool Need(const Ref &node, const char *key, double lo, double hi, double &out, std::string &err) {
  const Ref v = node[key];
  if (v.GetKind() != JKind::Number) {
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

bool Optional(const Ref &node, const char *key, double lo, double hi, double &out,
              std::string &err) {
  return node[key].GetKind() == JKind::Invalid ? true : Need(node, key, lo, hi, out, err);
}

bool OptionalInt(const Ref &node, const char *key, double lo, double hi, int &out,
                 std::string &err) {
  double v = (double)out;
  if (!Optional(node, key, lo, hi, v, err)) return false;
  out = (int)v;
  return true;
}

bool NeedString(const Ref &node, const char *key, std::string &out, std::string &err) {
  const Ref v = node[key];
  if (v.GetKind() != JKind::String) {
    err = std::string("missing or non-string field: ") + key;
    return false;
  }
  out = v.Str();
  return true;
}

}  // namespace

bool Scene::Read(const Ref &node, std::string &err) {
  if (node.GetKind() != JKind::Object) {
    err = "scene is not an object";
    return false;
  }
  if (!NeedString(node, "id", Id_, err)) return false;

  const Ref kind = node["kind"];
  if (kind.StrEquals("run")) Kind_ = Kind::Run;
  else if (kind.StrEquals("interactive")) Kind_ = Kind::Interactive;
  else {
    err = "scene " + Id_ + ": kind is neither interactive nor run";
    return false;
  }

  if (!Need(node, "lat", -90.0, 90.0, Lat_, err)) return false;
  if (!Need(node, "lon", -180.0, 180.0, Lon_, err)) return false;
  if (!Need(node, "eyeM", 0.0, 10000.0, EyeM_, err)) return false;
  if (!Need(node, "yawDeg", -360.0, 360.0, YawDeg_, err)) return false;
  if (!Need(node, "pitchDeg", -90.0, 90.0, PitchDeg_, err)) return false;
  if (!Need(node, "fovDeg", 1.0, 179.0, FovDeg_, err)) return false;
  if (!Need(node, "windDeg", 0.0, 360.0, WindDeg_, err)) return false;
  if (!Need(node, "windMs", 0.0, 120.0, WindMs_, err)) return false;
  if (!Need(node, "cloudCover", 0.0, 1.0, CloudCover_, err)) return false;
  if (!NeedString(node, "utc", Utc_, err)) return false;
  if (!ParseIsoUtc(Utc_.c_str(), UtcS_)) {
    err = "utc is not YYYY-MM-DDThh:mm:ssZ: " + Utc_;
    return false;
  }

  HasLensAslM_ = node["lensAslM"].GetKind() == JKind::Number;
  if (HasLensAslM_ && !Need(node, "lensAslM", -500.0, 20000.0, LensAslM_, err)) return false;
  if (!Optional(node, "windClockS", 0.0, 1.0e6, WindClockS_, err)) return false;
  if (!Optional(node, "viewKm", 0.1, 1000.0, ViewKm_, err)) return false;
  if (!Optional(node, "orthoM", 0.0, 1.0e6, OrthoM_, err)) return false;
  if (node["snapshot"].GetKind() == JKind::String) Snapshot_ = node["snapshot"].Str();

  if (!ReadExposure(node, err) || !ReadResolution(node, err)) return false;
  if (!OptionalInt(node, "settleFrames", -1.0, 4096.0, SettleFrames_, err)) return false;
  if (Kind_ != Kind::Run) return true;
  return ReadRuns(node, err);
}

/* "exposure": { "mode": "auto"|"manual", "keyEv"|"compEv": <stops> }
 * Absent -> auto, no compensation. Present -> the mode's OWN stop field is required, because a mode
 * declared without its number is a scene that did not finish saying what it wanted. */
bool Scene::ReadExposure(const Ref &node, std::string &err) {
  const Ref e = node["exposure"];
  if (e.GetKind() == JKind::Invalid) return true;
  if (e.GetKind() != JKind::Object) {
    err = "exposure is not an object";
    return false;
  }
  const Ref mode = e["mode"];
  if (mode.GetKind() != JKind::String) {
    err = "exposure needs a string field: mode (auto|manual)";
    return false;
  }
  const bool manual = mode.StrEquals("manual");
  if (!manual && !mode.StrEquals("auto")) {
    err = "exposure.mode is neither auto nor manual: " + mode.Str();
    return false;
  }
  Exposure_.Mode = manual ? Render::ExposureMode::Manual : Render::ExposureMode::Auto;

  double stops = 0.0;
  if (!Need(e, manual ? "keyEv" : "compEv", -20.0, 20.0, stops, err)) return false;
  if (manual) Exposure_.KeyEv = (float)stops;
  else Exposure_.CompEv = (float)stops;
  return true;
}

/* "render": { "width": <px>, "height": <px>, "why": "<reason>" }
 * Absent -> the budget's 1280x720. The reason is REQUIRED for any other size and refused when
 * missing: a size nobody justified is how the budget's subject drifts without anyone noticing. */
bool Scene::ReadResolution(const Ref &node, std::string &err) {
  const Ref r = node["render"];
  if (r.GetKind() == JKind::Invalid) return true;
  if (r.GetKind() != JKind::Object) {
    err = "scene " + Id_ + ": render is not an object";
    return false;
  }
  double w = 0.0, h = 0.0;
  if (!Need(r, "width", 16.0, 8192.0, w, err)) return false;
  if (!Need(r, "height", 16.0, 8192.0, h, err)) return false;
  Resolution_.Width = (int)w;
  Resolution_.Height = (int)h;
  const bool budget = Resolution_.Width == kBudgetWidth && Resolution_.Height == kBudgetHeight;
  if (r["why"].GetKind() == JKind::String) Resolution_.Why = r["why"].Str();
  if (!budget && Resolution_.Why.empty()) {
    err = "scene " + Id_ + ": render " + std::to_string(Resolution_.Width) + "x" +
          std::to_string(Resolution_.Height) + " is not the budget's " +
          std::to_string(kBudgetWidth) + "x" + std::to_string(kBudgetHeight) +
          " and states no why";
    return false;
  }
  return true;
}

bool Scene::ReadMotion(const Ref &node, Run &run, std::string &err) {
  Run::MotionRun &m = run.Motion;
  if (!OptionalInt(node, "frames", 1.0, 1.0e6, m.Frames, err)) return false;
  if (!Optional(node, "fps", 1.0, 1000.0, m.Fps, err)) return false;
  if (node["animation"].GetKind() != JKind::Invalid && !m.Move.Read(node["animation"], err))
    return false;

  const Ref world = node["world"];
  if (world.GetKind() == JKind::String) {
    if (world.StrEquals("streaming")) m.World = Run::Stream::Streaming;
    else if (world.StrEquals("frozen")) m.World = Run::Stream::Frozen;
    else {
      err = "motion.world is neither frozen nor streaming: " + world.Str();
      return false;
    }
  }
  const Ref give = node["give"];
  if (give.GetKind() == JKind::String) {
    if (give.StrEquals("profile")) m.Give = Run::Product::Profile;
    else if (give.StrEquals("stills")) m.Give = Run::Product::Stills;
    else {
      err = "motion.give is neither stills nor profile: " + give.Str();
      return false;
    }
  }
  if (!NeedString(node, "path", m.Path, err)) return false;
  if (node["depth"].GetKind() == JKind::String) m.Depth = node["depth"].Str();
  return true;
}

bool Scene::ReadRuns(const Ref &node, std::string &err) {
  const Ref rs = node["runs"];
  if (rs.GetKind() != JKind::Array || rs.Size() == 0) {
    err = "scene " + Id_ + ": a run scene needs a non-empty runs array";
    return false;
  }
  for (size_t i = 0; i < rs.Size(); i++) {
    const Ref r = rs[i];
    const Ref kind = r["kind"];
    Run out;
    if (kind.StrEquals("motion")) {
      out.What = Run::Kind::Motion;
      if (!ReadMotion(r, out, err)) return false;
    } else if (kind.StrEquals("classDump")) {
      out.What = Run::Kind::ClassDump;
      if (!NeedString(r, "path", out.ClassDump.Path, err)) return false;
      if (!Optional(r, "spanM", 1.0, 1.0e5, out.ClassDump.SpanM, err)) return false;
      if (!Optional(r, "stepM", 1.0e-3, 100.0, out.ClassDump.StepM, err)) return false;
    } else if (kind.StrEquals("classCompare")) {
      out.What = Run::Kind::ClassCompare;
    } else if (kind.StrEquals("windProbe")) {
      out.What = Run::Kind::WindProbe;
      if (!NeedString(r, "path", out.WindProbe.Path, err)) return false;
      if (!OptionalInt(r, "samples", 1.0, 1.0e5, out.WindProbe.Samples, err)) return false;
      if (!OptionalInt(r, "frames", 1.0, 1.0e5, out.WindProbe.Frames, err)) return false;
      if (!Optional(r, "dxM", 1.0e-4, 100.0, out.WindProbe.DxM, err)) return false;
      if (!Optional(r, "dtS", 1.0e-6, 3600.0, out.WindProbe.DtS, err)) return false;
    } else if (kind.StrEquals("subject")) {
      out.What = Run::Kind::Subject;
      if (r["template"].GetKind() == JKind::String) out.Subject.Template = r["template"].Str();
      if (r["species"].GetKind() == JKind::String) out.Subject.Species = r["species"].Str();
      if (out.Subject.Template.empty() == out.Subject.Species.empty()) {
        err = "subject run needs exactly one of template / species";
        return false;
      }
      if (r["dir"].GetKind() == JKind::String) out.Subject.Dir = r["dir"].Str();
      if (!Optional(r, "heightM", 0.0, 200.0, out.Subject.HeightM, err)) return false;
      if (!OptionalInt(r, "turnSteps", 1.0, 360.0, out.Subject.TurnSteps, err)) return false;
      if (!OptionalInt(r, "leafMult", 1.0, 64.0, out.Subject.LeafMult, err)) return false;
      const Ref leaf = r["foliage"];
      if (leaf.GetKind() == JKind::String) {
        if (leaf.StrEquals("bare")) out.Subject.Leaf = Run::Foliage::Bare;
        else if (leaf.StrEquals("leaves")) out.Subject.Leaf = Run::Foliage::Leaves;
        else {
          err = "subject.foliage is neither bare nor leaves: " + leaf.Str();
          return false;
        }
      }
    } else {
      err = "scene " + Id_ + ": unknown run kind " + kind.Str("(missing)");
      return false;
    }
    Runs_.push_back(out);
  }
  /* THE SUBJECT BENCH REPLACES THE WORLD (doc/goal.md §3) — no tiles, no OSM, no DEM — so it cannot
   * share a scene with a run that needs one. */
  for (const Run &r : Runs_)
    if (r.What == Run::Kind::Subject && Runs_.size() != 1) {
      err = "scene " + Id_ + ": a subject run has no world and must be the only run in its scene";
      return false;
    }
  return true;
}

}  // namespace outshine::Clients
