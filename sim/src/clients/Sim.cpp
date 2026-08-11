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

/* [SET] the contact row a wall carries. Its own row rather than the trunk's, because the two answer
 * a collision differently and the table is where that difference is declared. */
constexpr Generators::ContactMaterial kWallContact{2};

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
  /* ONE REGION IS ONE TILE, and this is where that stops being a convention: the ring's regions and
   * the vector field's tiles are addressed by the same key, so a region's outlines are one tile's
   * outlines and neither side may be re-zoomed alone. */
  if (W_.Vectors().Zoom() != Ring_.Zoom()) {
    Log::Error("sim", "region_zoom_is_not_the_vector_zoom",
               {{"ring", (double)Ring_.Zoom()}, {"vectors", (double)W_.Vectors().Zoom()}});
    return false;
  }
  const World::VegetationTemplates::Rule *built = Veg_.Find("buildings", "*");
  const World::VegetationTemplates::Rule *wet = Veg_.Find("water_polygons", "water");
  if (!built || !wet) {
    Log::Error("sim", "outline_class_undeclared",
               {{"buildings", built != nullptr}, {"water_polygons", wet != nullptr}});
    return false;
  }
  BuiltRow_ = built->Tpl;
  WetRow_ = wet->Tpl;
  Structures_.emplace(BuiltRow_, kWallContact);
  Lakes_.emplace(WetRow_);
  if (!Gens_.Add(kBuiltRank, *Structures_)) return false;
  if (!Gens_.Add(kWaterRank, *Lakes_)) return false;
  if (!Assets_.Species.empty()) {
    if (!ReadSpecies(Assets_.Species.c_str(), &Species_)) {
      Log::Error("sim", "species_unreadable",
                 {{"path", Assets_.Species}, {"why", Species_.Error()}});
      return false;
    }
    Trees_.emplace(StemOf(Species_),
                   Span<const float>(StandsPerM2_.data(), StandsPerM2_.size()), Veg_.Limit());
    if (!Gens_.Add(kTreeRank, *Trees_)) return false;
  }
  if (!OpenPool()) return false;
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
  const Generators::Region broadest = Ring_.Broadest();
  Pool_.emplace(Generators::RegionPool::Extent{*widest, broadest},
                Generators::RegionPool::Shape{(int)Ring_.Count(), (uint32_t)bodies, 8.0});
  Forge_.emplace(Gens_);
  Log::Info("sim", "region_pool", {{"zoom", (double)widest->Zoom()},
      {"widestSpanEm", widest->SpanEm()}, {"widestSpanNm", widest->SpanNm()},
      {"broadestSpanEm", broadest.SpanEm()}, {"broadestSpanNm", broadest.SpanNm()},
      {"slots", (double)Ring_.Count()}, {"bodiesPerSlot", (double)bodies},
      {"slotKB", (double)Pool_->SlotBytes() / 1024.0},
      {"poolKB", (double)Pool_->HeapBytes() / 1024.0}});
  return true;
}

/* THE REGION'S OWN OUTLINES, cut out of the world's decoded vectors and put into the frame a
 * generator works in — metres east and north of the region anchor. The core resolved what each of
 * them carries: a footprint's roof off its own ground and an OSM tag, a water body's level off its
 * shore. Neither number is derived here, which is the split doc/architecture.md draws.
 *
 * NULL WHILE THIS REGION'S VECTOR TILE IS STILL OUT. One region is one tile on both sources, so the
 * rule the DEM block already follows holds here too: taken whole or not at all. A region built from
 * half its outlines would stand for good with a street of houses missing.
 *
 * A ring clipped at a tile seam arrives twice, once per tile, and both copies are kept: their union
 * is the outline, which is all a point query and a bounding box need. */
std::shared_ptr<const Generators::FeatureField> Sim::Features(
    const Generators::Region &region) const {
  if (!W_.Vectors().Decoded(region.X(), region.Y())) return nullptr;
  const std::vector<double> &points = W_.Vectors().Points();

  double latLo = 0.0, lonLo = 0.0, latHi = 0.0, lonHi = 0.0;
  region.Geo(0.0, 0.0, &latLo, &lonLo);
  region.Geo(region.SpanEm(), region.SpanNm(), &latHi, &lonHi);

  std::vector<Generators::FeatureField::Feature> features;
  std::vector<Generators::FeatureField::Ring> rings;
  std::vector<Generators::FeatureField::Vertex> vertices;
  const auto take = [&](uint32_t firstPoint, uint32_t count, int coverRow,
                        Generators::FeatureTop top) {
    if (count < 3) return;
    double minLat = 1.0e9, maxLat = -1.0e9, minLon = 1.0e9, maxLon = -1.0e9;
    for (uint32_t k = 0; k < count; k++) {
      const double lat = points[((size_t)firstPoint + k) * 2];
      const double lon = points[((size_t)firstPoint + k) * 2 + 1];
      minLat = lat < minLat ? lat : minLat;
      maxLat = lat > maxLat ? lat : maxLat;
      minLon = lon < minLon ? lon : minLon;
      maxLon = lon > maxLon ? lon : maxLon;
    }
    if (maxLat < latLo || minLat > latHi || maxLon < lonLo || minLon > lonHi) return;
    Generators::FeatureField::Feature f{};
    f.FirstRing = (uint32_t)rings.size();
    f.RingCount = 1;
    f.CoverRow = coverRow;
    f.Top = top;
    rings.push_back({(uint32_t)vertices.size(), count});
    for (uint32_t k = 0; k < count; k++) {
      double eastM = 0.0, northM = 0.0;
      region.Enu(points[((size_t)firstPoint + k) * 2], points[((size_t)firstPoint + k) * 2 + 1],
                 &eastM, &northM);
      vertices.push_back({(float)eastM, (float)northM});
    }
    features.push_back(f);
  };

  for (const World::BuildingField::Footprint &fp : W_.Footprints().Footprints())
    take(fp.FirstPoint, fp.PointCount, BuiltRow_,
         Generators::FeatureTop::At(fp.BaseM + fp.HeightM));
  for (const World::WaterField::Surface &s : W_.WaterBodies().Surfaces())
    take(s.FirstPoint, s.PointCount, WetRow_, Generators::FeatureTop::At(s.LevelM));

  return Generators::FeatureField::Of(
      Span<const Generators::FeatureField::Feature>(features.data(), features.size()),
      Span<const Generators::FeatureField::Ring>(rings.data(), rings.size()),
      Span<const Generators::FeatureField::Vertex>(vertices.data(), vertices.size()));
}

/* THE GROUND OF ONE REGION, TAKEN OUT OF THE POOL WHOLE. One region is one DEM tile at one zoom, so
 * the posting block the patch wants is the block the tile already holds: the region is found once
 * and every posting after that is arithmetic. Recovering the same block by asking the point oracle
 * per posting cost 16 641 geodetic round trips and 16 641 slot scans to rebuild bytes that were
 * already there, on the thread that draws.
 *
 * The postings sit on the DEM's own node spacing; how far the patch's own interpolation stands from
 * the drawn surface between them is unmeasured. */
bool Sim::Snapshot(const Generators::Region &region, Generators::Ground::Snapshot *out,
                   double *ms) const {
  const double t0 = MonotonicMs();
  const auto done = [&](bool ok) { *ms = MonotonicMs() - t0; return ok; };
  const FbGroundBlock block = fb_stream_ground_block(region.Zoom(), region.X(), region.Y());
  if (block.Where() != FbGroundBlock::State::Resolved) return done(false);

  const int side = (int)(region.SpanNm() / fb_stream_ground_post_m(region.AnchorLat()) + 0.5) + 1;
  std::vector<Generators::GroundPatch::Posting> postings((size_t)side * (size_t)side);
  std::vector<double> row((size_t)side);
  const double stepE = region.SpanEm() / (double)(side - 1);
  const double stepN = region.SpanNm() / (double)(side - 1);
  for (int j = 0; j < side; j++) {
    /* The region's own frame scales longitude by the latitude of the row, so one row is equally
     * spaced in longitude and the next one is not — which is exactly the shape the block reads. */
    double lat = 0.0, lonFrom = 0.0, latAgain = 0.0, lonNext = 0.0;
    region.Geo(0.0, (double)j * stepN, &lat, &lonFrom);
    region.Geo(stepE, (double)j * stepN, &latAgain, &lonNext);
    block.AslMRow(lat, lonFrom, lonNext - lonFrom, side, row.data());
    for (int i = 0; i < side; i++)
      postings[(size_t)j * (size_t)side + (size_t)i].Height = GroundSample::At(row[(size_t)i]);
  }
  out->Patch = Generators::GroundPatch::Complete(
      region, side,
      Span<const Generators::GroundPatch::Posting>(postings.data(), postings.size()));
  out->Classes = W_.Classes().Read();
  out->Features = Features(region);
  out->Table = Table_;
  return done(out->Patch && out->Classes && out->Features);
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
 * this thread — the DEM oracle it reads is not re-entrant (world/TerrainLoader.h) — and the
 * generators run on the forge's.
 *
 * ONE SNAPSHOT ATTEMPT PER TURN, ROUND ROBIN. A ring of nine unready regions used to try all nine
 * in the frame that asked and try them all again in the next, so a DEM in flight was a SUSTAINED
 * frame cost for as long as it was in flight. The cursor is what keeps that budget from coupling the
 * ring to its slowest member: a region whose DEM never lands is stepped over next turn instead of
 * standing in front of the eight behind it. */
void Sim::Ask() {
  if (!Forge_->Idle()) return;
  for (size_t n = 0; n < Ring_.Count(); n++) {
    const size_t k = (Asked_ + n) % Ring_.Count();
    const std::optional<Generators::Region> region = Ring_.At(k, Stance_.Lat, Stance_.Lon);
    if (!region || !Reached(*region) || Standing(*region)) continue;

    Asked_ = k + 1;
    Generators::Ground::Snapshot snapshot;
    if (!Snapshot(*region, &snapshot, &SnapshotMs_)) return;
    const std::optional<Generators::Ground> ground = Generators::Ground::Of(*region, snapshot);
    if (!ground) return;
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
  const double t0 = MonotonicMs();
  Release();
  Gather();
  Ask();
  PopulateMs_ = MonotonicMs() - t0;
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
      {"featureKB", (double)grown.Where.FeatureHeapBytes() / 1024.0},
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

/* One pass begins here, which is also where the ring's cost for it is zero again: a pass that never
 * reaches Populate spent nothing on it and must not publish the last one that did. */
void Sim::Advance() {
  PopulateMs_ = 0.0;
  W_.Update(Stance_.Lat, Stance_.Lon);
}

/* THE STANDPOINT IS CHECKED AGAINST WHAT IS DATA. Terrain and buildings come from DEM and OSM, so
 * the eye is LIFTED above them; a tree is a draw from a landcover density and the eye is not moved
 * for one — the stand that would hold it is dropped where the picture is collected instead.
 * The roof comes out of the same generator every other caller asks, so it waits for the region the
 * eye stands in and not merely for a vector tile. */
void Sim::Settle() {
  if (!RoofChecked_ && Stand_.LensDeclared()) {
    const std::optional<Generators::Ground> ground = GroundAt(Stance_.Lat, Stance_.Lon);
    Generators::Body roof;
    if (ground && Structures_) {
      RoofChecked_ = true;
      double eastM = 0.0, northM = 0.0;
      ground->Where().Enu(Stance_.Lat, Stance_.Lon, &eastM, &northM);
      if (Structures_->At(*ground, eastM, northM, &roof)) {
        const double before = Stand_.AltAslM();
        Stand_.SetRoofAslM(roof.BaseAslM + (double)roof.HeightM);
        if (Stand_.AltAslM() != before)
          Log::Info("sim", "standpoint_roof", {{"roofAslM", roof.BaseAslM + (double)roof.HeightM},
              {"liftM", Stand_.AltAslM() - before}, {"eyeM", Stand_.EyeAglM()},
              {"totalLiftM", Stand_.LiftM()}});
        Look(Stance_);
      }
    }
  }
  Populate();
}

/* ONE REGION, BUILT WHERE IT IS ASKED FOR. A point query has no ring behind it and no forge: the
 * region containing the place is snapshotted on this thread and thrown away again, which is what
 * lets the server target answer a place it never walked to. */
std::optional<Generators::Ground> Sim::GroundAt(double lat, double lon) const {
  const Generators::Region region = Generators::Region::Of(Ring_.Zoom(), lat, lon);
  Generators::Ground::Snapshot snapshot;
  double ms = 0.0;
  if (!Snapshot(region, &snapshot, &ms)) return std::nullopt;
  return Generators::Ground::Of(region, snapshot);
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

  const std::optional<Generators::Ground> ground = GroundAt(lat, lon);
  if (!ground) return p;
  p.OutlinesResolved = true;
  double eastM = 0.0, northM = 0.0;
  ground->Where().Enu(lat, lon, &eastM, &northM);
  Generators::Body structure;
  if (Structures_ && Structures_->At(*ground, eastM, northM, &structure))
    p.StructureHeightM = (double)structure.HeightM;
  if (Lakes_) p.Water = Lakes_->DepthAt(*ground, eastM, northM);
  return p;
}

} // namespace outshine::Clients
