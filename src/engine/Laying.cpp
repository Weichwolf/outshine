#include "GeodeticCamera.h"
#include "Digest.h"
#include "math/RenderFrame.h"
#include "math/Quantile.h"
#include "math/Units.h"
#include "math/Vec2.h"
#include "math/Vec3.h"
#include "Capacity.h"
#include "Log.h"
#include <algorithm>
#include <atomic>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <expected>
#include <format>
#include <memory>
#include <cmath>
#include "Heap.h"
#include "TangentFrame.h"
#include <array>
#include <cassert>
#include <functional>
#include <optional>
#include <span>
#include <string_view>
#include <numbers>
#include <string>
#include <ratio>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <chrono>
#include <vector>

#include "Fit.h"
#include "ReferenceLine.h"

#include "spatial/Census.h"
#include "spatial/Drape.h"
#include "geo/PlaceKey.h"
#include "spatial/Refine.h"
#include "Corridors.h"
#include "TerrainMesh.h"
#include "TerrainPress.h"
#include "EngineHeld.h"
#include "GroundWorldCandidate.h"
#include "GroundBuildSchedule.h"
#include "GroundMesher.h"
#include "TransportNetwork.h"

namespace outshine {
namespace Says {
constexpr auto WaterCreationFailed = "could not publish water geometry";
constexpr auto MaterialCreationFailed = "could not create ground materials";
constexpr auto CorridorTerrainIncomplete =
    "corridor generation reached terrain without a DEM field";
constexpr auto GroundContactIncomplete = "ground patchwork has no ready contact terrain";
}

constexpr uint64_t kLowWord = 0xFFFFFFFFULL;

static_assert(Ground::kStreamGrid == 2 * kPatchGrid,
              "the elevation stream is opened ONE zoom below the finest tile so its posting equals "
              "the patchwork's vertex spacing; that holds only while a stream tile carries twice "
              "the intervals a patchwork tile lays, and if either grid moves the zoom must be "
              "re-derived rather than kept");

namespace {

constexpr double kPerMille = 1000.0;
constexpr float kVerticalSlopeDeg = 90.0f;

constexpr float kWallRed = 0.74f;
constexpr float kWallGreen = 0.71f;
constexpr float kWallBlue = 0.65f;
constexpr float kWallRoughness = 0.88f;
constexpr float kTileRed = 0.42f;
constexpr float kTileGreen = 0.20f;
constexpr float kTileBlue = 0.14f;
constexpr float kTileRoughness = 0.72f;

constexpr double kPadApronM = 6.0;
constexpr double kWaterBedM = 2.0;
constexpr double kWaterBankM = kWaterBedM / kBatterRise;
constexpr size_t kBounceProbeStride = 16;
constexpr size_t kPlayableStructureCandidates = 1;
constexpr size_t kRefinedStructureCandidates = 4;
constexpr size_t kTerrainSheetsPerFrame = 96;
constexpr size_t kTerrainResidencySheetsPerFrame = 128;
constexpr size_t kEarthworkSheetsPerFrame = 32;
constexpr size_t kEarthworkPointsPerFrame = 8192;
constexpr size_t kCorridorLanesPerFrame = 128;
constexpr size_t kCorridorNodesPerFrame = 64;
constexpr size_t kNetworkItemsPerFrame = 1024;

bool GroundSourcesReady(const Ground::GroundStack &stack, GroundQuality quality) {
  return quality == GroundQuality::Refined ? stack.Ingested() : stack.IngestedWithin(0);
}

}

class GroundBuildState {
public:
  struct MeshBuild {
    Generators::TerrainMesh Mesh;
    size_t NextSheet = 0;
    bool SheetsStitched = false;
    bool ResidencyStarted = false;
    bool ResidencyReady = false;
    double LongestSliceMs = 0.0;
    double LongestResidencySliceMs = 0.0;
  };

  GroundBuildState(Render::SceneRenderer &renderer,
                   const Surrounds &world,
                   const Ground::BuildingField &footprints,
                   Around coverage,
                   GroundRevision revision)
      : Coverage_(coverage), Revision_(revision), Candidate_(renderer, world, footprints) {}

  [[nodiscard]] bool Matches(const GroundRevision &revision) const noexcept {
    return Revision_.Region == revision.Region && Revision_.Classes == revision.Classes &&
           Revision_.Footprints == revision.Footprints &&
           Revision_.VectorGeneration == revision.VectorGeneration &&
           Revision_.StreetTiles == revision.StreetTiles &&
           Revision_.WaterTiles == revision.WaterTiles &&
           Revision_.Projection == revision.Projection && Revision_.Coverage == revision.Coverage &&
           Revision_.Quality == revision.Quality;
  }

  [[nodiscard]] uint32_t RevisionDifference(const GroundRevision &revision) const noexcept {
    uint32_t difference = 0;
    difference |= Revision_.Region != revision.Region ? 1u << 0u : 0u;
    difference |= Revision_.Classes != revision.Classes ? 1u << 1u : 0u;
    difference |= Revision_.Footprints != revision.Footprints ? 1u << 2u : 0u;
    difference |= Revision_.StreetTiles != revision.StreetTiles ? 1u << 3u : 0u;
    difference |= Revision_.WaterTiles != revision.WaterTiles ? 1u << 4u : 0u;
    difference |= Revision_.Projection != revision.Projection ? 1u << 5u : 0u;
    difference |= Revision_.Coverage != revision.Coverage ? 1u << 6u : 0u;
    difference |= Revision_.Quality != revision.Quality ? 1u << 7u : 0u;
    difference |= Revision_.VectorGeneration != revision.VectorGeneration ? 1u << 8u : 0u;
    return difference;
  }

  [[nodiscard]] const Around &Coverage() const noexcept { return Coverage_; }

  [[nodiscard]] const GroundRevision &Revision() const noexcept { return Revision_; }

  [[nodiscard]] GroundWorldCandidate &Candidate() noexcept { return Candidate_; }

  [[nodiscard]] MeshBuild &Meshing() noexcept { return Meshing_; }

  [[nodiscard]] MeshBuild &InitialMeshing() noexcept { return InitialMeshing_; }

  [[nodiscard]] Generators::TerrainPressJob *Pressing() noexcept { return Pressing_.get(); }

  void BeginsPressing(std::unique_ptr<Generators::TerrainPressJob> pressing) noexcept {
    Pressing_ = std::move(pressing);
  }

  void SamplesPressingSlice(double milliseconds) noexcept {
    LongestPressingSliceMs_ = std::max(LongestPressingSliceMs_, milliseconds);
  }

  [[nodiscard]] double LongestPressingSliceMs() const noexcept { return LongestPressingSliceMs_; }

  [[nodiscard]] Ground::BuildingField &Footprints() noexcept {
    return Candidate_.Products().Footprints;
  }

  void PublishesFootprints() noexcept { Revision_.Footprints = Footprints().Revision(); }

  [[nodiscard]] Patchwork *Laid() noexcept { return Patchwork_ ? &*Patchwork_ : nullptr; }

  void Lays(Patchwork patchwork) noexcept { Patchwork_.emplace(std::move(patchwork)); }

  [[nodiscard]] Core::GroundBuildSchedule::SheetPhase SheetBuilding() const noexcept {
    return Schedule_.SheetBuilding();
  }

  void CompletesSheetPhase() noexcept {
    RecordsProductPeak();
    [[maybe_unused]] const bool completed = Schedule_.CompletesSheetPhase();
    assert(completed);
  }

  [[nodiscard]] Core::GroundBuildSchedule::Stage NextStage() const noexcept {
    return Schedule_.NextStage();
  }

  void CompletesStage() noexcept {
    RecordsProductPeak();
    [[maybe_unused]] const bool completed = Schedule_.CompletesStage();
    assert(completed);
  }

  [[nodiscard]] size_t ProductPeakBytes() const noexcept { return ProductPeakBytes_; }

  void SamplesProductPeak() noexcept { RecordsProductPeak(); }

  void HoldsCorridors(std::vector<Yields> corridors) { Corridors_ = std::move(corridors); }

  [[nodiscard]] Generators::Corridors::Job *CorridorJob() noexcept { return CorridorJob_.get(); }

  void BeginsCorridors(std::unique_ptr<Generators::Corridors::Job> job) noexcept {
    CorridorJob_ = std::move(job);
  }

  void FinishesCorridors() noexcept { CorridorJob_.reset(); }

  [[nodiscard]] outshine::World::TransportNetworkBuildJob *NetworkJob() noexcept {
    return NetworkJob_.get();
  }

  void BeginsNetwork(std::unique_ptr<outshine::World::TransportNetworkBuildJob> job) noexcept {
    NetworkJob_ = std::move(job);
  }

  void FinishesNetwork() noexcept { NetworkJob_.reset(); }

  void SamplesCorridorSlice(double milliseconds) noexcept {
    LongestCorridorSliceMs_ = std::max(LongestCorridorSliceMs_, milliseconds);
  }

  [[nodiscard]] double LongestCorridorSliceMs() const noexcept { return LongestCorridorSliceMs_; }

  [[nodiscard]] std::vector<Yields> TakesCorridors() { return std::move(Corridors_); }

  [[nodiscard]] std::chrono::steady_clock::time_point Began() const noexcept { return Began_; }

  [[nodiscard]] bool Prepared() const noexcept { return Schedule_.Prepared(); }

  void MarksPrepared() noexcept {
    [[maybe_unused]] const bool prepared = Schedule_.MarksPrepared();
    assert(prepared);
  }

  [[nodiscard]] std::string_view Status() const noexcept {
    if (!Schedule_.Prepared()) { return "candidate"; }
    if (!Patchwork_) { return "patchwork"; }
    return Schedule_.Status();
  }

  [[nodiscard]] uint8_t Progress() const noexcept {
    if (!Schedule_.Prepared()) { return 0; }
    if (!Patchwork_) { return 1; }
    if (Schedule_.SheetBuilding() != Core::GroundBuildSchedule::SheetPhase::Ready) {
      return static_cast<uint8_t>(Schedule_.SheetBuilding()) + 2u;
    }
    return static_cast<uint8_t>(Schedule_.NextStage()) + 6u;
  }

private:
  void RecordsProductPeak() noexcept {
    const size_t phaseBytes = (Patchwork_ ? Patchwork_->HeapBytes() : 0u) +
                              Corridors_.capacity() * sizeof(Yields) +
                              Meshing_.Mesh.PositionsM.capacity() * sizeof(float) +
                              Meshing_.Mesh.Indices.capacity() * sizeof(uint32_t) +
                              InitialMeshing_.Mesh.PositionsM.capacity() * sizeof(float) +
                              InitialMeshing_.Mesh.Indices.capacity() * sizeof(uint32_t) +
                              (Pressing_ ? Pressing_->HeapBytes() : 0u);
    size_t corridorBytes = 0;
    for (const Yields &corridor : Corridors_) { corridorBytes += corridor.HeapBytes(); }
    ProductPeakBytes_ = std::max(
        ProductPeakBytes_, Candidate_.Products().OwnedHeapBytes() + phaseBytes + corridorBytes);
  }

  Around Coverage_;
  GroundRevision Revision_;
  GroundWorldCandidate Candidate_;
  std::optional<Patchwork> Patchwork_;
  std::unique_ptr<Generators::TerrainPressJob> Pressing_;
  std::unique_ptr<Generators::Corridors::Job> CorridorJob_;
  std::unique_ptr<outshine::World::TransportNetworkBuildJob> NetworkJob_;
  std::vector<Yields> Corridors_;
  MeshBuild Meshing_;
  MeshBuild InitialMeshing_;
  std::chrono::steady_clock::time_point Began_ = std::chrono::steady_clock::now();
  size_t ProductPeakBytes_ = 0;
  double LongestPressingSliceMs_ = 0.0;
  double LongestCorridorSliceMs_ = 0.0;
  Core::GroundBuildSchedule Schedule_;
};

Surrounds::Surrounds() = default;

Surrounds::~Surrounds() = default;

std::vector<float> Engine::State::PaletteOver(const Ground::VegetationTemplates &wearing,
                                              const Medium &fallback) {
  const size_t rows = wearing.TemplateCount();
  constexpr size_t kSurfaceStride = 8u;
  const size_t surfacesAt = kPaletteStride * (rows + 2u) + rows + 1u;
  std::vector<float> palette(surfacesAt + kSurfaceStride * (rows + 1u), 0.0f);
  palette[0] = std::bit_cast<float>(static_cast<uint32_t>(rows));
  palette[1] = std::bit_cast<float>(static_cast<uint32_t>(wearing.RockTemplate()));
  palette[2] = wearing.Limit().SlopeBandDeg();
  const auto rowAt = [](size_t row) { return kPaletteStride * (row + 1u); };
  for (size_t row = 0; row < rows; ++row) {
    for (size_t channel = 0; channel < kPaletteStride; ++channel) {
      palette[rowAt(row) + channel] = wearing.Rows()[row].Ground[channel];
    }
    palette[kPaletteStride * (rows + 2u) + row] = wearing.Rows()[row].Edge[3];
    for (size_t channel = 0; channel < kPaletteStride; ++channel) {
      palette[surfacesAt + kSurfaceStride * row + channel] =
          wearing.Rows()[row].GroundSurf[channel];
    }
    palette[surfacesAt + kSurfaceStride * row + kPaletteStride] = wearing.Rows()[row].Mix[1];
  }
  for (size_t channel = 0; channel < 3; ++channel) {
    palette[rowAt(rows) + channel] = fallback.GroundAlbedo[channel];
  }
  palette[rowAt(rows) + 3u] = Material{}.Roughness;
  palette[kPaletteStride * (rows + 2u) + rows] = kVerticalSlopeDeg;
  return palette;
}

Engine::State::Classed Engine::State::Classify(std::span<const float> groundPositionsM,
                                               GroundWorldCandidate &candidate) {
  Classed out;
  const std::shared_ptr<const ClassStructure> classes = World.Stack.Classes().Read();
  const Ground::VegetationTemplates &wearing = World.Stack.Vegetation();
  const Medium fallback = kEarthAir;
  if (classes && wearing.Ready()) {
    out.Structure = classes;
    out.Palette = PaletteOver(wearing, fallback);
  }
  if (out.Structure && !out.Palette.empty()) {
    const size_t rows = std::bit_cast<uint32_t>(out.Palette[0]);
    Vec3 wornSum = {{0.0, 0.0, 0.0}};
    double worn = 0.0;
    for (size_t at = 0; at + 2 < groundPositionsM.size(); at += 3u * kBounceProbeStride) {
      const int which = out.Structure->Evaluate(static_cast<double>(groundPositionsM[at]),
                                                -static_cast<double>(groundPositionsM[at + 2]),
                                                nullptr,
                                                nullptr);
      const size_t row =
          which >= 0 && std::cmp_less(which, rows) ? static_cast<size_t>(which) : rows;
      for (int channel = 0; channel < 3; ++channel) {
        wornSum[channel] += static_cast<double>(
            out.Palette[kPaletteStride + row * kPaletteStride + static_cast<size_t>(channel)]);
      }
      worn += 1.0;
    }
    if (worn > 0.0) {
      const Vec3 wornMean = {{wornSum[0] / worn, wornSum[1] / worn, wornSum[2] / worn}};
      candidate.Grounding(wornMean);
      Published.Places(
          "lighting: the ground it bounces off, red", kPerMille * wornMean[0], "albedo/1000");
      Published.Places("lighting: green", kPerMille * wornMean[1], "albedo/1000");
      Published.Places("lighting: blue", kPerMille * wornMean[2], "albedo/1000");
    }
  }
  const Render::SubjectEnvironment &lighting = candidate.AmbientStanding();
  Published.Places("lighting: the sky's own radiance, red", lighting.RadianceLinear[0], "cd/m2");
  Published.Places("lighting: sky green", lighting.RadianceLinear[1], "cd/m2");
  Published.Places("lighting: sky blue", lighting.RadianceLinear[2], "cd/m2");
  Published.Places(
      "lighting: the ground's bounced radiance, red", lighting.GroundLinear[0], "cd/m2");
  Published.Places("lighting: bounce green", lighting.GroundLinear[1], "cd/m2");
  Published.Places("lighting: bounce blue", lighting.GroundLinear[2], "cd/m2");
  Published.Places(
      "class field: the vegetation table is ready", wearing.Ready() ? 1.0 : 0.0, "yes/no");
  Published.Places(
      "class field: rows the table carries", static_cast<double>(wearing.TemplateCount()), "rows");
  Published.Places("class field: features the fine tier holds",
                   static_cast<double>(World.Stack.Classes().FeaturesHeld()),
                   "features");
  Published.Places("class field: of those it has taken",
                   static_cast<double>(World.Stack.Classes().FeaturesTaken()),
                   "features");
  Published.Places("class field: it published a structure", classes ? 1.0 : 0.0, "yes/no");
  Published.Places("class field: the version the colours used",
                   classes ? static_cast<double>(classes->Version()) : -1.0,
                   "version");
  if (classes) {
    uint64_t digest = kDigestBasis;
    const size_t words = classes->Bytes() / sizeof(uint32_t);
    for (size_t at = 0; at < words; ++at) {
      digest = (digest ^ classes->Words()[at]) * kDigestPrime;
    }
    Published.Places("class field: the structure's digest, low half",
                     static_cast<double>(digest & kLowWord),
                     "digest");
    Published.Places("class field: the structure's digest, high half",
                     static_cast<double>(digest >> 32U),
                     "digest");
  }
  Published.Places("class field: it calls itself complete",
                   World.Stack.Classes().Complete() ? 1.0 : 0.0,
                   "yes/no");
  Published.Places("class field: tiles it waits for",
                   static_cast<double>(World.Stack.Classes().PendingTiles()),
                   "tiles");
  Published.Places("class field: the fraction it has no data for",
                   classes ? classes->NoDataFraction() : -1.0,
                   "fraction");
  Published.Places("class field: the materials are loaded", wearing.Ready() ? 1.0 : 0.0, "yes/no");
  return out;
}

void Engine::State::TellsWhatTheGroundHolds(const TangentFrame &standing) {
  constexpr double kGroundCellM = 25.0;
  const Ground::BuildingField &prints = World.Stack.Footprints();
  const Vec3 &anchor = prints.Anchor();
  double away = 0.0;
  for (int axis = 0; axis < 3; ++axis) {
    const double step = anchor[axis] - standing.OriginEcef()[axis];
    away += step * step;
  }
  Published.Places("buildings: their anchor lies from the frame's origin", std::sqrt(away), "m");
  {
    std::vector<double> fill = prints.SeatSpreadM();
    std::vector<double> across = prints.FootprintAcrossM();
    const auto publishQuantile =
        [this](const char *name, std::span<const double> sample, double share) {
          if (const auto value = QuantileOf(sample, share)) { Published.Places(name, *value, "m"); }
        };
    if (!fill.empty()) {
      std::ranges::sort(fill);
      size_t wouldStamp = 0;
      for (const double filled : fill) {
        if (filled > kStampWorthM) { ++wouldStamp; }
      }
      publishQuantile("buildings: a stamp would fill, p50", fill, kMiddleQuantile);
      publishQuantile("buildings: a stamp would fill, p95", fill, kBroadQuantile);
      Published.Places("buildings: a stamp would fill, worst", fill.back(), "m");
      Published.Places(
          "buildings: footprints worth a stamp", static_cast<double>(wouldStamp), "footprints");
    }
    if (!across.empty()) {
      std::ranges::sort(across);
      size_t underOneCell = 0;
      for (const double wide : across) {
        if (wide < kGroundCellM) { ++underOneCell; }
      }
      publishQuantile("buildings: footprint across, p50", across, kMiddleQuantile);
      publishQuantile("buildings: footprint across, p05", across, kNarrowQuantile);
      Published.Places("buildings: and the narrowest of them", across.front(), "m");
      Published.Places("buildings: footprints narrower than a ground cell",
                       static_cast<double>(underOneCell),
                       "footprints");
    }
  }
  Published.Places("buildings: footprints the field holds",
                   static_cast<double>(prints.Footprints().size()),
                   "footprints");
  if (World.Stack.Vectors() != nullptr) {
    Published.Places("buildings: vector tiles the field settled",
                     static_cast<double>(World.Stack.Vectors()->Tiles().size()),
                     "tiles");
    Published.Places("buildings: OSM features it holds",
                     static_cast<double>(World.Stack.Vectors()->Features().size()),
                     "features");
  }
}

bool Engine::State::Models(const TangentFrame &standing,
                           GroundBuildProducts &build,
                           Phasing &clocks) {
  Geometry &ground = build.Ground;
  TellsWhatTheGroundHolds(standing);
  Material walls;
  walls.BaseColour[0] = kWallRed;
  walls.BaseColour[1] = kWallGreen;
  walls.BaseColour[2] = kWallBlue;
  walls.Roughness = kWallRoughness;
  Material tiles;
  tiles.BaseColour[0] = kTileRed;
  tiles.BaseColour[1] = kTileGreen;
  tiles.BaseColour[2] = kTileBlue;
  tiles.Roughness = kTileRoughness;
  const auto wallSurface = ground.addSurface("walls", walls);
  const auto roofSurface = ground.addSurface("roofs", tiles);
  if (!wallSurface || !roofSurface) {
    Error = Says::MaterialCreationFailed;
    return false;
  }
  build.Surfaces = {.Walls = static_cast<uint32_t>(wallSurface->index()),
                    .Roofs = static_cast<uint32_t>(roofSurface->index())};
  Published.Places(
      "rebuild: the ground ring took",
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - clocks.PhaseAt)
          .count(),
      "ms");
  clocks.PhaseAt = std::chrono::steady_clock::now();
  clocks.CensusAt = clocks.PhaseAt;
  Published.Places(
      "buildings: the wall surface", static_cast<double>(wallSurface->index()), "index");
  Published.Places(
      "buildings: the roof surface", static_cast<double>(roofSurface->index()), "index");
  Published.Places("buildings: tiles handed to the arena as pieces",
                   static_cast<double>(World.Pieces.Handed()),
                   "tiles");
  Published.Places(
      "buildings: pieces the arena refused", static_cast<double>(World.Pieces.Refused()), "pieces");
  Published.Places("buildings: triangles the tiles handed over",
                   static_cast<double>(World.Stack.Footprints().TrianglesHanded()),
                   "triangles");
  if (Picture.Standing) {
    Published.Places("buildings: pieces standing in the arena",
                     static_cast<double>(Picture.Device.PiecesStanding()),
                     "pieces");
    Published.Places("buildings: triangles the arena holds",
                     static_cast<double>(Picture.Device.PieceTriangles()),
                     "triangles");
    Published.Places("buildings: bytes the arena holds on the device",
                     static_cast<double>(Picture.Device.PieceBytesHeld()),
                     "bytes");
  }
  return true;
}

Engine::State::Laid Engine::State::Focuses(GroundRequest &request,
                                           LongitudeLatitude at,
                                           bool alsoWhenTilesLanded,
                                           GroundQuality quality) {
  const Around &over = request.Coverage;
  const GroundRevision previous = World.GroundPublished.Current().value_or(GroundRevision{});
  const double atLat = at.LatitudeDeg;
  const double atLon = at.LongitudeDeg;
  const Ground::TileFrac here = Ground::ToTileFracClamped(
      Ground::Geo{.LongitudeDeg = atLon, .LatitudeDeg = atLat}, over.Zoom);
  const uint64_t from = (static_cast<uint64_t>(static_cast<int64_t>(std::floor(here.X))) << 32U) ^
                        static_cast<uint64_t>(static_cast<int64_t>(std::floor(here.Y))) ^
                        (static_cast<uint64_t>(over.Levels) << 56u);
  World.Stack.Pool().Focus({.LongitudeDeg = atLon, .LatitudeDeg = atLat});
  ++World.Asked;
  Around asking = over;
  asking.Asking = true;
  asking.PlayableOnly = quality == GroundQuality::Playable;
  auto sees = World.Shipping.Covering().Lay(World.Stack.Pool(), asking);
  if (!sees) {
    Error = sees.error();
    return Laid::Refused;
  }
  World.AskedPending = sees->Pending;
  World.AskedWanted = sees->Tiles;
  if (asking.PlayableOnly) { World.AskedPlayablePending = sees->Pending; }
  const size_t resident = sees->Tiles > sees->Pending ? sees->Tiles - sees->Pending : 0;
  const std::shared_ptr<const ClassStructure> naming = World.Stack.Classes().Read();
  const uint64_t classes = naming ? naming->Version() : 0;
  const bool elsewhere = from != previous.Region;
  const bool renamed = classes != previous.Classes;
  const uint64_t footprints = World.Stack.Footprints().Revision();
  const Render::Viewpoint &view = Picture.Standing->Watching();
  const std::array<double, 3> projection{
      {static_cast<double>(view.Kind), view.YfovRad, view.YMagM}};
  const double visualRadiusM = Session.Declared.Ground.SightM > 0.0 ? Session.Declared.Ground.SightM
                                                                    : Scenario::kSightUnsaidM;
  request.Revision = {.Region = from,
                      .ResidentTiles = resident,
                      .Classes = classes,
                      .Footprints = footprints,
                      .VectorGeneration = World.Stack.Vectors() != nullptr
                                              ? World.Stack.Vectors()->Generation()
                                              : 0,
                      .StreetTiles = World.Stack.Ways().IngestedTiles(),
                      .WaterTiles = World.Stack.WaterBodies().IngestedTiles(),
                      .Projection = projection,
                      .Coverage = {.ContactRadiusM = 0.5 * World.Stack.Footprints().TileSpanM(),
                                   .VisualRadiusM = visualRadiusM,
                                   .MinimumSourceZoom = std::max(over.Zoom - 1, 0),
                                   .TargetSourceZoom = over.Zoom},
                      .Quality = quality};
  if (quality == GroundQuality::Refined) { World.RequestedRefinedGround = request.Revision; }
  Published.Places("building triangles the world meshed",
                   static_cast<double>(World.Stack.Footprints().TrianglesHanded()),
                   "triangles");
  Published.Places(
      "world: the bytes its fields hold", static_cast<double>(World.Stack.HeapBytes()), "bytes");
  Published.Places("world: of that, the land classes",
                   static_cast<double>(World.Stack.Classes().HeapBytes()),
                   "bytes");
  Published.Places(
      "world: the buildings", static_cast<double>(World.Stack.Footprints().HeapBytes()), "bytes");
  Published.Places("world: of those, the footprints it keeps",
                   static_cast<double>(World.Stack.Footprints().PrintBytes()),
                   "bytes");
  Published.Places("world: tiles baking on the workers right now",
                   static_cast<double>(World.StructureBuilds.Queued()),
                   "tiles");
  Published.Places("world: the building pieces the device holds",
                   Picture.Standing ? static_cast<double>(Picture.Device.PieceBytesHeld()) : 0.0,
                   "bytes");
  Published.Places(
      "world: the water", static_cast<double>(World.Stack.WaterBodies().HeapBytes()), "bytes");
  Published.Places(
      "world: the streets", static_cast<double>(World.Stack.Ways().HeapBytes()), "bytes");
  Published.Places("world: the ceiling its fields stand under",
                   static_cast<double>(Ground::GroundStack::kHoldsBytes),
                   "bytes");
  Published.Places("world: times a round stopped at that ceiling",
                   static_cast<double>(World.Stack.OverCeiling()),
                   "rounds");
  Published.Places("world: and the OSM features",
                   World.Stack.Vectors() != nullptr
                       ? static_cast<double>(World.Stack.Vectors()->HeapBytes())
                       : 0.0,
                   "bytes");
  Published.Places("tiles laid bare on the ellipsoid",
                   static_cast<double>(sees->Pending + sees->Absent + sees->Refused),
                   "tiles");
  if (!World.GroundPublished.NeedsRebuild(
          request.Revision, alsoWhenTilesLanded, World.RimsMissing > 0)) {
    return Laid::Unchanged;
  }

  Published.Places(
      "rebuilds since the world stood", static_cast<double>(World.Relaid + 1u), "rebuilds");
  Published.Places("rebuild: the eye walked into another tile", elsewhere ? 1.0 : 0.0, "yes/no");
  Published.Places("rebuild: tiles resident when it did", static_cast<double>(resident), "tiles");
  Published.Places("rebuild: and resident the time before",
                   static_cast<double>(previous.ResidentTiles),
                   "tiles");
  Published.Places("rebuild: the land classes were named anew", renamed ? 1.0 : 0.0, "yes/no");
  return Laid::Wanted;
}

void Engine::State::TellsTheRelief(Relieved over) {
  Published.Places("relief: the ring's tallest vertex ABOVE THE ELLIPSOID", over.Tallest, "m");
  Published.Places("relief: and how far out it lies", over.TallestOutM, "m");
  Published.Places("relief: the ring's lowest vertex above the ellipsoid", over.Lowest, "m");
  Published.Places(
      "relief: so the true relief, with the sphere taken out", over.Tallest - over.Lowest, "m");
}

std::expected<Engine::State::GroundRequest, Engine::State::Laid>
Engine::State::RingWanted(bool alsoWhenTilesLanded, GroundQuality quality) {
  const Scenario::Document &declared = Session.Declared;
  const double anchorLat = declared.Ground.Origin.LatitudeDeg;
  const double anchorLon = declared.Ground.Origin.LongitudeDeg;
  const LongitudeLatitude eyeStands = WhereTheEyeStands();
  const double atLat = eyeStands.LatitudeDeg;
  const double atLon = eyeStands.LongitudeDeg;
  Published.Places("the ring centres this far from the world's anchor",
                   std::hypot((atLat - anchorLat) * kMPerDegLat,
                              (atLon - anchorLon) * kMPerDegLon * std::cos(anchorLat * kDeg2Rad)),
                   "m");

  Around over;
  over.LatitudeDeg = atLat;
  over.LongitudeDeg = atLon;
  over.Zoom = World.Stack.FinestZoomOf(Data::DataKind::Elevation);
  {
    const double tileSpanM = 40075017.0 * std::cos(atLat * kDeg2Rad) / std::ldexp(1.0, over.Zoom);
    const double nearest = 4.0 * tileSpanM;
    const double wanted = declared.Ground.SightM > 0.0 ? declared.Ground.SightM : 240000.0;
    const double doublings = wanted > nearest ? std::log2(wanted / nearest) : 0.0;
    over.Levels = 1 + static_cast<int>(std::ceil(doublings));
    Published.Places("the sight a scenario declares", wanted, "m");
    Published.Places("and what one tile spans at the finest zoom", tileSpanM, "m");
    Published.Places("the elevation's own posting",
                     World.Stack.Ground().PostM(declared.Ground.Origin.LatitudeDeg),
                     "m");
    Published.Places("and the drawn mesh's vertex spacing",
                     over.Grid > 1 ? tileSpanM / static_cast<double>(over.Grid - 1) : 0.0,
                     "m");
  }
  if (Session.Views) {
    const Scenario::View &camera = Session.Views->Active();
    if (camera.Placement == Scenario::CameraPlacement::Geodetic &&
        camera.Geographic.SamplesHeight) {
      const auto position = ResolveGeodeticCamera(
          camera, {.LongitudeDeg = anchorLon, .LatitudeDeg = anchorLat}, &World.Stack.Ground());
      if (!position) {
        Error = position.error();
        return std::unexpected(Laid::Refused);
      }
      if (!*position) { return std::unexpected(Laid::Pending); }
    }
  }
  if (!Watches()) { return std::unexpected(Laid::Refused); }
  GroundRequest request{.Coverage = over, .Revision = {}};
  switch (Focuses(
      request, {.LongitudeDeg = atLon, .LatitudeDeg = atLat}, alsoWhenTilesLanded, quality)) {
    case Laid::Refused: return std::unexpected(Laid::Refused);
    case Laid::Pending: return std::unexpected(Laid::Pending);
    case Laid::Unchanged: return std::unexpected(Laid::Unchanged);
    case Laid::Wanted: break;
  }
  return request;
}

bool Engine::State::RefineGroundSheets(const TangentFrame &standing,
                                       Patchwork &patchwork,
                                       GroundBuildProducts &build) {
  {
    build.Sheets.Framed(standing);
    const Render::Viewpoint &eye = Picture.Standing->Watching();
    Generators::TerrainRefinementDetail detail{.EyeM = eye.EyeM};
    if (eye.Kind == Render::CameraKind::Orthographic) {
      detail.OrthographicPxPerM = static_cast<double>(Picture.Frame.HeightPx) / (2.0 * eye.YMagM);
    } else {
      detail.FocalPx =
          static_cast<double>(Picture.Frame.HeightPx) / (2.0 * std::tan(eye.YfovRad * 0.5));
    }
    if (!build.Sheets.RefineByError(patchwork,
                                    {.Side = Render::GroundLattice::kSide, .Halo = 1},
                                    detail,
                                    Render::GroundLattice::kPages,
                                    Error)) {
      return false;
    }
    Published.Places("ground: virtual tiles the lattice refines to",
                     static_cast<double>(std::ranges::count_if(
                         patchwork.Sheets, [](const Sheet &sheet) { return sheet.Virtual; })),
                     "tiles");
  }
  return true;
}

namespace {
void AppendBuildingStamps(const Ground::BuildingField &pads,
                          std::span<const double> points,
                          const TangentFrame &standing,
                          std::vector<Yields> &yielding) {
  for (const Ground::BuildingField::Footprint &one : pads.Footprints()) {
    if (one.PointCount < 3) { continue; }
    Yields made;
    made.RingEastNorthM.reserve(static_cast<size_t>(one.PointCount) * 2u);
    made.LowE = kBeyondAnyCoordinate;
    made.HighE = -kBeyondAnyCoordinate;
    made.LowN = kBeyondAnyCoordinate;
    made.HighN = -kBeyondAnyCoordinate;
    bool whole = true;
    for (uint32_t step = 0; step < one.PointCount && whole; ++step) {
      const size_t at = (static_cast<size_t>(one.FirstPoint) + step) * 2u;
      if (at + 1 >= points.size()) {
        whole = false;
        break;
      }
      const EastNorthUp seated = standing.Place({.LongitudeDeg = points[at + 1],
                                                 .LatitudeDeg = points[at],
                                                 .HeightM = static_cast<double>(one.SeatM)});
      const double eastM = seated.EastM;
      const double northM = seated.NorthM;
      made.RingEastNorthM.push_back(eastM);
      made.RingEastNorthM.push_back(northM);
      made.LowE = std::min(made.LowE, eastM);
      made.HighE = std::max(made.HighE, eastM);
      made.LowN = std::min(made.LowN, northM);
      made.HighN = std::max(made.HighN, northM);
    }
    if (!whole) { continue; }
    {
      const size_t first = static_cast<size_t>(one.FirstPoint) * 2u;
      const EastNorthUp placed = standing.Place({.LongitudeDeg = points[first + 1],
                                                 .LatitudeDeg = points[first],
                                                 .HeightM = static_cast<double>(one.SeatM)});
      made.PlateauM = placed.UpM;
    }
    made.ApronM = kPadApronM;
    made.YieldM = std::fabs(static_cast<double>(one.SeatM) - static_cast<double>(one.BaseM));
    made.SeamEastNorthM = made.RingEastNorthM;
    yielding.push_back(std::move(made));
  }
}
}

namespace {
void AppendLakeStamps(std::span<const Ground::WaterField::Surface> lakes,
                      std::span<const double> points,
                      const TangentFrame &standing,
                      std::vector<Yields> &yielding) {
  for (const Ground::WaterField::Surface &lake : lakes) {
    if (lake.PointCount < 3) { continue; }
    const size_t last = (static_cast<size_t>(lake.FirstPoint) + lake.PointCount) * 2u;
    if (last > points.size()) { continue; }
    Yields made;
    made.RingEastNorthM.reserve(static_cast<size_t>(lake.PointCount) * 2u);
    made.LowE = kBeyondAnyCoordinate;
    made.HighE = -kBeyondAnyCoordinate;
    made.LowN = kBeyondAnyCoordinate;
    made.HighN = -kBeyondAnyCoordinate;
    std::vector<double> bedM;
    bedM.reserve(lake.PointCount);
    for (uint32_t step = 0; step < lake.PointCount; ++step) {
      const size_t at = (static_cast<size_t>(lake.FirstPoint) + step) * 2u;
      const EastNorthUp shore =
          standing.Place({.LongitudeDeg = points[at + 1],
                          .LatitudeDeg = points[at],
                          .HeightM = static_cast<double>(lake.LevelM) - kWaterBedM});
      made.RingEastNorthM.push_back(shore.EastM);
      made.RingEastNorthM.push_back(shore.NorthM);
      made.LowE = std::min(made.LowE, shore.EastM);
      made.HighE = std::max(made.HighE, shore.EastM);
      made.LowN = std::min(made.LowN, shore.NorthM);
      made.HighN = std::max(made.HighN, shore.NorthM);
      bedM.push_back(shore.UpM);
    }
    made.AtE = 0.5 * (made.LowE + made.HighE);
    made.AtN = 0.5 * (made.LowN + made.HighN);
    made.SagInv = 1.0 / kWgs84A;
    double plateau = 0.0;
    for (size_t corner = 0; corner < bedM.size(); ++corner) {
      const double dE = made.RingEastNorthM[corner * 2u] - made.AtE;
      const double dN = made.RingEastNorthM[corner * 2u + 1u] - made.AtN;
      plateau += bedM[corner] + 0.5 * (dE * dE + dN * dN) * made.SagInv;
    }
    made.PlateauM = plateau / static_cast<double>(bedM.size());
    made.ApronM = kWaterBankM;
    made.YieldM = kWaterBedM;
    made.Kind = Stamp::Basin;
    made.SeamEastNorthM = made.RingEastNorthM;
    yielding.push_back(std::move(made));
  }
}
}

bool Engine::State::PressGroundEarthworks(const TangentFrame &standing,
                                          Patchwork &patchwork,
                                          GroundBuildState &state) {
  if (state.Pressing() == nullptr) {
    const Ground::BuildingField &pads = state.Candidate().Products().Footprints;
    std::vector<Yields> corridor = state.TakesCorridors();
    const Ground::OsmField *const shapes = World.Stack.Vectors();
    if (shapes != nullptr) {
      uint64_t tileOrder = kDigestBasis;
      for (const Ground::OsmField::Tile &tile : shapes->Tiles()) {
        tileOrder = (tileOrder ^ static_cast<uint32_t>(tile.X)) * kDigestPrime;
        tileOrder = (tileOrder ^ static_cast<uint32_t>(tile.Y)) * kDigestPrime;
      }
      Published.Places("ground candidate: OSM tile order, low half",
                       static_cast<double>(tileOrder & kLowWord),
                       "digest");
      Published.Places("ground candidate: OSM tile order, high half",
                       static_cast<double>(tileOrder >> 32U),
                       "digest");
    }
    std::vector<Yields> yielding;
    if (shapes != nullptr) { AppendBuildingStamps(pads, shapes->Points(), standing, yielding); }
    const size_t builtPads = yielding.size();
    if (shapes != nullptr) {
      AppendLakeStamps(World.Stack.WaterBodies().Surfaces(), shapes->Points(), standing, yielding);
    }
    const size_t builtLakes = yielding.size() - builtPads;
    Published.Places("ground: lakes that press it", static_cast<double>(builtLakes), "lakes");
    yielding.insert(yielding.end(),
                    std::make_move_iterator(corridor.begin()),
                    std::make_move_iterator(corridor.end()));
    Published.Places("ground: pads that press it", static_cast<double>(builtPads), "pads");
    Published.Places("ground: corridor pieces that press it",
                     static_cast<double>(yielding.size() - builtPads - builtLakes),
                     "pieces");
    state.BeginsPressing(std::make_unique<Generators::TerrainPressJob>(
        std::move(yielding),
        patchwork,
        standing,
        Generators::TerrainPageLayout{.Side = Render::GroundLattice::kSide, .Halo = 1},
        kMostEarthworkM));
    state.SamplesProductPeak();
    return true;
  }
  const auto sliceAt = std::chrono::steady_clock::now();
  const bool completed =
      state.Pressing()->Advance(kEarthworkSheetsPerFrame, kEarthworkPointsPerFrame);
  state.SamplesPressingSlice(
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - sliceAt)
          .count());
  state.SamplesProductPeak();
  if (!completed) { return true; }
  const Generators::PressedTerrain pressed_ = state.Pressing()->Take();
  Published.Places("ground: pressing gather", pressed_.GatherMs, "ms");
  Published.Places("ground: pressing decide", pressed_.DecideMs, "ms");
  Published.Places("ground: pressing buckets", pressed_.BucketMs, "ms");
  Published.Places("ground: pressing reject", pressed_.RejectMs, "ms");
  Published.Places("ground: pressing apply", pressed_.ApplyMs, "ms");
  Published.Places("ground: pressing write", pressed_.WriteMs, "ms");
  Published.Places("ground: pressing floors", pressed_.FloorsMs, "ms");
  Published.Places("ground: longest gather slice", pressed_.LongestGatherMs, "ms");
  Published.Places("ground: longest decide slice", pressed_.LongestDecideMs, "ms");
  Published.Places("ground: longest reject slice", pressed_.LongestRejectMs, "ms");
  Published.Places("ground: longest initialize slice", pressed_.LongestInitializeMs, "ms");
  Published.Places("ground: longest apply slice", pressed_.LongestApplyMs, "ms");
  Published.Places("ground: longest write slice", pressed_.LongestWriteMs, "ms");
  Published.Places("ground: longest reproject slice", pressed_.LongestReprojectMs, "ms");
  Published.Places("ground: longest floors slice", pressed_.LongestFloorsMs, "ms");
  Published.Places(
      "ground: lattice nodes the stamps pressed", static_cast<double>(pressed_.Nodes), "nodes");
  Published.Places("ground: stamps refused as STRUCTURES, past the earthwork bound",
                   static_cast<double>(pressed_.Structures),
                   "yields");
  Published.Places("ground: nodes held where a stamp still asked past the bound",
                   static_cast<double>(pressed_.Held),
                   "nodes");
  Published.Places("ground: and the deepest it cut", pressed_.DeepestM, "m");
  Published.Places("ground: and the highest it filled", pressed_.RaisedM, "m");
  for (const auto &[what, floors] :
       {std::pair{"pads", &pressed_.Pads}, std::pair{"corridor pieces", &pressed_.Corridors}}) {
    Published.Places(std::format("ground: {} with a lattice node inside", what),
                     static_cast<double>(floors->Stamps),
                     "stamps");
    Published.Places(std::format("ground: {} no lattice node reaches", what),
                     static_cast<double>(floors->Unreached),
                     "stamps");
    Published.Places(std::format("ground: nodes inside those {}", what),
                     static_cast<double>(floors->Nodes),
                     "nodes");
    Published.Places(std::format("ground: of those {} nodes, another stamp decided", what),
                     static_cast<double>(floors->Contested),
                     "nodes");
    Published.Places(
        std::format("ground: nodes inside {} above their plane after the press, worst", what),
        floors->AboveM,
        "m");
    Published.Places(
        std::format("ground: nodes inside {} that fill, below it after the press, worst", what),
        floors->BelowM,
        "m");
    Published.Places(std::format("ground: nodes inside {} that do not fill, below it, worst", what),
                     floors->UnfilledM,
                     "m");
    Published.Places(std::format("ground: those {} nodes above it before the press, worst", what),
                     floors->WasAboveM,
                     "m");
    Published.Places(
        std::format("ground: those filling {} nodes below it before the press, worst", what),
        floors->WasBelowM,
        "m");
  }
  const double pressingMs =
      pressed_.GatherMs + pressed_.DecideMs + pressed_.WriteMs + pressed_.FloorsMs;
  Published.Places("ground: of that, pressing", pressingMs, "ms");
  Published.Places("ground candidate: earthworks", pressingMs, "ms");
  Published.Places(
      "ground candidate: longest earthwork slice", state.LongestPressingSliceMs(), "ms");
  state.CompletesStage();
  return true;
}

bool Engine::State::BuildWaterSurfaces(const TangentFrame &standing,
                                       Geometry &ground,
                                       MaterialInstance ringSurface) {

  const auto waterAt = std::chrono::steady_clock::now();
  const Ground::WaterField &wet = World.Stack.WaterBodies();
  const Ground::OsmField *const vectors = World.Stack.Vectors();
  std::vector<float> places;
  std::vector<float> facing;
  std::vector<float> lidUv;
  std::vector<uint32_t> order;
  size_t lidsLaid = 0;
  size_t lidsRefused = 0;
  {
    const auto points = vectors != nullptr ? vectors->Points() : std::span<const double>{};
    const auto surfaces = vectors != nullptr
                              ? std::span<const Ground::WaterField::Surface>(wet.Surfaces())
                              : std::span<const Ground::WaterField::Surface>{};
    for (const Ground::WaterField::Surface &lake : surfaces) {
      if (lake.PointCount < 3) {
        ++lidsRefused;
        continue;
      }
      const size_t last = (static_cast<size_t>(lake.FirstPoint) + lake.PointCount) * 2;
      if (last > points.size()) {
        ++lidsRefused;
        continue;
      }
      const size_t began = places.size();
      for (uint32_t step = 1; step + 1 < lake.PointCount; ++step) {
        const std::array<uint32_t, 3> corners = {{0u, step, step + 1u}};
        for (const uint32_t corner : corners) {
          const size_t at = (static_cast<size_t>(lake.FirstPoint) + corner) * 2;
          double eastM = 0.0;
          double upM = 0.0;
          double northM = 0.0;
          const EastNorthUp placed = standing.Place({.LongitudeDeg = points[at + 1],
                                                     .LatitudeDeg = points[at],
                                                     .HeightM = static_cast<double>(lake.LevelM)});
          eastM = placed.EastM;
          upM = placed.UpM;
          northM = placed.NorthM;
          places.push_back(static_cast<float>(eastM));
          places.push_back(static_cast<float>(upM));
          places.push_back(static_cast<float>(RenderFrame::ZOfNorth(northM)));
          facing.push_back(0.0f);
          facing.push_back(1.0f);
          facing.push_back(0.0f);
          lidUv.push_back(static_cast<float>(eastM));
          lidUv.push_back(static_cast<float>(northM));
          order.push_back(static_cast<uint32_t>(order.size()));
        }
      }
      if (places.size() > began) {
        ++lidsLaid;
      } else {
        ++lidsRefused;
      }
    }
  }
  Published.Places(
      "water: of that, laying the surfaces",
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - waterAt).count(),
      "ms");
  Published.Places("water: surfaces laid", static_cast<double>(lidsLaid), "surfaces");
  Published.Places("water: surfaces refused", static_cast<double>(lidsRefused), "surfaces");
  const size_t waterTriangles = order.size() / 3;
  Published.Places("water: triangles", static_cast<double>(waterTriangles), "triangles");
  if (order.size() >= 3) {
    const auto createdPart = ground.addPart("water", ringSurface);
    if (!createdPart) {
      Error = Says::WaterCreationFailed;
      return false;
    }
    const int wetPart = *createdPart;
    const bool tookWater =
        wetPart >= 0 &&
        ground.setPositions(wetPart, std::span<const float>(places.data(), places.size())) &&
        ground.setNormals(wetPart, std::span<const float>(facing.data(), facing.size())) &&
        ground.setTriangles(wetPart, std::span<const uint32_t>(order.data(), order.size())) &&
        ground.setTexture(wetPart, std::span<const float>(lidUv.data(), lidUv.size()), 0);
    Published.Places("water: the geometry took it", tookWater ? 1.0 : 0.0, "yes/no");
    if (!tookWater) {
      Error = Says::WaterCreationFailed;
      return false;
    }
  }
  return true;
}

void Engine::State::ReportGroundPlacements() {
  for (size_t part = 0; part < Picture.Standing->Shown().Parts.size(); ++part) {
    const Render::ShapePart &one = Picture.Standing->Shown().Parts[part];
    Published.Places("restand: subject part " + std::to_string(part) + " first vertex",
                     static_cast<double>(one.FirstVertex),
                     "");
    Published.Places("restand: subject part " + std::to_string(part) + " vertex count",
                     static_cast<double>(one.VertexCount),
                     "");
    Published.Places("restand: subject part " + std::to_string(part) + " first index",
                     static_cast<double>(one.FirstIndex),
                     "");
    Published.Places("restand: subject part " + std::to_string(part) + " index count",
                     static_cast<double>(one.IndexCount),
                     "");
  }
  for (size_t part = 0; part < Picture.Standing->PartsStanding(); ++part) {
    const double *const m = Picture.Standing->PlacementStanding(part);
    if (m == nullptr) { continue; }
    double most = 0.0;
    for (int at = 0; at < 16; ++at) { most += std::fabs(m[at]); }
    Published.Places("restand: part " + std::to_string(part) +
                         " placement, sum of the absolute terms",
                     most,
                     "");
    Published.Places(
        "restand: part " + std::to_string(part) + " diagonal", m[0] + m[5] + m[10] + m[15], "");
  }
}

Engine::State::GroundBuildProgress Engine::State::BeginsGroundBuild(const GroundRequest &request) {
  if (!World.GroundBuild || !World.GroundBuild->Matches(request.Revision)) {
    if (World.GroundBuild) {
      Published.Places("ground candidate: revision mismatch mask",
                       static_cast<double>(World.GroundBuild->RevisionDifference(request.Revision)),
                       "bits");
    }
    World.GroundBuild = std::make_unique<GroundBuildState>(
        Picture.Device, World, World.Stack.Footprints(), request.Coverage, request.Revision);
    ++World.GroundCandidates;
    Published.Places(
        "ground candidate: starts", static_cast<double>(World.GroundCandidates), "candidates");
    return GroundBuildProgress::Pending;
  }
  GroundBuildState &state = *World.GroundBuild;
  Published.Places(
      "ground candidate: progress", static_cast<double>(state.Progress()), "stage index");
  if (state.Prepared()) { return GroundBuildProgress::Ready; }
  if (auto prepared = state.Candidate().Prepare(*Picture.Standing, &Picture.Face); !prepared) {
    Error = std::move(prepared.error());
    World.GroundBuild.reset();
    return GroundBuildProgress::Failed;
  }
  state.MarksPrepared();
  return GroundBuildProgress::Pending;
}

Engine::State::GroundBuildProgress Engine::State::BeginsGroundSheets(const TangentFrame &standing,
                                                                     Patchwork &patchwork,
                                                                     const Around &coverage) {
  GroundBuildState &state = *World.GroundBuild;
  GroundBuildProducts &build = state.Candidate().Products();
  switch (state.SheetBuilding()) {
    case Core::GroundBuildSchedule::SheetPhase::NeedsFields: {
      const auto prepared = build.Sheets.PrepareFields(
          patchwork,
          World.Stack.Ground(),
          {.FinestZoom = coverage.Zoom, .RequestsMost = kTerrainSheetsPerFrame});
      if (!prepared) {
        Error = prepared.error();
        World.GroundBuild.reset();
        return GroundBuildProgress::Failed;
      }
      if (*prepared) { state.CompletesSheetPhase(); }
      return GroundBuildProgress::Pending;
    }
    case Core::GroundBuildSchedule::SheetPhase::NeedsRefinement:
      if (!RefineGroundSheets(standing, patchwork, build)) {
        World.GroundBuild.reset();
        return GroundBuildProgress::Failed;
      }
      state.CompletesSheetPhase();
      return GroundBuildProgress::Pending;
    case Core::GroundBuildSchedule::SheetPhase::NeedsHalos: {
      const auto haloAt = std::chrono::steady_clock::now();
      Published.Places("ground: sheets the lattice haloed",
                       static_cast<double>(build.Sheets.Halos(patchwork, coverage.Zoom)),
                       "sheets");
      build.RimsMissing = build.Sheets.RimsMissing();
      Published.Places("ground: rims copied for want of a neighbour",
                       static_cast<double>(build.RimsMissing),
                       "sheets");
      Published.Places(
          "ground: of that, haloing",
          std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - haloAt)
              .count(),
          "ms");
      state.CompletesSheetPhase();
      return GroundBuildProgress::Pending;
    }
    case Core::GroundBuildSchedule::SheetPhase::NeedsMesh: {
      GroundBuildState::MeshBuild &meshing = state.InitialMeshing();
      const auto began = std::chrono::steady_clock::now();
      const size_t end =
          std::min(meshing.NextSheet + kTerrainSheetsPerFrame, patchwork.Sheets.size());
      for (; meshing.NextSheet < end; ++meshing.NextSheet) {
        Generators::AppendTerrainMeshSheet(meshing.Mesh,
                                           patchwork.Sheets[meshing.NextSheet],
                                           standing,
                                           {.Side = Render::GroundLattice::kSide, .Halo = 1});
      }
      const double sliceMs =
          std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began)
              .count();
      meshing.LongestSliceMs = std::max(meshing.LongestSliceMs, sliceMs);
      state.SamplesProductPeak();
      if (meshing.NextSheet < patchwork.Sheets.size()) { return GroundBuildProgress::Pending; }
      Published.Places(
          "ground candidate: longest initial mesh slice", meshing.LongestSliceMs, "ms");
      build.PositionsM = std::move(meshing.Mesh.PositionsM);
      build.Indices = std::move(meshing.Mesh.Indices);
      TellsTheRelief({.Tallest = meshing.Mesh.TallestM,
                      .Lowest = meshing.Mesh.LowestM,
                      .TallestOutM = meshing.Mesh.TallestDistanceM});
      state.CompletesSheetPhase();
      return GroundBuildProgress::Ready;
    }
    case Core::GroundBuildSchedule::SheetPhase::Ready: return GroundBuildProgress::Ready;
  }
  return GroundBuildProgress::Failed;
}

Engine::State::GroundBuildProgress Engine::State::BeginsGroundClasses() {
  GroundBuildState &state = *World.GroundBuild;
  if (state.NextStage() != Core::GroundBuildSchedule::Stage::NeedsClasses) {
    return GroundBuildProgress::Ready;
  }
  GroundBuildProducts &build = state.Candidate().Products();
  static const Heap::Tag kClassingTag("ground-classify");
  const Heap::Tagged classing(kClassingTag);
  Classed classed = Classify(build.PositionsM, state.Candidate());
  build.ClassPalette = std::move(classed.Palette);
  build.ClassStructure = std::move(classed.Structure);
  state.CompletesStage();
  return GroundBuildProgress::Pending;
}

Engine::State::GroundBuildProgress Engine::State::BeginsGroundSurface() {
  GroundBuildState &state = *World.GroundBuild;
  if (state.NextStage() != Core::GroundBuildSchedule::Stage::NeedsGroundSurface) {
    return GroundBuildProgress::Ready;
  }
  GroundBuildProducts &build = state.Candidate().Products();
  Material bare;
  const Medium held = kEarthAir;
  for (int channel = 0; channel < 3; ++channel) {
    bare.BaseColour[channel] = held.GroundAlbedo[channel];
  }
  const auto ringSurface = build.Ground.addSurface("ground", bare);
  if (!ringSurface) {
    Error = Says::MaterialCreationFailed;
    World.GroundBuild.reset();
    return GroundBuildProgress::Failed;
  }
  build.GroundMaterial = bare;
  build.GroundSurface = *ringSurface;
  state.CompletesStage();
  return GroundBuildProgress::Pending;
}

Engine::State::GroundBuildProgress Engine::State::BeginsGroundModels(const TangentFrame &standing) {
  GroundBuildState &state = *World.GroundBuild;
  if (state.NextStage() != Core::GroundBuildSchedule::Stage::NeedsModels) {
    return GroundBuildProgress::Ready;
  }
  const auto began = std::chrono::steady_clock::now();
  Phasing clocks{.PhaseAt = began, .CensusAt = began, .WiresAt = began};
  static const Heap::Tag kModellingTag("ground-model");
  const Heap::Tagged modelling(kModellingTag);
  GroundWorldCandidate &candidate = state.Candidate();
  GroundBuildProducts &build = candidate.Products();
  if (!Models(standing, build, clocks) ||
      !candidate.SetGroundGeometry(build.Ground.clone(), 0, build.GroundMaterial, Error)) {
    World.GroundBuild.reset();
    return GroundBuildProgress::Failed;
  }
  state.CompletesStage();
  return GroundBuildProgress::Pending;
}

Engine::State::GroundBuildProgress Engine::State::BeginsGroundNetwork() {
  GroundBuildState &state = *World.GroundBuild;
  if (state.NextStage() != Core::GroundBuildSchedule::Stage::NeedsNetwork) {
    return GroundBuildProgress::Ready;
  }
  GroundBuildProducts &build = state.Candidate().Products();
  const std::optional<GroundRevision> &publishedRevision = World.GroundPublished.Current();
  const GroundRevision &requestedRevision = state.Revision();
  const bool sourcesChanged =
      !publishedRevision || publishedRevision->Region != requestedRevision.Region ||
      publishedRevision->ResidentTiles != requestedRevision.ResidentTiles ||
      publishedRevision->VectorGeneration != requestedRevision.VectorGeneration ||
      publishedRevision->StreetTiles != requestedRevision.StreetTiles;
  if (build.Network != nullptr && World.Stack.Ways().Ways().size() == build.NetworkOfWays &&
      !sourcesChanged) {
    state.CompletesStage();
    return GroundBuildProgress::Pending;
  }
  if (World.Stack.Vectors() == nullptr) {
    build.Network.reset();
    build.NetworkOfWays = 0;
    state.CompletesStage();
    return GroundBuildProgress::Pending;
  }
  if (state.NetworkJob() == nullptr) {
    const int sourceZoom = state.Coverage().Zoom;
    auto started = outshine::World::TransportNetworkBuildJob::Begin(
        World.Stack, [&sheets = build.Sheets, sourceZoom](LongitudeLatitude at) {
          return sheets.AslMAt(sourceZoom, at);
        });
    if (!started) {
      Error = std::move(started.error());
      World.GroundBuild.reset();
      return GroundBuildProgress::Failed;
    }
    state.BeginsNetwork(
        std::make_unique<outshine::World::TransportNetworkBuildJob>(std::move(*started)));
    return GroundBuildProgress::Pending;
  }
  auto advanced = state.NetworkJob()->Advance(kNetworkItemsPerFrame);
  if (!advanced) {
    Error = std::move(advanced.error());
    World.GroundBuild.reset();
    return GroundBuildProgress::Failed;
  }
  if (!*advanced) { return GroundBuildProgress::Pending; }
  const double longestSliceMs = state.NetworkJob()->LongestSliceMs();
  auto completed = std::move(*state.NetworkJob()).Take();
  if (!completed) {
    Error = completed.error();
    World.GroundBuild.reset();
    return GroundBuildProgress::Failed;
  }
  const outshine::World::TransportNetwork::Built &mapped = *completed;
  build.Network = mapped.Graph;
  build.NetworkOfWays = World.Stack.Ways().Ways().size();
  Published.Places("network: ways it holds", static_cast<double>(mapped.Ways), "ways");
  Published.Places("network: laying ways", mapped.LayMs, "ms");
  Published.Places("network: weaving topology", mapped.WeaveMs, "ms");
  Published.Places("network: beginning weave", mapped.BeginWeaveMs, "ms");
  Published.Places("network: cleaning weave temporaries", mapped.CleanupWeaveMs, "ms");
  Published.Places("network: longest weave cleanup slice", mapped.CleanupWeaveLongestMs, "ms");
  Published.Places("network: longest weave slice", mapped.WeaveLongestMs, "ms");
  Published.Places("network: longest snap slice", mapped.WeaveSlices.SnapMs, "ms");
  Published.Places("network: longest edge creation slice", mapped.WeaveSlices.EdgesMs, "ms");
  Published.Places("network: longest edge index slice", mapped.WeaveSlices.IndexMs, "ms");
  Published.Places("network: longest adjacency slice", mapped.WeaveSlices.AdjacencyMs, "ms");
  Published.Places("network: longest tie slice", mapped.WeaveSlices.TieMs, "ms");
  Published.Places("network: longest weave publish slice", mapped.WeaveSlices.PublishMs, "ms");
  Published.Places("network: classifying crossings", mapped.CrossingsMs, "ms");
  Published.Places("network: longest crossing slice", mapped.CrossingsLongestMs, "ms");
  Published.Places("network: longest crossing pair slice", mapped.CrossingSlices.TestMs, "ms");
  Published.Places("network: crossing publication", mapped.CrossingSlices.PublishMs, "ms");
  Published.Places("network: crossing point span", mapped.CrossingSweep.SpanMs, "ms");
  Published.Places("network: crossing segment list", mapped.CrossingSweep.SegmentsMs, "ms");
  Published.Places("network: crossing grid", mapped.CrossingSweep.GridMs, "ms");
  Published.Places("network: crossing cell filing", mapped.CrossingSweep.FilingMs, "ms");
  Published.Places("network: crossing pair tests", mapped.CrossingSweep.TestMs, "ms");
  Published.Places("network: crossing candidate pairs",
                   static_cast<double>(mapped.CrossingSweep.CandidatePairs),
                   "pairs");
  Published.Places("network: crossing cache", mapped.CrossingSweep.CacheMs, "ms");
  Published.Places("network: elevating nodes", mapped.ElevateMs, "ms");
  Published.Places("network: beginning elevation", mapped.BeginElevationMs, "ms");
  Published.Places("network: longest elevation slice", mapped.ElevateLongestMs, "ms");
  Published.Places(
      "network: longest node sample slice", mapped.ElevationSlices.SampleNodesMs, "ms");
  Published.Places(
      "network: longest point write slice", mapped.ElevationSlices.WritePointsMs, "ms");
  Published.Places("network: longest station slice", mapped.ElevationSlices.StationsMs, "ms");
  Published.Places("network: longest slope slice", mapped.ElevationSlices.SlopesMs, "ms");
  Published.Places("network: longest grade slice", mapped.ElevationSlices.GradesMs, "ms");
  Published.Places("network: publishing graph", mapped.PublishMs, "ms");
  Published.Places("network: longest build slice", longestSliceMs, "ms");
  Published.Places("network: nodes", static_cast<double>(mapped.Nodes), "nodes");
  Published.Places("network: edges", static_cast<double>(mapped.Edges), "edges");
  Published.Places("network: nodes where three or more edges meet",
                   static_cast<double>(mapped.Junctions),
                   "nodes");
  Published.Places(
      "network: points with a height", static_cast<double>(mapped.Elevated.Points), "points");
  Published.Places("network: points the ground refused a height",
                   static_cast<double>(mapped.Elevated.Refused),
                   "points");
  Published.Places("network: steepest grade", mapped.Elevated.SteepestGrade, "m/m");
  Published.Places(
      "network: steepest grade on a sealed way", mapped.Elevated.SteepestSealedGrade, "m/m");
  state.FinishesNetwork();
  state.CompletesStage();
  return GroundBuildProgress::Pending;
}

Engine::State::GroundBuildProgress
Engine::State::BeginsGroundBakes(const TangentFrame &standing) const {
  (void)standing;
  GroundBuildState &state = *World.GroundBuild;
  if (state.NextStage() != Core::GroundBuildSchedule::Stage::NeedsBakes) {
    return GroundBuildProgress::Ready;
  }
  if (!StructuresReady(state.Footprints(), state.Revision())) {
    return GroundBuildProgress::Pending;
  }
  state.CompletesStage();
  return GroundBuildProgress::Pending;
}

std::string_view Engine::State::GroundBuildStatus() const noexcept {
  return World.GroundBuild ? World.GroundBuild->Status() : "absent";
}

size_t Engine::State::StructureCandidatesMost() const noexcept {
  return (!World.GroundPublished.Current() && !World.GroundBuild) ||
                 (World.GroundBuild &&
                  World.GroundBuild->Revision().Quality == GroundQuality::Playable)
             ? kPlayableStructureCandidates
             : kRefinedStructureCandidates;
}

Ground::BuildingField *Engine::State::CandidateFootprints() const noexcept {
  return World.GroundBuild ? &World.GroundBuild->Footprints() : nullptr;
}

bool Engine::State::StagesGroundBakes(size_t landsMost) {
  if (!World.GroundBuild ||
      World.GroundBuild->NextStage() != Core::GroundBuildSchedule::Stage::NeedsBakes) {
    return true;
  }
  GroundBuildState &state = *World.GroundBuild;
  GroundWorldCandidate &candidate = state.Candidate();
  GroundBuildProducts &build = candidate.Products();
  const auto heights = state.Revision().Quality == GroundQuality::Refined
                           ? StructureBuildQueue::HeightRequirement::FineOnly
                           : StructureBuildQueue::HeightRequirement::AllowFallback;
  auto ready = World.StructureBuilds.NextLandings(
      World.Stack, state.Footprints(), WhereTheEyeStands(), landsMost, heights);
  if (!ready) {
    Error = Generators::Describe(ready.error());
    return false;
  }
  build.Pieces.Wears(build.Surfaces);
  for (const StructureBuildQueue::Landing &landing : *ready) {
    if (!build.Pieces.Hands(landing.Tile, *landing.Baked, landing.AnchorEcef, Error)) {
      return false;
    }
  }
  World.StructureBuilds.CommitsLandings(World.Stack, state.Footprints(), *ready);
  const int finestZoom = World.Stack.FinestZoomOf(Data::DataKind::Elevation);
  const StructureBuildQueue::HeightSource heightAt = [&build, finestZoom](LongitudeLatitude at) {
    return build.Sheets.AslMAt(finestZoom, at);
  };
  (void)World.StructureBuilds.Posts(World.Stack,
                                    state.Footprints(),
                                    WhereTheEyeStands(),
                                    heightAt,
                                    StructureCandidatesMost(),
                                    heights);
  return true;
}

Engine::State::GroundBuildProgress Engine::State::BeginsGroundPatchwork(const Around &coverage) {
  GroundBuildState &state = *World.GroundBuild;
  if (state.Laid() != nullptr) { return GroundBuildProgress::Ready; }
  static const Heap::Tag kPatchingTag("ground-patchwork");
  const Heap::Tagged patching(kPatchingTag);
  auto made = World.Shipping.Covering().Lay(World.Stack.Pool(), coverage);
  if (!made) {
    Error = made.error();
    World.GroundBuild.reset();
    return GroundBuildProgress::Failed;
  }
  if (made->ContactPending != 0 || made->Sheets.empty() ||
      (state.Revision().Quality == GroundQuality::Refined && made->Pending != 0)) {
    if (made->Pending != 0) { return GroundBuildProgress::Pending; }
    Error = Says::GroundContactIncomplete;
    World.GroundBuild.reset();
    return GroundBuildProgress::Failed;
  }
  state.Lays(*std::move(made));
  return GroundBuildProgress::Pending;
}

bool Engine::State::BuildGroundCorridors(const TangentFrame &standing,
                                         const Around &coverage,
                                         GroundBuildState &state) {
  const auto began = std::chrono::steady_clock::now();
  GroundBuildProducts &build = state.Candidate().Products();
  const TriangleBvh surface;
  Published.Places("ground: triangles the drape can reach",
                   static_cast<double>(build.Indices.size()) / 3.0,
                   "triangles");
  Drape drapedOver{.Surface = surface, .Field = {}};
  size_t fieldMisses = 0;
  std::optional<Drape::EastNorth> firstFieldMiss;
  drapedOver.Field = [&coverage, &build, &fieldMisses, &firstFieldMiss](Drape::EastNorth at) {
    const std::optional<double> sampled = build.Sheets.FieldUpM(coverage.Zoom, at);
    fieldMisses += sampled ? 0u : 1u;
    if (!sampled && !firstFieldMiss) { firstFieldMiss = at; }
    return sampled;
  };
  std::vector<Yields> corridors;
  std::vector<DiagnosticSample> notes;
  const Generators::Corridors::Site site{.Stack = World.Stack,
                                         .Network = build.Network.get(),
                                         .Standing = standing,
                                         .Draped = drapedOver,
                                         .Classes = build.ClassStructure,
                                         .CensusAt = state.Began(),
                                         .EyeLatDeg = coverage.LatitudeDeg,
                                         .EyeLonDeg = coverage.LongitudeDeg,
                                         .FocalPx = build.Footprints.FocalPx()};
  if (state.CorridorJob() == nullptr) {
    state.BeginsCorridors(Generators::Corridors::Begin(site));
    state.SamplesProductPeak();
    return true;
  }
  const auto paved = World.Shipping.Corridors().Advance(*state.CorridorJob(),
                                                        site,
                                                        kCorridorLanesPerFrame,
                                                        kCorridorNodesPerFrame,
                                                        build.Ground,
                                                        &corridors,
                                                        &notes);
  state.SamplesCorridorSlice(
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count());
  state.SamplesProductPeak();
  if (!paved) {
    Error = paved.error();
    return false;
  }
  if (fieldMisses > 0) {
    if (firstFieldMiss) {
      const Ground::OsmField *const shapes = World.Stack.Vectors();
      const int vectorZoom =
          shapes != nullptr && !shapes->Tiles().empty() ? shapes->Tiles().front().Z : -1;
      Error = std::format("{}: east {:.3f} m, north {:.3f} m, {} misses, sheet zoom {}, "
                          "DEM zoom {}, OSM zoom {}",
                          Says::CorridorTerrainIncomplete,
                          firstFieldMiss->EastM,
                          firstFieldMiss->NorthM,
                          fieldMisses,
                          coverage.Zoom,
                          World.Stack.Ground().BlockZoom(),
                          vectorZoom);
    } else {
      Error = Says::CorridorTerrainIncomplete;
    }
    return false;
  }
  if (!*paved) { return true; }
  for (const DiagnosticSample &one : notes) {
    Published.Places(one.Name, one.Value, one.Unit.c_str());
  }
  Published.Places(
      "ground candidate: corridor drape field misses", static_cast<double>(fieldMisses), "queries");
  build.Sheets.ForgetsFields();
  state.HoldsCorridors(std::move(corridors));
  state.CompletesStage();
  Published.Places(
      "ground candidate: longest corridor slice", state.LongestCorridorSliceMs(), "ms");
  Published.Places("ground candidate: corridors", state.CorridorJob()->WorkMs(), "ms");
  state.FinishesCorridors();
  return true;
}

bool Engine::State::BuildGroundTerrainMesh(const TangentFrame &standing,
                                           Patchwork &patchwork,
                                           GroundBuildState &state) {
  const auto began = std::chrono::steady_clock::now();
  GroundBuildProducts &build = state.Candidate().Products();
  GroundBuildState::MeshBuild &meshing = state.Meshing();
  if (!meshing.SheetsStitched) {
    if (!build.Sheets.Stitch(patchwork, Error)) { return false; }
    meshing.SheetsStitched = true;
    Published.Places(
        "ground candidate: sheet stitching",
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count(),
        "ms");
    return true;
  }
  if (!meshing.ResidencyStarted) {
    if (!build.Sheets.BeginResidency(patchwork, Error)) { return false; }
    meshing.ResidencyStarted = true;
    Published.Places(
        "ground candidate: residency preparation",
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count(),
        "ms");
    return true;
  }
  if (!meshing.ResidencyReady) {
    const auto advanced = build.Sheets.AdvanceResidency(patchwork, kTerrainResidencySheetsPerFrame);
    if (!advanced) {
      Error = advanced.error();
      return false;
    }
    const double sliceMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
    meshing.LongestResidencySliceMs = std::max(meshing.LongestResidencySliceMs, sliceMs);
    state.SamplesProductPeak();
    if (!*advanced) { return true; }
    meshing.ResidencyReady = true;
    Published.Places(
        "ground candidate: longest residency slice", meshing.LongestResidencySliceMs, "ms");
    return true;
  }
  const size_t end = std::min(meshing.NextSheet + kTerrainSheetsPerFrame, patchwork.Sheets.size());
  for (; meshing.NextSheet < end; ++meshing.NextSheet) {
    Generators::AppendTerrainMeshSheet(meshing.Mesh,
                                       patchwork.Sheets[meshing.NextSheet],
                                       standing,
                                       {.Side = Render::GroundLattice::kSide, .Halo = 1});
  }
  const double sliceMs =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
  meshing.LongestSliceMs = std::max(meshing.LongestSliceMs, sliceMs);
  state.SamplesProductPeak();
  if (meshing.NextSheet < patchwork.Sheets.size()) { return true; }
  Published.Places("ground candidate: longest native mesh slice", meshing.LongestSliceMs, "ms");
  build.PositionsM = std::move(meshing.Mesh.PositionsM);
  build.Indices = std::move(meshing.Mesh.Indices);
  Published.Places(
      "ground: height pages standing", static_cast<double>(build.Sheets.Standing()), "pages");
  Published.Places(
      "ground: tiles the lattice draws", static_cast<double>(build.Sheets.Instances()), "tiles");
  for (const auto &[name, kind] : {std::pair{"virtual", build.Sheets.Seams().Virtual},
                                   std::pair{"real", build.Sheets.Seams().Real}}) {
    const std::string at = std::string("ground: seam, ") + name + ", ";
    Published.Places(at + "edges stitched", static_cast<double>(kind.Edges), "edges");
    Published.Places(at + "even nodes off the coarser node, worst", kind.EvenM, "m");
    Published.Places(
        at + "odd nodes off the coarser chord before the stitch, worst", kind.OddBeforeM, "m");
    Published.Places(at + "odd nodes off the coarser chord after it, worst", kind.OddAfterM, "m");
    if (kind.OddBeforeM > 0.0) {
      Published.Places(at + "worst odd node longitude", kind.WorstLongitudeDeg, "deg");
      Published.Places(at + "worst odd node latitude", kind.WorstLatitudeDeg, "deg");
      Published.Places(at + "worst odd node fine zoom", kind.WorstFineZoom, "zoom");
      Published.Places(at + "worst odd node coarse zoom", kind.WorstCoarseZoom, "zoom");
    }
  }
  const uint64_t sheets = build.Sheets.Digest();
  Published.Places(
      "ground: the sheets' digest, low half", static_cast<double>(sheets & kLowWord), "digest");
  Published.Places(
      "ground: the sheets' digest, high half", static_cast<double>(sheets >> 32U), "digest");
  Published.Places("ground: sheets NOT drawn for want of nodes",
                   static_cast<double>(build.Sheets.Flat()),
                   "tiles");
  state.CompletesStage();
  Published.Places(
      "ground candidate: terrain mesh",
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count(),
      "ms");
  return true;
}

bool Engine::State::PublishGroundGeometry(GroundBuildState &state) {
  const auto began = std::chrono::steady_clock::now();
  GroundWorldCandidate &candidate = state.Candidate();
  GroundBuildProducts &build = candidate.Products();
  const size_t drivenParts = Picture.Standing->CarriedParts();
  Published.Places("restand: the carried count the world hands over",
                   static_cast<double>(drivenParts),
                   "carried");
  Published.Places(
      "restand: parts in the geometry", static_cast<double>(build.Ground.parts()), "parts");
  candidate.GroundIs(build.GroundSurface.index());
  const auto classesBegan = std::chrono::steady_clock::now();
  if (build.ClassStructure && !build.ClassPalette.empty() &&
      !candidate.SetGroundClasses(
          {build.ClassStructure->Words(), build.ClassStructure->Bytes() / sizeof(uint32_t)},
          build.ClassPalette,
          Error)) {
    return false;
  }
  Published.Places(
      "ground candidate: class upload",
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - classesBegan)
          .count(),
      "ms");
  candidate.Digests(Session.Declared.Render.Audits);
  size_t handed = 0;
  for (int part = 0; part < build.Ground.parts(); ++part) {
    handed += build.Ground.trianglesOf(part).size() / 3u;
  }
  Published.Places(
      "the triangles handed to the renderer", static_cast<double>(handed), "triangles");
  Published.Places("in this many parts", static_cast<double>(build.Ground.parts()), "parts");
  const auto geometryBegan = std::chrono::steady_clock::now();
  if (!candidate.SetGroundGeometry(
          std::move(build.Ground), drivenParts, build.GroundMaterial, Error)) {
    return false;
  }
  Published.Places(
      "ground candidate: scene geometry",
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - geometryBegan)
          .count(),
      "ms");
  state.CompletesStage();
  Published.Places(
      "ground candidate: geometry",
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count(),
      "ms");
  return true;
}

bool Engine::State::Grounds(bool alsoWhenTilesLanded, GroundQuality quality) {
  static const Heap::Tag kLayingTag("world-ground");
  const Heap::Tagged laying(kLayingTag);
  auto phaseAt = std::chrono::steady_clock::now();
  const Scenario::Document &declared = Session.Declared;
  if (!declared.Ground.Declared || !Picture.Standing || !World.Stack.Opened()) { return true; }
  if (!GroundSourcesReady(World.Stack, quality)) { return true; }
  const double anchorLat = declared.Ground.Origin.LatitudeDeg;
  const double anchorLon = declared.Ground.Origin.LongitudeDeg;

  const auto asked = RingWanted(alsoWhenTilesLanded, quality);
  if (!asked) { return asked.error() == Laid::Unchanged || asked.error() == Laid::Pending; }
  const GroundBuildProgress progress = BeginsGroundBuild(*asked);
  if (progress != GroundBuildProgress::Ready) { return progress != GroundBuildProgress::Failed; }
  GroundBuildState &state = *World.GroundBuild;
  const Around &over = state.Coverage();
  GroundWorldCandidate &candidate = state.Candidate();
  GroundBuildProducts &build = candidate.Products();

  const auto rebuildBegan = state.Began();

  const GroundBuildProgress patchwork = BeginsGroundPatchwork(over);
  if (patchwork != GroundBuildProgress::Ready) { return patchwork != GroundBuildProgress::Failed; }
  Patchwork &laid = *state.Laid();

  const double frameLat = anchorLat;
  const double frameLon = anchorLon;
  const TangentFrame standing =
      TangentFrame::At({.LongitudeDeg = frameLon, .LatitudeDeg = frameLat});
  const GroundBuildProgress sheetProgress = BeginsGroundSheets(standing, laid, over);
  if (sheetProgress != GroundBuildProgress::Ready) {
    return sheetProgress != GroundBuildProgress::Failed;
  }
  const GroundBuildProgress classes = BeginsGroundClasses();
  if (classes == GroundBuildProgress::Failed) { return false; }
  const GroundBuildProgress materialProgress = BeginsGroundSurface();
  if (materialProgress != GroundBuildProgress::Ready) {
    return materialProgress != GroundBuildProgress::Failed;
  }
  Geometry &ground = build.Ground;
  const MaterialInstance ringSurface = build.GroundSurface;

  const GroundBuildProgress models = BeginsGroundModels(standing);
  if (models != GroundBuildProgress::Ready) { return models != GroundBuildProgress::Failed; }
  const GroundBuildProgress network = BeginsGroundNetwork();
  if (network != GroundBuildProgress::Ready) { return network != GroundBuildProgress::Failed; }
  const GroundBuildProgress bakes = BeginsGroundBakes(standing);
  if (bakes != GroundBuildProgress::Ready) { return bakes != GroundBuildProgress::Failed; }
  if (state.NextStage() == Core::GroundBuildSchedule::Stage::NeedsCorridors) {
    return BuildGroundCorridors(standing, over, state);
  }
  if (state.NextStage() == Core::GroundBuildSchedule::Stage::NeedsEarthworks) {
    return PressGroundEarthworks(standing, laid, state);
  }
  if (state.NextStage() == Core::GroundBuildSchedule::Stage::NeedsTerrainMesh) {
    return BuildGroundTerrainMesh(standing, laid, state);
  }
  if (state.NextStage() == Core::GroundBuildSchedule::Stage::NeedsWater) {
    const auto began = std::chrono::steady_clock::now();
    if (!BuildWaterSurfaces(standing, ground, ringSurface)) { return false; }
    state.CompletesStage();
    Published.Places(
        "ground candidate: water",
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count(),
        "ms");
    return true;
  }
  if (state.NextStage() == Core::GroundBuildSchedule::Stage::NeedsGeometry) {
    return PublishGroundGeometry(state);
  }
  if (state.NextStage() != Core::GroundBuildSchedule::Stage::NeedsPublication) {
    Error = "ground candidate reached an invalid stage";
    return false;
  }
  phaseAt = std::chrono::steady_clock::now();
  state.PublishesFootprints();
  if (auto published =
          candidate.Publish(World, World.Stack.Footprints(), Picture.Standing, state.Revision());
      !published) {
    Error = std::move(published.error());
    World.GroundBuild.reset();
    return false;
  }
  Published.Places(
      "ground candidate: publication",
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - phaseAt).count(),
      "ms");
  Published.Places("ground publication: quality",
                   state.Revision().Quality == GroundQuality::Refined ? 1.0 : 0.0,
                   "0=playable 1=refined");
  const uint64_t geometry = World.Pieces.Digest();
  Published.Places(
      "the geometry the world built, high half", static_cast<double>(geometry >> 32U), "digest");
  Published.Places("and its low half", static_cast<double>(geometry & kLowWord), "digest");
  Published.Places("ground candidate: direct CPU product peak",
                   static_cast<double>(state.ProductPeakBytes()),
                   "bytes");
  World.GroundBuild.reset();
  Published.Places(
      "rebuild: of that, walking it into the proxy", Picture.Standing->BuildMs(), "ms");
  Published.Places("rebuild: standing render plan", Picture.Standing->PlanMs(), "ms");
  Published.Places("rebuild: of THAT, copying the subject", Picture.Standing->CarryMs(), "ms");
  Published.Places(
      "rebuild: standing and submitting INSIDE Build", Picture.Standing->InsideMs(), "ms");
  Published.Places("rebuild: shaping what was built", Picture.Standing->ReshapeMs(), "ms");
  Published.Places("rebuild: composing it", Picture.Standing->ComposeMs(), "ms");
  Published.Places("stand: shaping it a second time", Picture.Standing->ReshapeAgainMs(), "ms");
  Published.Places("stand: the proxy taking it", Picture.Standing->ProxyStandsMs(), "ms");
  Published.Places("stand: placing every part", Picture.Standing->PlacesMs(), "ms");
  Published.Places("stand: dressing them", Picture.Standing->WearsMs(), "ms");
  Published.Places("stand: their emitted radiance", Picture.Standing->LampsMs(), "ms");
  Published.Places("stand: the lamps and the key", Picture.Standing->LitMs(), "ms");
  Published.Places("stand: the medium's own tables", Picture.Standing->MediumMs(), "ms");

  Published.Places("stand: times the sky was integrated",
                   static_cast<double>(Picture.Standing->SkyIntegrations()),
                   "integrations");
  Published.Places("stand: sweeping the bounds to frame it", Picture.Standing->FramingMs(), "ms");
  Published.Places("rebuild: resolving its surface", Picture.Standing->ResolveMs(), "ms");
  Published.Places("rebuild: and its bounds", Picture.Standing->BoundsMs(), "ms");
  Published.Places(
      "rebuild: cutting it into clusters", Picture.Standing->Clustering().BuildMs, "ms");
  Published.Places("cook: clusters with no parent above them",
                   static_cast<double>(Picture.Standing->Clustering().RootClusters),
                   "clusters");
  Published.Places("cook: clusters in all",
                   static_cast<double>(Picture.Standing->Clustering().Clusters),
                   "clusters");
  Published.Places(
      "rebuild: of the streams, packing them", Picture.Standing->TransferMetrics().PackingMs, "ms");
  Published.Places("restand: the geometry handed over, digested",
                   Picture.Standing->TransferMetrics().DigestValue(),
                   "");
  Published.Places(
      "rebuild: digesting what it handed over", Picture.Standing->TransferMetrics().DigestMs, "ms");
  Published.Places(
      "rebuild: and the device taking them", Picture.Standing->TransferMetrics().UploadMs, "ms");
  Published.Places("rebuild: residency upload attempts",
                   static_cast<double>(Picture.Device.TakeUploadAttempts()),
                   "uploads");
  Published.Places("rebuild: bytes offered for upload",
                   static_cast<double>(Picture.Device.TakeUploadBytes()),
                   "bytes");
  Published.Places("rebuild: device buffer allocation attempts",
                   static_cast<double>(Picture.Device.TakeBufferAllocationAttempts()),
                   "buffers");
  Published.Places("rebuild: staging buffer allocation attempts",
                   static_cast<double>(Picture.Device.TakeStagingAllocationAttempts()),
                   "buffers");
  Published.Places("rebuild: laying the surface", Picture.Standing->SurfaceMs(), "ms");
  Published.Places("rebuild: settling placements and lights", Picture.Standing->StandMs(), "ms");
  Published.Places("rebuild: and the streams to the device", Picture.Standing->SubmitMs(), "ms");
  Published.Places(
      "rebuild: and handing it to the device took",
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - phaseAt).count(),
      "ms");
  phaseAt = std::chrono::steady_clock::now();
  Published.Places("restand: parts the proxy then stands with",
                   static_cast<double>(Picture.Standing->PartsStanding()),
                   "parts");
  Published.Places("restand: instances it carries",
                   static_cast<double>(Picture.Standing->InstancesStanding()),
                   "instances");
  Published.Places(
      "restand: the near plane the renderer stands on", Picture.Standing->NearStanding(), "m");
  ReportGroundPlacements();
  World.GroundTiles = laid.Tiles;
  Published.Places("tiles the ring laid", static_cast<double>(laid.Tiles), "tiles");
  Published.Places("tiles it is still waiting for", static_cast<double>(laid.Pending), "tiles");
  Published.Places("tiles the stack does not hold", static_cast<double>(laid.Absent), "tiles");
  Published.Places("tiles it refused", static_cast<double>(laid.Refused), "tiles");
  Published.Places("the sun stands this high", Picture.Standing->Standing().KeyElevationDeg, "deg");
  Published.Places("and bears", Picture.Standing->Standing().KeyBearingDeg, "deg");
  Published.Places("the light that reaches the ground", Picture.Standing->MeteredLux(), "lux");
  Published.Places("and the exposure metered from it",
                   Picture.Standing->Standing().KeyFromClock ? 1.0 : 0.0,
                   "yes/no");
  Published.Places("times the terrain was rebuilt", static_cast<double>(World.Relaid), "rebuilds");
  ++World.Rebuilds;
  World.RebuildMs =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - rebuildBegan)
          .count();
  Published.Places("and what the last rebuild took", World.RebuildMs, "ms");
  Published.Places(
      "rebuild: times the world was built WHOLE", static_cast<double>(World.Rebuilds), "rebuilds");
  Published.Places("and how often it was asked about", static_cast<double>(World.Asked), "walks");
  Published.Places(
      "levels the cascade laid", static_cast<double>(over.Zoom - laid.CoarsestZoom + 1), "levels");
  Published.Places(
      "tiles it skipped as already covered", static_cast<double>(laid.Skipped), "tiles");
  Published.Places("tiles the last rebuild laid bare", static_cast<double>(laid.Bare), "tiles");
  World.Pending = laid.Pending;
  World.Bare = laid.Bare;
  World.Wanted = laid.Tiles;
  Published.Places(
      "tiles that overlap a finer level", static_cast<double>(laid.Overlapped), "tiles");
  return true;
}
}
