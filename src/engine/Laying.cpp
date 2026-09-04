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
#include <cstdio>
#include <vector>

#include "Fit.h"
#include "ReferenceLine.h"

#include "spatial/Census.h"
#include "spatial/Drape.h"
#include "geo/PlaceKey.h"
#include "spatial/Refine.h"
#include "EngineHeld.h"
#include "GroundMesher.h"

namespace outshine {

namespace {

[[nodiscard]] uint64_t DigestOver(const Raised &built) {
  uint64_t mixed = kDigestBasis;
  const auto fold = [&mixed](uint64_t one) { mixed = (mixed ^ one) * kDigestPrime; };
  const auto foldVertex = [&fold](const StoredVertex &one) {
    const auto *const held = reinterpret_cast<const float *>(&one);
    for (size_t at = 0; at < kStoredVertexFloats; ++at) { fold(std::bit_cast<uint32_t>(held[at])); }
  };
  for (const StoredVertex &one : built.WallCorners) { foldVertex(one); }
  for (const StoredVertex &one : built.RoofCorners) { foldVertex(one); }
  for (const uint32_t one : built.WallRun) { fold(one); }
  for (const uint32_t one : built.RoofRun) { fold(one); }
  return mixed;
}

} // namespace

constexpr double kNoLeastYet = 1.0e9;

static_assert(Ground::kStreamGrid == 2 * (kPatchGrid - 1),
              "the elevation stream is opened ONE zoom below the finest tile so its posting equals "
              "the patchwork's vertex spacing; that holds only while a stream tile carries twice "
              "the intervals a patchwork tile lays, and if either grid moves the zoom must be "
              "re-derived rather than kept");

namespace {

constexpr double kLeastSineBetween = 1.0e-3;
constexpr double kLeastCapM = 0.01;
constexpr double kLeastSpanM = 0.05;
constexpr float kUnlitTint = 0.65f;
constexpr double kAdriftMostM = 500.0;

constexpr float kLagoonRed = 0.05f;
constexpr float kLagoonGreen = 0.11f;
constexpr float kLagoonBlue = 0.16f;
constexpr float kLagoonRoughness = 0.14f;

constexpr double kPerMille = 1000.0;
constexpr double kFootprintReachM = 3200.0;
constexpr double kSliverAreaM2 = 0.01;
constexpr double kSliverEdgeM = 5.0;
constexpr double kLongEdgeM = 20.0;
constexpr double kUnraisedDeckM = -1.0e29;
constexpr double kRoseLeast = 0.05;

constexpr float kWallRed = 0.74f;
constexpr float kWallGreen = 0.71f;
constexpr float kWallBlue = 0.65f;
constexpr float kWallRoughness = 0.88f;
constexpr float kTileRed = 0.42f;
constexpr float kTileGreen = 0.20f;
constexpr float kTileBlue = 0.14f;
constexpr float kTileRoughness = 0.72f;

constexpr double kRoadStepM = 16.0;
constexpr double kNodeSnapM = 2.0;
constexpr double kCrossCellM = 32.0;
constexpr double kMeetsWithinM = 10.0;
constexpr int kLevelPasses = 24;
constexpr double kLevelledM = 0.01;
constexpr int kRampPasses = 12;
constexpr int kChordPasses = 4;
constexpr double kChordWithinM = 0.20;
constexpr double kLeastCrestK = 10.0;
constexpr double kPadApronM = 6.0;
constexpr double kVergeM = 1.5;
constexpr double kBatterRun = 1.5;
constexpr double kLeastApronM = 3.0;
constexpr double kMostApronM = 240.0;
constexpr double kFinestGroundM = 3.0;
constexpr size_t kMostYieldTriangles = 24000;
constexpr double kTrimMostWidths = 4.0;
constexpr double kFitWithinM = 0.5;

constexpr double kStampWorthM = 0.25;
constexpr double kBrokenGroundM = 1.0;
constexpr double kFitTightestM = 5.5;
constexpr double kLeastRoadM = 2.0;

constexpr int kClassPasses = 0;

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

uint32_t Engine::State::HalvesEdge(Dividing over,
                                   const ClassStructure &classes,
                                   Halving across,
                                   std::vector<int> &classOf,
                                   std::vector<double> &atGeo) const {
  const Ground::VegetationTemplates &wearing = World.Stack.Vegetation();
  const Render::Medium fallback = Render::kEarthAir;
  Patchwork &laid = over.Laid;
  std::vector<float> &inFrame = over.InFrame;
  std::vector<float> &tinted = over.Tinted;
  std::vector<float> &classUv = over.Uv;
  const uint32_t a = across.From;
  const uint32_t b = across.To;
  const auto made = static_cast<uint32_t>(inFrame.size() / 3u);
  for (int axis = 0; axis < 3; ++axis) {
    inFrame.push_back(0.5f * (inFrame[static_cast<size_t>(a) * 3u + static_cast<size_t>(axis)] +
                              inFrame[static_cast<size_t>(b) * 3u + static_cast<size_t>(axis)]));
    laid.NormalM.push_back(0.5f *
                           (laid.NormalM[static_cast<size_t>(a) * 3u + static_cast<size_t>(axis)] +
                            laid.NormalM[static_cast<size_t>(b) * 3u + static_cast<size_t>(axis)]));
  }
  const double lat =
      0.5 * (atGeo[static_cast<size_t>(a) * 2u] + atGeo[static_cast<size_t>(b) * 2u]);
  const double lon =
      0.5 * (atGeo[static_cast<size_t>(a) * 2u + 1u] + atGeo[static_cast<size_t>(b) * 2u + 1u]);
  atGeo.push_back(lat);
  atGeo.push_back(lon);
  double edgeM = 0.0;
  int second = -1;
  const LongitudeLatitude at = {.LongitudeDeg = lon, .LatitudeDeg = lat};
  const int names = World.Stack.Classes().ClassAt(classes, at, &edgeM, &second);
  classOf.push_back(names);
  const EastNorth on = World.Stack.Classes().Project(at);
  classUv.push_back(static_cast<float>(on.EastM));
  classUv.push_back(static_cast<float>(on.NorthM));
  const bool named = names >= 0 && static_cast<size_t>(names) < wearing.TemplateCount();
  const Ground::VegetationTemplates::Row &wore =
      wearing.Rows()[named ? static_cast<size_t>(names) : 0];
  for (int channel = 0; channel < 3; ++channel) {
    tinted.push_back(named ? wore.Ground[channel]
                           : fallback.GroundAlbedo[static_cast<size_t>(channel)]);
  }
  tinted.push_back(1.0f);
  return made;
}

size_t Engine::State::DividesAtClassEdges(Dividing over,
                                          const ClassStructure &classes,
                                          std::vector<int> &classOf,
                                          std::vector<double> &atGeo) const {
  Patchwork &laid = over.Laid;
  size_t classDivided = 0;
  for (int pass = 0; pass < kClassPasses; ++pass) {
    std::unordered_map<uint64_t, uint32_t> split;
    for (size_t at = 0; at + 2 < laid.Index.size(); at += 3) {
      for (int edge = 0; edge < 3; ++edge) {
        const uint32_t a = laid.Index[at + static_cast<size_t>(edge)];
        const uint32_t b = laid.Index[at + static_cast<size_t>((edge + 1) % 3)];
        if (classOf[a] == classOf[b]) { continue; }
        split.emplace(EdgeKey(a, b), kNoVertex);
      }
    }
    if (split.empty()) { break; }
    const auto halve = [&](uint32_t a, uint32_t b) {
      const auto found = split.find(EdgeKey(a, b));
      if (found == split.end()) { return kNoVertex; }
      if (found->second != kNoVertex) { return found->second; }
      found->second = HalvesEdge(over, classes, {.From = a, .To = b}, classOf, atGeo);
      return found->second;
    };
    std::vector<uint32_t> finer;
    finer.reserve(laid.Index.size() * 2u);
    for (size_t at = 0; at + 2 < laid.Index.size(); at += 3) {
      const std::array<uint32_t, 3> face = {
          {laid.Index[at], laid.Index[at + 1u], laid.Index[at + 2u]}};
      const std::array<uint32_t, 3> cut = {
          {halve(face[0], face[1]), halve(face[1], face[2]), halve(face[2], face[0])}};
      Divide(face, cut, finer);
    }
    classDivided += (finer.size() - laid.Index.size()) / 3u;
    laid.Index.swap(finer);
  }
  return classDivided;
}

size_t Engine::State::NamesEveryVertex(Dividing over,
                                       const ClassStructure &classes,
                                       std::vector<int> &classOf,
                                       std::vector<double> &atGeo) const {
  const Ground::VegetationTemplates &wearing = World.Stack.Vegetation();
  const Render::Medium fallback = Render::kEarthAir;
  const Patchwork &laid = over.Laid;
  std::vector<float> &tinted_ = over.Tinted;
  std::vector<float> &classUv_ = over.Uv;
  size_t named = 0;
  for (size_t at = 0, one = 0; at + 2 < laid.PositionM.size(); at += 3, ++one) {
    const Vec3 held = {{laid.OriginEcef[0] + static_cast<double>(laid.PositionM[at]),
                        laid.OriginEcef[1] + static_cast<double>(laid.PositionM[at + 1]),
                        laid.OriginEcef[2] + static_cast<double>(laid.PositionM[at + 2])}};
    const Ground::Geo where =
        Ground::EcefToGeoWgs84(Ground::Ecef{.X = held[0], .Y = held[1], .Z = held[2]});
    double edgeM = 0.0;
    int second = -1;
    const LongitudeLatitude place = {.LongitudeDeg = where.LongitudeDeg,
                                     .LatitudeDeg = where.LatitudeDeg};
    const int which = World.Stack.Classes().ClassAt(classes, place, &edgeM, &second);
    {
      const EastNorth on = World.Stack.Classes().Project(place);
      classUv_[one * 2] = static_cast<float>(on.EastM);
      classUv_[one * 2 + 1] = static_cast<float>(on.NorthM);
    }
    const bool stands = which >= 0 && static_cast<size_t>(which) < wearing.TemplateCount();
    if (stands) { ++named; }
    const Ground::VegetationTemplates::Row &wore =
        wearing.Rows()[stands ? static_cast<size_t>(which) : 0];
    tinted_[one * 4] = stands ? wore.Ground[0] : fallback.GroundAlbedo[0];
    tinted_[one * 4 + 1] = stands ? wore.Ground[1] : fallback.GroundAlbedo[1];
    tinted_[one * 4 + 2] = stands ? wore.Ground[2] : fallback.GroundAlbedo[2];
    tinted_[one * 4 + 3] = 1.0f;
    classOf.push_back(which);
    atGeo.push_back(where.LatitudeDeg);
    atGeo.push_back(where.LongitudeDeg);
  }
  return named;
}

Engine::State::Classed Engine::State::Classify(Patchwork &laid, std::vector<float> &inFrame) {
  Classed out;
  const std::shared_ptr<const ClassStructure> classes = World.Stack.Classes().Read();
  const Ground::VegetationTemplates &wearing = World.Stack.Vegetation();
  std::vector<int> classOf;
  std::vector<double> atGeo;
  size_t classDivided = 0;
  const Render::Medium fallback = Render::kEarthAir;
  size_t named = 0;
  if (classes && wearing.Ready()) {
    out.Structure = classes;
    out.Palette = PaletteOver(wearing, fallback);
    out.Tinted.resize((inFrame.size() / 3) * 4);
    out.Uv.resize((inFrame.size() / 3) * 2);
    named = NamesEveryVertex({.Laid = laid, .InFrame = inFrame, .Tinted = out.Tinted, .Uv = out.Uv},
                             *classes,
                             classOf,
                             atGeo);
    classDivided =
        DividesAtClassEdges({.Laid = laid, .InFrame = inFrame, .Tinted = out.Tinted, .Uv = out.Uv},
                            *classes,
                            classOf,
                            atGeo);
    Published.Places("class field: triangles the boundary divided",
                     static_cast<double>(classDivided),
                     "triangles");
  }
  if (!out.Tinted.empty()) {
    Vec3 wornSum;
    const size_t worn = out.Tinted.size() / 4;
    for (size_t one = 0; one < worn; ++one) {
      for (int channel = 0; channel < 3; ++channel) {
        wornSum[channel] += static_cast<double>(out.Tinted[one * 4 + static_cast<size_t>(channel)]);
      }
    }
    const Vec3 wornMean = {{wornSum[0] / static_cast<double>(worn),
                            wornSum[1] / static_cast<double>(worn),
                            wornSum[2] / static_cast<double>(worn)}};
    Picture.Standing->Grounding(wornMean);
    Published.Places(
        "lighting: the ground it bounces off, red", kPerMille * wornMean[0], "albedo/1000");
    Published.Places("lighting: green", kPerMille * wornMean[1], "albedo/1000");
    Published.Places("lighting: blue", kPerMille * wornMean[2], "albedo/1000");
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
  Published.Places(
      "the ring's vertices a land class names", static_cast<double>(named), "vertices");
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
  Published.Places("out of, for a class", static_cast<double>(inFrame.size()) / 3.0, "vertices");
  return out;
}

void Engine::State::TellsHowTheRingSinks(std::span<const float> inFrame) {
  double least = kBeyondAnyCoordinate;
  double most = -kBeyondAnyCoordinate;
  size_t within = 0;
  size_t deepest = 0;
  double deepestOut = 0.0;
  for (size_t at = 0; at + 2 < inFrame.size(); at += 3) {
    const auto east = static_cast<double>(inFrame[at]);
    const auto south = static_cast<double>(inFrame[at + 2]);
    if (east * east + south * south > kFootprintReachM * kFootprintReachM) { continue; }
    const auto up = static_cast<double>(inFrame[at + 1]);
    if (up < least) {
      least = up;
      deepest = at / 3u;
      deepestOut = std::sqrt(east * east + south * south);
    }
    most = up > most ? up : most;
    ++within;
  }
  Published.Places("ground: the ring within 3.2 km runs from", within > 0 ? least : 0.0, "m up");
  Published.Places("ground: to", within > 0 ? most : 0.0, "m up");
  Published.Places("ground: over this many ring vertices", static_cast<double>(within), "vertices");
  Published.Places("ground: the deepest of those is vertex", static_cast<double>(deepest), "index");
  Published.Places("ground: and it lies this far out", deepestOut, "m");
  size_t sunken = 0;
  double sunkEastLeast = kBeyondAnyCoordinate;
  double sunkEastMost = -kBeyondAnyCoordinate;
  double sunkSouthLeast = kBeyondAnyCoordinate;
  double sunkSouthMost = -kBeyondAnyCoordinate;
  for (size_t at = 0; at + 2 < inFrame.size(); at += 3) {
    const auto east = static_cast<double>(inFrame[at]);
    const auto south = static_cast<double>(inFrame[at + 2]);
    if (east * east + south * south > kFootprintReachM * kFootprintReachM) { continue; }
    if (static_cast<double>(inFrame[at + 1]) >= -100.0) { continue; }
    ++sunken;
    sunkEastLeast = std::min(sunkEastLeast, east);
    sunkEastMost = std::max(sunkEastMost, east);
    sunkSouthLeast = std::min(sunkSouthLeast, south);
    sunkSouthMost = std::max(sunkSouthMost, south);
  }
  Published.Places(
      "ground: of those, how many sit below -100 m", static_cast<double>(sunken), "vertices");
  Published.Places(
      "ground: the sunken ones span, east from", sunken > 0 ? sunkEastLeast : 0.0, "m");
  Published.Places("ground: east to", sunken > 0 ? sunkEastMost : 0.0, "m");
  Published.Places("ground: south from", sunken > 0 ? sunkSouthLeast : 0.0, "m");
  Published.Places("ground: south to", sunken > 0 ? sunkSouthMost : 0.0, "m");
}

void Engine::State::TellsWhatTheGroundHolds(const TangentFrame &standing,
                                            std::span<const float> inFrame) {
  constexpr double kGroundCellM = 25.0;
  const Ground::BuildingField &prints = World.Stack.Footprints();
  const Raised &built = prints.Built();
  const Vec3 &anchor = prints.Anchor();
  double away = 0.0;
  for (int axis = 0; axis < 3; ++axis) {
    const double step = anchor[axis] - standing.OriginEcef()[axis];
    away += step * step;
  }
  Published.Places("buildings: their anchor lies from the frame's origin", std::sqrt(away), "m");
  Published.Places("buildings: floats in the soup",
                   static_cast<double>(built.WallCorners.size() + built.RoofCorners.size()),
                   "floats");
  Published.Places("buildings: the field's last delta began at",
                   static_cast<double>(prints.AddedFirst()),
                   "floats");
  Published.Places("buildings: and ran for", static_cast<double>(prints.AddedCount()), "floats");
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
  TellsHowTheRingSinks(inFrame);
}

void Engine::State::Models(const TangentFrame &standing,
                           std::span<const float> inFrame,
                           LongitudeLatitude stands,
                           Geometry &ground,
                           Phasing &clocks) {
  const Scenario::Document &declared = Session.Declared;
  const double anchorLat = stands.LatitudeDeg;
  const double anchorLon = stands.LongitudeDeg;
  const Ground::BuildingField &prints = World.Stack.Footprints();
  const Raised &built = prints.Built();
  const Vec3 &anchor = prints.Anchor();
  TellsWhatTheGroundHolds(standing, inFrame);
  const auto builtAt = std::chrono::steady_clock::now();
  if (built.WallRun.size() + built.RoofRun.size() >= 3) {
    if (World.CarriedFrom[0] != anchorLat || World.CarriedFrom[1] != anchorLon) {
      World.WallCarried = 0;
      World.RoofCarried = 0;
      World.CarriedFrom[0] = anchorLat;
      World.CarriedFrom[1] = anchorLon;
    }
    std::vector<float> &wallPlaces = World.WallPlaces;
    std::vector<float> &wallFacing = World.WallFacing;
    std::vector<float> &roofPlaces = World.RoofPlaces;
    std::vector<float> &roofFacing = World.RoofFacing;
    World.WallCarried = CarryIntoTheFrame(
        {.Corners = built.WallCorners, .AnchorEcefM = anchor, .Already = World.WallCarried},
        standing,
        {.PlacesM = wallPlaces, .Turned = wallFacing});
    World.RoofCarried = CarryIntoTheFrame(
        {.Corners = built.RoofCorners, .AnchorEcefM = anchor, .Already = World.RoofCarried},
        standing,
        {.PlacesM = roofPlaces, .Turned = roofFacing});
    const std::vector<uint32_t> &wallRun = built.WallRun;
    const std::vector<uint32_t> &roofRun = built.RoofRun;
    const size_t wallVerts = wallPlaces.size() / 3;
    const size_t wallTris = wallRun.size() / 3;
    const size_t vertices = wallVerts + roofPlaces.size() / 3;
    const size_t triangles = wallTris + roofRun.size() / 3;
    const auto placeAt = [&](size_t one) {
      return one < wallVerts ? wallPlaces.data() + one * 3
                             : roofPlaces.data() + (one - wallVerts) * 3;
    };
    const auto turnAt = [&](size_t one) {
      return one < wallVerts ? wallFacing.data() + one * 3
                             : roofFacing.data() + (one - wallVerts) * 3;
    };
    const auto cornerOf = [&](size_t tri, size_t corner) -> size_t {
      return tri < wallTris ? wallRun[tri * 3 + corner]
                            : wallVerts + roofRun[(tri - wallTris) * 3 + corner];
    };
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
    Published.Places(
        "rebuild: the ground ring took",
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - clocks.PhaseAt)
            .count(),
        "ms");
    clocks.PhaseAt = std::chrono::steady_clock::now();
    const int builtPart = ground.addPart("walls", wallSurface);
    const int roofPart = ground.addPart("roofs", roofSurface);
    Published.Places(
        "rebuild: of that, carrying both parts into the frame",
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - clocks.PhaseAt)
            .count(),
        "ms");
    clocks.CensusAt = std::chrono::steady_clock::now();
    if (declared.Render.Audits) {
      const std::array<Counted, 2> made = {
          {{.PlacesM = wallPlaces, .Facing = wallFacing, .Run = wallRun},
           {.PlacesM = roofPlaces, .Facing = roofFacing, .Run = roofRun}}};
      const Census counted = CensusOver(made);
      Published.Places("solid: building corners identical in POSITION AND NORMAL",
                       static_cast<double>(counted.Identical),
                       "corners");
      Published.Places("solid: and how many distinct ones remain",
                       static_cast<double>(counted.Distinct),
                       "corners");
      Published.Places("solid: building vertices welded away as coincident",
                       static_cast<double>(counted.Coincident),
                       "vertices");
      Published.Places("solid: building vertices standing apart",
                       static_cast<double>(counted.Apart),
                       "vertices");
      Published.Places("rebuild: of that, welding and counting edges",
                       std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() -
                                                                 clocks.CensusAt)
                           .count(),
                       "ms");
      clocks.CensusAt = std::chrono::steady_clock::now();
      Published.Places("solid: building triangles with two corners in one place",
                       static_cast<double>(counted.Degenerate),
                       "triangles");
      Published.Places("solid: building edges on ONE triangle, so a HOLE",
                       static_cast<double>(counted.Open),
                       "edges");
      Published.Places("solid: building edges on MORE than two, so not a surface",
                       static_cast<double>(counted.Overused),
                       "edges");
      Published.Places("solid: building edges in all", static_cast<double>(counted.Edges), "edges");
    }
    const size_t roofTriangles = roofRun.size() / 3;
    const size_t wallTriangles = wallRun.size() / 3;
    Published.Places("buildings: roof triangles", static_cast<double>(roofTriangles), "triangles");
    Published.Places("buildings: wall triangles", static_cast<double>(wallTriangles), "triangles");
    {
      size_t upright = 0;
      size_t facingDown = 0;
      for (size_t one = 0; one + 2 < wallFacing.size(); one += 3) {
        const auto aloft = static_cast<double>(wallFacing[one + 1]);
        if (aloft < -0.5) {
          ++facingDown;
        } else if (aloft > -0.5 && aloft < 0.5) {
          ++upright;
        }
      }
      Published.Places(
          "buildings: wall normals standing upright", static_cast<double>(upright), "normals");
      Published.Places(
          "buildings: wall normals facing DOWN", static_cast<double>(facingDown), "normals");
    }
    const bool tookPlaces =
        builtPart >= 0 && roofPart >= 0 &&
        ground.setPositions(builtPart,
                            std::span<const float>(wallPlaces.data(), wallPlaces.size())) &&
        ground.setPositions(roofPart, std::span<const float>(roofPlaces.data(), roofPlaces.size()));
    const bool tookFacing =
        tookPlaces &&
        ground.setNormals(builtPart,
                          std::span<const float>(wallFacing.data(), wallFacing.size())) &&
        ground.setNormals(roofPart, std::span<const float>(roofFacing.data(), roofFacing.size()));
    const bool tookRun =
        tookFacing &&
        ground.setTriangles(builtPart, std::span<const uint32_t>(wallRun.data(), wallRun.size())) &&
        ground.setTriangles(roofPart, std::span<const uint32_t>(roofRun.data(), roofRun.size()));
    Published.Places(
        "buildings: the part they were given", static_cast<double>(builtPart), "index");
    Published.Places(
        "buildings: the wall surface", static_cast<double>(wallSurface.index()), "index");
    Published.Places(
        "buildings: the roof surface", static_cast<double>(roofSurface.index()), "index");
    Published.Places("buildings: positions taken", tookPlaces ? 1.0 : 0.0, "yes/no");
    Published.Places("buildings: normals taken", tookFacing ? 1.0 : 0.0, "yes/no");
    Published.Places("buildings: triangles taken", tookRun ? 1.0 : 0.0, "yes/no");
    Published.Places(
        "buildings: parts the geometry holds", static_cast<double>(ground.parts()), "parts");
    Published.Places(
        "buildings: triangles this rebuild meshed", static_cast<double>(triangles), "triangles");
    Published.Places(
        "rebuild: of that, carrying the buildings into the frame",
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - builtAt)
            .count(),
        "ms");
    Published.Places("buildings: corners the soup holds", static_cast<double>(vertices), "corners");
    if (declared.Render.Audits) {
      double up = 0.0;
      double down = 0.0;
      double sideways = 0.0;
      double unlengthed = 0.0;
      const double inward = 0.0;
      for (size_t at = 0; at < vertices; ++at) {
        const float *const aim = turnAt(at);
        const double x = aim[0];
        const double y = aim[1];
        const double z = aim[2];
        const double length = std::sqrt(x * x + y * y + z * z);
        if (!(length > 0.5)) {
          unlengthed += 1.0;
          continue;
        }
        const double aloft = y / length;
        if (aloft > 0.5) {
          up += 1.0;
        } else if (aloft < -0.5) {
          down += 1.0;
        } else {
          sideways += 1.0;
        }
      }
      Published.Places("buildings: normals pointing up", up, "normals");
      Published.Places("buildings: normals pointing DOWN", down, "normals");
      Published.Places("buildings: normals lying sideways", sideways, "normals");
      Published.Places("buildings: normals with no length", unlengthed, "normals");
      Published.Places("buildings: normals in all", static_cast<double>(vertices), "normals");
      (void)inward;
    }
    if (declared.Render.Audits) {
      size_t needles = 0;
      size_t reaching = 0;
      double longest = 0.0;
      double furthest = 0.0;
      for (size_t tri = 0; tri < triangles; ++tri) {
        const float *const a = placeAt(cornerOf(tri, 0));
        const float *const b = placeAt(cornerOf(tri, 1));
        const float *const c = placeAt(cornerOf(tri, 2));
        const double ux = b[0] - a[0];
        const double uy = b[1] - a[1];
        const double uz = b[2] - a[2];
        const double vx = c[0] - a[0];
        const double vy = c[1] - a[1];
        const double vz = c[2] - a[2];
        const double wx = c[0] - b[0];
        const double wy = c[1] - b[1];
        const double wz = c[2] - b[2];
        const double nx = uy * vz - uz * vy;
        const double ny = uz * vx - ux * vz;
        const double nz = ux * vy - uy * vx;
        const double area = 0.5 * std::sqrt(nx * nx + ny * ny + nz * nz);
        const double edge = std::sqrt(std::max({ux * ux + uy * uy + uz * uz,
                                                vx * vx + vy * vy + vz * vz,
                                                wx * wx + wy * wy + wz * wz}));
        if (area < kSliverAreaM2 && edge > kSliverEdgeM) {
          ++needles;
          longest = edge > longest ? edge : longest;
        }
        if (edge > kLongEdgeM) {
          ++reaching;
          furthest = edge > furthest ? edge : furthest;
        }
      }
      Published.Places(
          "buildings: triangles that are needles", static_cast<double>(needles), "triangles");
      Published.Places("buildings: the longest edge one carries", longest, "m");
      Published.Places(
          "buildings: triangles reaching over 20 m", static_cast<double>(reaching), "triangles");
      Published.Places("buildings: the furthest any reaches", furthest, "m");
    }
    if (declared.Render.Audits) {
      double least = kBeyondAnyCoordinate;
      double most = -kBeyondAnyCoordinate;
      double nearest = kBeyondAnyCoordinate;
      double farthest = 0.0;
      for (size_t at = 0; at < vertices; ++at) {
        const float *const held = placeAt(at);
        const auto up = static_cast<double>(held[1]);
        const auto east = static_cast<double>(held[0]);
        const auto south = static_cast<double>(held[2]);
        const double away = std::sqrt(east * east + south * south);
        least = up < least ? up : least;
        most = up > most ? up : most;
        nearest = away < nearest ? away : nearest;
        farthest = away > farthest ? away : farthest;
      }
      Published.Places("buildings stand between", least, "m up");
      Published.Places("and", most, "m up");
      Published.Places("their nearest vertex lies", nearest, "m out");
      Published.Places("their farthest", farthest, "m out");
    }
  } else {
    Published.Places("buildings: triangles this rebuild meshed", 0.0, "triangles");
  }
}

double Engine::State::LeastSeen(double held, double seen) {
  if (!(seen > 0.0)) { return held; }
  return held > 0.0 ? std::min(held, seen) : seen;
}

void Engine::State::NotesFit(Paved &into, const Fitted &got) {
  ++into.FitLaid;
  ++into.FitsMeasured;
  into.FitRadiusTightestM = LeastSeen(into.FitRadiusTightestM, got.TightestRadiusM);
}

void Engine::State::FitAlongLane(Paved &into) {
  size_t from = 0;
  size_t cuts = 0;
  bool wholeWay = true;
  while (from * 2u + 6u <= into.FitEastNorth.size()) {
    ReferenceLine fitted;
    const Fitted got = Fit(std::span<const double>(into.FitEastNorth.data() + from * 2u,
                                                   into.FitEastNorth.size() - from * 2u),
                           kFitWithinM,
                           kFitTightestM,
                           fitted);
    if (got.Laid) {
      NotesFit(into, got);
      into.FitUndrivable += got.Undrivable;
      break;
    }
    if (got.Undrivable == 0 || got.TightestDemandedAtVertex == 0) {
      if (wholeWay) { ++into.FitRefused; }
      ++into.FitUnsplittable;
      break;
    }

    const size_t upTo = got.TightestDemandedAtVertex;
    if (upTo + 1u >= 3u) {
      ReferenceLine piece;
      const Fitted head =
          Fit(std::span<const double>(into.FitEastNorth.data() + from * 2u, (upTo + 1u) * 2u),
              kFitWithinM,
              kFitTightestM,
              piece);
      if (head.Laid) {
        NotesFit(into, head);
      } else {
        ++into.FitUnsplittable;
      }
    }
    ++into.FitTooTight;
    ++cuts;
    into.TightestDemandM = LeastSeen(into.TightestDemandM, got.TightestDemandedM);
    from += upTo + 1u;
    wholeWay = false;
  }
  into.FitCuts += cuts;
}

void Engine::State::TrimLaneEnds(size_t laneAt, Paved &into) {
  const std::vector<double> reached = ReachedAlong(into.Along);
  const double wholeM = reached.back();
  const double fromM = into.TrimM[laneAt * 2u];
  const double toM = wholeM - into.TrimM[laneAt * 2u + 1u];
  if (toM - fromM < kLeastRoadM || (fromM <= kLeastCapM && toM >= wholeM - kLeastCapM)) { return; }

  const auto standAt = [&](double alongM) {
    size_t at = 1;
    while (at + 1 < reached.size() && reached[at] < alongM) { ++at; }
    const double span = reached[at] - reached[at - 1];
    const double part = span > kLeastTurnRad ? (alongM - reached[at - 1]) / span : 0.0;
    const RoadStation &from = into.Along[at - 1];
    const RoadStation &to = into.Along[at];
    return RoadStation{.EastM = from.EastM + (to.EastM - from.EastM) * part,
                       .SouthM = from.SouthM + (to.SouthM - from.SouthM) * part,
                       .GradeM = from.GradeM + (to.GradeM - from.GradeM) * part};
  };
  std::vector<RoadStation> kept;
  kept.push_back(standAt(fromM));
  for (size_t at = 0; at < into.Along.size(); ++at) {
    if (reached[at] > fromM && reached[at] < toM) { kept.push_back(into.Along[at]); }
  }
  kept.push_back(standAt(toM));
  into.Along.swap(kept);
}

void Engine::State::FitLane(size_t laneAt, Paved &into) {
  into.FitEastNorth.clear();
  into.FitEastNorth.reserve(into.Along.size() * 2u);
  for (const RoadStation &one : into.Along) {
    into.FitEastNorth.push_back(one.EastM);
    into.FitEastNorth.push_back(-one.SouthM);
  }
  FitAlongLane(into);
  TrimLaneEnds(laneAt, into);
}

void Engine::State::RefineChords(const Paving &on, Paved &into) {
  for (int pass = 0; pass < kChordPasses; ++pass) {
    size_t added = 0;
    into.Finer.clear();
    into.Finer.reserve(into.Along.size() * 2u);
    for (size_t at = 1; at < into.Along.size(); ++at) {
      into.Finer.push_back(into.Along[at - 1u]);
      const double midE = 0.5 * (into.Along[at - 1u].EastM + into.Along[at].EastM);
      const double midS = 0.5 * (into.Along[at - 1u].SouthM + into.Along[at].SouthM);
      const double chord = 0.5 * (into.Along[at - 1u].GradeM + into.Along[at].GradeM);
      const double overM = on.Draped.At({.EastM = midE, .SouthM = midS}, chord);
      if (std::fabs(overM - chord) <= kChordWithinM) { continue; }
      into.Finer.push_back(RoadStation{.EastM = midE, .SouthM = midS, .GradeM = overM});
      ++added;
    }
    into.Finer.push_back(into.Along.back());
    into.Along.swap(into.Finer);
    into.ChordAdded += added;
    if (added == 0) { break; }
  }
}

size_t Engine::State::StepsAcross(const Paving &on, Spanning between) {
  const double perLon = kMPerDegLon * std::cos(on.Points[between.Here] * kDeg2Rad);
  const double spanE = (on.Points[between.Next + 1] - on.Points[between.Here + 1]) * perLon;
  const double spanN = (on.Points[between.Next] - on.Points[between.Here]) * kMPerDegLat;
  return static_cast<size_t>(1.0 + std::sqrt(spanE * spanE + spanN * spanN) / kRoadStepM);
}

uint64_t Engine::State::SharedNodeAt(const Paving &on, double latDeg, double lonDeg) {
  const uint64_t key = PlaceKey({.LongitudeDeg = lonDeg, .LatitudeDeg = latDeg});
  const auto seen = on.SharedNodes.find(key);
  return seen != on.SharedNodes.end() && seen->second > 1u ? key : 0u;
}

bool Engine::State::StationsAlong(const Paving &on,
                                  const Ground::StreetField::Way &lane,
                                  const std::function<bool(LongitudeLatitude, uint64_t)> &station) {
  for (uint32_t step = 0; step + 1 < lane.PointCount; ++step) {
    const size_t here = (static_cast<size_t>(lane.FirstPoint) + step) * 2;
    const size_t next = here + 2;
    if (next + 1 >= on.Points.size()) { return false; }
    const size_t pieces = StepsAcross(on, {.Here = here, .Next = next});
    for (size_t piece = 0; piece < pieces; ++piece) {
      const double at = static_cast<double>(piece) / static_cast<double>(pieces);
      const double onLat = on.Points[here] + (on.Points[next] - on.Points[here]) * at;
      const double onLon = on.Points[here + 1] + (on.Points[next + 1] - on.Points[here + 1]) * at;
      if (!station({.LongitudeDeg = onLon, .LatitudeDeg = onLat},
                   piece == 0 ? SharedNodeAt(on, onLat, onLon) : 0u)) {
        return false;
      }
    }
  }
  const size_t last = (static_cast<size_t>(lane.FirstPoint) + lane.PointCount - 1u) * 2;
  return last + 1 < on.Points.size() &&
         station({.LongitudeDeg = on.Points[last + 1], .LatitudeDeg = on.Points[last]},
                 SharedNodeAt(on, on.Points[last], on.Points[last + 1]));
}

void Engine::State::DesignLane(const Paving &on,
                               const Ground::StreetField::Way &lane,
                               size_t laneAt,
                               Paved &into) const {
  into.Along.clear();
  bool whole = true;
  const auto station = [&](LongitudeLatitude over, uint64_t node) {
    const std::optional<Grounded> stood = GroundUnder(on.Standing, on.Draped, over);
    if (!stood) { return false; }
    into.Along.push_back(
        {.EastM = stood->EastM, .SouthM = stood->SouthM, .GradeM = stood->GradeM, .Node = node});
    return true;
  };
  whole = StationsAlong(on, lane, station);
  if (!whole || into.Along.size() < 2) {
    ++into.RefusedWays;
    return;
  }
  RefineChords(on, into);

  if (lane.MaxGradient > 0.0f) {
    World.Shipping.Paving().Design(std::span<RoadStation>(into.Along.data(), into.Along.size()),
                                   static_cast<double>(lane.MaxGradient),
                                   kLeastCrestK);
  }
  for (RoadStation &one : into.Along) {
    if (one.Node != 0u || into.AtCrossing.empty()) { continue; }
    const auto east = static_cast<int64_t>(std::floor(one.EastM / kCrossCellM));
    const auto south = static_cast<int64_t>(std::floor(one.SouthM / kCrossCellM));
    const auto atE = static_cast<uint64_t>(east + 0x20000000LL);
    const auto atS = static_cast<uint64_t>(south + 0x20000000LL);
    const auto near = into.AtCrossing.find((atE << 32U) | atS);
    if (near == into.AtCrossing.end()) { continue; }
    for (const auto &met : near->second) {
      const double offE = one.EastM - met.EastM;
      const double offS = one.SouthM - met.SouthM;
      if (offE * offE + offS * offS <= kMeetsWithinM * kMeetsWithinM) {
        one.Node = met.Named;
        break;
      }
    }
  }
  into.Designed[laneAt] = into.Along;
}

double Engine::State::StepAlongM(std::span<const RoadStation> along, size_t at) {
  if (at == 0 || at >= along.size()) { return 0.0; }
  const double spanE = along[at].EastM - along[at - 1].EastM;
  const double spanS = along[at].SouthM - along[at - 1].SouthM;
  return std::sqrt(spanE * spanE + spanS * spanS);
}

std::vector<double> Engine::State::ReachedAlong(std::span<const RoadStation> along) {
  std::vector<double> reached(along.size(), 0.0);
  for (size_t at = 1; at < along.size(); ++at) {
    reached[at] = reached[at - 1] + StepAlongM(along, at);
  }
  return reached;
}

void Engine::State::MarksWaterCrossing(const Paving &on, size_t laneAt, Paved &into) const {
  double overWaterM = 0.0;
  for (size_t at = 1; at < into.Along.size(); ++at) {
    double lat = 0.0;
    double lon = 0.0;
    const double midE = 0.5 * (into.Along[at - 1].EastM + into.Along[at].EastM);
    const double midS = 0.5 * (into.Along[at - 1].SouthM + into.Along[at].SouthM);
    const LongitudeLatitude midAt = on.Standing.Geo({.EastM = midE, .NorthM = -midS});
    lat = midAt.LatitudeDeg;
    lon = midAt.LongitudeDeg;
    double edgeM = 0.0;
    int second = -1;
    const int which = World.Stack.Classes().ClassAt(
        *on.Classes, {.LongitudeDeg = lon, .LatitudeDeg = lat}, &edgeM, &second);
    ++into.AskedOverBridge;
    if (which < 0 || static_cast<size_t>(which) >= World.Stack.Vegetation().TemplateCount()) {
      continue;
    }
    ++into.NamedOverBridge;
    if (World.Stack.Vegetation().Rows()[static_cast<size_t>(which)].GroundClass != on.WaterRow) {
      continue;
    }
    ++into.WetOverBridge;
    overWaterM += StepAlongM(into.Along, at);
  }
  if (overWaterM > 0.0) {
    double clear = 0.0;
    for (const Ground::VegetationTemplates::WaterBand &band :
         World.Stack.Vegetation().WaterBands()) {
      clear = static_cast<double>(band.ClearanceM);
      if (overWaterM <= static_cast<double>(band.RunM)) { break; }
    }
    if (clear > 0.0) {
      double stood = -kBeyondAnyCoordinate;
      for (const RoadStation &one : into.Along) { stood = std::max(stood, one.GradeM); }
      into.DeckM[laneAt] = std::max(into.DeckM[laneAt], stood + clear);
      ++into.DecksOverWater;
      into.MostOverWaterM = std::max(into.MostOverWaterM, clear);
    }
  }
}

void Engine::State::LevelsDeckOrApproach(const Paving &on,
                                         const Ground::StreetField::Way &lane,
                                         size_t laneAt,
                                         Paved &into) {
  if (lane.Bridge) {
    double deck = into.DeckM[laneAt];
    for (const RoadStation &one : into.Along) { deck = std::max(deck, one.GradeM); }
    for (RoadStation &one : into.Along) { one.GradeM = deck; }
  } else if (!into.EndM.empty()) {
    const size_t first = static_cast<size_t>(lane.FirstPoint) * 2;
    const size_t last = first + (static_cast<size_t>(lane.PointCount) - 1u) * 2;
    if (last + 1 < on.Points.size()) {
      const auto from = into.EndM.find(
          PlaceKey({.LongitudeDeg = on.Points[first + 1], .LatitudeDeg = on.Points[first]}));
      const auto to = into.EndM.find(
          PlaceKey({.LongitudeDeg = on.Points[last + 1], .LatitudeDeg = on.Points[last]}));
      if (from != into.EndM.end() && to != into.EndM.end()) {
        const std::vector<double> reached = ReachedAlong(into.Along);
        const double runM = reached.back();
        for (size_t at = 0; at < into.Along.size(); ++at) {
          const double along01 = runM > kLeastRunM ? reached[at] / runM : 0.0;
          const double wanted = from->second + (to->second - from->second) * along01;
          into.Along[at].GradeM = std::max(into.Along[at].GradeM, wanted);
        }
      }
    }
  }
}

void Engine::State::PaveLane(const Paving &on,
                             Pass pass,
                             size_t laneAt,
                             Paved &into,
                             std::vector<Yields> &corridor,
                             RoadRaised &pavement) const {
  const Ground::StreetField::Way &lane = on.Ways.Ways()[laneAt];
  if (lane.Form != Ground::StreetField::Shape::Ribbon || lane.PointCount < 2 ||
      !(lane.HalfWidthM > 0.0f)) {
    into.RefusedWays += pass == Pass::Designing ? 1u : 0u;
    return;
  }
  if (pass == Pass::Paving) {
    into.Along = into.Designed[laneAt];
    if (into.Along.size() < 2) { return; }
  } else {
    DesignLane(on, lane, laneAt, into);
    return;
  }

  auto tookFrom = std::chrono::steady_clock::now();
  const auto since = [&tookFrom] {
    const auto was = tookFrom;
    tookFrom = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(tookFrom - was).count();
  };
  {
    const Heap::Tagged fitting("road-fit");
    FitLane(laneAt, into);
  }
  into.FitMs += since();
  if (into.Along.size() < 2) {
    ++into.RefusedWays;
    return;
  }
  if (lane.Bridge && on.WaterRow >= 0 && on.Classes) { MarksWaterCrossing(on, laneAt, into); }
  LevelsDeckOrApproach(on, lane, laneAt, into);
  into.LaidWays += lane.Bridge ? 1u : 0u;
  into.GroundWays += lane.Bridge ? 0u : 1u;
  const bool sealed =
      lane.CoverRow >= 0 &&
      static_cast<size_t>(lane.CoverRow) < World.Stack.Vegetation().TemplateCount() &&
      World.Stack.Vegetation().Rows()[static_cast<size_t>(lane.CoverRow)].Mix[2] >= 1.0f;
  RoadProfile profile = RoadProfile::Rounded;
  if (sealed) { profile = lane.Lanes >= 2 ? RoadProfile::Kerbed : RoadProfile::Simple; }
  Vec3f wears = {{0.5f, 0.5f, 0.5f}};
  if (lane.CoverRow >= 0 &&
      static_cast<size_t>(lane.CoverRow) < World.Stack.Vegetation().TemplateCount()) {
    const Vec4f &cover = World.Stack.Vegetation().Rows()[static_cast<size_t>(lane.CoverRow)].Ground;
    wears = {{cover[0], cover[1], cover[2]}};
  }
  into.WaterMs += since();
  if (lane.Bridge) {
    into.Swept += World.Shipping.Paving().Sweep(
        std::span<const RoadStation>(into.Along.data(), into.Along.size()),
        {.HalfWidthM = static_cast<double>(lane.HalfWidthM),
         .Profile = profile,
         .WearsLinear = wears,
         .Crossfall = std::atan(kCrossfall)},
        pavement);
  }
  into.SweepMs += since();
  for (size_t at = 1; at < into.Along.size(); ++at) {
    const double runE = into.Along[at].EastM - into.Along[at - 1u].EastM;
    const double runS = into.Along[at].SouthM - into.Along[at - 1u].SouthM;
    const double runM = std::sqrt(runE * runE + runS * runS);
    if (!(runM > kLeastSpanM)) { continue; }
    const double groundAt = on.Draped.At(
        {.EastM = into.Along[at].EastM, .SouthM = -into.Along[at].SouthM}, into.Along[at].GradeM);
    const double groundBefore =
        on.Draped.At({.EastM = into.Along[at - 1u].EastM, .SouthM = -into.Along[at - 1u].SouthM},
                     into.Along[at - 1u].GradeM);
    const double yieldM = std::max(std::fabs(into.Along[at].GradeM - groundAt),
                                   std::fabs(into.Along[at - 1u].GradeM - groundBefore));
    const double outE = -runS / runM;
    const double outS = runE / runM;
    const double half = static_cast<double>(lane.HalfWidthM) + kVergeM;
    double reliefM = std::fabs(groundAt - groundBefore);
    for (const double hand : {1.0, -1.0}) {
      for (int end = 0; end < 2; ++end) {
        const RoadStation &one = end == 0 ? into.Along[at - 1u] : into.Along[at];
        const double sideE = one.EastM + outE * half * hand;
        const double sideS = one.SouthM + outS * half * hand;
        reliefM = std::max(
            reliefM, on.Draped.At({.EastM = sideE, .SouthM = -sideS}, one.GradeM) - one.GradeM);
      }
    }
    if (yieldM < kStampWorthM && reliefM < kBrokenGroundM) { continue; }
    Yields made;
    made.RingEastSouthM = {into.Along[at - 1u].EastM + outE * half,
                           into.Along[at - 1u].SouthM + outS * half,
                           into.Along[at].EastM + outE * half,
                           into.Along[at].SouthM + outS * half,
                           into.Along[at].EastM - outE * half,
                           into.Along[at].SouthM - outS * half,
                           into.Along[at - 1u].EastM - outE * half,
                           into.Along[at - 1u].SouthM - outS * half};
    made.LowE = made.HighE = made.RingEastSouthM[0];
    made.LowS = made.HighS = made.RingEastSouthM[1];
    for (size_t k = 2; k + 1 < made.RingEastSouthM.size(); k += 2) {
      made.LowE = std::min(made.LowE, made.RingEastSouthM[k]);
      made.HighE = std::max(made.HighE, made.RingEastSouthM[k]);
      made.LowS = std::min(made.LowS, made.RingEastSouthM[k + 1]);
      made.HighS = std::max(made.HighS, made.RingEastSouthM[k + 1]);
    }
    made.AtE = into.Along[at - 1u].EastM;
    made.AtS = into.Along[at - 1u].SouthM;
    made.PlateauM = into.Along[at - 1u].GradeM;
    const double rise = (into.Along[at].GradeM - into.Along[at - 1u].GradeM) / runM;
    made.SlopeE = rise * runE / runM;
    made.SlopeS = rise * runS / runM;
    made.ApronM = std::clamp(kBatterRun * yieldM, kLeastApronM, kMostApronM);
    made.YieldM = yieldM;
    const bool rests = !lane.Bridge || at == 1u || at + 1u == into.Along.size();
    if (rests) {
      made.SeamEastSouthM = {
          into.Along[at - 1u].EastM + outE * static_cast<double>(lane.HalfWidthM),
          into.Along[at - 1u].SouthM + outS * static_cast<double>(lane.HalfWidthM),
          into.Along[at].EastM + outE * static_cast<double>(lane.HalfWidthM),
          into.Along[at].SouthM + outS * static_cast<double>(lane.HalfWidthM),
          into.Along[at].EastM - outE * static_cast<double>(lane.HalfWidthM),
          into.Along[at].SouthM - outS * static_cast<double>(lane.HalfWidthM),
          into.Along[at - 1u].EastM - outE * static_cast<double>(lane.HalfWidthM),
          into.Along[at - 1u].SouthM - outS * static_cast<double>(lane.HalfWidthM)};
    }
    made.Fills = !lane.Bridge;
    corridor.push_back(std::move(made));
  }
  if (!lane.Bridge && into.Along.size() > 3) {
    const size_t shutFrom = static_cast<size_t>(lane.FirstPoint) * 2u;
    const size_t shutTo = shutFrom + (static_cast<size_t>(lane.PointCount) - 1u) * 2u;
    const bool shut = shutTo + 1 < on.Points.size() &&
                      std::fabs(on.Points[shutFrom] - on.Points[shutTo]) < 1.0e-7 &&
                      std::fabs(on.Points[shutFrom + 1] - on.Points[shutTo + 1]) < 1.0e-7;
    if (shut) {
      Yields island;
      island.RingEastSouthM.reserve(into.Along.size() * 2u);
      island.LowE = island.HighE = into.Along.front().EastM;
      island.LowS = island.HighS = into.Along.front().SouthM;
      double summed = 0.0;
      for (const RoadStation &one : into.Along) {
        island.RingEastSouthM.push_back(one.EastM);
        island.RingEastSouthM.push_back(one.SouthM);
        island.LowE = std::min(island.LowE, one.EastM);
        island.HighE = std::max(island.HighE, one.EastM);
        island.LowS = std::min(island.LowS, one.SouthM);
        island.HighS = std::max(island.HighS, one.SouthM);
        summed += one.GradeM;
      }
      island.AtE = into.Along.front().EastM;
      island.AtS = into.Along.front().SouthM;
      island.PlateauM = summed / static_cast<double>(into.Along.size());
      island.ApronM = kLeastApronM;
      island.YieldM = kBrokenGroundM;
      island.Fills = true;
      corridor.push_back(std::move(island));
    }
  }
  {
    const size_t first = static_cast<size_t>(lane.FirstPoint) * 2u;
    const size_t last = first + (static_cast<size_t>(lane.PointCount) - 1u) * 2u;
    if (last + 1 < on.Points.size()) {
      const std::array<uint64_t, 2> key = {
          {PlaceKey({.LongitudeDeg = on.Points[first + 1], .LatitudeDeg = on.Points[first]}),
           PlaceKey({.LongitudeDeg = on.Points[last + 1], .LatitudeDeg = on.Points[last]})}};
      for (int side = 0; side < 2; ++side) {
        const RoadStation &at = side == 0 ? into.Along.front() : into.Along.back();
        const RoadStation &to = side == 0 ? into.Along[1] : into.Along[into.Along.size() - 2u];
        double outE = to.EastM - at.EastM;
        double outS = to.SouthM - at.SouthM;
        const double run = std::sqrt(outE * outE + outS * outS);
        if (!(run > kLeastRunM)) { continue; }
        outE /= run;
        outS /= run;
        into.Gates[key[side]].push_back(
            RoadGate{.EastM = at.EastM,
                     .SouthM = at.SouthM,
                     .GradeM = at.GradeM,
                     .OutE = outE,
                     .OutS = outS,
                     .HalfWidthM = static_cast<double>(lane.HalfWidthM)});
      }
    }
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
  const bool grew = alsoWhenTilesLanded && resident != World.LaidResident;
  const bool renamed = classes != World.LaidClasses;
  {
    const Raised &standing = World.Stack.Footprints().Built();
    const size_t triangles = (standing.WallRun.size() + standing.RoofRun.size()) / 3u;
    Published.Places(
        "building triangles the world meshed", static_cast<double>(triangles), "triangles");
  }
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
  Published.Places("world: and the raised geometry",
                   static_cast<double>(World.Stack.Footprints().RaisedBytes()),
                   "bytes");
  Published.Places("world: of that, the bytes it actually fills",
                   static_cast<double>(World.Stack.Footprints().RaisedUsedBytes()),
                   "bytes");
  Published.Places(
      "world: the frame copies the renderer reads",
      static_cast<double>(CapacityBytes(World.WallPlaces) + CapacityBytes(World.WallFacing) +
                          CapacityBytes(World.RoofPlaces) + CapacityBytes(World.RoofFacing)),
      "bytes");
  Published.Places("world: of those copies, the bytes actually filled",
                   static_cast<double>((World.WallPlaces.size() + World.WallFacing.size() +
                                        World.RoofPlaces.size() + World.RoofFacing.size()) *
                                       sizeof(float)),
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
    const uint64_t geometry = DigestOver(World.Stack.Footprints().Built());
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

void Engine::State::LayLanesIntoNetwork(const Ground::StreetField &ways,
                                        std::span<const double> points,
                                        Path::Network &net,
                                        std::vector<size_t> &netToLane) {
  netToLane.reserve(ways.Ways().size());
  for (size_t at = 0; at < ways.Ways().size(); ++at) {
    const Ground::StreetField::Way &lane = ways.Ways()[at];
    if (lane.Form != Ground::StreetField::Shape::Ribbon || lane.PointCount < 2) { continue; }
    const size_t first = static_cast<size_t>(lane.FirstPoint) * 2;
    if (first + static_cast<size_t>(lane.PointCount) * 2 > points.size()) { continue; }
    net.Lay(points.subspan(first, static_cast<size_t>(lane.PointCount) * 2),
            Path::WayClass{.HalfWidthM = static_cast<double>(lane.HalfWidthM),
                           .MaxGradient = 0.0,
                           .MinRadiusM = 0.0,
                           .Friction = 0.0,
                           .Lanes = lane.Lanes,
                           .Spans = lane.Bridge});
    netToLane.push_back(at);
  }
}

void Engine::State::FileCrossing(const Path::Network::Crossing &one,
                                 const TangentFrame &standing,
                                 Paved &into) {
  const EastNorthUp crossedAt = standing.Place(
      {.LongitudeDeg = one.LongitudeDeg, .LatitudeDeg = one.LatitudeDeg, .HeightM = 0.0});
  const uint64_t named =
      PlaceKey({.LongitudeDeg = one.LongitudeDeg, .LatitudeDeg = one.LatitudeDeg}) | 1ULL;
  const auto east = static_cast<int64_t>(std::floor(crossedAt.EastM / kCrossCellM));
  const auto south = static_cast<int64_t>(std::floor(-crossedAt.NorthM / kCrossCellM));
  for (int64_t stepE = -1; stepE <= 1; ++stepE) {
    for (int64_t stepS = -1; stepS <= 1; ++stepS) {
      const auto atE = static_cast<uint64_t>(east + stepE + 0x20000000LL);
      const auto atS = static_cast<uint64_t>(south + stepS + 0x20000000LL);
      into.AtCrossing[(atE << 32U) | atS].push_back(
          Meets{.EastM = crossedAt.EastM, .SouthM = -crossedAt.NorthM, .Named = named});
    }
  }
}

void Engine::State::RaiseDeckOver(const Path::Network::Crossing &one,
                                  const Ground::StreetField &ways,
                                  std::span<const size_t> netToLane,
                                  const TangentFrame &standing,
                                  const Drape &drapedOver,
                                  Paved &into) const {
  if (one.OverWay >= netToLane.size() || one.UnderWay >= netToLane.size()) { return; }
  const size_t a = netToLane[one.OverWay];
  const size_t b = netToLane[one.UnderWay];
  const Ground::StreetField::Way &first = ways.Ways()[a];
  const Ground::StreetField::Way &second = ways.Ways()[b];
  if (first.Bridge == second.Bridge) { return; }
  const size_t spans = first.Bridge ? a : b;
  const Ground::StreetField::Way &below = first.Bridge ? second : first;
  const std::optional<double> stood =
      World.Stack.Ground()
          .At({.LongitudeDeg = one.LongitudeDeg, .LatitudeDeg = one.LatitudeDeg})
          .AslM();
  if (!stood) { return; }
  const EastNorthUp at = standing.Place(
      {.LongitudeDeg = one.LongitudeDeg, .LatitudeDeg = one.LatitudeDeg, .HeightM = *stood});
  const double onDrawn = drapedOver.At({.EastM = at.EastM, .SouthM = -at.NorthM}, at.UpM);
  const double need = onDrawn + static_cast<double>(below.ClearanceM);
  if (need <= into.DeckM[spans]) { return; }
  if (into.DeckM[spans] < kUnraisedDeckM) { ++into.DecksRaised; }
  into.DeckM[spans] = need;
  into.MostRaisedM = std::max(into.MostRaisedM, need - onDrawn);
}

void Engine::State::Crosses(const Ground::StreetField &ways,
                            const Ground::OsmField &vectors,
                            const TangentFrame &standing,
                            const Drape &drapedOver,
                            Paved &into) const {
  auto partAt = std::chrono::steady_clock::now();
  const auto part = [&partAt] {
    const auto was = partAt;
    partAt = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(partAt - was).count();
  };

  Path::Network net(Path::Snap{.CellM = kNodeSnapM}, Path::Sphere{.RadiusM = Data::kWgs84A});
  std::vector<size_t> netToLane;
  LayLanesIntoNetwork(ways, vectors.Points(), net, netToLane);
  into.CrossNetworkMs = part();

  std::vector<Path::Network::Crossing> crossed;
  const auto swept = net.Crossings(crossed);
  if (!swept) { return; }
  into.CrossSweepMs = part();
  into.CrossingsSeen = crossed.size();
  into.PairsTested = swept->PairsTested;
  into.PairsPruned = swept->PairsPruned;
  into.FullestCell = swept->FullestCell;
  for (const Path::Network::Crossing &one : crossed) { FileCrossing(one, standing, into); }
  into.CrossFilingMs = part();
  for (const Path::Network::Crossing &one : crossed) {
    RaiseDeckOver(one, ways, netToLane, standing, drapedOver, into);
  }
  into.CrossDecksMs = part();
}

std::optional<Engine::State::Ends> Engine::State::EndsOf(const Ground::OsmField &vectors,
                                                         const Ground::StreetField::Way &lane) {
  const std::span<const double> points = vectors.Points();
  const size_t first = static_cast<size_t>(lane.FirstPoint) * 2u;
  const size_t last = first + (static_cast<size_t>(lane.PointCount) - 1u) * 2u;
  if (last + 1 >= points.size()) { return std::nullopt; }
  Ends out;
  out.At = {{points[first], points[first + 1], points[last], points[last + 1]}};
  out.Key = {{PlaceKey({.LongitudeDeg = out.At[1], .LatitudeDeg = out.At[0]}),
              PlaceKey({.LongitudeDeg = out.At[3], .LatitudeDeg = out.At[2]})}};
  return out;
}

std::optional<Engine::State::Grounded> Engine::State::GroundUnder(const TangentFrame &standing,
                                                                  const Drape &drapedOver,
                                                                  LongitudeLatitude at) const {
  const std::optional<double> stood = World.Stack.Ground().At(at).AslM();
  if (!stood) { return std::nullopt; }
  const EastNorthUp enu = standing.Place(
      {.LongitudeDeg = at.LongitudeDeg, .LatitudeDeg = at.LatitudeDeg, .HeightM = *stood});
  const Drape::EastSouth on = {.EastM = enu.EastM, .SouthM = -enu.NorthM};
  return Grounded{.EastM = on.EastM, .SouthM = on.SouthM, .GradeM = drapedOver.At(on, enu.UpM)};
}

void Engine::State::RaisesEnds(std::span<const uint64_t> key, double deckM, Paved &into) {
  for (const uint64_t one : key) {
    const auto found = into.EndM.find(one);
    if (found == into.EndM.end()) {
      into.EndM.emplace(one, deckM);
      continue;
    }
    found->second = std::max(found->second, deckM);
  }
}

void Engine::State::SeedsBridgeEnds(const Ground::StreetField &ways,
                                    const Ground::OsmField &vectors,
                                    const TangentFrame &standing,
                                    const Drape &drapedOver,
                                    Paved &into) const {
  for (size_t at = 0; at < ways.Ways().size(); ++at) {
    const Ground::StreetField::Way &lane = ways.Ways()[at];
    if (lane.Form != Ground::StreetField::Shape::Ribbon || lane.PointCount < 2) { continue; }
    const std::optional<Ends> ends = EndsOf(vectors, lane);
    if (!ends) { continue; }
    const std::array<uint64_t, 2> &key = ends->Key;
    for (int side = 0; side < 2; ++side) {
      const size_t axis = static_cast<size_t>(side) * 2u;
      const std::optional<Grounded> under =
          GroundUnder(standing,
                      drapedOver,
                      {.LongitudeDeg = ends->At[axis + 1u], .LatitudeDeg = ends->At[axis]});
      if (!under) { continue; }
      const double stood = under->GradeM;
      const auto found = into.EndM.find(key[side]);
      if (found == into.EndM.end()) {
        into.EndM.emplace(key[side], stood);
        into.GroundEndM.emplace(key[side], stood);
      } else {
        found->second = std::max(found->second, stood);
        const auto seeded = into.GroundEndM.find(key[side]);
        if (seeded != into.GroundEndM.end()) { seeded->second = std::max(seeded->second, stood); }
      }
    }
    if (lane.Bridge && into.DeckM[at] > kUnraisedDeckM) { RaisesEnds(key, into.DeckM[at], into); }
  }
}

double Engine::State::HighestDeckM(const Paved &over) {
  double mostDeckM = 0.0;
  for (const auto &one : over.EndM) {
    const auto seeded = over.GroundEndM.find(one.first);
    if (seeded == over.GroundEndM.end()) { continue; }
    mostDeckM = std::max(mostDeckM, one.second - seeded->second);
  }
  return mostDeckM;
}

void Engine::State::EasesRamps(const Ground::StreetField &ways,
                               const Ground::OsmField &vectors,
                               double mostDeckM,
                               Paved &into) {
  for (int pass = 0; pass < kRampPasses; ++pass) {
    for (const Ground::StreetField::Way &lane : ways.Ways()) {
      if (lane.Form != Ground::StreetField::Shape::Ribbon || lane.PointCount < 2) { continue; }
      if (!(lane.MaxGradient > 0.0f)) { continue; }
      const std::optional<Ends> ends = EndsOf(vectors, lane);
      if (!ends) { continue; }
      const std::array<uint64_t, 2> &key = ends->Key;
      const auto low = into.EndM.find(key[0]);
      const auto high = into.EndM.find(key[1]);
      if (low == into.EndM.end() || high == into.EndM.end()) { continue; }
      const double perLon = 111320.0 * std::cos(ends->At[0] * kDeg2Rad);
      const double runE = (ends->At[3] - ends->At[1]) * perLon;
      const double runN = (ends->At[2] - ends->At[0]) * kMPerDegLat;
      const double runM = std::sqrt(runE * runE + runN * runN);
      const double mostM = runM * static_cast<double>(lane.MaxGradient);
      const double apartM = high->second - low->second;
      const auto capped = [&](uint64_t at, double toM) {
        const auto seeded = into.GroundEndM.find(at);
        return seeded == into.GroundEndM.end() ? toM : std::min(toM, seeded->second + mostDeckM);
      };
      if (apartM > mostM) {
        low->second = capped(low->first, high->second - mostM);
      } else if (-apartM > mostM) {
        high->second = capped(high->first, low->second - mostM);
      }
    }
  }
}

void Engine::State::GradesApproaches(const Ground::StreetField &ways,
                                     const Ground::OsmField &vectors,
                                     const TangentFrame &standing,
                                     const Drape &drapedOver,
                                     Paved &into) const {
  for (const Ground::StreetField::Way &lane : ways.Ways()) {
    if (lane.Bridge || lane.Form != Ground::StreetField::Shape::Ribbon) { continue; }
    if (lane.PointCount < 2) { continue; }
    const std::optional<Ends> ends = EndsOf(vectors, lane);
    if (!ends) { continue; }
    const std::array<uint64_t, 2> &key = ends->Key;
    const std::optional<Grounded> low = GroundUnder(
        standing, drapedOver, {.LongitudeDeg = ends->At[1], .LatitudeDeg = ends->At[0]});
    const std::optional<Grounded> high = GroundUnder(
        standing, drapedOver, {.LongitudeDeg = ends->At[3], .LatitudeDeg = ends->At[2]});
    if (!low || !high) { continue; }
    const Vec2 stood = {{low->GradeM, high->GradeM}};
    double rose = 0.0;
    for (int side = 0; side < 2; ++side) {
      const auto found = into.EndM.find(key[side]);
      if (found == into.EndM.end()) { continue; }
      rose = std::max(rose, found->second - stood[side]);
    }
    if (rose > kRoseLeast) {
      ++into.RampsRaised;
      into.SteepestRamp = std::max(into.SteepestRamp, rose);
    }
  }
}

void Engine::State::Bridges(const Ground::StreetField &ways,
                            const Ground::OsmField &vectors,
                            const TangentFrame &standing,
                            const Drape &drapedOver,
                            Paved &into) {
  SeedsBridgeEnds(ways, vectors, standing, drapedOver, into);
  Published.Places("streets: the highest deck a ramp must reach", HighestDeckM(into), "m");
  EasesRamps(ways, vectors, HighestDeckM(into), into);
  GradesApproaches(ways, vectors, standing, drapedOver, into);
}

namespace {

struct Leaving {
  uint32_t Way = 0;
  uint8_t Side = 0;
  float DirE = 0.0f;
  float DirN = 0.0f;
  float HalfM = 0.0f;
};

void EndsMeetingAt(const Ground::StreetField &ways,
                   std::span<const double> points,
                   std::unordered_map<uint64_t, std::vector<Leaving>> &meeting) {
  for (size_t at = 0; at < ways.Ways().size(); ++at) {
    const Ground::StreetField::Way &lane = ways.Ways()[at];
    if (lane.Form != Ground::StreetField::Shape::Ribbon || lane.PointCount < 2) { continue; }
    if (!(lane.HalfWidthM > 0.0f)) { continue; }
    const size_t first = static_cast<size_t>(lane.FirstPoint) * 2u;
    const size_t last = first + (static_cast<size_t>(lane.PointCount) - 1u) * 2u;
    if (last + 1 >= points.size()) { continue; }
    for (int side = 0; side < 2; ++side) {
      const size_t here = side == 0 ? first : last;
      const size_t next = side == 0 ? first + 2u : last - 2u;
      const double perLon = 111320.0 * std::cos(points[here] * kDeg2Rad);
      double outE = (points[next + 1] - points[here + 1]) * perLon;
      double outN = (points[next] - points[here]) * kMPerDegLat;
      const double run = std::sqrt(outE * outE + outN * outN);
      if (!(run > kLeastRunM)) { continue; }
      outE /= run;
      outN /= run;
      meeting[PlaceKey({.LongitudeDeg = points[here + 1], .LatitudeDeg = points[here]})].push_back(
          Leaving{.Way = static_cast<uint32_t>(at),
                  .Side = static_cast<uint8_t>(side),
                  .DirE = static_cast<float>(outE),
                  .DirN = static_cast<float>(outN),
                  .HalfM = lane.HalfWidthM});
    }
  }
}

double BackOffFor(const Leaving &mine, std::span<const Leaving> leaving) {
  double back = 0.0;
  for (const Leaving &other : leaving) {
    if (other.Way == mine.Way && other.Side == mine.Side) { continue; }
    const double cosBetween =
        static_cast<double>(mine.DirE) * other.DirE + static_cast<double>(mine.DirN) * other.DirN;
    const double sinBetween = std::fabs(static_cast<double>(mine.DirE) * other.DirN -
                                        static_cast<double>(mine.DirN) * other.DirE);
    if (sinBetween < kLeastSineBetween) { continue; }
    back =
        std::max(back,
                 (static_cast<double>(other.HalfM) + static_cast<double>(mine.HalfM) * cosBetween) /
                     sinBetween);
  }
  return back;
}

double SharpestForkFor(const Leaving &mine, std::span<const Leaving> leaving) {
  double sharpest = kDegPerHalfTurn;
  for (const Leaving &other : leaving) {
    if (other.Way == mine.Way && other.Side == mine.Side) { continue; }
    const double between = std::acos(std::clamp(static_cast<double>(mine.DirE) * other.DirE +
                                                    static_cast<double>(mine.DirN) * other.DirN,
                                                -1.0,
                                                1.0));
    sharpest = std::min(sharpest, kDegPerHalfTurn - between * kRad2Deg);
  }
  return sharpest;
}

} // namespace

void Engine::State::Shortens(const Ground::StreetField &ways,
                             const Ground::OsmField &vectors,
                             Paved &into) {
  std::unordered_map<uint64_t, std::vector<Leaving>> meeting;
  EndsMeetingAt(ways, vectors.Points(), meeting);

  std::vector<uint64_t> met;
  met.reserve(meeting.size());
  for (const auto &one : meeting) {
    if (one.second.size() >= 2) { met.push_back(one.first); }
  }
  std::ranges::sort(met);

  for (const uint64_t at : met) {
    const std::vector<Leaving> &leaving = meeting[at];
    for (const Leaving &mine : leaving) {
      const double back = BackOffFor(mine, leaving);
      const double capped = std::min(back, static_cast<double>(mine.HalfM) * kTrimMostWidths);
      into.TrimM[static_cast<size_t>(mine.Way) * 2u + mine.Side] = capped;
      if (capped > kLeastCapM) {
        ++into.EndsTrimmed;
        into.DeepestTrimM = std::max(into.DeepestTrimM, capped);
      }
      if (back > capped + kLeastCapM) {
        ++into.EndsStillCrossing;
        const double shortBy = back - capped;
        into.LongestShortM = std::max(into.LongestShortM, shortBy);
        into.ShortestByM = into.CapsBit == 0 ? shortBy : std::min(into.ShortestByM, shortBy);
        const double fork = SharpestForkFor(mine, leaving);
        into.SharpestForkDeg = into.CapsBit == 0 ? fork : std::min(into.SharpestForkDeg, fork);
        ++into.CapsBit;
      }
    }
  }
}

namespace {

struct Meeting {
  uint64_t Node = 0;
  uint32_t Lane = 0;
  double GradeM = 0.0;
};

struct MetAt {
  uint32_t First = 0;
  uint32_t Count = 0;
};

std::vector<Meeting> MeetingsIn(std::span<const std::vector<RoadStation>> designed) {
  std::vector<Meeting> met;
  for (uint32_t lane = 0; lane < static_cast<uint32_t>(designed.size()); ++lane) {
    for (const RoadStation &one : designed[lane]) {
      if (one.Node == 0u) { continue; }
      met.push_back({.Node = one.Node, .Lane = lane, .GradeM = one.GradeM});
    }
  }
  std::ranges::stable_sort(met, {}, &Meeting::Node);
  return met;
}

std::vector<MetAt> JunctionsIn(std::span<const Meeting> met) {
  std::vector<MetAt> meetings;
  for (uint32_t at = 0; at < static_cast<uint32_t>(met.size());) {
    uint32_t past = at;
    while (past < met.size() && met[past].Node == met[at].Node) { ++past; }
    if (past - at >= 2) { meetings.push_back({.First = at, .Count = past - at}); }
    at = past;
  }
  return meetings;
}

double RelaxMeetings(std::span<const Meeting> met,
                     std::span<const MetAt> meetings,
                     std::span<double> shiftM) {
  std::vector<double> pullM(shiftM.size(), 0.0);
  std::vector<uint32_t> pulls(shiftM.size(), 0u);
  double movedM = 0.0;
  for (int round = 0; round < kLevelPasses; ++round) {
    std::ranges::fill(pullM, 0.0);
    std::ranges::fill(pulls, 0u);
    for (const MetAt &node : meetings) {
      const std::span<const Meeting> held(met.data() + node.First, node.Count);
      double wanted = 0.0;
      for (const Meeting &one : held) { wanted += one.GradeM + shiftM[one.Lane]; }
      wanted /= static_cast<double>(node.Count);
      for (const Meeting &one : held) {
        pullM[one.Lane] += wanted - (one.GradeM + shiftM[one.Lane]);
        ++pulls[one.Lane];
      }
    }
    double most = 0.0;
    for (size_t lane = 0; lane < shiftM.size(); ++lane) {
      if (pulls[lane] == 0u) { continue; }
      const double by = pullM[lane] / static_cast<double>(pulls[lane]);
      shiftM[lane] += by;
      most = std::max(most, std::fabs(by));
    }
    movedM = most;
    if (most < kLevelledM) { break; }
  }
  return movedM;
}

} // namespace

double Engine::State::LevelsWhereWaysMeet(Paved &into) {
  const std::vector<Meeting> met = MeetingsIn(into.Designed);
  const std::vector<MetAt> meetings = JunctionsIn(met);
  std::vector<double> shiftM(into.Designed.size(), 0.0);
  const double movedM = RelaxMeetings(met, meetings, shiftM);

  for (size_t lane = 0; lane < into.Designed.size(); ++lane) {
    if (shiftM[lane] == 0.0) { continue; }
    for (RoadStation &one : into.Designed[lane]) { one.GradeM += shiftM[lane]; }
  }
  return movedM;
}

size_t Engine::State::RaisesTheJunctionBodies(Paved &into, RoadRaised &pavement) const {
  std::vector<uint64_t> nodes;
  nodes.reserve(into.Gates.size());
  for (const auto &one : into.Gates) {
    if (one.second.size() >= 2) { nodes.push_back(one.first); }
  }
  std::ranges::sort(nodes);
  const int asphalt = World.Stack.Materials().Find("asphalt");
  Vec3f wears = {{0.5f, 0.5f, 0.5f}};
  if (asphalt >= 0) { wears = World.Stack.Materials().At(static_cast<size_t>(asphalt)).Albedo; }
  size_t raised = 0;
  for (const uint64_t node : nodes) {
    const std::vector<RoadGate> &met = into.Gates[node];
    World.Shipping.Paving().Junction(
        std::span<const RoadGate>(met.data(), met.size()), wears, pavement);
    ++raised;
  }
  return raised;
}

void Engine::State::TellsWhatTheFitFound(Paved &into) {
  if (into.FitsMeasured == 0) { return; }
  Published.Places(
      "streets: ways a reference line was fitted to", static_cast<double>(into.FitLaid), "ways");
  Published.Places(
      "streets: and ways the fit refused", static_cast<double>(into.FitRefused), "ways");
  Published.Places("streets: corners too tight to drive, cut instead",
                   static_cast<double>(into.FitTooTight),
                   "corners");
  Published.Places("streets: cuts the split made", static_cast<double>(into.FitCuts), "cuts");
  Published.Places(
      "streets: stations a chord asked for", static_cast<double>(into.ChordAdded), "stations");
  Published.Places(
      "streets: pieces the sweep laid on a line", static_cast<double>(into.Swept.Pieces), "pieces");
  Published.Places("streets: cuts the sweep made", static_cast<double>(into.Swept.Cuts), "cuts");
  Published.Places(
      "streets: pieces the sweep could not lay", static_cast<double>(into.Swept.Refused), "pieces");
  Published.Places(
      "streets: of those, the fit refused", static_cast<double>(into.Swept.Why.Fit), "pieces");
  Published.Places(
      "streets: of those, the rise refused", static_cast<double>(into.Swept.Why.Rise), "pieces");
  Published.Places(
      "streets: of those, the bank refused", static_cast<double>(into.Swept.Why.Bank), "pieces");
  Published.Places(
      "streets: of those, the sweep refused", static_cast<double>(into.Swept.Why.Sweep), "pieces");
  Published.Places("streets: of those, too short to lay",
                   static_cast<double>(into.Swept.Why.TooShort),
                   "pieces");
  Published.Places("streets: pieces the split still could not lay",
                   static_cast<double>(into.FitUnsplittable),
                   "pieces");
  Published.Places("streets: the tightest radius a corner demanded", into.TightestDemandM, "m");
  Published.Places(
      "streets: the radius a fitted line found, tightest", into.FitRadiusTightestM, "m");
  Published.Places(
      "streets: reference lines measured", static_cast<double>(into.FitsMeasured), "ways");
  Published.Places("streets: stations the fit calls undrivable",
                   static_cast<double>(into.FitUndrivable),
                   "stations");
}

void Engine::State::HandsThePavingOver(const RoadRaised &pavement, Geometry &ground) {
  if (pavement.Index.size() < 3) { return; }
  Material tarmac;
  for (int channel = 0; channel < 3; ++channel) { tarmac.BaseColour[channel] = 1.0f; }
  {
    const int asphalt = World.Stack.Materials().Find("asphalt");
    tarmac.Roughness = asphalt >= 0
                           ? World.Stack.Materials().At(static_cast<size_t>(asphalt)).Roughness
                           : kUnlitTint;
  }
  const MaterialInstance paved = ground.addSurface("streets", tarmac);
  const int pavedPart = ground.addPart("streets", paved);
  const bool tookPaving =
      pavedPart >= 0 &&
      ground.setPositions(
          pavedPart,
          std::span<const float>(pavement.PositionM.data(), pavement.PositionM.size())) &&
      ground.setNormals(pavedPart,
                        std::span<const float>(pavement.NormalM.data(), pavement.NormalM.size())) &&
      ground.setColours(
          pavedPart,
          std::span<const float>(pavement.ColourRgba.data(), pavement.ColourRgba.size())) &&
      ground.setTriangles(pavedPart,
                          std::span<const uint32_t>(pavement.Index.data(), pavement.Index.size()));
  Published.Places(
      "streets: the surface they were given", static_cast<double>(paved.index()), "index");
  Published.Places("streets: the part they were given", static_cast<double>(pavedPart), "index");
  Published.Places("streets: the geometry took them", tookPaving ? 1.0 : 0.0, "yes/no");
  Published.Places(
      "streets: parts the geometry now holds", static_cast<double>(ground.parts()), "parts");
}

void Engine::State::Paves(const TangentFrame &standing,
                          const std::shared_ptr<const ClassStructure> &classStructure,
                          const Drape &drapedOver,
                          std::vector<Yields> &corridor,
                          RoadRaised &pavement,
                          Geometry &ground,
                          Phasing &clocks) {
  Published.Places(
      "rebuild: of that, the drape the buildings stand on",
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - clocks.CensusAt)
          .count(),
      "ms");
  clocks.WiresAt = std::chrono::steady_clock::now();
  const Ground::StreetField &ways = World.Stack.Ways();
  const Ground::OsmField *const vectors = World.Stack.Vectors();
  Paved into;
  into.DeckM.assign(ways.Ways().size(), -kBeyondAnyCoordinate);
  into.TrimM.assign(ways.Ways().size() * 2u, 0.0);
  into.Designed.resize(ways.Ways().size());
  const int waterRow = World.Stack.Materials().Find("water");

  auto tookFrom = std::chrono::steady_clock::now();
  const auto since = [&tookFrom] {
    const auto was = tookFrom;
    tookFrom = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(tookFrom - was).count();
  };
  const auto pavesAt = std::chrono::steady_clock::now();
  if (vectors != nullptr) { Crosses(ways, *vectors, standing, drapedOver, into); }
  Published.Places("streets: of that, finding the crossings", since(), "ms");
  Published.Places(
      "streets: of finding them, laying the lanes into a network", into.CrossNetworkMs, "ms");
  Published.Places("streets: of finding them, the sweep itself", into.CrossSweepMs, "ms");
  Published.Places("streets: of finding them, filing each one", into.CrossFilingMs, "ms");
  Published.Places("streets: of finding them, raising a deck over each", into.CrossDecksMs, "ms");
  Published.Places("streets: of finding them, pairs the sweep tested",
                   static_cast<double>(into.PairsTested),
                   "pairs");
  Published.Places("streets: and pairs its boxes threw out first",
                   static_cast<double>(into.PairsPruned),
                   "pairs");
  Published.Places(
      "streets: segments in the fullest square", static_cast<double>(into.FullestCell), "segments");
  if (vectors != nullptr && into.DecksRaised > 0) {
    Bridges(ways, *vectors, standing, drapedOver, into);
  }
  Published.Places(
      "streets: ways a ramp lifted off the ground", static_cast<double>(into.RampsRaised), "ways");
  Published.Places("streets: and the most one was lifted", into.SteepestRamp, "m");
  Published.Places(
      "streets: crossings the plan found", static_cast<double>(into.CrossingsSeen), "crossings");
  Published.Places(
      "streets: decks a crossing raised", static_cast<double>(into.DecksRaised), "decks");
  Published.Places("streets: and the most one stands over what it crosses", into.MostRaisedM, "m");
  Published.Places("streets: of that, raising the decks", since(), "ms");
  if (vectors != nullptr) { Shortens(ways, *vectors, into); }
  Published.Places("streets: of that, shortening the ends", since(), "ms");
  Published.Places("streets: ends a cap shortened", static_cast<double>(into.CapsBit), "ends");
  Published.Places("streets: and the most one lost", into.LongestShortM, "m");
  Published.Places("streets: the sharpest fork a cap bit at", into.SharpestForkDeg, "deg");
  Published.Places(
      "streets: way ends a junction trimmed", static_cast<double>(into.EndsTrimmed), "ends");
  Published.Places("streets: and the deepest trim", into.DeepestTrimM, "m");
  Published.Places("streets: ends STILL crossing, the cap bit",
                   static_cast<double>(into.EndsStillCrossing),
                   "ends");
  if (vectors != nullptr) {
    const std::span<const double> points = vectors->Points();
    std::unordered_map<uint64_t, uint32_t> sharedNodes;
    for (const Ground::StreetField::Way &one : ways.Ways()) {
      if (one.Form != Ground::StreetField::Shape::Ribbon) { continue; }
      for (uint32_t step = 0; step < one.PointCount; ++step) {
        const size_t at = (static_cast<size_t>(one.FirstPoint) + step) * 2u;
        if (at + 1 >= points.size()) { break; }
        ++sharedNodes[PlaceKey({.LongitudeDeg = points[at + 1], .LatitudeDeg = points[at]})];
      }
    }
    const Paving on{.Ways = ways,
                    .Vectors = *vectors,
                    .Points = points,
                    .SharedNodes = sharedNodes,
                    .Draped = drapedOver,
                    .Standing = standing,
                    .Classes = classStructure,
                    .WaterRow = waterRow};
    const Heap::Tagged paving("road-pave");
    for (const Pass pass : {Pass::Designing, Pass::Paving}) {
      const std::string_view doing = Doing(pass);
      for (size_t laneAt = 0; laneAt < ways.Ways().size(); ++laneAt) {
        PaveLane(on, pass, laneAt, into, corridor, pavement);
      }
      Published.Places(std::format("streets: of that, {} every lane", doing), since(), "ms");
      Published.Places(std::format("streets: of {}, the fit", doing), into.FitMs, "ms");
      Published.Places(std::format("streets: of {}, the water", doing), into.WaterMs, "ms");
      Published.Places(std::format("streets: of {}, the sweep", doing), into.SweepMs, "ms");
      into.FitMs = 0.0;
      into.WaterMs = 0.0;
      into.SweepMs = 0.0;
      if (pass == Pass::Designing) {
        Published.Places("streets: the levelling's last shift", LevelsWhereWaysMeet(into), "m");
        Published.Places("streets: of that, levelling the junctions", since(), "ms");
      }
    }
  }
  Published.Places("streets: stations under a bridge asked",
                   static_cast<double>(into.AskedOverBridge),
                   "stations");
  Published.Places(
      "streets: of those a class named", static_cast<double>(into.NamedOverBridge), "stations");
  Published.Places(
      "streets: and of those, water", static_cast<double>(into.WetOverBridge), "stations");
  Published.Places(
      "streets: the water class the table names", static_cast<double>(waterRow), "index");
  Published.Places("streets: a class structure stood", classStructure ? 1.0 : 0.0, "yes/no");
  Published.Places(
      "streets: decks a WATERWAY raised", static_cast<double>(into.DecksOverWater), "decks");
  Published.Places("streets: and the clearance the widest one took", into.MostOverWaterM, "m");
  const auto junctionsAt = std::chrono::steady_clock::now();
  Published.Places("streets: junction bodies raised",
                   static_cast<double>(RaisesTheJunctionBodies(into, pavement)),
                   "junctions");
  Published.Places(
      "streets: of that, raising the junction bodies",
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - junctionsAt)
          .count(),
      "ms");
  TellsWhatTheFitFound(into);
  Published.Places("streets: ways laid as ribbons, all of them FLOATING",
                   static_cast<double>(into.LaidWays),
                   "ways");
  Published.Places(
      "streets: ways the GROUND carries instead", static_cast<double>(into.GroundWays), "ways");
  Published.Places(
      "streets: ways the field holds", static_cast<double>(ways.Ways().size()), "ways");
  Published.Places(
      "streets: features it walked at all", static_cast<double>(ways.LookedCount()), "features");
  Published.Places(
      "streets: features no rule named", static_cast<double>(ways.UnruledCount()), "features");
  Published.Places("streets: features a rule gave no width",
                   static_cast<double>(ways.UnwidthedCount()),
                   "features");
  Published.Places(
      "streets: features that are tunnels", static_cast<double>(ways.TunnelCount()), "features");
  Published.Places(
      "streets: ways OSM calls a bridge", static_cast<double>(ways.BridgeCount()), "ways");
  Published.Places(
      "streets: ways that state a layer", static_cast<double>(ways.LayeredCount()), "ways");
  Published.Places(
      "streets: ways whose layer is a STRING", static_cast<double>(ways.LayerSaidCount()), "ways");
  Published.Places("streets: ways it refused", static_cast<double>(into.RefusedWays), "ways");
  const size_t pavedTriangles = pavement.Index.size() / 3;
  Published.Places("streets: triangles", static_cast<double>(pavedTriangles), "triangles");
  const auto handingAt = std::chrono::steady_clock::now();
  HandsThePavingOver(pavement, ground);
  Published.Places(
      "streets: of that, handing the paving over",
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - handingAt)
          .count(),
      "ms");
  Published.Places(
      "streets: everything Paves did",
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - pavesAt).count(),
      "ms");
}

void Engine::State::TellsTheRelief(Relieved over) {
  Published.Places("relief: the ring's tallest vertex ABOVE THE ELLIPSOID", over.Tallest, "m");
  Published.Places("relief: and how far out it lies", over.TallestOutM, "m");
  Published.Places("relief: the ring's lowest vertex above the ellipsoid", over.Lowest, "m");
  Published.Places(
      "relief: so the true relief, with the sphere taken out", over.Tallest - over.Lowest, "m");
}

void Engine::State::TellsWhatCrossed(const Geometry &ground) {
  size_t handed = 0;
  for (int part = 0; part < ground.parts(); ++part) {
    handed += ground.trianglesOf(part).size() / 3u;
  }
  Published.Places(
      "the triangles handed to the renderer", static_cast<double>(handed), "triangles");
  Published.Places("in this many parts", static_cast<double>(ground.parts()), "parts");
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
  if (Picture.Standing->Watched()) {
    const Vec3 &at = Picture.Standing->Watching().EyeM;
    const TangentFrame eyed = TangentFrame::At({.LongitudeDeg = atLon, .LatitudeDeg = atLat});
    for (int axis = 0; axis < 3; ++axis) {
      over.EyeM[axis] = eyed.OriginEcef()[axis] + at[0] * eyed.EastEcef()[axis] +
                        at[1] * eyed.UpEcef()[axis] - at[2] * eyed.NorthEcef()[axis];
      over.Up[axis] = static_cast<float>(eyed.UpEcef()[axis]);
    }
  }
  if (Picture.Frame.HeightPx > 0) {
    const double halfFov = 0.5 * 55.0 * kDeg2Rad;
    over.FocalPx =
        static_cast<float>(0.5 * static_cast<double>(Picture.Frame.HeightPx) / std::tan(halfFov));
  }
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
  std::vector<float> inFrame;
  inFrame.resize(laid->PositionM.size());
  double sank = 0.0;
  double sankAt = 0.0;
  double tallest = -kNoLeastYet;
  double lowest = kNoLeastYet;
  double tallestOut = 0.0;
  for (size_t at = 0; at + 2 < laid->PositionM.size(); at += 3) {
    const Vec3 held = {{laid->OriginEcef[0] + static_cast<double>(laid->PositionM[at]),
                        laid->OriginEcef[1] + static_cast<double>(laid->PositionM[at + 1]),
                        laid->OriginEcef[2] + static_cast<double>(laid->PositionM[at + 2])}};
    double eastM = 0.0;
    double upM = 0.0;
    double northM = 0.0;
    const EastNorthUp eastMEnu = standing.Place(held);
    eastM = eastMEnu.EastM;
    upM = eastMEnu.UpM;
    northM = eastMEnu.NorthM;
    inFrame[at] = static_cast<float>(eastM);
    inFrame[at + 1] = static_cast<float>(upM);
    inFrame[at + 2] = static_cast<float>(-northM);
    const Ground::Geo where =
        Ground::EcefToGeoWgs84(Ground::Ecef{.X = held[0], .Y = held[1], .Z = held[2]});
    const double below = where.HeightM - upM;
    if (below > sank) {
      sank = below;
      sankAt = std::sqrt(eastM * eastM + northM * northM);
    }
    if (where.HeightM > tallest) {
      tallest = where.HeightM;
      tallestOut = std::sqrt(eastM * eastM + northM * northM);
    }
    lowest = std::min(where.HeightM, lowest);
  }
  TellsTheRelief({.Tallest = tallest, .Lowest = lowest, .TallestOutM = tallestOut});
  Classed classed;
  {
    const Heap::Tagged classing("ground-classify");
    classed = Classify(*laid, inFrame);
  }
  std::vector<float> &tinted = classed.Tinted;
  std::vector<float> &classUv = classed.Uv;
  const std::vector<float> &classPalette = classed.Palette;
  const std::shared_ptr<const ClassStructure> &classStructure = classed.Structure;
  Published.Places("the ring's vertex that sinks furthest below its own altitude", sank, "m");
  Published.Places("and how far out it lies", sankAt, "m");
  Published.Places("a sphere would sink it by", sankAt * sankAt / (2.0 * Data::kWgs84A), "m");

  {
    double nearest = kBeyondAnyCoordinate;
    double atUp = 0.0;
    double farthest = 0.0;
    double farUp = 0.0;
    for (size_t at = 0; at + 2 < inFrame.size(); at += 3) {
      const auto east = static_cast<double>(inFrame[at]);
      const auto south = static_cast<double>(inFrame[at + 2]);
      const double away = east * east + south * south;
      if (away < nearest) {
        nearest = away;
        atUp = static_cast<double>(inFrame[at + 1]);
      }
      if (away > farthest) {
        farthest = away;
        farUp = static_cast<double>(inFrame[at + 1]);
      }
    }
    Published.Places("the ring's nearest vertex to the frame origin", std::sqrt(nearest), "m");
    Published.Places("and its up", atUp, "m");
    Published.Places("its farthest vertex", std::sqrt(farthest), "m");
    Published.Places("and THAT one's up", farUp, "m");
  }
  {
    for (size_t at = 0; at + 2 < laid->Index.size(); at += 3) {
      std::swap(laid->Index[at + 1], laid->Index[at + 2]);
    }
    for (size_t at = 0; at + 2 < laid->NormalM.size(); at += 3) {
      const Vec3 held = {{static_cast<double>(laid->NormalM[at]),
                          static_cast<double>(laid->NormalM[at + 1]),
                          static_cast<double>(laid->NormalM[at + 2])}};
      double alongEast = 0.0;
      double alongUp = 0.0;
      double alongNorth = 0.0;
      const EastNorthUp alongEastEnu = standing.Turn(held);
      alongEast = alongEastEnu.EastM;
      alongUp = alongEastEnu.UpM;
      alongNorth = alongEastEnu.NorthM;
      laid->NormalM[at] = static_cast<float>(alongEast);
      laid->NormalM[at + 1] = static_cast<float>(alongUp);
      laid->NormalM[at + 2] = static_cast<float>(-alongNorth);
    }
  }
  {
    double up = 0.0;
    double down = 0.0;
    double sideways = 0.0;
    double unlengthed = 0.0;
    for (size_t at = 0; at + 2 < laid->NormalM.size(); at += 3) {
      const double x = laid->NormalM[at];
      const double y = laid->NormalM[at + 1];
      const double z = laid->NormalM[at + 2];
      const double length = std::sqrt(x * x + y * y + z * z);
      if (!(length > 0.5)) {
        unlengthed += 1.0;
        continue;
      }
      const double upward = y / length;
      if (upward > 0.5) {
        up += 1.0;
      } else if (upward < -0.5) {
        down += 1.0;
      } else {
        sideways += 1.0;
      }
    }
    Published.Places("the ring's normals that point up", up, "normals");
    Published.Places("its normals that point DOWN", down, "normals");
    Published.Places("its normals that lie sideways", sideways, "normals");
    {
      double steepest = 0.0;
      double mean = 0.0;
      double counted = 0.0;
      for (size_t at = 0; at + 2 < laid->NormalM.size(); at += 3) {
        const double x = laid->NormalM[at];
        const double y = laid->NormalM[at + 1];
        const double z = laid->NormalM[at + 2];
        const double length = std::sqrt(x * x + y * y + z * z);
        if (!(length > kLeastRunM)) { continue; }
        const double leanDeg = std::acos(std::fmin(1.0, y / length)) * kRad2Deg;
        steepest = leanDeg > steepest ? leanDeg : steepest;
        mean += leanDeg;
        counted += 1.0;
      }
      Published.Places("the steepest the ring's surface leans", steepest, "deg");
      Published.Places("how far it leans on average", counted > 0.0 ? mean / counted : 0.0, "deg");
    }
    Published.Places("its normals with no length at all", unlengthed, "normals");
    const size_t normals = laid->NormalM.size() / 3;
    Published.Places("its normals in all", static_cast<double>(normals), "normals");
    {
      double least = kBeyondAnyCoordinate;
      double most = -kBeyondAnyCoordinate;
      const std::vector<float> &held = laid->PositionM;
      for (size_t at = 1; at < held.size(); at += 3) {
        const auto y = static_cast<double>(held[at]);
        least = std::min(least, y);
        most = std::max(most, y);
      }
      Published.Places("the ground ring's lowest vertex", least, "m");
      Published.Places("the ground ring's highest", most, "m");
    }
  }
  Geometry ground;
  Material bare;
  {
    const Render::Medium held = Render::kEarthAir;
    for (int channel = 0; channel < 3; ++channel) {
      bare.BaseColour[channel] = held.GroundAlbedo[channel];
    }
  }
  if (!tinted.empty()) {
    for (int channel = 0; channel < 3; ++channel) { bare.BaseColour[channel] = 1.0f; }
  }
  const MaterialInstance ringSurface = ground.addSurface("ground", bare);
  const int ringPart = ground.addPart("ground", ringSurface);

  Phasing clocks{.PhaseAt = phaseAt, .CensusAt = censusAt, .WiresAt = wiresAt};
  {
    const Heap::Tagged modelling("ground-model");
    Models(
        standing, inFrame, {.LongitudeDeg = anchorLon, .LatitudeDeg = anchorLat}, ground, clocks);
  }
  phaseAt = clocks.PhaseAt;
  censusAt = clocks.CensusAt;
  wiresAt = clocks.WiresAt;

  std::vector<Yields> corridor;
  RoadRaised pavement;
  Published.Places(
      "rebuild: of that, the ring and the buildings into the frame",
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - censusAt)
          .count(),
      "ms");
  censusAt = std::chrono::steady_clock::now();
  const TriangleBvh surface = TriangleBvh::Over(inFrame, laid->Index);
  Published.Places(
      "rebuild: of that, the surface the world stands on",
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - censusAt)
          .count(),
      "ms");
  Published.Places("ring: triangles the drape can reach",
                   static_cast<double>(laid->Index.size()) / 3.0,
                   "triangles");
  const Drape drapedOver{.Surface = surface};
  Paves(standing, classStructure, drapedOver, corridor, pavement, ground, clocks);

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
    yielding.insert(yielding.end(),
                    std::make_move_iterator(corridor.begin()),
                    std::make_move_iterator(corridor.end()));
    Yielded told;
    const Heap::Tagged yielding_("ground-yield");
    World.Shipping.Covering().Yield(
        std::span<const Yields>(yielding),
        Budget{.FinestM = kFinestGroundM, .MostTriangles = kMostYieldTriangles},
        GroundMesh{.PositionM = &inFrame,
                   .NormalM = &laid->NormalM,
                   .ColourRgba = tinted.empty() ? nullptr : &tinted,
                   .Uv = classUv.empty() ? nullptr : &classUv,
                   .Index = &laid->Index},
        told);
    Published.Places("ground: of that, refining", told.RefineMs, "ms");
    Published.Places("ground: of that, cutting the seams", told.CutMs, "ms");
    Published.Places("ground: of that, sewing them", told.SewMs, "ms");
    Published.Places("ground: of that, pressing", told.PressMs, "ms");
    Published.Places("ground: of that, counting the seams", told.SeamMs, "ms");
    Published.Places("ground: pads that may press it", static_cast<double>(builtPads), "pads");
    Published.Places("ground: corridor pieces that may press it",
                     static_cast<double>(yielding.size() - builtPads),
                     "pieces");
    Published.Places("ground: yields the budget took", static_cast<double>(told.Taken), "yields");
    Published.Places(
        "ground: yields the budget REFUSED", static_cast<double>(told.Refused), "yields");
    Published.Places(
        "ground: passes it refined the ring in", static_cast<double>(told.Passes), "passes");
    Published.Places("ground: vertices the refinement added",
                     static_cast<double>(told.VerticesAdded),
                     "vertices");
    Published.Places("ground: triangles the refinement added",
                     static_cast<double>(told.TrianglesAdded),
                     "triangles");
    Published.Places(
        "ground: the carriageway's footprint corners", static_cast<double>(told.Seams), "corners");
    Published.Places("ground: of those, a ground vertex shares the spot",
                     static_cast<double>(told.SeamsShared),
                     "corners");
    Published.Places(
        "ground: ring vertices a pad pressed", static_cast<double>(told.Pressed), "vertices");
    Published.Places("ground: and the deepest it pressed", told.DeepestM, "m");
    Published.Places("ground: and the highest it filled", told.RaisedM, "m");
  }
  (void)ground.setPositions(ringPart, std::span<const float>(inFrame.data(), inFrame.size()));
  (void)ground.setNormals(ringPart,
                          std::span<const float>(laid->NormalM.data(), laid->NormalM.size()));
  (void)ground.setTriangles(ringPart,
                            std::span<const uint32_t>(laid->Index.data(), laid->Index.size()));
  if (!tinted.empty()) {
    (void)ground.setColours(ringPart, std::span<const float>(tinted.data(), tinted.size()));
  }
  if (!classUv.empty()) {
    (void)ground.setTexture(ringPart, std::span<const float>(classUv.data(), classUv.size()), 0);
  }

  {
    const auto waterAt = std::chrono::steady_clock::now();
    const Ground::WaterField &wet = World.Stack.WaterBodies();
    const Ground::OsmField *const vectors = World.Stack.Vectors();
    std::vector<float> places;
    std::vector<float> facing;
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
      Material lagoon;
      lagoon.BaseColour[0] = kLagoonRed;
      lagoon.BaseColour[1] = kLagoonGreen;
      lagoon.BaseColour[2] = kLagoonBlue;
      lagoon.Roughness = kLagoonRoughness;
      lagoon.DoubleSided = true;
      const MaterialInstance wetSurface = ground.addSurface("water", lagoon);
      const int wetPart = ground.addPart("water", wetSurface);
      const bool tookWater =
          wetPart >= 0 &&
          ground.setPositions(wetPart, std::span<const float>(places.data(), places.size())) &&
          ground.setNormals(wetPart, std::span<const float>(facing.data(), facing.size())) &&
          ground.setTriangles(wetPart, std::span<const uint32_t>(order.data(), order.size()));
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
  {
    double least = kBeyondAnyCoordinate;
    double most = -kBeyondAnyCoordinate;
    for (size_t at = 0; at + 2 < inFrame.size(); at += 3) {
      const auto up = static_cast<double>(inFrame[at + 1]);
      least = std::min(up, least);
      most = std::max(up, most);
    }
    double west = kBeyondAnyCoordinate;
    double east = -kBeyondAnyCoordinate;
    double north = kBeyondAnyCoordinate;
    double south = -kBeyondAnyCoordinate;
    for (size_t at = 0; at + 2 < inFrame.size(); at += 3) {
      const auto alongE = static_cast<double>(inFrame[at]);
      const auto alongS = static_cast<double>(inFrame[at + 2]);
      west = std::min(alongE, west);
      east = std::max(alongE, east);
      north = std::min(alongS, north);
      south = std::max(alongS, south);
    }
    if (most >= least) {
      Published.Places("the ring's lowest vertex", least, "m");
      Published.Places("its highest", most, "m");
      Published.Places("so the relief it carries", most - least, "m");
      Published.Places("and the ground it spans, east to west", east - west, "m");
      Published.Places("north to south", south - north, "m");
      double summed = 0.0;
      size_t counted = 0;
      for (size_t at = 0; at + 2 < inFrame.size(); at += 3) {
        summed += static_cast<double>(inFrame[at + 1]);
        ++counted;
      }
      const double mean = counted > 0 ? summed / static_cast<double>(counted) : 0.0;
      size_t adrift = 0;
      for (size_t at = 0; at + 2 < inFrame.size(); at += 3) {
        const double off = static_cast<double>(inFrame[at + 1]) - mean;
        if (off > kAdriftMostM || off < -kAdriftMostM) { ++adrift; }
      }
      Published.Places("the height its vertices average", mean, "m");
      Published.Places(
          "vertices more than 500 m from that average", static_cast<double>(adrift), "vertices");
      Published.Places("out of", static_cast<double>(counted), "vertices");
    }
  }
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
  Published.Places("clusters the ring holds", static_cast<double>(laid->ClustersHeld), "clusters");
  Published.Places("clusters it drew", static_cast<double>(laid->ClustersDrawn), "clusters");
  Published.Places("ring: clusters carried for the device",
                   static_cast<double>(laid->Clusters.size()),
                   "clusters");
  Published.Places("ring: the whole index list they cut from",
                   static_cast<double>(laid->AllIndex.size()),
                   "indices");
  Published.Places("ring: against the list the CPU selected",
                   static_cast<double>(laid->Index.size()),
                   "indices");
  Published.Places("ring: clusters it holds", static_cast<double>(laid->ClustersHeld), "clusters");
  Published.Places(
      "ring: clusters the CPU drew", static_cast<double>(laid->ClustersDrawn), "clusters");
  Published.Places("the worst error any of them carries", laid->WorstErrM, "m");
  return true;
}
} // namespace outshine
