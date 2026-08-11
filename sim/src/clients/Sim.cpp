#include "Sim.h"

#include <chrono>
#include <cmath>

#include "Camera.h"
#include "ClassStructure.h"
#include "GroundSample.h"
#include "Ephemeris.h"
#include "Geodesy.h"
#include "Log.h"
#include "PixelFocalLength.h"
#include "Species.h"
#include "StackProbe.h"
#include "TerrainLoader.h"
#include "Units.h"

namespace outshine::Clients {
namespace {

constexpr int kAlbedoTileSize = 512;

/* THE RING. One region is one OSM tile, so no second grid exists, and the radius is what the reach
 * needs: a disc of Sim::kReachM spans 1800 m against a z14 region's 1506, so it can lie across
 * three regions on either axis and no fewer will do. */
constexpr Generators::Schedule::Ring kRing{14, 1};

/* [SET] kg/m3, air-dry broadleaf timber (Wagenfuehr, Holzatlas: beech 680...720). A stem's mass is
 * its own cone against it, which is the only mass a stand has to carry. */
constexpr double kWoodDensityKgPerM3 = 700.0;

/* [SET] the ground class row a trunk's contact carries. One row, because what a contact does is
 * physics and no generator names a material of its own yet. */
constexpr Generators::ContactMaterial kTrunkContact{1};

double MonotonicMs() {
  return std::chrono::duration<double, std::milli>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

Generators::Forest::Stem StemOf(const Generators::TreeSpecies &sp) {
  Generators::Forest::Stem stem;
  stem.HeightM = (double)sp.HeightM();
  stem.HeightSigma = sp.HeightSigma();
  stem.TrunkRadiusM = sp.DbhM() > 0.0f ? 0.5f * sp.DbhM()
                                       : sp.GrowthParams().BaseRadius * sp.HeightM();
  /* A cone of the declared stem, which is the standing volume the forestry tables measure. */
  const double r = (double)stem.TrunkRadiusM;
  stem.MassKg = (float)(kPi * r * r * stem.HeightM / 3.0 * kWoodDensityKgPerM3);
  stem.Contact = kTrunkContact;
  return stem;
}

}  // namespace

Sim::Sim(const Scene &scene, const Assets &assets)
    : Scene_(scene),
      Assets_(assets),
      Wind_(Scene_),
      Stance_{Scene_.Lat(), Scene_.Lon(), Scene_.YawDeg(), Scene_.PitchDeg()},
      WindDeg_(Scene_.WindDeg()),
      WindMs_(Scene_.WindMs()),
      Clk_((double)Scene_.UtcS()),
      W_(PixelFocalLength(Scene_.RenderResolution().Height, Scene_.FovDeg())),
      Ring_(kRing) {
  /* The thread that builds this object is the one that will draw on it, and this is the earliest
   * moment at which the engine can say so. */
  StackProbe::Enter(StackProbe::Purpose::Frame);
  ViewM_ = Scene_.ViewM();
  OrthoM_ = Scene_.OrthoM();
  Stand_.SetEyeAglM(Scene_.EyeM());
  if (Scene_.HasLensAslM()) Stand_.SetLensAslM(Scene_.LensAslM());
}

bool Sim::LoadTables() {
  if (!Mats_.Load(Assets_.GroundMaterials.c_str())) {
    Log::Error("sim", "ground_materials_failed",
               {{"path", Assets_.GroundMaterials}, {"why", Mats_.Error()}});
    return false;
  }
  if (!Veg_.Load(Assets_.Vegetation.c_str(), Mats_)) {
    Log::Error("sim", "vegetation_table_failed",
               {{"path", Assets_.Vegetation}, {"why", Veg_.Error()}});
    return false;
  }
  /* THE DECLARED TABLE A GENERATOR STANDS ON, and it is the class model's own rows read once: a
   * generator has no spelling for the vegetation table, and a per-region copy of it would be the
   * same statement in as many places as there are regions. */
  const World::VegetationTemplates::Row *rows = Veg_.Rows();
  const size_t nrows = Veg_.TemplateCount();
  std::vector<Generators::GroundTable::Row> table(nrows);
  StandsPerM2_.resize(nrows);
  for (size_t i = 0; i < nrows; i++) {
    for (int c = 0; c < 3; c++) table[i].Surface.Albedo[c] = rows[i].Ground[c];
    table[i].Surface.Roughness = rows[i].Ground[3];
    table[i].SlopeMaxDeg = rows[i].Edge[3];
    StandsPerM2_[i] = rows[i].Edge[2];
  }
  Table_ = Generators::GroundTable::Of(
      Span<const Generators::GroundTable::Row>(table.data(), table.size()));
  NoFeatures_ = Generators::FeatureField::Of(Span<const Generators::FeatureField::Feature>(),
                                             Span<const Generators::FeatureField::Ring>(),
                                             Span<const Generators::FeatureField::Vertex>());
  if (!Assets_.Species.empty()) {
    if (!ReadSpecies(Assets_.Species.c_str(), &Species_)) {
      Log::Error("sim", "species_unreadable",
                 {{"path", Assets_.Species}, {"why", Species_.Error()}});
      return false;
    }
    Trees_.emplace(StemOf(Species_),
                   Span<const float>(StandsPerM2_.data(), StandsPerM2_.size()), Veg_.Limit());
    if (!Gens_.Add(Generators::Rank{0}, *Trees_)) return false;
  }
  if (Gens_.Count() > 0 && !OpenPool()) return false;
  SunPos(Stance_.Lat, Stance_.Lon, Clk_, &SunEl_, &SunAz_);
  return true;
}

/* THE BUDGET IS THE GENERATORS' OWN DECLARATION, summed: every one of them claims out of the same
 * sink, so the slot has to hold all of them at once. A table edited denser moves this number without
 * anybody remembering to, which is the whole reason it is not written down as a constant. */
bool Sim::OpenPool() {
  const std::optional<Generators::Region> widest = Ring_.Widest(Stance_.Lat, Stance_.Lon);
  if (!widest) {
    Log::Error("sim", "ring_has_no_region", {{"lat", Stance_.Lat}, {"lon", Stance_.Lon}});
    return false;
  }
  uint64_t bodies = 0;
  for (size_t g = 0; g < Gens_.Count(); g++)
    bodies += Gens_.At(g).Proposes(widest->SpanEm() * widest->SpanNm());
  Pool_.emplace(*widest, Generators::RegionPool::Shape{(int)Ring_.Count(), (uint32_t)bodies, 8.0});
  Forge_.emplace(Gens_);
  Log::Info("sim", "region_pool", {{"zoom", (double)widest->Zoom()},
      {"widestSpanEm", widest->SpanEm()}, {"widestSpanNm", widest->SpanNm()},
      {"slots", (double)Ring_.Count()}, {"bodiesPerSlot", (double)bodies},
      {"slotKB", (double)Pool_->SlotBytes() / 1024.0},
      {"poolKB", (double)Pool_->HeapBytes() / 1024.0}});
  return true;
}

/* THE GROUND OF ONE REGION, taken whole or not at all: a region with one posting still in flight is
 * left for the next turn, which is what stops a stand from being dropped for a tile that had not
 * landed. It STOPS AT THAT POSTING — filling all 16 641 and then learning from the patch that one
 * was missing cost the same grid again every turn, for every unready region in the ring, for as
 * long as a DEM was in flight. The postings sit on the DEM's own node spacing; how far the patch's
 * own interpolation stands from the drawn surface between them is unmeasured. */
bool Sim::Snapshot(const Generators::Region &region, Generators::Ground::Snapshot *out,
                   double *ms) const {
  const double t0 = MonotonicMs();
  const int side = (int)(region.SpanNm() / fb_stream_ground_post_m(region.AnchorLat()) + 0.5) + 1;
  std::vector<Generators::GroundPatch::Posting> postings((size_t)side * (size_t)side);
  const double stepE = region.SpanEm() / (double)(side - 1);
  const double stepN = region.SpanNm() / (double)(side - 1);
  for (int j = 0; j < side; j++) {
    for (int i = 0; i < side; i++) {
      double lat = 0.0, lon = 0.0;
      region.Geo((double)i * stepE, (double)j * stepN, &lat, &lon);
      const GroundSample h = fb_stream_ground(lat, lon);
      if (h.Where() != GroundSample::State::Resolved) {
        *ms = MonotonicMs() - t0;
        return false;
      }
      postings[(size_t)j * (size_t)side + (size_t)i].Height = h;
    }
  }
  out->Patch = Generators::GroundPatch::Complete(
      region, side,
      Span<const Generators::GroundPatch::Posting>(postings.data(), postings.size()));
  out->Classes = W_.Classes().Read();
  out->Features = NoFeatures_;
  out->Table = Table_;
  *ms = MonotonicMs() - t0;
  return out->Patch && out->Classes;
}

bool Sim::Reached(const Generators::Region &region) const {
  double eastM = 0.0, northM = 0.0;
  region.Enu(Stance_.Lat, Stance_.Lon, &eastM, &northM);
  const double de = eastM < 0.0 ? -eastM : (eastM > region.SpanEm() ? eastM - region.SpanEm() : 0.0);
  const double dn = northM < 0.0 ? -northM : (northM > region.SpanNm() ? northM - region.SpanNm() : 0.0);
  return de * de + dn * dn < kReachM * kReachM;
}

bool Sim::Names(const Generators::Region &region) const {
  for (size_t k = 0; k < Ring_.Count(); k++) {
    const std::optional<Generators::Region> named = Ring_.At(k, Stance_.Lat, Stance_.Lon);
    if (named && named->Is(region)) return true;
  }
  return false;
}

bool Sim::Standing(const Generators::Region &region) const {
  for (const Populated &p : Grown_)
    if (p.Where.Where().Is(region)) return true;
  for (const Generators::Region &refused : Refused_)
    if (refused.Is(region)) return true;
  return false;
}

/* WHAT THE RING NO LONGER NAMES GOES BACK, and what is under way for such a region is dropped. The
 * ring is a function of the eye's own region, so it changes at a crossing and not at a step: a
 * region held against the reach alone would be generated again every time the eye wandered over one
 * threshold. */
void Sim::Release() {
  for (size_t i = Grown_.size(); i-- > 0;) {
    if (Names(Grown_[i].Where.Where())) continue;
    Grown_.erase(Grown_.begin() + (long)i);
    Version_++;
  }
  for (size_t i = Refused_.size(); i-- > 0;)
    if (!Names(Refused_[i])) Refused_.erase(Refused_.begin() + (long)i);
  const std::optional<Generators::Region> under = Forge_->UnderWay();
  if (under && !Names(*under)) Forge_->Cancel();
}

void Sim::Gather() {
  std::optional<Populated> grown = Forge_->Collect();
  if (!grown) return;
  const Generators::Region where = grown->Where.Where();
  /* A REGION THAT DOES NOT FIT IS REFUSED WHOLE. Half of one is a straight lattice-row edge in the
   * picture and a count nobody can attribute; the budget is the pool's, and what exceeds it is a
   * declaration this client cannot hold. */
  uint32_t full = 0;
  for (const Generators::Yield &yield : grown->Yields)
    full += yield.Claims(Generators::Claim::Outcome::Full);
  if (full > 0) {
    Log::Error("sim", "region_refused", {{"zoom", (double)where.Zoom()}, {"x", (double)where.X()},
        {"y", (double)where.Y()}, {"why", std::string("body budget")},
        {"bodyCapacity", (double)grown->Space.Sink().Capacity()},
        {"refusedClaims", (double)full}});
    Refused_.push_back(where);
    return;
  }
  Say(*grown, SnapshotMs_);
  Grown_.push_back(std::move(*grown));
  Version_++;
}

/* ONE REQUEST AT A TIME, because one region is in flight by construction. The snapshot is taken on
 * this thread — the DEM oracle it reads has one cache and one thread may hold it — and the
 * generators run on the forge's. */
void Sim::Ask() {
  if (!Forge_->Idle()) return;
  for (size_t k = 0; k < Ring_.Count(); k++) {
    const std::optional<Generators::Region> region = Ring_.At(k, Stance_.Lat, Stance_.Lon);
    if (!region || !Reached(*region) || Standing(*region)) continue;

    Generators::Ground::Snapshot snapshot;
    if (!Snapshot(*region, &snapshot, &SnapshotMs_)) continue;
    const std::optional<Generators::Ground> ground = Generators::Ground::Of(*region, snapshot);
    if (!ground) continue;
    std::optional<Generators::RegionPool::Lease> lease = Pool_->TryAcquire(*ground);
    if (!lease) {
      Log::Warn("sim", "region_pool_empty",
                {{"zoom", (double)region->Zoom()}, {"x", (double)region->X()},
                 {"y", (double)region->Y()}, {"regions", (double)Grown_.size()}});
      return;
    }
    Forge_->Request(*ground, std::move(*lease));
    return;
  }
}

void Sim::Populate() {
  if (!Forge_) return;
  Release();
  Gather();
  Ask();
}

/* THE WHOLE PARTITION IN ONE LINE: every candidate of the region leaves through exactly one name, so
 * a count that does not sum to the lattice is a case nobody wrote. The two millisecond fields are
 * the two threads it cost — the snapshot on the caller's, the generators on the forge's. */
void Sim::Say(const Populated &grown, double snapshotMs) const {
  const Generators::Region &region = grown.Where.Where();
  std::vector<LogField> fields{{"zoom", (double)region.Zoom()}, {"x", (double)region.X()},
      {"y", (double)region.Y()}, {"occupyMs", grown.OccupyMs}, {"snapshotMs", snapshotMs},
      {"slotKB", (double)Pool_->SlotBytes() / 1024.0},
      {"patchKB", (double)grown.Where.PatchHeapBytes() / 1024.0},
      {"speciesLimitAslM", Veg_.Limit().SpeciesLimitM(region.AnchorLat())}};
  for (const Generators::Yield &yield : grown.Yields) {
    fields.push_back({"placed", (double)yield.Claims(Generators::Claim::Outcome::Placed)});
    fields.push_back({"occupied", (double)yield.Claims(Generators::Claim::Outcome::Occupied)});
    fields.push_back({"outside", (double)yield.Claims(Generators::Claim::Outcome::Outside)});
    fields.push_back({"full", (double)yield.Claims(Generators::Claim::Outcome::Full)});
    for (const Generators::Yield::Note &note : yield.Notes())
      fields.push_back({note.Name, note.Raised ? note.Peak : (double)note.Times});
  }
  Log::Info("sim", "region_grown", fields);
}

bool Sim::RingStands() const {
  if (!Forge_) return true;
  if (!Forge_->Idle()) return false;
  for (size_t k = 0; k < Ring_.Count(); k++) {
    const std::optional<Generators::Region> region = Ring_.At(k, Stance_.Lat, Stance_.Lon);
    if (!region || !Reached(*region)) continue;
    if (!Standing(*region)) return false;
  }
  return true;
}

long Sim::StandCount() const {
  long n = 0;
  for (const Populated &p : Grown_)
    n += (long)p.Space.Sink().Placed().Size();
  return n;
}

void Sim::StartTelemetry() {
  if (Identity_) Bus_.Register(Identity_);
  Bus_.Register(&Stream_);
}

Sim::Bring Sim::ResolveGround(double lat, double lon, double *out) const {
  const GroundSample g = fb_stream_ground(lat, lon);
  if (g.TryAslM(out)) return Bring::Open;
  /* A hole never becomes a height. */
  return g.Where() == GroundSample::State::Pending ? Bring::Waiting : Bring::Failed;
}

Sim::Bring Sim::Open() {
  if (Opened_) return Bring::Failed;
  if (!Streaming_) {
    if (!W_.Open(Base_.c_str(), Stance_.Lat, Stance_.Lon, ViewM_, kAlbedoTileSize)) {
      Log::Error("sim", "world_open_failed", {{"base", Base_}});
      return Bring::Failed;
    }
    Streaming_ = true;
  }
  double ground = 0.0;
  const Bring got = ResolveGround(Stance_.Lat, Stance_.Lon, &ground);
  if (got == Bring::Waiting) return got;
  if (got == Bring::Failed) {
    Log::Error("sim", "ground_unresolved",
               {{"lat", Stance_.Lat}, {"lon", Stance_.Lon}, {"base", Base_}});
    return got;
  }
  Stand_.SetGroundAslM(ground);

  State_.Env.SunElDeg = SunEl_;
  State_.Env.SunAzDeg = SunAz_;
  MoonPos(Stance_.Lat, Stance_.Lon, Clk_, &State_.Env.MoonElDeg, &State_.Env.MoonAzDeg,
          &State_.Env.MoonPhase);
  State_.Env.CloudCover = (float)Scene_.CloudCover();
  Look(Stance_);

  W_.SetVegetation(&Veg_);
  W_.SetWeather(&Wind_);
  const WindNed w = Wind_.WindNedMs(Stance_.Lat, Stance_.Lon, Stand_.AltAslM());
  Log::Info("sim", "stand", {{"groundM", Stand_.GroundAslM()}, {"eyeM", Stand_.EyeAglM()},
      {"pitchDeg", Stance_.PitchDeg}, {"aslM", Stand_.AltAslM()}, {"liftM", Stand_.LiftM()},
      {"sunElDeg", (double)SunEl_}, {"sunAzDeg", (double)SunAz_},
      {"moonElDeg", (double)State_.Env.MoonElDeg}, {"cloudCover", (double)State_.Env.CloudCover},
      {"windN", w.N}, {"windE", w.E}, {"windD", w.D}});
  Opened_ = true;
  return Bring::Open;
}

void Sim::SetSkyOffsetS(double s) {
  const double t = Clk_ + s;
  SunPos(Stance_.Lat, Stance_.Lon, t, &SunEl_, &SunAz_);
  MoonPos(Stance_.Lat, Stance_.Lon, t, &State_.Env.MoonElDeg, &State_.Env.MoonAzDeg,
          &State_.Env.MoonPhase);
  State_.Env.SunElDeg = SunEl_;
  State_.Env.SunAzDeg = SunAz_;
}

void Sim::Look(const Stance &s) {
  Stance_ = s;
  double groundAslM = 0.0;
  if (fb_stream_ground(s.Lat, s.Lon).TryAslM(&groundAslM)) Stand_.SetGroundAslM(groundAslM);
  const double asl = Stand_.AltAslM();
  GeoToEcef(s.Lat, s.Lon, asl, Eye_);
  CameraBasisEcef(s.YawDeg, s.PitchDeg, 0.0, s.Lat, s.Lon, Fwd_, Right_, Up_);
  State_.Platform.AltM = (float)asl;
  State_.Platform.YawDeg = (float)s.YawDeg;
  State_.Platform.PitchDeg = (float)s.PitchDeg;
}

void Sim::Advance() { W_.Update(Stance_.Lat, Stance_.Lon); }

/* THE STANDPOINT IS CHECKED AGAINST WHAT IS DATA. Terrain and buildings come from DEM and OSM, so
 * the eye is LIFTED above them; a tree is a draw from a landcover density and the eye is not moved
 * for one — the stand that would hold it is dropped where the picture is collected instead.
 * Buildings only exist once the vector tiles have landed, which is why this waits for residency. */
void Sim::Settle() {
  if (!RoofChecked_ && Stand_.LensDeclared()) {
    RoofChecked_ = true;
    const double roof = W_.RoofAslAt(Stance_.Lat, Stance_.Lon);
    const double before = Stand_.AltAslM();
    Stand_.SetRoofAslM(roof);
    if (Stand_.AltAslM() != before) {
      Log::Info("sim", "standpoint_roof", {{"roofAslM", roof},
          {"liftM", Stand_.AltAslM() - before}, {"eyeM", Stand_.EyeAglM()},
          {"totalLiftM", Stand_.LiftM()}});
      Look(Stance_);
    }
  }
  Populate();
}

Sim::Place Sim::At(double lat, double lon) const {
  Place p;
  p.GroundResolved = fb_stream_ground(lat, lon).TryAslM(&p.GroundAslM);

  const std::shared_ptr<const ClassStructure> cls = W_.Classes().Read();
  if (cls) {
    double e = 0.0, n = 0.0;
    W_.Classes().Project(lat, lon, &e, &n);
    int runnerUp = -1;
    p.Class = cls->Evaluate(e, n, &p.ClassEdgeM, &runnerUp);
  }

  const double roof = W_.RoofAslAt(lat, lon);
  if (p.GroundResolved && World::World::SurfaceStands(roof)) p.StructureHeightM = roof - p.GroundAslM;
  const double level = W_.WaterAslAt(lat, lon);
  if (p.GroundResolved && World::World::SurfaceStands(level)) p.WaterDepthM = level - p.GroundAslM;
  return p;
}

} // namespace outshine::Clients
