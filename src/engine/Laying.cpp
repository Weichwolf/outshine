#include "Digest.h"
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
#include <functional>
#include <optional>
#include <span>
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
#include "EngineHeld.h"
#include "GroundMesher.h"

namespace outshine {

static_assert(Ground::kStreamGrid == 2 * kPatchGrid,
              "the elevation stream is opened ONE zoom below the finest tile so its posting equals "
              "the patchwork's vertex spacing; that holds only while a stream tile carries twice "
              "the intervals a patchwork tile lays, and if either grid moves the zoom must be "
              "re-derived rather than kept");

namespace {

constexpr double kPerMille = 1000.0;

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
constexpr int kLatticeVirtualLevels = 4;

constexpr size_t kBounceProbeStride = 16;

} // namespace

std::vector<float> Engine::State::PaletteOver(const Ground::VegetationTemplates &wearing,
                                              const Render::Medium &fallback) {
  const size_t rows = wearing.TemplateCount();
  std::vector<float> palette(kPaletteStride * (rows + 2u), 0.0f);
  palette[0] = std::bit_cast<float>(static_cast<uint32_t>(rows));
  const auto rowAt = [](size_t row) { return kPaletteStride * (row + 1u); };
  for (size_t row = 0; row < rows; ++row) {
    for (size_t channel = 0; channel < 3; ++channel) {
      palette[rowAt(row) + channel] = wearing.Rows()[row].Ground[channel];
    }
    palette[rowAt(row) + 3u] = wearing.Rows()[row].Mix[2];
  }
  for (size_t channel = 0; channel < 3; ++channel) {
    palette[rowAt(rows) + channel] = fallback.GroundAlbedo[channel];
  }
  return palette;
}

Engine::State::Classed Engine::State::Classify(std::span<const float> groundPositionsM) {
  Classed out;
  const std::shared_ptr<const ClassStructure> classes = World.Stack.Classes().Read();
  const Ground::VegetationTemplates &wearing = World.Stack.Vegetation();
  const Render::Medium fallback = Render::kEarthAir;
  if (classes && wearing.Ready()) {
    out.Structure = classes;
    out.Palette = PaletteOver(wearing, fallback);
  }
  if (out.Structure && !out.Palette.empty()) {
    const size_t rows = out.Palette.size() / kPaletteStride - 2u;
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
      Picture.Standing->Grounding(wornMean);
      Published.Places(
          "lighting: the ground it bounces off, red", kPerMille * wornMean[0], "albedo/1000");
      Published.Places("lighting: green", kPerMille * wornMean[1], "albedo/1000");
      Published.Places("lighting: blue", kPerMille * wornMean[2], "albedo/1000");
    }
  }
  const Render::SubjectEnvironment &lighting = Picture.Standing->AmbientStanding();
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
    if (!fill.empty()) {
      std::ranges::sort(fill);
      std::ranges::sort(across);
      size_t wouldStamp = 0;
      for (const double filled : fill) {
        if (filled > kStampWorthM) { ++wouldStamp; }
      }
      size_t underOneCell = 0;
      for (const double wide : across) {
        if (wide < kGroundCellM) { ++underOneCell; }
      }
      Published.Places(
          "buildings: a stamp would fill, p50", QuantileOf(fill, kMiddleQuantile), "m");
      Published.Places("buildings: a stamp would fill, p95", QuantileOf(fill, kBroadQuantile), "m");
      Published.Places("buildings: a stamp would fill, worst", fill.back(), "m");
      Published.Places(
          "buildings: footprints worth a stamp", static_cast<double>(wouldStamp), "footprints");
      Published.Places(
          "buildings: footprint across, p50", QuantileOf(across, kMiddleQuantile), "m");
      Published.Places(
          "buildings: footprint across, p05", QuantileOf(across, kNarrowQuantile), "m");
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

void Engine::State::Models(const TangentFrame &standing,
                           LongitudeLatitude stands,
                           Geometry &ground,
                           Phasing &clocks) {
  (void)stands;
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
  const MaterialInstance wallSurface = ground.addSurface("walls", walls);
  const MaterialInstance roofSurface = ground.addSurface("roofs", tiles);
  World.Pieces.Wears({.Walls = static_cast<uint32_t>(wallSurface.index()),
                      .Roofs = static_cast<uint32_t>(roofSurface.index())});
  Published.Places(
      "rebuild: the ground ring took",
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - clocks.PhaseAt)
          .count(),
      "ms");
  clocks.PhaseAt = std::chrono::steady_clock::now();
  clocks.CensusAt = clocks.PhaseAt;
  Published.Places(
      "buildings: the wall surface", static_cast<double>(wallSurface.index()), "index");
  Published.Places(
      "buildings: the roof surface", static_cast<double>(roofSurface.index()), "index");
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
                     static_cast<double>(Picture.Standing->PiecesStanding()),
                     "pieces");
    Published.Places("buildings: triangles the arena holds",
                     static_cast<double>(Picture.Standing->PieceTriangles()),
                     "triangles");
    Published.Places("buildings: bytes the arena holds on the device",
                     static_cast<double>(Picture.Standing->PieceBytesHeld()),
                     "bytes");
  }
}

Engine::State::Laid
Engine::State::Focuses(const Around &over, LongitudeLatitude at, bool alsoWhenTilesLanded) {
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
  auto sees = World.Shipping.Covering().Lay(World.Stack.Pool(), asking);
  if (!sees) {
    Error = sees.error();
    return Laid::Refused;
  }
  World.Pending = sees->Pending;
  World.Bare = sees->Bare;
  World.Wanted = sees->Tiles;
  World.AskedPending = sees->Pending;
  World.AskedWanted = sees->Tiles;
  const size_t resident = sees->Tiles > sees->Pending ? sees->Tiles - sees->Pending : 0;
  const std::shared_ptr<const ClassStructure> naming = World.Stack.Classes().Read();
  const uint64_t classes = naming ? naming->Version() : 0;
  const bool elsewhere = from != World.LaidFrom;
  const bool grew =
      alsoWhenTilesLanded && (resident != World.LaidResident || World.RimsMissing > 0);
  const bool renamed = classes != World.LaidClasses;
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
                   static_cast<double>(World.Bakes.Queued()),
                   "tiles");
  Published.Places("world: the building pieces the device holds",
                   Picture.Standing ? static_cast<double>(Picture.Standing->PieceBytesHeld()) : 0.0,
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
  if (World.EverLaid && !elsewhere && !grew && !renamed) { return Laid::Unchanged; }

  {
    constexpr uint64_t kLowWord = 0xFFFFFFFFULL;
    const uint64_t geometry = World.Pieces.Digest();
    Published.Places(
        "the geometry the world built, high half", static_cast<double>(geometry >> 32U), "digest");
    Published.Places("and its low half", static_cast<double>(geometry & kLowWord), "digest");
  }
  Published.Places(
      "rebuilds since the world stood", static_cast<double>(World.Relaid + 1u), "rebuilds");
  Published.Places("rebuild: the eye walked into another tile", elsewhere ? 1.0 : 0.0, "yes/no");
  Published.Places("rebuild: tiles resident when it did", static_cast<double>(resident), "tiles");
  Published.Places(
      "rebuild: and resident the time before", static_cast<double>(World.LaidResident), "tiles");
  Published.Places("rebuild: the land classes were named anew", renamed ? 1.0 : 0.0, "yes/no");
  World.LaidFrom = from;
  World.LaidResident = resident;
  World.LaidClasses = classes;
  World.EverLaid = true;
  ++World.Relaid;
  return Laid::Wanted;
}

void Engine::State::TellsTheRelief(Relieved over) {
  Published.Places("relief: the ring's tallest vertex ABOVE THE ELLIPSOID", over.Tallest, "m");
  Published.Places("relief: and how far out it lies", over.TallestOutM, "m");
  Published.Places("relief: the ring's lowest vertex above the ellipsoid", over.Lowest, "m");
  Published.Places(
      "relief: so the true relief, with the sphere taken out", over.Tallest - over.Lowest, "m");
}

std::expected<Around, Engine::State::Laid> Engine::State::RingWanted(bool alsoWhenTilesLanded) {
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
  if (!Watches()) { return std::unexpected(Laid::Refused); }
  switch (Focuses(over, {.LongitudeDeg = atLon, .LatitudeDeg = atLat}, alsoWhenTilesLanded)) {
    case Laid::Refused: return std::unexpected(Laid::Refused);
    case Laid::Unchanged: return std::unexpected(Laid::Unchanged);
    case Laid::Wanted: break;
  }
  return over;
}

bool Engine::State::Grounds(bool alsoWhenTilesLanded) {
  const Heap::Tagged laying("world-ground");
  auto phaseAt = std::chrono::steady_clock::now();
  auto censusAt = phaseAt;
  auto wiresAt = phaseAt;
  const Scenario::Document &declared = Session.Declared;
  if (!declared.Ground.Declared) { return true; }
  if (!Picture.Standing || !World.Stack.Opened()) { return true; }
  const double anchorLat = declared.Ground.Origin.LatitudeDeg;
  const double anchorLon = declared.Ground.Origin.LongitudeDeg;

  const auto asked = RingWanted(alsoWhenTilesLanded);
  if (!asked) { return asked.error() == Laid::Unchanged; }
  const Around over = *asked;

  const auto rebuildBegan = std::chrono::steady_clock::now();
  {}

  std::optional<Patchwork> patchwork;
  {
    const Heap::Tagged patching("ground-patchwork");
    auto made = World.Shipping.Covering().Lay(World.Stack.Pool(), over);
    if (!made) {
      Error = made.error();
      return false;
    }
    patchwork = *std::move(made);
  }
  Patchwork *const laid = &*patchwork;

  const double frameLat = anchorLat;
  const double frameLon = anchorLon;
  const TangentFrame standing =
      TangentFrame::At({.LongitudeDeg = frameLon, .LatitudeDeg = frameLat});
  {
    World.Sheets.Framed(standing);
    Published.Places(
        "ground: virtual tiles the lattice refines to",
        static_cast<double>(HeightSheets::Refine(
            *laid,
            {.FinestZoom = over.Zoom,
             .Levels = kLatticeVirtualLevels,
             .Eye = {.LongitudeDeg = over.LongitudeDeg, .LatitudeDeg = over.LatitudeDeg}})),
        "tiles");
    const auto haloAt = std::chrono::steady_clock::now();
    Published.Places(
        "ground: sheets the lattice haloed",
        static_cast<double>(World.Sheets.Halos(*laid, World.Stack.Ground(), over.Zoom)),
        "sheets");
    World.RimsMissing = World.Sheets.RimsMissing();
    Published.Places("ground: rims copied for want of a neighbour",
                     static_cast<double>(World.RimsMissing),
                     "sheets");
    Published.Places(
        "ground: of that, haloing",
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - haloAt)
            .count(),
        "ms");
  }
  {
    const HeightSheets::Soup soup = World.Sheets.SoupOf(*laid);
    World.GroundPositionsM = soup.PositionM;
    World.GroundIndex = soup.Index;
    TellsTheRelief(
        {.Tallest = soup.TallestM, .Lowest = soup.LowestM, .TallestOutM = soup.TallestOutM});
  }
  Classed classed;
  {
    const Heap::Tagged classing("ground-classify");
    classed = Classify(World.Sheets.SoupOf(*laid, over.Zoom).PositionM);
  }
  const std::vector<float> &classPalette = classed.Palette;
  const std::shared_ptr<const ClassStructure> &classStructure = classed.Structure;
  Geometry ground;
  Material bare;
  {
    const Render::Medium held = Render::kEarthAir;
    for (int channel = 0; channel < 3; ++channel) {
      bare.BaseColour[channel] = held.GroundAlbedo[channel];
    }
  }
  const MaterialInstance ringSurface = ground.addSurface("ground", bare);

  Phasing clocks{.PhaseAt = phaseAt, .CensusAt = censusAt, .WiresAt = wiresAt};
  {
    const Heap::Tagged modelling("ground-model");
    Models(standing, {.LongitudeDeg = anchorLon, .LatitudeDeg = anchorLat}, ground, clocks);
  }
  phaseAt = clocks.PhaseAt;
  censusAt = clocks.CensusAt;
  wiresAt = clocks.WiresAt;

  std::vector<Yields> corridor;
  Published.Places(
      "rebuild: of that, the ring and the buildings into the frame",
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - censusAt)
          .count(),
      "ms");
  censusAt = std::chrono::steady_clock::now();
  const TriangleBvh surface = TriangleBvh::Over(
      std::span<const float>(World.GroundPositionsM.data(), World.GroundPositionsM.size()),
      std::span<const uint32_t>(World.GroundIndex.data(), World.GroundIndex.size()));
  Published.Places(
      "rebuild: of that, the surface the world stands on",
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - censusAt)
          .count(),
      "ms");
  Published.Places("ground: triangles the drape can reach",
                   static_cast<double>(World.GroundIndex.size()) / 3.0,
                   "triangles");
  Drape drapedOver{.Surface = surface};
  World.Sheets.ForgetsFields();
  drapedOver.Field = [this, &over](double eastM, double southM) {
    return World.Sheets.FieldUpM(
        World.Stack.Ground(), over.Zoom, {.EastM = eastM, .SouthM = southM});
  };
  {
    std::vector<Measure> notes;
    World.Shipping.Corridors().Lay({.Stack = World.Stack,
                                    .Standing = standing,
                                    .Draped = drapedOver,
                                    .Classes = classStructure,
                                    .CensusAt = clocks.CensusAt,
                                    .EyeLatDeg = over.LatitudeDeg,
                                    .EyeLonDeg = over.LongitudeDeg,
                                    .FocalPx = World.Stack.Footprints().FocalPx()},
                                   ground,
                                   &corridor,
                                   &notes);
    for (const Measure &one : notes) { Published.Places(one.What, one.How, one.Unit.c_str()); }
    clocks.WiresAt = std::chrono::steady_clock::now();
  }
  World.Sheets.ForgetsFields();

  {
    const Ground::BuildingField &pads = World.Stack.Footprints();
    const Ground::OsmField *const shapes = World.Stack.Vectors();
    std::vector<Yields> yielding;
    if (shapes != nullptr) {
      const std::span<const double> points = shapes->Points();
      for (const Ground::BuildingField::Footprint &one : pads.Footprints()) {
        if (one.PointCount < 3) { continue; }
        Yields made;
        made.RingEastSouthM.reserve(static_cast<size_t>(one.PointCount) * 2u);
        made.LowE = kBeyondAnyCoordinate;
        made.HighE = -kBeyondAnyCoordinate;
        made.LowS = kBeyondAnyCoordinate;
        made.HighS = -kBeyondAnyCoordinate;
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
          made.RingEastSouthM.push_back(eastM);
          made.RingEastSouthM.push_back(-northM);
          made.LowE = std::min(made.LowE, eastM);
          made.HighE = std::max(made.HighE, eastM);
          made.LowS = std::min(made.LowS, -northM);
          made.HighS = std::max(made.HighS, -northM);
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
        made.SeamEastSouthM = made.RingEastSouthM;
        yielding.push_back(std::move(made));
      }
    }
    const size_t builtPads = yielding.size();
    if (shapes != nullptr) {
      const std::span<const double> points = shapes->Points();
      for (const Ground::WaterField::Surface &lake : World.Stack.WaterBodies().Surfaces()) {
        if (lake.PointCount < 3) { continue; }
        const size_t last = (static_cast<size_t>(lake.FirstPoint) + lake.PointCount) * 2u;
        if (last > points.size()) { continue; }
        Yields made;
        made.RingEastSouthM.reserve(static_cast<size_t>(lake.PointCount) * 2u);
        made.LowE = kBeyondAnyCoordinate;
        made.HighE = -kBeyondAnyCoordinate;
        made.LowS = kBeyondAnyCoordinate;
        made.HighS = -kBeyondAnyCoordinate;
        std::vector<double> bedM;
        bedM.reserve(lake.PointCount);
        for (uint32_t step = 0; step < lake.PointCount; ++step) {
          const size_t at = (static_cast<size_t>(lake.FirstPoint) + step) * 2u;
          const EastNorthUp shore =
              standing.Place({.LongitudeDeg = points[at + 1],
                              .LatitudeDeg = points[at],
                              .HeightM = static_cast<double>(lake.LevelM) - kWaterBedM});
          made.RingEastSouthM.push_back(shore.EastM);
          made.RingEastSouthM.push_back(-shore.NorthM);
          made.LowE = std::min(made.LowE, shore.EastM);
          made.HighE = std::max(made.HighE, shore.EastM);
          made.LowS = std::min(made.LowS, -shore.NorthM);
          made.HighS = std::max(made.HighS, -shore.NorthM);
          bedM.push_back(shore.UpM);
        }
        made.AtE = 0.5 * (made.LowE + made.HighE);
        made.AtS = 0.5 * (made.LowS + made.HighS);
        made.SagInv = 1.0 / Data::kWgs84A;
        double plateau = 0.0;
        for (size_t corner = 0; corner < bedM.size(); ++corner) {
          const double dE = made.RingEastSouthM[corner * 2u] - made.AtE;
          const double dS = made.RingEastSouthM[corner * 2u + 1u] - made.AtS;
          plateau += bedM[corner] + 0.5 * (dE * dE + dS * dS) * made.SagInv;
        }
        made.PlateauM = plateau / static_cast<double>(bedM.size());
        made.ApronM = kWaterBankM;
        made.YieldM = kWaterBedM;
        made.Kind = Stamp::Basin;
        made.SeamEastSouthM = made.RingEastSouthM;
        yielding.push_back(std::move(made));
      }
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
    const auto pressAt = std::chrono::steady_clock::now();
    const HeightSheets::Pressed pressed_ = World.Sheets.Press(yielding, *laid, kMostEarthworkM);
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
      Published.Places(
          std::format("ground: nodes inside {} that do not fill, below it, worst", what),
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
    Published.Places(
        "ground: of that, pressing",
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - pressAt)
            .count(),
        "ms");
    if (!World.Sheets.Hands(*laid, Error)) { return false; }
    const HeightSheets::Soup pressed = World.Sheets.SoupOf(*laid);
    World.GroundPositionsM = pressed.PositionM;
    World.GroundIndex = pressed.Index;
  }
  Published.Places(
      "ground: height pages standing", static_cast<double>(World.Sheets.Standing()), "pages");
  Published.Places(
      "ground: tiles the lattice draws", static_cast<double>(World.Sheets.Instances()), "tiles");
  for (const auto &[name, kind] : {std::pair{"virtual", World.Sheets.Seams().Virtual},
                                   std::pair{"real", World.Sheets.Seams().Real}}) {
    const std::string at = std::string("ground: seam, ") + name + ", ";
    Published.Places(at + "edges stitched", static_cast<double>(kind.Edges), "edges");
    Published.Places(at + "even nodes off the coarser node, worst", kind.EvenM, "m");
    Published.Places(
        at + "odd nodes off the coarser chord before the stitch, worst", kind.OddBeforeM, "m");
    Published.Places(at + "odd nodes off the coarser chord after it, worst", kind.OddAfterM, "m");
  }
  Published.Places("ground: sheets NOT drawn for want of nodes",
                   static_cast<double>(World.Sheets.Flat()),
                   "tiles");
  {
    const auto waterAt = std::chrono::steady_clock::now();
    const Ground::WaterField &wet = World.Stack.WaterBodies();
    const Ground::OsmField *const vectors = World.Stack.Vectors();
    std::vector<float> places;
    std::vector<float> facing;
    std::vector<float> lidUv;
    std::vector<uint32_t> order;
    size_t lidsLaid = 0;
    size_t lidsRefused = 0;
    if (vectors != nullptr) {
      const std::span<const double> points = vectors->Points();
      for (const Ground::WaterField::Surface &lake : wet.Surfaces()) {
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
        const bool whole = true;
        for (uint32_t step = 1; step + 1 < lake.PointCount && whole; ++step) {
          const std::array<uint32_t, 3> corners = {{0u, step, step + 1u}};
          for (const uint32_t corner : corners) {
            const size_t at = (static_cast<size_t>(lake.FirstPoint) + corner) * 2;
            double eastM = 0.0;
            double upM = 0.0;
            double northM = 0.0;
            const EastNorthUp placed =
                standing.Place({.LongitudeDeg = points[at + 1],
                                .LatitudeDeg = points[at],
                                .HeightM = static_cast<double>(lake.LevelM)});
            eastM = placed.EastM;
            upM = placed.UpM;
            northM = placed.NorthM;
            places.push_back(static_cast<float>(eastM));
            places.push_back(static_cast<float>(upM));
            places.push_back(static_cast<float>(-northM));
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
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - waterAt)
            .count(),
        "ms");
    Published.Places("water: surfaces laid", static_cast<double>(lidsLaid), "surfaces");
    Published.Places("water: surfaces refused", static_cast<double>(lidsRefused), "surfaces");
    const size_t waterTriangles = order.size() / 3;
    Published.Places("water: triangles", static_cast<double>(waterTriangles), "triangles");
    if (order.size() >= 3) {
      const int wetPart = ground.addPart("water", ringSurface);
      const bool tookWater =
          wetPart >= 0 &&
          ground.setPositions(wetPart, std::span<const float>(places.data(), places.size())) &&
          ground.setNormals(wetPart, std::span<const float>(facing.data(), facing.size())) &&
          ground.setTriangles(wetPart, std::span<const uint32_t>(order.data(), order.size())) &&
          ground.setTexture(wetPart, std::span<const float>(lidUv.data(), lidUv.size()), 0);
      Published.Places("water: the geometry took it", tookWater ? 1.0 : 0.0, "yes/no");
    }
  }

  Published.Places(
      "rebuild: of that, the streets and the water",
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - wiresAt).count(),
      "ms");
  Published.Places(
      "rebuild: and the buildings, streets and water took",
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - phaseAt).count(),
      "ms");
  phaseAt = std::chrono::steady_clock::now();
  const Render::Medium air = Render::kEarthAir;
  Material wearing;
  for (int channel = 0; channel < 3; ++channel) {
    wearing.BaseColour[channel] = air.GroundAlbedo[channel];
  }

  const size_t drivenParts = Picture.Standing->CarriedParts();
  Published.Places("restand: the carried count the world hands over",
                   static_cast<double>(drivenParts),
                   "carried");
  Published.Places("restand: parts in the geometry", static_cast<double>(ground.parts()), "parts");
  Published.Places(
      "rebuild: and assembling one subject took",
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - phaseAt).count(),
      "ms");
  phaseAt = std::chrono::steady_clock::now();
  Picture.Standing->GroundIs(ringSurface.index());
  if (classStructure && !classPalette.empty() &&
      !Picture.Standing->GroundClasses(classStructure->Words(),
                                       classStructure->Bytes() / sizeof(uint32_t),
                                       classPalette.data(),
                                       classPalette.size(),
                                       Error)) {
    return false;
  }
  Picture.Standing->Digests(declared.Render.Audits);
  {
    size_t handed = 0;
    for (int part = 0; part < ground.parts(); ++part) {
      handed += ground.trianglesOf(part).size() / 3u;
    }
    Published.Places(
        "the triangles handed to the renderer", static_cast<double>(handed), "triangles");
    Published.Places("in this many parts", static_cast<double>(ground.parts()), "parts");
  }
  if (!Picture.Standing->Restand(std::move(ground), drivenParts, wearing, Error)) { return false; }
  Published.Places(
      "rebuild: of that, walking it into the proxy", Picture.Standing->BuildMs(), "ms");
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
  {
    static const std::array<const char *const, 3> kSky = {"the ambient the sky casts, red",
                                                          "the ambient the sky casts, green",
                                                          "the ambient the sky casts, blue"};
    static const std::array<const char *const, 3> kGround = {
        "the ambient the ground bounces, red",
        "the ambient the ground bounces, green",
        "the ambient the ground bounces, blue"};
    for (size_t at = 0; at < 3; ++at) {
      Published.Places(kSky[at], Picture.Standing->AmbientStood()[at], "");
      Published.Places(kGround[at], Picture.Standing->GroundStood()[at], "");
    }
  }
  Published.Places("stand: times the sky was integrated",
                   static_cast<double>(Picture.Standing->SkyIntegrations()),
                   "integrations");
  Published.Places("stand: sweeping the bounds to frame it", Picture.Standing->FramingMs(), "ms");
  Published.Places("rebuild: resolving its surface", Picture.Standing->ResolveMs(), "ms");
  Published.Places("rebuild: and its bounds", Picture.Standing->BoundsMs(), "ms");
  Published.Places("rebuild: cutting it into clusters", Render::CookedMs(), "ms");
  Published.Places("cook: clusters with no parent above them",
                   static_cast<double>(Render::CookedRootless()),
                   "clusters");
  Published.Places(
      "cook: clusters in all", static_cast<double>(Render::CookedClusters()), "clusters");
  Published.Places("rebuild: of the streams, packing them", Render::PackedMs(), "ms");
  Published.Places(
      "restand: the geometry handed over, digested", Render::HandedGeometryDigest(), "");
  Published.Places("rebuild: digesting what it handed over", Render::DigestedMs(), "ms");
  Published.Places("rebuild: and the device taking them", Render::HandedMs(), "ms");
  Published.Places("rebuild: uploads the residency made",
                   static_cast<double>(Render::SubjectResidency::UploadsTaken()),
                   "uploads");
  Published.Places("rebuild: megabytes they carried",
                   static_cast<double>(Render::SubjectResidency::UploadMBTaken()),
                   "MB");
  Published.Places("rebuild: device buffers created",
                   static_cast<double>(Render::SubjectResidency::BuffersMadeTaken()),
                   "buffers");
  Published.Places("rebuild: staging buffers created",
                   static_cast<double>(Render::SubjectResidency::StagingMadeTaken()),
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
  World.GroundTiles = laid->Tiles;
  Published.Places("tiles the ring laid", static_cast<double>(laid->Tiles), "tiles");
  Published.Places("tiles it is still waiting for", static_cast<double>(laid->Pending), "tiles");
  Published.Places("tiles the stack does not hold", static_cast<double>(laid->Absent), "tiles");
  Published.Places("tiles it refused", static_cast<double>(laid->Refused), "tiles");
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
      "levels the cascade laid", static_cast<double>(over.Zoom - laid->CoarsestZoom + 1), "levels");
  Published.Places(
      "tiles it skipped as already covered", static_cast<double>(laid->Skipped), "tiles");
  Published.Places("tiles the last rebuild laid bare", static_cast<double>(laid->Bare), "tiles");
  World.Pending = laid->Pending;
  World.Bare = laid->Bare;
  World.Wanted = laid->Tiles;
  Published.Places(
      "tiles that overlap a finer level", static_cast<double>(laid->Overlapped), "tiles");
  return true;
}
} // namespace outshine
