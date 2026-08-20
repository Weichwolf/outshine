#include "Scene.h"

#include "CivilTime.h"

namespace outshine::Scenario {

bool Scene::Read(const Json::Ref &node, const std::string &path, std::string &err) {
  Fields named(node, path, err);
  if (!named.NeedString("id", Id_)) return false;
  named.Rename(path + "(" + Id_ + ")");

  std::string kind;
  if (!named.NeedString("kind", kind)) return false;
  if (kind == "run") Kind_ = Kind::Run;
  else if (kind == "interactive") Kind_ = Kind::Interactive;
  else return named.Refuse("kind", "is neither interactive nor run: " + kind);

  if (!named.OptionalString("why", Why_)) return false;
  if (!named.Need("fovDeg", 1.0, 179.0, FovDeg_)) return false;
  if (!ReadStage(named, err)) return false;
  if (!ReadExposure(named, err) || !ReadResolution(named, err) || !ReadJitter(named))
    return false;
  if (!named.OptionalInt("settleFrames", -1.0, 4096.0, SettleFrames_)) return false;
  if (Kind_ == Kind::Run && !ReadRuns(named, err)) return false;
  if (!named.Closed()) return false;

  for (const Run &r : Runs_) {
    const bool bench = r.What == Run::Kind::Bench;
    if (bench && !Stage_->AsStudio())
      return named.Refuse("runs", "carries a bench run and the stage is a world, which declares no subject");
    if (!bench && Stage_->AsStudio())
      return named.Refuse("runs", "carries a run that needs a world and the stage is a studio");
  }
  if (Kind_ == Kind::Run && Stage_->AsStudio() && Runs_.size() != 1)
    return named.Refuse("runs", "is a studio and a studio holds exactly one subject, so it makes exactly one product");
  return true;
}

bool Scene::ReadStage(Fields &scene, std::string &err) {
  Fields stage(scene.Child("stage"), scene.Under("stage"), err);
  const bool world = stage.Present("world"), studio = stage.Present("studio");
  if (world == studio)
    return stage.Refuse(world ? "declares both a world and a studio, and a scene shows one thing"
                              : "declares neither a world nor a studio");
  const char *arm = world ? "world" : "studio";
  const Json::Ref node = stage.Child(arm);
  if (!stage.Closed()) return false;
  return world ? ReadWorld(node, stage.Under(arm), err) : ReadStudio(node, stage.Under(arm), err);
}

bool Scene::ReadWorld(const Json::Ref &node, const std::string &path, std::string &err) {
  Fields f(node, path, err);
  double lat = 0.0, lon = 0.0;

  if (!f.Need("lat", -90.0, 90.0, lat) || !f.Need("lon", -180.0, 180.0, lon)) return false;
  const std::optional<Standpoint> where = Standpoint::At(lat, lon);
  if (!where)
    return f.Refuse("lat", "is outside the Mercator band this world is entered through, ±" +
                               std::to_string(kMercatorLatMaxDeg) + ": " + std::to_string(lat));
  WorldStage w(*where);

  if (!f.Need("eyeM", 0.0, 10000.0, w.EyeAglM)) return false;
  if (!f.Need("yawDeg", -360.0, 360.0, w.YawDeg)) return false;
  if (!f.Need("pitchDeg", -90.0, 90.0, w.PitchDeg)) return false;
  if (!f.Need("windDeg", 0.0, 360.0, w.WindFromDeg)) return false;
  if (!f.Need("windMs", 0.0, 120.0, w.WindMs)) return false;
  if (!f.Need("cloudCover", 0.0, 1.0, w.CloudCover)) return false;
  if (!f.NeedString("utc", w.Utc)) return false;
  if (!ParseIsoUtc(w.Utc.c_str(), w.UtcS))
    return f.Refuse("utc", "is not YYYY-MM-DDThh:mm:ssZ: " + w.Utc);

  w.HasLensAslM = f.Present("lensAslM");
  if (w.HasLensAslM && !f.Need("lensAslM", -500.0, 20000.0, w.LensAslM)) return false;
  if (!f.Optional("windClockS", 0.0, 1.0e6, w.WindClockS)) return false;
  double viewKm = w.ViewM / 1000.0;
  if (!f.Optional("viewKm", 0.1, 1000.0, viewKm)) return false;
  w.ViewM = viewKm * 1000.0;
  if (!f.Optional("orthoM", 0.0, 1.0e6, w.OrthoM)) return false;
  if (!f.OptionalString("snapshot", w.SnapshotPath)) return false;
  if (!f.Closed()) return false;

  Stage_.emplace(std::move(w));
  return true;
}

bool Scene::ReadStudio(const Json::Ref &node, const std::string &path, std::string &err) {
  Fields f(node, path, err);
  StudioStage s;

  Fields sub(f.Child("substrate"), f.Under("substrate"), err);
  if (!sub.OptionalString("class", s.Ground.MaterialClass)) return false;
  if (!sub.Need("groundAslM", -500.0, 9000.0, s.Ground.GroundAslM)) return false;
  if (!sub.Closed()) return false;

  Fields key(f.Child("keyLight"), f.Under("keyLight"), err);
  if (!key.Need("elevationDeg", -90.0, 90.0, s.Key.ElevationDeg)) return false;
  if (!key.Closed()) return false;

  std::string backdrop = "card";
  if (!f.OptionalString("backdrop", backdrop)) return false;
  if (backdrop == "card") s.Behind = Backdrop::Card;
  else if (backdrop == "none") s.Behind = Backdrop::None;
  else return f.Refuse("backdrop", "is neither card nor none: " + backdrop);

  if (!ReadSubject(f.Child("subject"), f.Under("subject"), s.Stands, err)) return false;
  if (!f.Closed()) return false;

  Stage_.emplace(std::move(s));
  return true;
}

bool Scene::ReadSubject(const Json::Ref &node, const std::string &path, Subject &out,
                        std::string &err) {
  Fields f(node, path, err);
  std::string generator;
  if (!f.NeedString("generator", generator)) return false;
  if (generator == "tree") {
    TreeSubject t;
    if (!f.NeedString("species", t.Species)) return false;
    if (!f.Optional("heightM", 0.0, 200.0, t.HeightM)) return false;
    if (!f.OptionalInt("leafMult", 1.0, 64.0, t.LeafMult)) return false;
    std::string foliage = "leaves";
    if (!f.OptionalString("foliage", foliage)) return false;
    if (foliage == "leaves") t.Leaf = Foliage::Leaves;
    else if (foliage == "bare") t.Leaf = Foliage::Bare;
    else return f.Refuse("foliage", "is neither bare nor leaves: " + foliage);
    out.What = t;
  } else if (generator == "sward") {
    SwardSubject g;
    if (!f.NeedString("template", g.Template)) return false;
    if (!f.Optional("heightM", 0.0, 200.0, g.HeightM)) return false;
    out.What = g;
  } else {
    return f.Refuse("generator", "names no generator this engine has: " + generator);
  }
  return f.Closed();
}

bool Scene::ReadExposure(Fields &scene, std::string &err) {
  if (!scene.Present("exposure")) { scene.Seen("exposure"); return true; }
  Fields f(scene.Child("exposure"), scene.Under("exposure"), err);
  std::string mode;
  if (!f.NeedString("mode", mode)) return false;
  const bool manual = mode == "manual";
  if (!manual && mode != "auto") return f.Refuse("mode", "is neither auto nor manual: " + mode);
  Exposure_.Mode = manual ? ExposureMode::Manual : ExposureMode::Auto;

  double stops = 0.0;
  if (!f.Need(manual ? "keyEv" : "compEv", -20.0, 20.0, stops)) return false;
  if (manual) Exposure_.KeyEv = (float)stops;
  else Exposure_.CompEv = (float)stops;
  return f.Closed();
}

bool Scene::ReadResolution(Fields &scene, std::string &err) {
  if (!scene.Present("render")) { scene.Seen("render"); return true; }
  Fields f(scene.Child("render"), scene.Under("render"), err);
  double w = 0.0, h = 0.0;
  if (!f.Need("width", 16.0, 8192.0, w) || !f.Need("height", 16.0, 8192.0, h)) return false;
  Resolution_.Width = (int)w;
  Resolution_.Height = (int)h;
  if (!f.OptionalString("why", Resolution_.Why)) return false;
  const bool budget = Resolution_.Width == kBudgetWidth && Resolution_.Height == kBudgetHeight;
  if (!budget && Resolution_.Why.empty())
    return f.Refuse("why", "is missing and " + std::to_string(Resolution_.Width) + "x" +
                               std::to_string(Resolution_.Height) + " is not the budget's " +
                               std::to_string(kBudgetWidth) + "x" + std::to_string(kBudgetHeight));
  return f.Closed();
}

bool Scene::ReadJitter(Fields &scene) {
  if (!scene.Present("jitter")) { scene.Seen("jitter"); return true; }
  const Json::Ref jitter = scene.Child("jitter");
  if (jitter.GetKind() != Json::Kind::Array || jitter.Size() != 2 ||
      jitter[(size_t)0].GetKind() != Json::Kind::Number ||
      jitter[(size_t)1].GetKind() != Json::Kind::Number)
    return scene.Refuse("jitter", "is not two numbers, in pixels");
  JitterPin_[0] = jitter[(size_t)0].Num();
  JitterPin_[1] = jitter[(size_t)1].Num();
  for (const double v : JitterPin_)
    if (v < -0.5 || v > 0.5)
      return scene.Refuse("jitter", "is outside [-0.5,0.5] px: " + std::to_string(v));
  HasJitterPin_ = true;
  return true;
}

bool Scene::ReadMotion(Fields &run, Run &out) {
  Run::MotionRun &m = out.Motion;
  if (!run.OptionalInt("frames", 1.0, 1.0e6, m.Frames)) return false;
  if (!run.Optional("fps", 1.0, 1000.0, m.Fps)) return false;

  std::string world = "frozen";
  if (!run.OptionalString("world", world)) return false;
  if (world == "streaming") m.World = Run::Stream::Streaming;
  else if (world == "frozen") m.World = Run::Stream::Frozen;
  else return run.Refuse("world", "is neither frozen nor streaming: " + world);

  std::string give = "stills";
  if (!run.OptionalString("give", give)) return false;
  if (give == "profile") m.Give = Run::Product::Profile;
  else if (give == "stills") m.Give = Run::Product::Stills;
  else return run.Refuse("give", "is neither stills nor profile: " + give);

  if (!run.NeedString("path", m.Path)) return false;
  if (!run.OptionalString("depth", m.Depth)) return false;
  return true;
}

bool Scene::ReadRuns(Fields &scene, std::string &err) {
  const Json::Ref rs = scene.Child("runs");
  if (rs.GetKind() != Json::Kind::Array || rs.Size() == 0)
    return scene.Refuse("runs", "is missing or empty, and a run scene records something");
  for (size_t i = 0; i < rs.Size(); i++) {
    const std::string path = scene.Under("runs") + "[" + std::to_string(i) + "]";
    Fields f(rs[i], path, err);
    std::string kind;
    if (!f.NeedString("kind", kind)) return false;
    Run out;
    if (kind == "motion") {
      out.What = Run::Kind::Motion;
      if (!ReadMotion(f, out)) return false;

      f.Seen("animation");
      if (f.Present("animation")) {
        std::string why;
        if (!out.Motion.Move.Read(f.Child("animation"), why)) return f.Refuse("animation", why);
      }
    } else if (kind == "classDump") {
      out.What = Run::Kind::ClassDump;
      if (!f.NeedString("path", out.ClassDump.Path)) return false;
      if (!f.Optional("spanM", 1.0, 1.0e5, out.ClassDump.SpanM)) return false;
      if (!f.Optional("stepM", 1.0e-3, 100.0, out.ClassDump.StepM)) return false;
    } else if (kind == "classCompare") {
      out.What = Run::Kind::ClassCompare;
    } else if (kind == "windProbe") {
      out.What = Run::Kind::WindProbe;
      if (!f.NeedString("path", out.WindProbe.Path)) return false;
      if (!f.OptionalInt("samples", 1.0, 1.0e5, out.WindProbe.Samples)) return false;
      if (!f.OptionalInt("frames", 1.0, 1.0e5, out.WindProbe.Frames)) return false;
      if (!f.Optional("dxM", 1.0e-4, 100.0, out.WindProbe.DxM)) return false;
      if (!f.Optional("dtS", 1.0e-6, 3600.0, out.WindProbe.DtS)) return false;
    } else if (kind == "bench") {
      out.What = Run::Kind::Bench;
      if (!f.OptionalString("dir", out.Bench.Dir)) return false;
      if (!f.OptionalInt("turnSteps", 1.0, 360.0, out.Bench.TurnSteps)) return false;
    } else {
      return f.Refuse("kind", "names no run this engine has: " + kind);
    }
    if (!f.Closed()) return false;
    Runs_.push_back(std::move(out));
  }
  return true;
}

}
