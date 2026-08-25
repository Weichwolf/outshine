#include "Sim.h"

#include "DeclaredSources.h"

#include <chrono>
#include <cmath>

#include "CameraBasis.h"
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

constexpr Generators::Schedule::Ring kRing{14, 1};

constexpr double kWoodDensityKgPerM3 = 700.0;

constexpr Generators::ContactMaterial kTrunkContact{1};

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

  const double r = (double)stem.TrunkRadiusM;
  stem.MassKg = (float)(kPi * r * r * stem.HeightM / 3.0 * kWoodDensityKgPerM3);
  stem.Contact = kTrunkContact;
  return stem;
}

Sim::Stance StanceOf(const SceneLegacy::WorldStage *world) {
  if (!world) return {SceneLegacy::kAnchorLatDeg, SceneLegacy::kAnchorLonDeg, 0.0, 0.0};
  return {world->Where.LatDeg(), world->Where.LonDeg(), world->YawDeg, world->PitchDeg};
}

}

Sim::Sim(const SceneLegacy::Scene &scene, const Assets &assets)
    : Scene_(scene),
      Assets_(assets),

      Wind_(Scene_.Staged().AsWorld()),
      Stance_(StanceOf(Scene_.Staged().AsWorld())),
      W_(PixelFocalLength(Scene_.RenderResolution().Height, Scene_.FovDeg())),
      Ring_(kRing) {

  StackProbe::Enter(StackProbe::Purpose::Frame);
  if (const SceneLegacy::WorldStage *w = WorldStage()) {
    ViewM_ = w->ViewM;
    OrthoM_ = w->OrthoM;
    WindDeg_ = w->WindFromDeg;
    WindMs_ = w->WindMs;
    WindClockS_ = w->WindClockS;
    Clk_ = (double)w->UtcS;
    Stand_.SetEyeAglM(w->EyeAglM);
    if (w->HasLensAslM) Stand_.SetLensAslM(w->LensAslM);
  } else {
    Stand_.SetGroundAslM(Scene_.Staged().AsStudio()->Ground.GroundAslM);
  }
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

  const Ground::VegetationTemplates::Row *rows = Veg_.Rows();
  const size_t nrows = Veg_.TemplateCount();
  std::vector<Generators::GroundTable::Row> table(nrows);
  StandsPerM2_.resize(nrows);
  for (size_t i = 0; i < nrows; i++) {
    for (int c = 0; c < 3; c++) table[i].Surface.BaseColour[c] = rows[i].Ground[c];
    table[i].Surface.Roughness = rows[i].Ground[3];
    table[i].SlopeMaxDeg = rows[i].Edge[3];
    StandsPerM2_[i] = rows[i].Edge[2];
  }
  Table_ = Generators::GroundTable::Of(
      Span<const Generators::GroundTable::Row>(table.data(), table.size()));

  if (W_.Vectors().Zoom() != Ring_.Zoom()) {
    Log::Error("sim", "region_zoom_is_not_the_vector_zoom",
               {{"ring", (double)Ring_.Zoom()}, {"vectors", (double)W_.Vectors().Zoom()}});
    return false;
  }
  const Ground::VegetationTemplates::Rule *built =
      Veg_.Find(Ground::OsmLayerName(Ground::OsmLayer::Buildings), "*");
  const Ground::VegetationTemplates::Rule *wet =
      Veg_.Find(Ground::OsmLayerName(Ground::OsmLayer::WaterPolygons), "water");
  if (!built || !wet) {
    Log::Error("sim", "outline_class_undeclared",
               {{"layer", std::string(built ? Ground::OsmLayerName(Ground::OsmLayer::WaterPolygons)
                                            : Ground::OsmLayerName(Ground::OsmLayer::Buildings))}});
    return false;
  }
  BuiltRow_ = built->Tpl;
  WetRow_ = wet->Tpl;
  Structures_.emplace(kWallContact);
  Lakes_.emplace();
  Ways_.emplace();
  if (!Gens_.Add(kBuiltRank, *Structures_)) return false;
  if (!Gens_.Add(kWaterRank, *Lakes_)) return false;
  if (!Gens_.Add(kWayRank, *Ways_)) return false;
  if (!Assets_.Species.empty()) {
    std::string why;
    if (!ReadSpecies(Assets_.Species.c_str(), Species_, why)) {
      Log::Error("sim", "species_unreadable", {{"path", Assets_.Species}, {"why", why}});
      return false;
    }
    std::vector<Generators::Forest::Stem> stems;
    stems.reserve(Species_.size());
    for (const Generators::TreeSpecies &one : Species_) { stems.push_back(StemOf(one)); }
    Trees_.emplace(Span<const Generators::Forest::Stem>(stems.data(), stems.size()),
                   Span<const float>(StandsPerM2_.data(), StandsPerM2_.size()), Veg_.Limit());
    if (!Gens_.Add(kTreeRank, *Trees_)) return false;
  }

  if (!WorldStage()) return true;
  if (!OpenPool()) return false;
  EarthSunPos(Stance_.Lat, Stance_.Lon, Clk_, &SunEl_, &SunAz_);
  return true;
}

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

std::shared_ptr<const Generators::FeatureField> Sim::Features(
    const Generators::Region &region) const {
  if (!W_.Vectors().Settled(region.X(), region.Y())) return nullptr;
  const int tile = W_.Vectors().TileIndex(region.X(), region.Y());
  const std::span<const double> points = W_.Vectors().Points();

  std::vector<Generators::FeatureField::Feature> features;
  std::vector<Generators::FeatureField::Ring> rings;
  std::vector<Generators::FeatureField::Vertex> vertices;
  const auto take = [&](const Generators::FeatureField::Feature &proto, uint32_t firstPoint,
                        uint32_t count) {
    const uint32_t least = proto.Form == Generators::FeatureForm::Ribbon ? 2u : 3u;
    if (count < least) return;
    Generators::FeatureField::Feature f = proto;
    f.FirstRing = (uint32_t)rings.size();
    f.RingCount = 1;
    rings.push_back({(uint32_t)vertices.size(), count});
    for (uint32_t k = 0; k < count; k++) {
      double eastM = 0.0, northM = 0.0;
      region.Enu(points[((size_t)firstPoint + k) * 2], points[((size_t)firstPoint + k) * 2 + 1],
                 &eastM, &northM);
      vertices.push_back({(float)eastM, (float)northM});
    }
    features.push_back(f);
  };

  for (const Ground::BuildingField::Footprint &fp : W_.Footprints().OfTile(tile)) {
    Generators::FeatureField::Feature f{};
    f.CoverRow = BuiltRow_;
    f.Kind = Generators::FeatureKind::Structure;
    f.Form = Generators::FeatureForm::Area;
    f.Base = Generators::FeatureLevel::At(fp.BaseM);
    f.Top = Generators::FeatureLevel::At(fp.BaseM + fp.HeightM);
    take(f, fp.FirstPoint, fp.PointCount);
  }
  for (const Ground::WaterField::Surface &s : W_.WaterBodies().OfTile(tile)) {
    Generators::FeatureField::Feature f{};
    f.CoverRow = WetRow_;
    f.Kind = Generators::FeatureKind::Water;
    f.Form = Generators::FeatureForm::Area;
    f.Top = Generators::FeatureLevel::At(s.LevelM);
    take(f, s.FirstPoint, s.PointCount);
  }
  for (const Ground::StreetField::Way &w : W_.Ways().OfTile(tile)) {
    Generators::FeatureField::Feature f{};
    f.CoverRow = w.CoverRow;
    f.Kind = Generators::FeatureKind::Way;
    f.Form = w.Form == Ground::StreetField::Shape::Ribbon ? Generators::FeatureForm::Ribbon
                                                         : Generators::FeatureForm::Area;
    f.HalfWidthM = w.HalfWidthM;
    take(f, w.FirstPoint, w.PointCount);
  }

  return Generators::FeatureField::Of(
      Span<const Generators::FeatureField::Feature>(features.data(), features.size()),
      Span<const Generators::FeatureField::Ring>(rings.data(), rings.size()),
      Span<const Generators::FeatureField::Vertex>(vertices.data(), vertices.size()));
}

Sim::Snapped Sim::Snapshot(const Generators::Region &region, Generators::Ground::Snapshot *out,
                           SnapshotCost *cost) const {
  const double t0 = MonotonicMs();
  const auto done = [&](Snapped how) { cost->TotalMs = MonotonicMs() - t0; return how; };
  const outshine::Ground::GroundBlock block =
      W_.Ground().BlockAt(region.Zoom(), region.X(), region.Y());
  switch (block.Where()) {
    case outshine::Ground::GroundBlock::State::Pending: return done(Snapped::Waiting);
    case outshine::Ground::GroundBlock::State::Missing: return done(Snapped::NoGround);
    case outshine::Ground::GroundBlock::State::Resolved: break;
  }

  const int side = (int)(region.SpanNm() / W_.Ground().PostM(region.AnchorLat()) + 0.5) + 1;
  std::vector<Generators::GroundPatch::Posting> postings((size_t)side * (size_t)side);
  std::vector<double> row((size_t)side);
  const double stepE = region.SpanEm() / (double)(side - 1);
  const double stepN = region.SpanNm() / (double)(side - 1);
  for (int j = 0; j < side; j++) {

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
  const double tFeat = MonotonicMs();
  out->Features = Features(region);
  cost->FeatureMs = MonotonicMs() - tFeat;
  out->Table = Table_;
  return done(out->Patch && out->Classes && out->Features ? Snapped::Taken : Snapped::Waiting);
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
  Say(*grown, SnapshotCost_);
  Grown_.push_back(std::move(*grown));
  Version_++;
}

void Sim::Ask() {
  if (!Forge_->Idle()) return;
  for (size_t n = 0; n < Ring_.Count(); n++) {
    const size_t k = (Asked_ + n) % Ring_.Count();
    const std::optional<Generators::Region> region = Ring_.At(k, Stance_.Lat, Stance_.Lon);
    if (!region || !Reached(*region) || Standing(*region)) continue;

    Asked_ = k + 1;
    Generators::Ground::Snapshot snapshot;
    const Snapped snapped = Snapshot(*region, &snapshot, &SnapshotCost_);
    if (snapped == Snapped::Waiting) return;

    if (snapped == Snapped::NoGround) {
      Log::Warn("sim", "region_without_ground", {{"zoom", (double)region->Zoom()},
          {"x", (double)region->X()}, {"y", (double)region->Y()}});
      Refused_.push_back(*region);
      return;
    }
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

void Sim::Say(const Populated &grown, const SnapshotCost &cost) const {
  const Generators::Region &region = grown.Where.Where();
  std::vector<LogField> fields{{"zoom", (double)region.Zoom()}, {"x", (double)region.X()},
      {"y", (double)region.Y()}, {"occupyMs", grown.OccupyMs},
      {"snapshotMs", cost.TotalMs}, {"featureMs", cost.FeatureMs},
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

  Bus_.Register(&Where_);
  Bus_.Register(&Stream_);
}

Sim::Bring Sim::ResolveGround(double lat, double lon, double *out) const {
  const GroundSample g = W_.Ground().At(lat, lon);
  if (g.TryAslM(out)) return Bring::Open;

  return g.Where() == GroundSample::State::Pending ? Bring::Waiting : Bring::Failed;
}

Sim::Bring Sim::Open() {
  if (Opened_) return Bring::Failed;
  if (!Streaming_) {
    if (!Wire_) {
      Log::Error("sim", "no_transport", {{"why", std::string("SetTransport was never called")}});
      return Bring::Failed;
    }
    Content_ = std::make_unique<Data::ContentStore>(Store_);
    Sources_ = std::make_unique<Data::SourceSet>(*Content_);
    std::string refused;
    if (!Data::RegisterDeclared(*Sources_, Data::ShippedProviders(), Assets_.Stars, refused)) {
      Log::Error("sim", "source_registry_refused", {{"why", refused}});
      return Bring::Failed;
    }
    Log::Info("sim", "sources", {{"count", (int)Sources_->Count()},
                                 {"contentStore", Content_->Directory()},
                                 {"contentStoreUsed", Content_->Enabled()},
                                 {"stars", Assets_.Stars}});

    for (size_t i = 0; i < Sources_->Count(); i++) {
      const Data::SourceDecl &decl = Sources_->At(i).Declaration();
      Log::Info("sim", "source",
                {{"id", decl.Id}, {"version", (int)decl.Version}, {"kind", std::string(Name(decl.Kind))},
                 {"rank", (int)decl.Order}, {"minZoom", decl.MinZoom}, {"maxZoom", decl.MaxZoom},
                 {"ancestorFill", decl.AncestorFill}, {"wire", std::string(Name(decl.Wire))},
                 {"keeps", std::string(Name(decl.Keeps))}, {"need", std::string(Name(decl.Need))},
                 {"latency", std::string(Name(decl.Latency))},
                 {"declaredPayloadKB", (double)decl.TypicalPayloadBytes / 1024.0},
                 {"retryBudget", decl.RetryBudget}});
    }
    if (!W_.Open(*Sources_, *Wire_, Stance_.Lat, Stance_.Lon, ViewM_)) {
      Log::Error("sim", "world_open_failed", {});
      return Bring::Failed;
    }
    Streaming_ = true;
  }
  double ground = 0.0;
  const Bring got = ResolveGround(Stance_.Lat, Stance_.Lon, &ground);
  if (got == Bring::Waiting) return got;
  if (got == Bring::Failed) {
    Log::Error("sim", "ground_unresolved", {{"lat", Stance_.Lat}, {"lon", Stance_.Lon}});
    return got;
  }
  Stand_.SetGroundAslM(ground);

  State_.Env.SunElDeg = SunEl_;
  State_.Env.SunAzDeg = SunAz_;
  EarthMoonPos(Stance_.Lat, Stance_.Lon, Clk_, &State_.Env.MoonElDeg, &State_.Env.MoonAzDeg,
          &State_.Env.MoonPhase);
  State_.Env.CloudCover = (float)(WorldStage() ? WorldStage()->CloudCover : 0.0);
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
  EarthSunPos(Stance_.Lat, Stance_.Lon, t, &SunEl_, &SunAz_);
  EarthMoonPos(Stance_.Lat, Stance_.Lon, t, &State_.Env.MoonElDeg, &State_.Env.MoonAzDeg,
          &State_.Env.MoonPhase);
  State_.Env.SunElDeg = SunEl_;
  State_.Env.SunAzDeg = SunAz_;
}

void Sim::Look(const Stance &s) {
  Stance_ = s;
  double groundAslM = 0.0;
  if (W_.Ground().At(s.Lat, s.Lon).TryAslM(&groundAslM)) Stand_.SetGroundAslM(groundAslM);
  const double asl = Stand_.AltAslM();
  GeoToEcef(s.Lat, s.Lon, asl, Eye_);
  CameraBasisEcef(s.YawDeg, s.PitchDeg, 0.0, s.Lat, s.Lon, Fwd_, Right_, Up_);
  State_.Platform.AltM = (float)asl;
  State_.Platform.YawDeg = (float)s.YawDeg;
  State_.Platform.PitchDeg = (float)s.PitchDeg;
  Where_.Moved({s.Lat, s.Lon, asl, s.YawDeg, s.PitchDeg});
}

void Sim::Advance() {
  PopulateMs_ = 0.0;
  W_.Update(Stance_.Lat, Stance_.Lon);
}

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

std::optional<Generators::Ground> Sim::GroundAt(double lat, double lon) const {
  const Generators::Region region = Generators::Region::Of(Ring_.Zoom(), lat, lon);
  Generators::Ground::Snapshot snapshot;
  SnapshotCost cost;
  if (Snapshot(region, &snapshot, &cost) != Snapped::Taken) return std::nullopt;
  return Generators::Ground::Of(region, snapshot);
}

Sim::Place Sim::At(double lat, double lon) const {
  Place p;
  p.GroundResolved = W_.Ground().At(lat, lon).TryAslM(&p.GroundAslM);

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
  if (Ways_) p.Made = Ways_->MadeAt(*ground, eastM, northM);
  return p;
}

}
