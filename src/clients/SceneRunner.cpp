#include "SceneRunner.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <map>

#include "Geodesy.h"
#include "Json.h"
#include "Log.h"
#include "TreeFoliage.h"
#include "TreeGrower.h"
#include "TreeLeaf.h"
#include "TreeMesh.h"
#include "ModelLadder.h"
#include "Species.h"
#include "TreePrototype.h"
#include "TreeSpecies.h"
#include "Units.h"
#include "WindField.h"

namespace outshine::Clients {
namespace {

using Target = Animation::Target;

/* THE BENCH'S CARD IS A DATUM, not a DEM sample: the subject bench has to run with no network at
 * all, and a plant's verdict may not depend on a tile server answering. 100.6 m is the reference
 * scene's own measured ground (`/elev?lat=52.10602&lon=9.43453&block=1`), so the air column over the
 * bench is the air column over the scene. */
const double kSubjectGroundAslM = 100.6;
const char *kSpeciesDir = "src/assets/world/species";

double MsBetween(std::chrono::steady_clock::time_point a, std::chrono::steady_clock::time_point b) {
  return std::chrono::duration<double, std::milli>(b - a).count();
}

double NowMs() {
  return std::chrono::duration<double, std::milli>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

/* [SET] What a delivered product may still be on the wire for once the run has nothing left to do.
 * The collector's own POST timeout is 4 s natively, so this is that with room for the largest
 * product this tree writes — a 61 MiB class dump. */
constexpr double kDeliverWaitMs = 20000.0;

double Percentile(const std::vector<double> &sorted, double p) {
  if (sorted.empty()) return 0.0;
  return sorted[(size_t)(p * (double)(sorted.size() - 1) + 0.5)];
}

/* THE BENCH'S FLOOR IS THE TEMPLATE'S DECLARED SUBSTRATE, and it has to be read from the class table
 * rather than from the resolved vegetation row: `swardClosure` overwrites a row's ground reflectance
 * with the stand's own aggregate colour — for `meadow` completely. On the bench that would be the
 * subject's colour standing in for the ground it is judged against. */
[[nodiscard]] bool SubjectSubstrate(const World::GroundMaterials &mats, const std::string &vegPath,
                      const std::string &tpl, std::string *name, float rgb[3]) {
  FILE *f = fopen(vegPath.c_str(), "rb");
  if (!f) return false;
  std::string text;
  char buf[8192];
  for (size_t n; (n = fread(buf, 1, sizeof buf, f)) > 0;) text.append(buf, n);
  fclose(f);

  Json doc;
  if (!doc.Parse(text.c_str(), text.size())) return false;
  const Json::Ref ts = doc.Root()["templates"];
  for (size_t i = 0; i < ts.Size(); i++) {
    if (ts[i]["name"].Str() != tpl) continue;
    const std::string cls = ts[i]["ground"]["class"].Str();
    const int mi = mats.Find(cls);
    if (mi < 0) return false;
    *name = cls;
    for (int c = 0; c < 3; c++) rgb[c] = mats.At((size_t)mi).Albedo[c];
    return true;
  }
  return false;
}

}  // namespace


/* ONE TURN. Everything below is entered from here and returns to here; no stage loops. */
SceneRunner::Progress SceneRunner::Step() {
  switch (Stage_) {
    case Stage::BringUp: BringUp(); break;
    case Stage::Dispatch: Dispatch(); break;
    case Stage::Settle:
      if (SettleAt_ < Settle_) { App_.Frame(); SettleAt_++; break; }
      Stage_ = Stage::SettleDrain;
      break;
    /* THE SETTLE FRAMES ARE SUBMITTED AND NOT WAITED FOR, so without this the first timed frame pays
     * the whole queue: measured 503.8 ms of which 496.0 was GPU, against a 11.3 ms median over the
     * same 240 frames. That is a number about the encoder, not about the frame. */
    case Stage::SettleDrain:
      if (App_.Renderer().GpuIdle() == Render::ReadState::Pending) break;
      Log::Info("run", "settled", {{"frames", Settle_}, {"path", Move_->Path},
          {"channels", (double)Move_->Move.ChannelCount()}});
      FrameAt_ = 0;
      Stage_ = Stage::Frame;
      break;
    case Stage::Frame: MotionFrame(); break;
    case Stage::Still: WriteStill(); break;
    case Stage::Depth: WriteDepth(); break;
    case Stage::FrameDrain: MotionRow(); break;
    case Stage::MotionClose: MotionClose(); break;
    case Stage::CompareViz:
    case Stage::CompareDepth: CompareClasses(); break;
    case Stage::Irradiance: Settled(); break;
    case Stage::Exposure: Counters(); break;
    case Stage::Bench:
      switch (Bench_->Step()) {
        case SubjectBench::Progress::Running: break;
        case SubjectBench::Progress::Done: Finish(Rc_); break;
        case SubjectBench::Progress::Failed: Finish(1); break;
      }
      break;
    case Stage::Deliver: {
      switch (Out_.Settle()) {
        case Artifacts::Delivery::Complete: Stage_ = Stage::Done; break;
        case Artifacts::Delivery::Refused:
          Log::Error("run", "product_refused");
          if (!Rc_) Rc_ = 1;
          Stage_ = Stage::Done;
          break;
        case Artifacts::Delivery::InFlight:
          if (NowMs() - StageFromMs_ > kDeliverWaitMs) {
            Log::Error("run", "product_undelivered", {{"waitedMs", NowMs() - StageFromMs_}});
            if (!Rc_) Rc_ = 1;
            Stage_ = Stage::Done;
          }
          break;
      }
      break;
    }
    case Stage::Done: return Progress::Done;
  }
  return Stage_ == Stage::Done ? Progress::Done : Progress::Running;
}

void SceneRunner::Finish(int rc) {
  Rc_ = rc;
  StageFromMs_ = NowMs();
  Stage_ = Stage::Deliver;
}

/* THE BRING-UP IS THE RUN'S FIRST STAGE and not the client's preamble: whether a scene needs a world
 * at all is the scene's own statement, and the subject bench is the one that does not. */
void SceneRunner::BringUp() {
  if (App_.Busy()) {
    App_.Step();
    return;
  }
  switch (App_.Stage()) {
    case Outshine::Phase::Prepared:
      if (!Scene_.Runs().empty() && Scene_.Runs().front().What == Scene::Run::Kind::Subject) {
        if (!BenchBegin()) { Finish(1); return; }
        Stage_ = Stage::Bench;
        return;
      }
      App_.Open();
      return;
    case Outshine::Phase::Playing: Stage_ = Stage::Dispatch; return;
    case Outshine::Phase::Declared:
    case Outshine::Phase::Device:
    case Outshine::Phase::Ground:
    case Outshine::Phase::Stars:
    case Outshine::Phase::Loading:
    case Outshine::Phase::Failed: Finish(1); return;
  }
}

/* THE COUNTERS COME AFTER THE PRODUCTS, because they describe what was DRAWN and the load draws no
 * world at all — read before the first scene frame they would all be zero. Every run restores the
 * declared standpoint and the declared clocks, so what they describe is still the declared scene. */
void SceneRunner::Dispatch() {
  if (Rc_ == 0 && RunIx_ < Scene_.Runs().size()) {
    const Scene::Run &run = Scene_.Runs()[RunIx_++];
    switch (run.What) {
      case Scene::Run::Kind::Motion: MotionBegin(run.Motion); return;
      case Scene::Run::Kind::ClassDump: Rc_ = DumpClasses(run.ClassDump); return;
      case Scene::Run::Kind::ClassCompare: Stage_ = Stage::CompareViz; return;
      case Scene::Run::Kind::WindProbe: Rc_ = ProbeWind(run.WindProbe); return;
      case Scene::Run::Kind::Subject:
        if (!BenchBegin()) { Finish(1); return; }
        Stage_ = Stage::Bench;
        return;
    }
    Rc_ = 1;
    return;
  }
  Render::Renderer &R = App_.Renderer();
  const long stands = App_.Measured().TreeStands;
  if (stands > 0)
    Log::Info("run", "trees_lod", {{"stands", (double)stands},
        {"meshStands", (double)R.ModelMeshInstances()}, {"meshRadiusM", R.ModelMeshRadiusM()},
        {"rank0", (double)R.ModelLevelInstances(0)}, {"rank1", (double)R.ModelLevelInstances(1)},
        {"rank2", (double)R.ModelLevelInstances(2)}, {"rank3", (double)R.ModelLevelInstances(3)},
        {"impostors", (double)R.ModelImpostorInstances()},
        {"tris", (double)R.ModelTriangleCount()}});

  /* THE FIELD ITSELF. Nothing below the size of a tree answers to it, so no stage
   * reads it today; it is published because a branch and a rotor owe the anchor. */
  Render::WindField wf;
  wf.SetDeclared(App_.Simulation().WindDeg(), App_.Simulation().WindMs());
  Log::Info("run", "wind", {{"declaredFromDeg", App_.Simulation().WindDeg()}, {"declaredMs", App_.Simulation().WindMs()},
      {"canopyMs", wf.CanopyMs()},
      {"profileRatio", App_.Simulation().WindMs() > 0.0 ? wf.CanopyMs() / App_.Simulation().WindMs() : 0.0},
      {"eigenHz", wf.EigenHz()}, {"waveLenM", wf.WaveLengthM()},
      {"phaseSpeedMs", wf.PhaseSpeedMs()}, {"gustAmp", wf.GustAmplitude()},
      {"dirE", wf.DirEast()}, {"dirN", wf.DirNorth()},
      {"cauchyMean", wf.CauchyAt(wf.CanopyMs(), 0.527)},
      {"tipDegMean", Render::WindField::TipAngleRad(wf.CauchyAt(wf.CanopyMs(), 0.527)) / kDeg2Rad},
      {"clockS", R.GetWindClock()}});
  Stage_ = Stage::Irradiance;
}

void SceneRunner::Settled() {
  float irr[Render::IrradianceStage::kFloats] = {};
  const Render::ReadState st = App_.Renderer().ReadIrradiance(irr);
  if (st == Render::ReadState::Pending) return;
  if (st == Render::ReadState::Ready) {
    const double sunY = 0.2126 * irr[0] + 0.7152 * irr[1] + 0.0722 * irr[2];
    const double skyY = 0.2126 * irr[4] + 0.7152 * irr[5] + 0.0722 * irr[6];
    const double sunUp = std::sin((double)App_.Simulation().SunElDeg() * kPi / 180.0);
    Log::Info("run", "irradiance", {{"sunDirectNormalY", sunY}, {"skyDiffuseHorizY", skyY},
        {"sunElDeg", (double)App_.Simulation().SunElDeg()}, {"totalHorizY", sunY * sunUp + skyY},
        {"sunRGB", std::to_string(irr[0]) + "," + std::to_string(irr[1]) + "," + std::to_string(irr[2])},
        {"skyRGB", std::to_string(irr[4]) + "," + std::to_string(irr[5]) + "," + std::to_string(irr[6])}});
  }
  Stage_ = Stage::Exposure;
}

void SceneRunner::Counters() {
  Render::Renderer &R = App_.Renderer();
  World::World &W = App_.Simulation().Scenery();
  float met[Render::ExposureStage::kMeterFloats] = {};
  const Render::ReadState st = R.ReadExposure(met);
  if (st == Render::ReadState::Pending) return;
  if (st == Render::ReadState::Ready)
    Log::Info("run", "exposure", {{"expScale", (double)met[0]}, {"keyLog2", (double)met[1]},
        {"horizE", (double)met[2]}});
  std::string lvl;
  for (int i = 0; i < Render::TerrainDraw::kLevelBins; i++)
    if (R.TerrainTrianglesByLevel()[i] > 0)
      lvl += "L" + std::to_string(i) + "=" + std::to_string(R.TerrainTrianglesByLevel()[i]) + " ";
  Log::Info("run", "terrain", {{"cutLevels", lvl}, {"targetTotal", W.TargetTotal()},
      {"targetSettled", W.TargetSettledN()}, {"progress", (double)W.LoadProgress()},
      {"draws", R.DrawCount()}, {"terrainTiles", R.TerrainVisibleTiles()},
      {"terrainDraws", R.TerrainDrawCalls()}, {"terrainTris", (double)R.TerrainTriangleCount()},
      {"buildingTris", (double)(R.BuildingVertexCount() / 3)},
      {"shadowTris", (double)R.ShadowTriangleCount()}, {"shadowDraws", R.ShadowDrawCalls()},
      {"triangles", (double)R.TriangleCount()},
      {"classVramMB", (double)R.ClassVramBytes() / (1024.0 * 1024.0)},
      {"tileMeshMB", (double)R.TileMeshBytes() / (1024.0 * 1024.0)},
      {"temporalVramMB", (double)R.TemporalVramBytes() / (1024.0 * 1024.0)},
      {"buildingVerts", (int)R.BuildingVertexCount()}});
  if (const std::shared_ptr<const ClassStructure> cls = W.Classes().Read())
    Log::Info("run", "class", {{"version", (double)cls->Version()},
        {"edges", (int)cls->Measured().Edges}, {"seeds", (int)cls->Measured().Seeds},
        {"bufferKB", cls->Bytes() / 1024.0}, {"noDataFrac", cls->NoDataFraction()},
        {"unknownKinds", (int)W.Classes().UnknownKinds()},
        {"unknownFeatures", (int)W.Classes().UnknownFeatures()},
        {"seedOverflow", cls->Measured().Overflow}, {"buildMs", W.Classes().MaxBuildMs()},
        {"fineSubmits", (int)W.Classes().FineSubmits()},
        {"coarseSubmits", (int)W.Classes().CoarseSubmits()}});
  Finish(Rc_);
}

std::string SceneRunner::FrameName(const std::string &path, int frame, const char *ext) const {
  char buf[32];
  snprintf(buf, sizeof buf, "/%04d.%s", frame, ext);
  return path + buf;
}

/* THE RECORDING, and a still is its one-frame case. Every delivered frame carries the SAME
 * `settleFrames` frames of temporal history: the accumulator is emptied, `settleFrames - 1` frames
 * are rendered at the run's frame-0 state, and the run's own first render completes the count.
 * Without that emptying the picture carries a history built while the tiles were still arriving, and
 * two runs whose tiles arrived in a different order differ in colour although their depth is
 * bit-identical. */
void SceneRunner::MotionBegin(const Scene::Run::MotionRun &m) {
  Move_ = &m;
  /* The origin is where the run STARTS, not what the file says: a snapshot may have moved the eye
   * before any run began, and a channel in metres is measured from the standpoint it moves. */
  Base_ = {App_.Simulation().Lat(), App_.Simulation().Lon(), App_.Simulation().YawDeg(),
           App_.Simulation().PitchDeg()};
  LonPerM_ = 1.0 / (kMPerDeg * std::cos(Base_.Lat * kDeg2Rad));
  Moves_ = m.Move.Drives(Target::CameraEastM) || m.Move.Drives(Target::CameraNorthM) ||
           m.Move.Drives(Target::CameraYawDeg) || m.Move.Drives(Target::CameraPitchDeg);
  Profile_ = m.Give == Scene::Run::Product::Profile;
  Settle_ = Scene_.SettleFrames() >= 0 ? Scene_.SettleFrames()
                                       : App_.Renderer().TemporalSettleFrames();
  MotionApply(0.0);
  App_.Renderer().ResetTemporal();
  Csv_.clear();
  Ms_.clear();
  Ms_.reserve((size_t)m.Frames);
  if (Profile_)
    Csv_ = "frame,timeS,distM,frameMs,worldMs,meshMs,uploadMs,buildingMs,bDecodeMs,"
           "classMs,populateMs,renderMs,gpuMs,nodes,drawnLeaves,terrainTiles,draws,terrainTris,"
           "buildingVerts,built,evicted,classVramMB,temporalVramMB,"
           "collectMs,regionX,regionY,regionVersion,regionsStanding,regionBusy\n";
  else if (m.Frames > 1 && !Out_.MakeDir(m.Path)) { Rc_ = 1; Stage_ = Stage::Dispatch; return; }
  SettleAt_ = 1;
  Stage_ = Stage::Settle;
}

void SceneRunner::MotionApply(double f) {
  const Scene::Run::MotionRun &m = *Move_;
  if (Moves_) {
    const double e = m.Move.Drives(Target::CameraEastM) ? m.Move.At(Target::CameraEastM, f) : 0.0;
    const double n = m.Move.Drives(Target::CameraNorthM) ? m.Move.At(Target::CameraNorthM, f) : 0.0;
    const double yaw = m.Move.Drives(Target::CameraYawDeg) ? m.Move.At(Target::CameraYawDeg, f)
                                                           : Base_.YawDeg;
    const double pitch = m.Move.Drives(Target::CameraPitchDeg)
                             ? m.Move.At(Target::CameraPitchDeg, f)
                             : Base_.PitchDeg;
    /* The accumulated angle becomes a bearing HERE and nowhere else (core/Keyframes.h). */
    App_.Look({Base_.Lat + n / kMPerDeg, Base_.Lon + e * LonPerM_, std::fmod(yaw, 360.0), pitch});
  }
  if (m.Move.Drives(Target::CameraFovDeg)) App_.SetFovDeg(m.Move.At(Target::CameraFovDeg, f));
  if (m.Move.Drives(Target::SkyClockS)) App_.SetSkyOffsetS(m.Move.At(Target::SkyClockS, f));
  if (m.Move.Drives(Target::ExposureCompEv))
    App_.SetExposureCompEv(m.Move.At(Target::ExposureCompEv, f));
  App_.SetWindClock(m.Move.Drives(Target::WindClockS) ? m.Move.At(Target::WindClockS, f)
                                                      : Scene_.WindClockS());
}

void SceneRunner::MotionFrame() {
  T0_ = std::chrono::steady_clock::now();
  MotionApply((double)FrameAt_);
  if (Move_->World == Scene::Run::Stream::Streaming)
    /* The virtual clock is the streaming PASS index at 60 Hz — monotonic across the load and the
     * run, which is what the world's 1 Hz counters are read against. */
    App_.Stream((double)App_.Simulation().Streaming().PassCount() * 1000.0 / 60.0);
  T1_ = std::chrono::steady_clock::now();
  App_.Frame();
  T2_ = std::chrono::steady_clock::now();
  Stage_ = Profile_ ? Stage::FrameDrain : Stage::Still;
}

void SceneRunner::MotionNextFrame() {
  FrameAt_++;
  Stage_ = FrameAt_ < Move_->Frames ? Stage::Frame : Stage::MotionClose;
}

void SceneRunner::WriteStill() {
  const Render::ReadState st = App_.Renderer().ReadPixels(Rgba_);
  if (st == Render::ReadState::Pending) return;
  if (st == Render::ReadState::Failed) {
    Log::Error("run", "readback_failed");
    Finish(1);
    return;
  }
  const std::string name = Move_->Frames == 1 ? Move_->Path : FrameName(Move_->Path, FrameAt_, "png");
  if (Out_.Png(name, Rgba_.data(), Scene_.RenderResolution().Width,
               Scene_.RenderResolution().Height) == Artifacts::Delivery::Refused) {
    Finish(1);
    return;
  }
  if (Move_->Depth.empty()) MotionNextFrame();
  else Stage_ = Stage::Depth;
}

void SceneRunner::WriteDepth() {
  const Render::ReadState st = App_.Renderer().ReadDepth(Depth_);
  if (st == Render::ReadState::Pending) return;
  if (st == Render::ReadState::Failed) {
    Log::Error("run", "depth_readback_failed");
    Finish(1);
    return;
  }
  const std::string name =
      Move_->Frames == 1 ? Move_->Depth : FrameName(Move_->Depth, FrameAt_, "f32");
  if (Out_.Bytes(name, Depth_.data(), Depth_.size() * sizeof(float)) ==
      Artifacts::Delivery::Refused) {
    Finish(1);
    return;
  }
  Log::Info("run", "depth_written", {{"path", name},
      {"nearM", (double)Render::Renderer::kNearM}, {"fovDeg", Scene_.FovDeg()}});
  MotionNextFrame();
}

void SceneRunner::MotionRow() {
  Render::Renderer &R = App_.Renderer();
  if (R.GpuIdle() == Render::ReadState::Pending) return;
  World::World &W = App_.Simulation().Scenery();
  const auto t3 = std::chrono::steady_clock::now();
  Ms_.push_back(MsBetween(T0_, t3));
  const Scene::Run::MotionRun &m = *Move_;
  const double e = m.Move.Drives(Target::CameraEastM) ? m.Move.At(Target::CameraEastM, FrameAt_) : 0.0;
  const double n = m.Move.Drives(Target::CameraNorthM) ? m.Move.At(Target::CameraNorthM, FrameAt_) : 0.0;
  const Generators::Region here = App_.Simulation().Here();
  char row[640];
  snprintf(row, sizeof row,
           "%d,%.6f,%.3f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,"
           "%d,%d,%d,%d,%ld,%u,%lld,%lld,%.3f,%.3f,%.4f,%d,%d,%llu,%zu,%d\n",
           FrameAt_, (double)FrameAt_ / m.Fps, std::sqrt(e * e + n * n), MsBetween(T0_, t3),
           W.PassMs(), W.MeshMs(), App_.Measured().UploadMs, W.BuildingMs(), W.BuildingDecodeMs(),
           W.ClassMs(), App_.Simulation().PopulateMs(), MsBetween(T1_, T2_), MsBetween(T2_, t3),
           W.NodeCount(), W.DrawnLeafCount(),
           R.TerrainVisibleTiles(), R.DrawCount(), R.TerrainTriangleCount(),
           R.BuildingVertexCount(), W.BuiltCount(), W.EvictedCount(),
           (double)R.ClassVramBytes() / (1024.0 * 1024.0),
           (double)R.TemporalVramBytes() / (1024.0 * 1024.0),
           App_.CollectMs(), here.X(), here.Y(), (unsigned long long)App_.Simulation().RegionVersion(),
           App_.Simulation().RegionsStanding(), App_.Simulation().RegionBusy() ? 1 : 0);
  Csv_ += row;
  MotionNextFrame();
}

void SceneRunner::MotionClose() {
  Render::Renderer &R = App_.Renderer();
  const Scene::Run::MotionRun &m = *Move_;
  if (Profile_) {
    if (Out_.Text(m.Path, Csv_) == Artifacts::Delivery::Refused) { Finish(1); return; }
    std::vector<double> sorted = Ms_;
    std::sort(sorted.begin(), sorted.end());
    double sum = 0.0;
    for (double v : Ms_) sum += v;
    /* THE DISTRIBUTION, never the mean alone: a stutter is the tail of a series and a mean cannot
     * see it (CLAUDE.md). The mean rides along only because it prices the whole run. */
    Log::Info("run", "motion", {{"path", m.Path}, {"frames", (double)m.Frames},
        {"fps", m.Fps}, {"meanMs", Ms_.empty() ? 0.0 : sum / (double)Ms_.size()},
        {"p50Ms", Percentile(sorted, 0.50)}, {"p95Ms", Percentile(sorted, 0.95)},
        {"p99Ms", Percentile(sorted, 0.99)},
        {"minMs", sorted.empty() ? 0.0 : sorted.front()},
        {"maxMs", sorted.empty() ? 0.0 : sorted.back()},
        {"meshStands", (double)R.ModelMeshInstances()},
        {"impostorStands", (double)R.ModelImpostorInstances()},
        {"treeTris", (double)R.ModelTriangleCount()},
        {"width", (double)Scene_.RenderResolution().Width},
        {"height", (double)Scene_.RenderResolution().Height}});
  } else {
    Log::Info("run", "stills", {{"path", m.Path}, {"frames", (double)m.Frames},
        {"w", Scene_.RenderResolution().Width}, {"h", Scene_.RenderResolution().Height}});
  }
  MotionApply(0.0);
  if (!Moves_) App_.Look(Base_);
  Stage_ = Stage::Dispatch;
}
/* THE CLASS AS THE SIMULATION SEES IT: the CPU evaluator over a declared world square, in the class
 * structure's own metric frame. No GPU is involved — this is the answer a headless actor gets, and
 * the picture is judged against it. */
int SceneRunner::DumpClasses(const Scene::Run::ClassDumpRun &d) const {
  const std::shared_ptr<const ClassStructure> cls = App_.Simulation().Scenery().Classes().Read();
  if (!cls) { Log::Error("run", "class_dump_without_structure"); return 1; }
  double ce = 0, cn = 0;
  App_.Simulation().Scenery().Classes().ToEnu(App_.Simulation().Lat(), App_.Simulation().Lon(), &ce, &cn);
  const int n = (int)(d.SpanM / d.StepM);
  /* SNAPPED TO THE WORLD, not to the camera: two runs from different standpoints must sample the
   * same points, or a sub-sample offset would show up as a disagreement that is not one. */
  const double e0 = std::floor((ce - d.SpanM * 0.5) / d.StepM) * d.StepM;
  const double n0 = std::floor((cn - d.SpanM * 0.5) / d.StepM) * d.StepM;
  char hdr[256];
  const int hl = snprintf(hdr, sizeof hdr, "OSCLS1 %d %.6f %.6f %.6f %.6f %.6f\n", n, e0, n0,
                          d.StepM, ce, cn);
  std::vector<uint8_t> blob((size_t)hl + (size_t)n * (size_t)n);
  for (int i = 0; i < hl; i++) blob[(size_t)i] = (uint8_t)hdr[i];
  for (int j = 0; j < n; j++)
    for (int i = 0; i < n; i++) {
      const int c = cls->Evaluate(e0 + ((double)i + 0.5) * d.StepM,
                                  n0 + ((double)j + 0.5) * d.StepM, nullptr, nullptr);
      blob[(size_t)hl + (size_t)j * (size_t)n + (size_t)i] = (uint8_t)(c < 0 ? 255 : c);
    }
  if (Out_.Bytes(d.Path, blob.data(), blob.size()) == Artifacts::Delivery::Refused) return 1;
  Log::Info("run", "class_dumped", {{"path", d.Path}, {"side", n}, {"stepM", d.StepM}});
  return 0;
}


/* CPU AGAINST GPU, on the very pixels that were drawn: one geometry, one predicate, two evaluators
 * (doc/architecture.md). The GPU class index is not read back — it is not expressible through the
 * tone curve — so the picture is PARTITIONED by its own colour and each partition is checked against
 * the CPU answer. A bijection is 0 % disagreement; the bound is the fray, because the fragment
 * refines the boundary and the CPU does not.
 *
 * TWO READBACKS, so two turns: the colour first, then the depth that says which pixels are ground. */
void SceneRunner::CompareClasses() {
  if (Stage_ == Stage::CompareViz) {
    const Render::ReadState st = App_.Renderer().ReadPixels(Rgba_);
    if (st == Render::ReadState::Pending) return;
    if (st == Render::ReadState::Failed) {
      Log::Error("run", "class_cmp_readback_failed");
      Rc_ = 1;
      Stage_ = Stage::Dispatch;
      return;
    }
    Stage_ = Stage::CompareDepth;
    return;
  }
  const Render::ReadState st = App_.Renderer().ReadDepth(Depth_);
  if (st == Render::ReadState::Pending) return;
  if (st == Render::ReadState::Failed) {
    Log::Error("run", "class_cmp_readback_failed");
    Rc_ = 1;
    Stage_ = Stage::Dispatch;
    return;
  }
  Stage_ = Stage::Dispatch;
  const std::vector<uint8_t> &viz = Rgba_;
  const std::vector<float> &depth = Depth_;
  const int width = Scene_.RenderResolution().Width, height = Scene_.RenderResolution().Height;
  const double tanH = std::tan(0.5 * Scene_.FovDeg() * kDeg2Rad);
  const double aspect = (double)width / (double)height;
  const World::ClassField &field = App_.Simulation().Scenery().Classes();
  const std::shared_ptr<const ClassStructure> cls = field.Read();
  if (!cls) { Log::Error("run", "class_cmp_without_structure"); Rc_ = 1; return; }
  const double *O = field.OriginEcef(), *Ea = field.EastEcef(), *No = field.NorthEcef();
  const double *eye = App_.Simulation().Eye(), *fwd = App_.Simulation().Fwd(), *right = App_.Simulation().Right(), *up = App_.Simulation().Up();
  std::map<uint32_t, std::map<int, long>> hist;
  long ground = 0, edgeNear = 0;
  for (int py = 0; py < height; py++)
    for (int px = 0; px < width; px++) {
      const float d = depth[(size_t)py * width + px];
      if (!(d > 1.0e-6f) || d >= 1.0f) continue;
      const double sx = (2.0 * ((double)px + 0.5) / width - 1.0) * tanH * aspect;
      const double sy = (1.0 - 2.0 * ((double)py + 0.5) / height) * tanH;
      double dir[3];
      for (int k = 0; k < 3; k++) dir[k] = fwd[k] + right[k] * sx + up[k] * sy;
      const double len = std::sqrt(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);
      for (int k = 0; k < 3; k++) dir[k] /= len;
      const double cosOff = 1.0 / len;   /* dir . fwd, since fwd is a unit vector */
      const double range = (double)Render::Renderer::kNearM / ((double)d * cosOff);
      if (range > 2000.0) continue;      /* beyond the near tier the CPU answers from the far one */
      double p[3];
      for (int k = 0; k < 3; k++) p[k] = eye[k] + dir[k] * range - O[k];
      const double pe = p[0] * Ea[0] + p[1] * Ea[1] + p[2] * Ea[2];
      const double pn = p[0] * No[0] + p[1] * No[1] + p[2] * No[2];
      double dist = 0.0;
      const int c = cls->Evaluate(pe, pn, &dist, nullptr);
      if (c < 0) continue;
      ground++;
      /* Within one fray width of an outline the two are ALLOWED to differ: the fragment blends and
       * the point query does not. footM at this range is the fragment's own width. */
      if (dist < 0.05 + range / 688.0) { edgeNear++; continue; }
      const size_t o = ((size_t)py * width + px) * 4;
      const uint32_t key = ((uint32_t)viz[o] << 16) | ((uint32_t)viz[o + 1] << 8) | viz[o + 2];
      hist[key][c]++;
    }
  long agree = 0, total = 0, solidAgree = 0, solidTotal = 0, solidColours = 0;
  for (const auto &kv : hist) {
    long best = 0, sum = 0;
    for (const auto &cc : kv.second) { sum += cc.second; if (cc.second > best) best = cc.second; }
    agree += best;
    total += sum;
    /* A colour carried by a thousand pixels is a CLASS; one carried by a handful is the resolve
     * filter's blend across an edge and says nothing about either evaluator. */
    if (sum >= 1000) { solidAgree += best; solidTotal += sum; solidColours++; }
  }
  Log::Info("run", "class_cmp", {{"groundPx", (double)ground}, {"comparedPx", (double)total},
      {"withinFrayPx", (double)edgeNear}, {"colours", (int)hist.size()},
      {"agreePct", total ? 100.0 * (double)agree / (double)total : 0.0},
      {"solidColours", (int)solidColours}, {"solidPx", (double)solidTotal},
      {"solidAgreePct", solidTotal ? 100.0 * (double)solidAgree / (double)solidTotal : 0.0}});
}
/* THE STATE CHANNEL: the declared field read on a world line along the wind at the
 * declared times — no picture, no GPU. */
int SceneRunner::ProbeWind(const Scene::Run::WindProbeRun &w) const {
  Render::WindField wf;
  wf.SetDeclared(App_.Simulation().WindDeg(), App_.Simulation().WindMs());
  std::string csv;
  char line[256];
  snprintf(line, sizeof line,
           "# dxM=%.6f nx=%d dtS=%.9f nt=%d bladeLenM=0.527 canopyMs=%.9f eigenHz=%.9f"
           " waveLenM=%.9f phaseSpeedMs=%.9f\n", w.DxM, w.Samples, w.DtS, w.Frames, wf.CanopyMs(),
           wf.EigenHz(), wf.WaveLengthM(), wf.PhaseSpeedMs());
  csv = line;
  for (int t = 0; t < w.Frames; t++) {
    const double tt = Scene_.WindClockS() + w.DtS * (double)t;
    for (int k = 0; k < w.Samples; k++) {
      /* The line runs ALONG the wind through the scene's own point, in absolute world metres. */
      const double xi = (double)k * w.DxM;
      const double u = wf.SpeedAt(xi * wf.DirEast(), xi * wf.DirNorth(), tt);
      const double th = Render::WindField::TipAngleRad(wf.CauchyAt(u, 0.527)) / kDeg2Rad;
      snprintf(line, sizeof line, "%s%.6f", k ? "," : "", th);
      csv += line;
    }
    csv += "\n";
  }
  if (Out_.Text(w.Path, csv) == Artifacts::Delivery::Refused) return 1;
  Log::Info("run", "wind_probe_written", {{"path", w.Path}, {"nx", w.Samples}, {"nt", w.Frames},
      {"dxM", w.DxM}, {"dtS", w.DtS}});
  return 0;
}

/* THE SUBJECT BENCH takes the binary over completely: no World, no tile stream, no scene light. The
 * grown mesh and its foliage are held by this object for the whole bench run, because the arrays are
 * uploaded once and the numbers logged below come off the same objects the picture was drawn from. */
bool SceneRunner::BenchBegin() {
  const Scene::Run::SubjectRun &s = Scene_.Runs().front().Subject;
  Bench_ = std::make_unique<SubjectBench>(App_.Renderer(), App_.Simulation().Vegetation(), Out_);
  SubjectBench &bench = *Bench_;
  Generators::TreeMesh &mesh = BenchMesh_;
  Generators::TreeFoliage &foliage = BenchFoliage_;
  if (!s.Species.empty()) {
    const std::string path = std::string(kSpeciesDir) + "/" + s.Species + ".json";
    Generators::TreeSpecies sp;
    if (!ReadSpecies(path.c_str(), &sp)) {
      Log::Error("run", "subject_species_unreadable", {{"path", path}, {"why", sp.Error()}});
      return false;
    }
    Generators::TreeGrower grower;
    const auto t0 = std::chrono::steady_clock::now();
    /* THE SAME MESH THE NEAREST FIELD RANK GETS. Asking for pixel 0 asked for a tube on every 3 mm
     * twig — a mesh nobody draws and one that truncated the crown. */
    grower.Grow(sp, mesh, ModelLadder::Error(0));
    const auto t1 = std::chrono::steady_clock::now();
    Generators::TreeLeaf::Build(sp.LeafParams(), mesh);
    const auto t2 = std::chrono::steady_clock::now();
    foliage.Build(mesh, sp, s.LeafMult);
    const auto t3 = std::chrono::steady_clock::now();

    const double h = s.HeightM > 0.0 ? s.HeightM : (double)sp.HeightM();
    const double crownX = (double)(mesh.BoxMax.X - mesh.BoxMin.X) * h;
    const double crownZ = (double)(mesh.BoxMax.Z - mesh.BoxMin.Z) * h;
    const double proj = 0.25 * kPi * crownX * crownZ;
    const bool leaves = s.Leaf == Scene::Run::Foliage::Leaves;

    Render::Renderer &r = App_.Renderer();
    float row[kMaterialRowFloats];
    Generators::TreePrototype::MaterialRow(Generators::TreePrototype::LookOf(sp), row);
    r.SetPrototypeMaterial(row);
    r.SetPrototypeLevel(0, Render::LevelMesh{mesh.BarkVerts.data(),
                                             (uint32_t)mesh.BarkVertexCount(),
                                             mesh.BarkIdx.data(),
                                             (uint32_t)mesh.BarkIdx.size()});
    r.SetPrototypeDetail(Render::DetailMesh{mesh.LeafVerts.data(),
                                            (uint32_t)mesh.LeafVertexCount(), mesh.LeafIdx.data(),
                                            (uint32_t)mesh.LeafIdx.size(),
                                            foliage.Instances().data(), (uint32_t)foliage.Count(),
                                            foliage.ScaleM()});
    r.SetSheetsVisible(leaves);
    Log::Info("run", "subject_tree", {{"species", s.Species}, {"heightM", h},
        {"declaredSpreadM", (double)sp.SpreadM()}, {"grownCrownXM", crownX},
        {"grownCrownZM", crownZ},
        {"spreadRatio", sp.SpreadM() > 0.0f ? 0.5 * (crownX + crownZ) / (double)sp.SpreadM() : -1.0},
        {"barkTris", (double)(mesh.BarkIdx.size() / 3)},
        {"leafTrisEach", (double)(mesh.LeafIdx.size() / 3)},
        {"leafPoints", (double)mesh.LeafPoints.size()},
        {"leafInstances", (double)foliage.Count()},
        {"leafTrisTotal", (double)(mesh.LeafIdx.size() / 3 * foliage.Count())},
        {"leafLenM", (double)foliage.ScaleM() * (double)sp.LeafParams().Length},
        {"oneLeafAreaM2", foliage.OneLeafAreaM2()},
        {"leafAreaM2", foliage.LeafAreaM2()}, {"crownProjM2", proj},
        {"lai", proj > 0.0 ? foliage.LeafAreaM2() / proj : -1.0},
        {"laiDeclared", (double)sp.Lai()}, {"laminaePerPoint", foliage.PerPoint()},
        {"meshKb", (double)mesh.Bytes() / 1024.0},
        {"instKb", (double)(foliage.Instances().size() * sizeof(float)) / 1024.0},
        {"growMs", MsBetween(t0, t1)}, {"leafMs", MsBetween(t1, t2)},
        {"foliageMs", MsBetween(t2, t3)},
        {"leafMult", (double)s.LeafMult}, {"leavesDrawn", leaves ? 1.0 : 0.0}});
    if (!bench.SelectTree(s.Species.c_str(), h)) {
      Log::Error("run", "subject_species_height_invalid", {{"name", s.Species}});
      return false;
    }
  } else if (!bench.Select(s.Template.c_str(), s.HeightM)) {
    Log::Error("run", "subject_unknown", {{"name", s.Template}});
    return false;
  }
  /* A TREE DECLARES NO SUBSTRATE. Without a template naming a ground class the floor stays at the
   * bench's 18 % neutral, which is what a subject without a declared substrate deserves. */
  if (!s.Template.empty()) {
    std::string subName;
    float subRgb[3] = {0, 0, 0};
    if (!SubjectSubstrate(App_.Simulation().Materials(), App_.Simulation().Files().Vegetation, s.Template, &subName, subRgb)) {
      Log::Error("run", "subject_substrate_unresolved", {{"template", s.Template}});
      return false;
    }
    bench.SetSubstrate(subName, subRgb);
  }
  if (!Out_.MakeDir(s.Dir)) {
    Log::Error("run", "subject_dir_unmakeable", {{"dir", s.Dir}});
    return false;
  }
  bench.Stand(Scene_.Lat(), Scene_.Lon(), kSubjectGroundAslM);
  bench.SetOutDir(s.Dir);
  bench.SetTurntableSteps(s.TurnSteps);
  return true;
}

} // namespace outshine::Clients
