#ifndef OUTSHINE_GENERATORS_OSM_URBAN_URBANAREA_H
#define OUTSHINE_GENERATORS_OSM_URBAN_URBANAREA_H

#include "TileSourceIdentity.h"
#include "Earth.h"
#include "math/Vec3.h"
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

namespace outshine::Generators::Osm {
class OsmField;

struct UrbanAreaRecipe {
  static constexpr double DefaultFillFraction = .75;
  double ProjectedPitchM = 32;
  double FillFraction = DefaultFillFraction;
  uint32_t PatchSide = 8;
};

struct UrbanAreaLimits {
  size_t CellsMost = 1048576;
  size_t SourceVerticesMost = 1048576;
  size_t FeaturesMost = 8192;
  std::chrono::steady_clock::time_point Deadline = std::chrono::steady_clock::time_point::max();
  std::stop_token Stop;
};

enum class UrbanSourceRole : uint8_t {
  Unclassified,
  OutsideScope,
  Invalid,
  Outside,
  Urban,
  OpenSpace,
  Corridor,
  Tunnel
};

struct UrbanSourceUse {
  uint32_t Input = 0, Feature = 0;
  UrbanSourceRole Role = UrbanSourceRole::Unclassified;
  std::optional<uint64_t> ProviderFeatureId;
};

struct UrbanAreaInput {
  Data::TileSourceIdentity Identity;
  std::string PayloadDigest;
};

struct UrbanLot {
  LongitudeLatitude Centre;
  double EastSpanM = 0, NorthSpanM = 0;
  float HeightM = 0;
};

struct UrbanPatchRange {
  uint32_t Column = 0, Row = 0, Columns = 0, Rows = 0;
};

struct UrbanPatchBounds {
  Vec3 CentreEcef;
  double RadiusM = 0;
};

struct UrbanArea {
  UrbanAreaRecipe Recipe;
  std::array<int64_t, 2> FirstCell{};
  std::array<double, 2> ProjectedOriginM{};
  double MetresPerProjectedMetre = 0;
  uint32_t Columns = 0, Rows = 0;
  std::vector<uint8_t> Tiers;
  std::vector<UrbanAreaInput> Inputs;
  std::vector<UrbanSourceUse> Sources;
  size_t Occupied = 0, LotTests = 0, PatchTests = 0;
  bool HasUrbanEvidence = false;

  static constexpr std::array<float, 5> HeightsM{0, 6, 12, 24, 48};

  [[nodiscard]] std::optional<UrbanLot> LotAt(uint32_t column, uint32_t row) const noexcept;
  [[nodiscard]] std::expected<UrbanPatchBounds, std::string_view>
  BoundPatch(UrbanPatchRange patch, std::array<double, 2> groundAslMinMaxM) const noexcept;
};

[[nodiscard]] std::expected<UrbanArea, std::string>
GenerateUrbanArea(std::span<const OsmField *const> inputs,
                  UrbanAreaRecipe recipe = {},
                  const UrbanAreaLimits &limits = {});
}
#endif
