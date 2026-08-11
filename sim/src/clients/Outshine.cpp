#include "Outshine.h"

#include <chrono>
#include <cstdlib>

#include "ClassStructure.h"
#include "ClusterDag.h"
#include "Geodesy.h"
#include "Log.h"
#include "PixelFocalLength.h"
#include "TerrainLoader.h"
#include "ModelLadder.h"
#include "Units.h"

namespace outshine::Clients {
namespace {

/* [SET] 10 s without one more ready tile. Long enough that the slowest single tile measured on this
 * host (a cold z14 vector bake, ~3 s) never trips it, short enough to name a hung server. */
constexpr double kStallSayMs = 10000.0;

/* [SET] What a bring-up phase may keep asking for before it is called dead, in milliseconds of wall
 * clock. Both carry over the bounds the counted retries they replace had: 2000 turns of 10 ms for
 * the device, 200 of 50 ms for the ground and the star catalogue. A wall clock and not a turn count,
 * because how long a turn lasts is the host's business and not this object's. */
constexpr double kDeviceWaitMs = 20000.0;
constexpr double kStreamWaitMs = 10000.0;

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

  Sim_.Scenery().StructureShapes(&Structures_);

  const World::VegetationTemplates &veg = Sim_.Vegetation();
  R_.SetVegetationTable(veg.Rows(), veg.RowBytes(), veg.RockTemplate(), veg.Limit().SlopeBandDeg());
  R_.SetSkyClock(Sim_.SkyClockS());
  SetFovDeg(scene.FovDeg());
  R_.SetOrthoM(Sim_.OrthoM());
  R_.SetWind(Sim_.WindDeg(), Sim_.WindMs());
  R_.SetExposure(Exposure_);

  if (gpu.Canvas) R_.Init(gpu.Canvas, res.Width, res.Height);
  else R_.InitOffscreen(res.Width, res.Height);
  PhaseFromMs_ = NowMs();
  Phase_ = Phase::Device;
  return true;
}

void Outshine::Open() {
  if (Phase_ != Phase::Prepared) return;
  /* The moon's albedo is a declared file and not a stream, so it is read here, once, and not in a
   * turn that may run a thousand times. */
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
  PhaseFromMs_ = NowMs();
  Phase_ = Phase::Ground;
}

bool Outshine::Busy() const {
  return Phase_ == Phase::Device || Phase_ == Phase::Ground || Phase_ == Phase::Stars ||
         Phase_ == Phase::Loading;
}

Outshine::Phase Outshine::Step() {
  switch (Phase_) {
    case Phase::Device: AwaitDevice(); break;
    case Phase::Ground: AwaitGround(); break;
    case Phase::Stars: AwaitStars(); break;
    case Phase::Loading: LoadPass(); break;
    case Phase::Declared:
    case Phase::Prepared:
    case Phase::Playing:
    case Phase::Failed: break;
  }
  return Phase_;
}

/* THE DEVICE HAS TO BE THERE BEFORE ANYTHING UPLOADS: a stage's Upload returns nothing without one
 * and drops its geometry in silence. Native Dawn is already up when Init returns; the browser's
 * request is a promise that resolves on the turn after this one. */
void Outshine::AwaitDevice() {
  if (!R_.Ready()) {
    if (NowMs() - PhaseFromMs_ > kDeviceWaitMs) {
      Log::Error("outshine", "device_init_failed", {{"waitedMs", NowMs() - PhaseFromMs_}});
      Phase_ = Phase::Failed;
    }
    return;
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
}

void Outshine::AwaitGround() {
  switch (Sim_.Open()) {
    case Sim::Bring::Waiting:
      if (NowMs() - PhaseFromMs_ > kStreamWaitMs) {
        Log::Error("outshine", "ground_wait_gave_up", {{"waitedMs", NowMs() - PhaseFromMs_}});
        Phase_ = Phase::Failed;
      }
      return;
    case Sim::Bring::Failed: Phase_ = Phase::Failed; return;
    case Sim::Bring::Open: break;
  }
  PhaseFromMs_ = NowMs();
  Phase_ = Phase::Stars;
}

/* After the world, because the catalogue comes down the same pool the tiles do — one transport, one
 * cache, and the only blocking HTTP left in the client is the pool's own threads. */
void Outshine::AwaitStars() {
  static uint8_t stars[kStarBytes];
  const FbStarBands bands = fb_fetch_stars(stars, kStarBytes);
  if (bands.Where == FbStarBands::State::Pending) {
    if (NowMs() - PhaseFromMs_ <= kStreamWaitMs) return;
    Log::Warn("outshine", "star_catalogue_unreachable", {{"base", Sim_.TilesBase()},
        {"waitedMs", NowMs() - PhaseFromMs_}});
  } else if (bands.Bytes > 0) {
    R_.SetStars(stars, bands.Bytes, Sim_.Lat(), Sim_.Lon());
  } else {
    Log::Warn("outshine", "star_catalogue_unreachable", {{"base", Sim_.TilesBase()}});
  }

  R_.SetCameraBasis(Sim_.Eye(), Sim_.Fwd(), Sim_.Right(), Sim_.Up());
  R_.SetSceneState(Sim_.SceneState());

  /* THE CLUSTER, GROWN ONCE. Not a generator call: it happens before any region exists, and every
   * stand in the world is an instance of what comes out of it. */
  if (!Sim_.Files().Species.empty()) {
    Tree_ = Generators::TreePrototype::Grow(Sim_.Species());
    if (!Tree_) { Phase_ = Phase::Failed; return; }
    const std::vector<Generators::TreePrototype::Rank> &ranks = Tree_->Ranks();
    for (size_t rank = 0; rank < ranks.size(); rank++) {
      const Generators::TreePrototype::Rank &r = ranks[rank];
      R_.SetPrototypeLevel((int)rank, Render::LevelMesh{r.BarkVerts.data(), r.BarkVertCount,
                                                        r.BarkIdx.data(),
                                                        (uint32_t)r.BarkIdx.size()});
      R_.SetPrototypeSheets((int)rank, Render::SheetSet{r.Cards.data(), r.CardCount, r.CardLeafM,
                                                        Generators::TreePrototype::kCardFanDeg});
      Log::Info("outshine", "tree_grown", {{"rank", (double)rank},
          {"pixelHeightFrac", (double)ModelLadder::Error((int)rank)},
          {"barkVerts", (double)r.BarkVertCount}, {"barkTris", (double)(r.BarkIdx.size() / 3)},
          {"cards", (double)r.CardCount}, {"cardLeafM", (double)r.CardLeafM},
          {"heightM", Tree_->HeightM()}});
    }
    float row[kMaterialRowFloats];
    Generators::TreePrototype::MaterialRow(Tree_->Look(), row);
    R_.SetPrototypeMaterial(row);
    /* The model height scales every instance; without it they all scale to null. */
    R_.SetPrototypeHeightM(Tree_->HeightM());
    R_.SetPrototypeBounds(Render::ModelBounds{Tree_->Reach().HalfWidth, Tree_->Reach().Top,
                                              Tree_->Reach().Bottom});
    R_.BakePrototypeImpostor();
    TreeDraw_.emplace(Generators::ClusterId{0}, Tree_->HeightM());
    if (!Draws_.Add(Sim::kTreeRank, *TreeDraw_)) { Phase_ = Phase::Failed; return; }
    /* THE COLLECTION'S CEILING, and it is the same declaration the occupancy pool was sized on:
     * every generator asked what it can claim over the disc the picture reaches across. Taken once,
     * here, because a vector that grows during play is a budget nobody declared. */
    uint32_t reserve = 0;
    const Generators::GeneratorSet &gens = Sim_.Content();
    const double discM2 = kPi * Sim::kReachM * Sim::kReachM;
    for (size_t g = 0; g < gens.Count(); g++) reserve += gens.At(g).Proposes(discM2);
    Stands_.Reserve(reserve);
    Log::Info("outshine", "stand_budget", {{"reachM", Sim::kReachM}, {"discM2", discM2},
        {"stands", (double)reserve}, {"KB", (double)Stands_.HeapBytes() / 1024.0}});
  }
  LoadFromMs_ = LoadMovedMs_ = LoadSaidMs_ = NowMs();
  LoadPasses_ = 0;
  LoadSettledWas_ = -1;
  Phase_ = Phase::Loading;
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
  Render::GpuTimer::Sample gpu;
  if (R_.TakeGpuTimes(gpu)) Frames_.AddGpu(gpu);
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
  c.TreeTriangles = R_.ModelTriangleCount();
  c.TreeStands = (long)Stands_.Count();
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
  const std::shared_ptr<const ClassStructure> structure = cls.Read();
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
  CollectStands();
}

/* THE COLLECTION, and it is where every view-dependent rule lives: how far the picture reaches and
 * which stand holds the lens. A region that has just been generated makes the whole answer again,
 * because a collection is one answer over the regions that stand and not an append to an older
 * one. */
/* THE REACH IS A CUT AROUND THE EYE, so a collection made only when the REGION SET changes stands
 * still between two of them: at 150 m/s the ring publishes every ~600 m, and 600 m worth of stands
 * then enter in a single frame — a wall of forest arriving at 300 m instead of one stand at a time
 * at 900. This is how far the eye may walk before the cut is made again, and it is what keeps the
 * size a stand ENTERS at inside the ladder's own tolerance: entering at R - d instead of R makes it
 * H*f/(R-d) - H*f/R pixels taller, and this d is where that difference reaches tau. */
double Outshine::RecollectStepM() const {
  const double hf = Tree_->HeightM() *
                    PixelFocalLength(Sim_.Declared().RenderResolution().Height, (double)R_.GetFovDeg());
  const double tau = (double)SseTauPx();
  return Sim::kReachM * Sim::kReachM * tau / (Sim::kReachM * tau + hf);
}

void Outshine::CollectStands() {
  const Span<const Sim::Populated> regions = Sim_.Regions();
  CollectMs_ = 0.0;
  if (!Tree_) return;
  double walkedE = 0.0, walkedN = 0.0;
  if (Collected_) CollectedFrom_.Project(Sim_.Lat(), Sim_.Lon(), &walkedE, &walkedN);
  const double step = RecollectStepM();
  const bool walked = !Collected_ || walkedE * walkedE + walkedN * walkedN > step * step;
  if (Sim_.RegionVersion() == StandingRegions_ && !walked) return;
  StandingRegions_ = Sim_.RegionVersion();
  const double t0 = NowMs();
  StandField::Lens lens{TangentFrame::At(Sim_.Lat(), Sim_.Lon()), Sim_.Standpoint().AltAslM(),
                        Sim::kReachM};
  CollectedFrom_ = lens.Frame;
  Collected_ = true;
  Stands_.Aim(lens, Tree_->Reach());
  for (const Sim::Populated &region : regions) {
    Stands_.From(region.Where.Where());
    Draws_.Draw(region.Where, Sim_.Content(),
                Span<const Generators::Yield>(region.Yields.data(), region.Yields.size()),
                region.Space.Sink().Placed(), Stands_);
  }
  R_.SetPrototypeInstances(Stands_.Stands().data(), Stands_.Count(), Stands_.Frame());
  CollectMs_ = NowMs() - t0;
  if (Stands_.Refused() > 0)
    Log::Error("outshine", "stands_truncated", {{"capacity", (double)Stands_.Capacity()},
        {"refusals", (double)Stands_.Refused()}, {"regions", (double)regions.Size()}});
  Log::Info("outshine", "stands_collected", {{"regions", (double)regions.Size()},
      {"placed", (double)Sim_.StandCount()}, {"stands", (double)Stands_.Count()},
      {"beyondReachM", Sim::kReachM}, {"beyondReach", (double)Stands_.BeyondReach()},
      {"nearestM", Stands_.NearestM()}, {"farthestM", Stands_.FarthestM()},
      {"inCrown", (double)Stands_.InCrown()}, {"collectMs", CollectMs_},
      {"standsKB", (double)Stands_.HeapBytes() / 1024.0},
      {"regionSlotKB", (double)Sim_.RegionSlotBytes() / 1024.0},
      {"instanceKB", (double)R_.ModelInstanceBytes() / 1024.0},
      {"prototypeKB", (double)R_.ModelPrototypeBytes() / 1024.0},
      {"impostorKB", (double)R_.ModelImpostorBytes() / 1024.0}});
}

Outshine::Progress Outshine::Stream(double nowMs) {
  if (Phase_ != Phase::Loading && Phase_ != Phase::Playing) return {};
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
  p.PopulateMs = Sim_.PopulateMs();
  p.TilesTotal = w.TargetTotal();
  p.TilesSettled = w.TargetSettledN();
  p.TilesInView = w.TargetInViewN();
  p.VectorTilesPending = w.BuildingPendingTiles();
  p.Built = w.BuiltCount();
  p.Evicted = w.EvictedCount();
  p.Resident = w.Resident();
  p.Pool = fb_tile_pool()->Counters();
  p.Admission = w.Admissions();
  Sim_.Streaming().AddPass(p);
  return {w.LoadProgress(), w.Resident() && Sim_.RingStands()};
}

/* THE PROGRESS FRAME IS A FRAME LIKE ANY OTHER, and that is the whole point: the renderer never
 * stops, so the bar moves at the display's rate while the fetches and the decodes run beside it.
 * What the loop does NOT do is draw the world — a half-arrived scene is not a picture of anything.
 * The virtual clock is the pass index at 60 Hz, so the world's own 1 Hz counters keep the same
 * meaning they have inside a run. */
void Outshine::LoadPass() {
  const World::World &w = Sim_.Scenery();
  /* The virtual clock is the streaming PASS index at 60 Hz, so the world's own 1 Hz counters keep
   * the same meaning they have inside a run. */
  const Progress p = Stream((double)LoadPasses_ * 1000.0 / 60.0);
  const double frameMs = OpenFrame();
  R_.RenderProgress(p.Fraction);
  CloseFrame(frameMs);
  LoadPasses_++;

  const double now = NowMs();
  if (p.Resident) {
    Sim_.Streaming().MarkResident(now);
    Phase_ = Phase::Playing;
    Log::Info("outshine", "loaded", {{"passes", (double)LoadPasses_}, {"loadMs", now - LoadFromMs_},
        {"targetTotal", w.TargetTotal()}, {"targetInView", w.TargetInViewN()},
        {"progress", (double)p.Fraction}});
    return;
  }
  /* A STALL IS SAID, NOT CAPPED. There is no ceiling to hit — a server that stops answering is a
   * fact about the server — but a load that spins in silence is a fact nobody can read. */
  if (w.TargetSettledN() != LoadSettledWas_) { LoadSettledWas_ = w.TargetSettledN(); LoadMovedMs_ = now; }
  if (now - LoadMovedMs_ > kStallSayMs && now - LoadSaidMs_ > kStallSayMs) {
    LoadSaidMs_ = now;
    Log::Warn("outshine", "load_stalled", {{"stalledS", (now - LoadMovedMs_) * 0.001},
        {"passes", (double)LoadPasses_}, {"targetSettled", LoadSettledWas_},
        {"targetTotal", w.TargetTotal()}, {"vectorPending", w.BuildingPendingTiles()},
        {"waitingFor", World::AwaitName(w.Waiting())}, {"progress", (double)p.Fraction}});
  }
}

} // namespace outshine::Clients
