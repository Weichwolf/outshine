#ifndef OUTSHINE_TEST_CANONICALGROUNDTERRAINFIXTURE_H
#define OUTSHINE_TEST_CANONICALGROUNDTERRAINFIXTURE_H

#include "CanonicalGroundSourceFixture.h"
#include "Earth.h"
#include "HeightRaster.h"
#include "Json.h"
#include "ReadTextFile.h"
#include "Sha256.h"
#include "TerrariumRaster.h"
#include "Units.h"
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <utility>

namespace outshine::Test::CanonicalGroundFixture {
struct Terrain {
  HeightRaster Raster;
  std::array<int, 3> Tile{};
  std::string SourceDatum;
  std::string HeightMapping;

  [[nodiscard]] std::optional<double> At(LongitudeLatitude at) const {
    const double cells = std::ldexp(1., Tile[0]);
    const double x =
        ((at.LongitudeDeg + kDegPerHalfTurn) / (2 * kDegPerHalfTurn) * cells - Tile[1]) *
        (Raster.Cols - 1);
    const double y =
        ((1 - std::asinh(std::tan(at.LatitudeDeg * kDeg2Rad)) / kPi) / 2 * cells - Tile[2]) *
        (Raster.Rows - 1);
    if (!std::isfinite(x) || !std::isfinite(y) || x < 0 || y < 0 || x >= Raster.Cols - 1 ||
        y >= Raster.Rows - 1) {
      return std::nullopt;
    }
    const auto column = static_cast<size_t>(std::floor(x));
    const auto row = static_cast<size_t>(std::floor(y));
    const double u = x - static_cast<double>(column);
    const double v = y - static_cast<double>(row);
    const auto index = row * Raster.Cols + column;
    return (1 - v) * ((1 - u) * Raster.Meters[index] + u * Raster.Meters[index + 1]) +
           v * ((1 - u) * Raster.Meters[index + Raster.Cols] +
                u * Raster.Meters[index + Raster.Cols + 1]);
  }
};

[[nodiscard]] inline std::expected<Terrain, std::string> LoadTerrain() {
  constexpr auto kMostBytes = size_t{1024} * 1024;
  const auto raw = ReadTextFile(std::string(kRoot) + "dem.webp", kMostBytes);
  const auto meta = ReadTextFile(std::string(kRoot) + "manifest.json", kMostBytes);
  if (!raw || !meta) { return std::unexpected("original bound DEM unavailable"); }
  Json manifest;
  if (!manifest.Parse(meta->data(), meta->size())) {
    return std::unexpected("DEM binding invalid");
  }
  const auto root = manifest.Root();
  if (Sha256Hex(*raw) != root["files"]["dem.webp"]["sha256"].Str() ||
      !root["terrain"]["sampling"].StrEquals("bilinear boundary posts, side-1")) {
    return std::unexpected("original DEM pin or sampler binding changed");
  }
  const auto bytes = std::span(reinterpret_cast<const uint8_t *>(raw->data()), raw->size());
  auto raster = Generators::Terrain::DecodeTerrariumWebp(bytes);
  if (!raster) { return std::unexpected(raster.error()); }
  const auto terrain = root["terrain"];
  return Terrain{.Raster = std::move(*raster),
                 .Tile = {terrain["tile"][size_t{0}].Int(),
                          terrain["tile"][size_t{1}].Int(),
                          terrain["tile"][size_t{2}].Int()},
                 .SourceDatum = terrain["source_datum"].Str(),
                 .HeightMapping = terrain["height_mapping"].Str()};
}
}
#endif
