#include "Outshine.h"

#include <chrono>
#include <cstdlib>

#include "ClassStructure.h"
#include "Geodesy.h"
#include "HeapProbe.h"
#include "Log.h"
#include "PixelFocalLength.h"
#include "TerrainLoader.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

namespace outshine::Clients {
namespace {

void PumpMs(int ms) {
#ifdef __EMSCRIPTEN__
  emscripten_sleep((unsigned)ms);
#else
  (void)ms;
#endif
}

/* [SET] 10 s without one more ready tile. Long enough that the slowest single tile measured on this
 * host (a cold z14 vector bake, ~3 s) never trips it, short enough to name a hung server. */
constexpr double kStallSayMs = 10000.0;

constexpr int kDeviceTries = 2000;
constexpr int kStarBytes = 262144;
/* [SET] Tile meshes handed to the device per frame. Upload is a budget, never a queue drain. */
constexpr int kUploadsPerFrame = 6;

double NowMs() {
  return std::chrono::duration<double, std::milli>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

}  // namespace

Outshine::Outshine(const Scene &scene, const Assets &assets)
    : Sim_(scene, assets), Exposure_(scene.Exposure()) {
  if (scene.HasJitterPin())
    R_.PinJitter((float)scene.JitterPinX(), (float)scene.JitterPinY());
}

void Outshine::Pump() { PumpMs(0); }

void Outshine::SetFovDeg(double deg) {
  R_.SetFovDeg(deg);
  Sim_.SetPixelFocalLength(PixelFocalLength(Sim_.Declared().RenderResolution().Height, deg));
}

void Outshine::SetExposureCompEv(double ev) {
  Exposure_.CompEv = (float)ev;
  R_.SetExposure(Exposure_);
}

void Outshine::SetSkyOffsetS(double s) {
  Sim_.SetSkyOffsetS(s);
  R_.SetSkyClock(Sim_.SkyClockS() + s);
  R_.SetSceneState(Sim_.SceneState());
}

void Outshine::SetWindClock(double s) { R_.SetWindClock(s); }

bool Outshine::Prepare(const Gpu &gpu) {
  if (Phase_ != Phase::Declared) return false;
  const Scene &scene = Sim_.Declared();
  if (!Sim_.LoadTables()) return false;
  Log::Info("outshine", "scene", {{"scene", scene.Id()}, {"lat", Sim_.Lat()},
      {"lon", Sim_.Lon()}, {"eyeM", Sim_.Standpoint().EyeAglM()}, {"yawDeg", Sim_.YawDeg()},
      {"pitchDeg", Sim_.PitchDeg()}, {"fovDeg", scene.FovDeg()}, {"utc", scene.Utc()},
      {"utcS", Sim_.SkyClockS()}, {"windDeg", Sim_.WindDeg()}, {"windMs", Sim_.WindMs()},
      {"cloudCover", scene.CloudCover()}, {"sunElDeg", (double)Sim_.SunElDeg()},
      {"sunAzDeg", (double)Sim_.SunAzDeg()}});
  /* Only a DEVIATION is an event: at the budget size there is nothing to justify. */
  const Scene::Resolution &res = scene.RenderResolution();
  if (!res.Why.empty())
    Log::Info("outshine", "render_size", {{"width", res.Width}, {"height", res.Height},
        {"why", res.Why}});

  const World::VegetationTemplates &veg = Sim_.Vegetation();
  R_.SetVegetationTable(veg.Rows(), veg.RowBytes(), veg.RockTemplate(), veg.Limit().SlopeBandDeg());
  R_.SetSkyClock(Sim_.SkyClockS());
  SetFovDeg(scene.FovDeg());
  R_.SetOrthoM(Sim_.OrthoM());
  R_.SetWind(Sim_.WindDeg(), Sim_.WindMs());
  R_.SetExposure(Exposure_);

  if (gpu.Canvas) R_.Init(gpu.Canvas, res.Width, res.Height);
  else R_.InitOffscreen(res.Width, res.Height);
  /* THE DEVICE HAS TO BE THERE BEFORE ANYTHING UPLOADS: a stage's Upload returns nothing without one
   * and drops its geometry in silence. Native Dawn is already up; the browser's request is a
   * promise. */
  for (int t = 0; t < kDeviceTries && !R_.Ready(); t++) PumpMs(10);
  if (!R_.Ready()) {
    Log::Error("outshine", "device_init_failed");
    return false;
  }
  Frames_.SetGpuAvailable(R_.GpuTimingAvailable());
  Sim_.StartTelemetry();
  Sim_.Bus().Register(&Frames_);
  Sim_.Bus().Register(&Memory_);
  Sim_.Bus().Start();
  ClockOriginMs_ = NowMs();
  Frames_.Open(ClockOriginMs_);
  Sim_.Streaming().Open(ClockOriginMs_);
  Phase_ = Phase::Prepared;
  return true;
}

bool Outshine::Open() {
  if (Phase_ != Phase::Prepared) return false;
  const Assets &files = Sim_.Files();
  if (!files.Moon.empty()) {
    uint8_t *rgba = nullptr;
    int w = 0, h = 0;
    if (fb_load_image_file(files.Moon.c_str(), &rgba, &w, &h)) {
      R_.SetMoonTexture(rgba, w, h);
      free(rgba);
    } else {
      Log::Warn("outshine", "moon_texture_missing", {{"path", files.Moon}});
    }
  }
  {
    static uint8_t stars[kStarBytes];
    const int n = fb_fetch_stars(Sim_.TilesBase().c_str(), stars, kStarBytes);
    if (n > 0) R_.SetStars(stars, n, Sim_.Lat(), Sim_.Lon());
    else Log::Warn("outshine", "star_catalogue_unreachable", {{"base", Sim_.TilesBase()}});
  }

  if (!Sim_.Open()) return false;
  R_.SetCameraBasis(Sim_.Eye(), Sim_.Fwd(), Sim_.Right(), Sim_.Up());
  R_.SetSceneState(Sim_.SceneState());

  if (!Sim_.Files().Species.empty()) {
    const std::optional<World::Forest::Prototype> tree = Sim_.GrowTrees();
    if (!tree) return false;
    for (size_t rank = 0; rank < tree->Ranks.size(); rank++) {
      const World::Forest::Prototype::Rank &r = tree->Ranks[rank];
      R_.SetTreeBark((int)rank, r.BarkVerts.data(), r.BarkVertCount, r.BarkIdx.data(),
                     (uint32_t)r.BarkIdx.size());
      R_.SetTreeCards((int)rank, r.Cards.data(), r.CardCount, r.CardLeafM, tree->CardFanDeg);
    }
    R_.SetTreeLook(tree->Look);
    /* bpar.z is the tree height and comes from SetTreeStand; without it every instance scales to
     * null. */
    R_.SetTreeStand(0.0, 0.0, 0.0, tree->HeightM);
    R_.SetTreeCrown(tree->Crown.HalfWidth, tree->Crown.Top, tree->Crown.Bottom);
    R_.BakeTreeImpostor();
  }
  Phase_ = Phase::Loading;
  return true;
}

/* The interval between two frames, not the encode: what a viewer feels is the period, and everything
 * the client did in between — streaming, a readback, a PNG — is part of it. A progress frame is
 * metered by the same pair, because there is no measurement mode: whether a second was spent loading
 * is a FIELD of its row (StreamTelemetry.h), never a reason to leave it out. */
double Outshine::OpenFrame() {
  const double now = NowMs();
  if (LastFrameMs_ > 0.0) Frames_.AddFrame(now - LastFrameMs_);
  LastFrameMs_ = now;
  return now;
}

void Outshine::CloseFrame(double startedMs) {
  HeapProbe::Sample();
  double stage[Render::GpuTimer::kPassCount];
  if (R_.TakeGpuTimes(stage)) Frames_.AddStages(stage);
  if (!Frames_.Due(startedMs)) return;
  Sim_.Bus().Tick((startedMs - ClockOriginMs_) * 0.001);
  Frames_.Reset(startedMs);
  Sim_.Streaming().Reset();
}

void Outshine::Frame() {
  const double now = OpenFrame();
  R_.RenderFrame();
  CloseFrame(now);
}

Outshine::Counters Outshine::Measured() const {
  const World::World &w = Sim_.Scenery();
  Counters c;
  c.Draws = R_.DrawCount();
  c.Triangles = (long)R_.TriangleCount();
  c.TreeTriangles = R_.TreeTriangleCount();
  c.TreeStands = Sim_.Forest().StandCount();
  c.BuildingVerts = R_.BuildingVertexCount();
  c.Built = w.BuiltCount();
  c.WorldMs = w.PassMs();
  c.MeshMs = w.MeshMs();
  c.UploadMs = UploadMs_;
  c.BuildingMs = w.BuildingMs();
  c.BuildingDecodeMs = w.BuildingDecodeMs();
  c.GroundAslM = Sim_.Standpoint().GroundAslM();
  c.AltAslM = Sim_.Standpoint().AltAslM();
  c.Fraction = w.LoadProgress();
  c.Resident = w.Resident();
  return c;
}

void Outshine::Look(const Stance &s) {
  Sim_.Look(s);
  R_.SetCameraBasis(Sim_.Eye(), Sim_.Fwd(), Sim_.Right(), Sim_.Up());
  R_.SetSceneState(Sim_.SceneState());
}

/* THE TILE PRODUCT. What the world offers, in its own priority order; what comes back is the slot
 * the renderer holds it in, and the world hands that same number back in its draw list. */
void Outshine::CollectTiles() {
  World::World &w = Sim_.Scenery();
  const double tUp = NowMs();
  if (R_.DeviceUsable()) {
    int budget = kUploadsPerFrame;
    for (const World::World::TileMesh &m : w.Uncollected()) {
      if (budget == 0) break;
      const int slot = R_.UploadTile(m.Verts, m.VertCount, m.Idx, m.IdxCount, m.Clusters,
                                     m.ClusterCount, m.OriginEcef, m.AnchorEcef);
      if (slot < 0) continue;
      w.Collect(m.Id, slot);
      budget--;
    }
  }
  UploadMs_ = NowMs() - tUp;
  for (int slot : w.TakeRetired()) R_.ReleaseTile(slot);
  const std::vector<int> &drawn = w.Drawn();
  R_.SetDrawList(drawn.data(), (int)drawn.size());
}

/* THE VERSION IS THE UPLOAD'S TRIGGER, not a dirty flag: what is on the device is named by the
 * structure it came from, so a missed or a doubled upload is a mismatch and not a guess. */
void Outshine::CollectClass() {
  const World::ClassField &cls = Sim_.Scenery().Classes();
  R_.SetClassFrame(cls.EastEcef(), cls.NorthEcef(), cls.Cam());
  const std::shared_ptr<const World::ClassStructure> structure = cls.Read();
  if (!structure || structure->Version() == ClassVersion_) return;
  const double t0 = NowMs();
  R_.WriteClassBuffer(structure->Words(), structure->Bytes());
  ClassVersion_ = structure->Version();
  Log::Debug("outshine", "class_uploaded", {{"version", (double)structure->Version()},
      {"uploadMs", NowMs() - t0}, {"streamMs", cls.LastStreamMs()},
      {"ingestMs", cls.LastIngestMs()}, {"bufferKB", structure->Bytes() / 1024.0}});
}

void Outshine::Collect() {
  World::World &w = Sim_.Scenery();
  CollectTiles();
  CollectClass();

  /* WHERE THE STAND STANDS. The ground fragment IS the stand (render/Sward.h) and reads it off the
   * world graticule, so all the renderer needs is the place and the local basis. */
  double east[3], north[3], up[3];
  EnuAxesEcef(Sim_.Lat(), Sim_.Lon(), east, north, up);
  R_.SetSwardBasis(Sim_.Lat(), Sim_.Lon(), east, north, up);

  const World::World::WaterSurface water = w.Water();
  if (water.Seq != WaterSeq_ && water.VertCount > 0) {
    WaterSeq_ = water.Seq;
    R_.SetWaterMesh(water.Verts, water.VertCount, water.AnchorEcef);
  }
  const World::World::BuildingSurface bld = w.Buildings();
  if (bld.Seq != BuildingSeq_ && bld.VertCount > 0) {
    BuildingSeq_ = bld.Seq;
    R_.SetBuildingMesh(bld.Verts, bld.VertCount, bld.Idx, bld.IdxCount, bld.Clusters,
                       bld.ClusterCount, bld.AnchorEcef);
  }
  const World::Forest &forest = Sim_.Forest();
  if (forest.Scattered() && !TreesStanding_) {
    TreesStanding_ = true;
    R_.SetTreeStands(forest.Stands().data(), (uint32_t)forest.StandCount(),
                     forest.StandDistM().data());
  }
}

Outshine::Progress Outshine::Stream(double nowMs) {
  if (Phase_ < Phase::Loading) return {};
  World::World &w = Sim_.Scenery();
  Sim_.Advance();
  w.Refine(Sim_.Sight(), nowMs);
  if (w.Resident()) Sim_.Settle();
  Collect();

  StreamTelemetry::Pass p;
  p.WorldMs = w.PassMs();
  p.MeshMs = w.MeshMs();
  p.UploadMs = UploadMs_;
  p.BuildingMs = w.BuildingMs();
  p.BuildingDecodeMs = w.BuildingDecodeMs();
  p.ClassMs = w.ClassMs();
  p.TilesTotal = w.TargetTotal();
  p.TilesReady = w.TargetReadyN();
  p.TilesInView = w.TargetInViewN();
  p.VectorTilesPending = w.BuildingPendingTiles();
  p.Built = w.BuiltCount();
  p.Evicted = w.EvictedCount();
  p.Resident = w.Resident();
  Sim_.Streaming().AddPass(p);
  return {w.LoadProgress(), w.Resident()};
}

/* THE PROGRESS FRAME IS A FRAME LIKE ANY OTHER, and that is the whole point: the renderer never
 * stops, so the bar moves at the display's rate while the fetches and the decodes run beside it.
 * What the loop does NOT do is draw the world — a half-arrived scene is not a picture of anything.
 * The virtual clock is the pass index at 60 Hz, so the world's own 1 Hz counters keep the same
 * meaning they have inside a run. */
bool Outshine::Load() {
  if (Phase_ != Phase::Loading) return false;
  const World::World &w = Sim_.Scenery();
  Progress p;
  long passes = 0;
  const double t0 = NowMs();
  double movedMs = t0, saidMs = t0;
  int wasReady = -1;
  while (!p.Resident) {
    p = Stream((double)passes * 1000.0 / 60.0);
    const double frameMs = OpenFrame();
    R_.RenderProgress(p.Fraction);
    CloseFrame(frameMs);
    Pump();
    passes++;
    /* A STALL IS SAID, NOT CAPPED. There is no ceiling to hit — a server that stops answering is a
     * fact about the server — but a load that spins in silence is a fact nobody can read. */
    const double now = NowMs();
    if (w.TargetReadyN() != wasReady) { wasReady = w.TargetReadyN(); movedMs = now; }
    if (now - movedMs > kStallSayMs && now - saidMs > kStallSayMs) {
      saidMs = now;
      Log::Warn("outshine", "load_stalled", {{"stalledS", (now - movedMs) * 0.001},
          {"passes", (double)passes}, {"targetReady", wasReady},
          {"targetTotal", w.TargetTotal()}, {"vectorPending", w.BuildingPendingTiles()},
          {"progress", (double)p.Fraction}});
    }
  }
  Sim_.Streaming().MarkResident(NowMs());
  Phase_ = Phase::Playing;
  Log::Info("outshine", "loaded", {{"passes", (double)passes}, {"loadMs", NowMs() - t0},
      {"targetTotal", w.TargetTotal()}, {"targetInView", w.TargetInViewN()},
      {"progress", (double)p.Fraction}});
  return true;
}

} // namespace outshine::Clients
