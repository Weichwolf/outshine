#ifndef OUTSHINE_TEST_CANONICALGROUNDTERRAINFIXTURE_H
#define OUTSHINE_TEST_CANONICALGROUNDTERRAINFIXTURE_H

#include "CanonicalGroundSourceFixture.h"
#include "Earth.h"
#include "HeightRaster.h"
#include "HeightField.h"
#include "TerrainGrid.h"
#include "TerrainFieldSet.h"
#include "TileSourceIdentity.h"
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
#include <memory>
#include <span>
#include <string>
#include <utility>

namespace outshine::Test::CanonicalGroundFixture {
struct Terrain {
  std::shared_ptr<const Ground::TerrainField> Raster;
  std::shared_ptr<const Ground::HeightField> Input;
  std::array<int, 3> Tile{};
  Generators::TerrainFieldSet::Binding Binding{};
  std::string SourceDatum;
  std::string HeightMapping;

  [[nodiscard]] std::optional<double> At(LongitudeLatitude at) const {
    const auto sampled = Input->SampleBilinearAt(at);
    return sampled ? sampled->Height.AslM() : std::nullopt;
  }

  [[nodiscard]] std::optional<double> ReferenceAt(LongitudeLatitude at) const {
    const double cells = std::ldexp(1., Tile[0]);
    const double x =
        ((at.LongitudeDeg + kDegPerHalfTurn) / (2 * kDegPerHalfTurn) * cells - Tile[1]) *
        (Raster->Cols() - 1);
    const double y =
        ((1 - std::asinh(std::tan(at.LatitudeDeg * kDeg2Rad)) / kPi) / 2 * cells - Tile[2]) *
        (Raster->Rows() - 1);
    if (!std::isfinite(x) || !std::isfinite(y) || x < 0 || y < 0 || x >= Raster->Cols() - 1 ||
        y >= Raster->Rows() - 1) {
      return std::nullopt;
    }
    const auto column = static_cast<size_t>(std::floor(x));
    const auto row = static_cast<size_t>(std::floor(y));
    const double u = x - static_cast<double>(column);
    const double v = y - static_cast<double>(row);
    const auto index = row * Raster->Cols() + column;
    return (1 - v) * ((1 - u) * Raster->Heights()[index] + u * Raster->Heights()[index + 1]) +
           v * ((1 - u) * Raster->Heights()[index + Raster->Cols()] +
                u * Raster->Heights()[index + Raster->Cols() + 1]);
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
  auto field = std::make_shared<Ground::TerrainField>(std::move(*raster));
  const auto tile = terrain["tile"];
  const Data::TileId address{.Zoom = tile[size_t{0}].Int(),
                             .X = static_cast<uint32_t>(tile[size_t{1}].Int()),
                             .Y = static_cast<uint32_t>(tile[size_t{2}].Int())};
  field->AddSource({.Kind = Data::DataKind::Elevation,
                    .Tile = address,
                    .SourceId = terrain["source_url"].Str(),
                    .Revision = terrain["source_sha256"].Str()});
  const Generators::TerrainFieldSet fields({{address, field}});
  const auto binding = fields.BilinearBindingAt(
      address.Zoom, terrain["source_datum"].Str(), terrain["height_mapping"].Str());
  if (!binding) { return std::unexpected("original terrain sampler/input binding unavailable"); }
  Ground::HeightField::Block block;
  if (!fields.ShareOriginalField(address, block)) {
    return std::unexpected("original native terrain lease unavailable");
  }
  auto input = Ground::HeightField::Of(address.Zoom, {std::move(block)});
  return Terrain{.Raster = std::move(field),
                 .Input = std::move(input),
                 .Tile = {address.Zoom, static_cast<int>(address.X), static_cast<int>(address.Y)},
                 .Binding = *binding,
                 .SourceDatum = terrain["source_datum"].Str(),
                 .HeightMapping = terrain["height_mapping"].Str()};
}
}
#endif
