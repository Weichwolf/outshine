#include "WaterAssetFormat.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace outshine::Generators::WaterFormat {
namespace {
constexpr size_t kSourceMinimumBytes = 37;
constexpr size_t kInputMinimumBytes = 24;
constexpr int kLatitudeLimitDeg = 90;
constexpr int kLongitudeLimitDeg = 180;

template <class T, class Fields>
bool Optional(BinaryValueWriter &out, const std::optional<T> &value, Fields fields) {
  return out(value.has_value()) && (!value || fields(out, *value));
}

template <class T, class Fields>
bool Optional(BinaryValueReader &in, std::optional<T> &value, Fields fields) {
  bool present = false;
  if (!in(present)) { return false; }
  value.reset();
  if (!present) { return true; }
  value.emplace();
  return fields(in, *value);
}

bool Sources(auto &archive, auto &sources) {
  return Records<kSourceMinimumBytes>(archive, sources, [](auto &out, auto &source) {
    auto &id = source.Identity;
    return out(id.From, id.Kind, id.Tile.Zoom, id.Tile.X, id.Tile.Y) &&
           Optional(
               out,
               id.NativeCell,
               [](auto &cellOut, auto &cell) { return cellOut(cell.SouthDeg, cell.WestDeg); }) &&
           out.Text(id.SourceId) && out.Text(id.Revision) && out.Text(source.InputDigest) &&
           out(source.FeatureCount);
  });
}

bool Inputs(auto &archive, auto &inputs) {
  return Records<kInputMinimumBytes>(archive, inputs, [](auto &out, auto &input) {
    return out(input.Source, input.Feature) &&
           Optional(out, input.FeatureId, [](auto &idOut, auto &id) { return idOut(id); }) &&
           out.Text(input.Layer) && out.Text(input.Kind) && out.Text(input.Crossing) &&
           out(input.Bridge, input.Tunnel) &&
           Optional(
               out, input.Intermittent, [](auto &flagOut, auto &flag) { return flagOut(flag); });
  });
}

bool ValidSource(const WaterAsset::Source &source) noexcept {
  const auto &id = source.Identity;
  if (id.From < Data::TileSourceIdentity::Origin::Provider ||
      id.From > Data::TileSourceIdentity::Origin::Shaped || id.Kind > Data::DataKind::Weather ||
      id.Tile.Zoom < 0 || id.Tile.Zoom > Data::TileId::MaximumZoom) {
    return false;
  }
  const uint32_t across = uint32_t{1} << static_cast<unsigned>(id.Tile.Zoom);
  return id.Tile.X < across && id.Tile.Y < across &&
         (!id.NativeCell || (id.NativeCell->SouthDeg >= -kLatitudeLimitDeg &&
                             id.NativeCell->SouthDeg < kLatitudeLimitDeg &&
                             id.NativeCell->WestDeg >= -kLongitudeLimitDeg &&
                             id.NativeCell->WestDeg < kLongitudeLimitDeg));
}
}

bool Metadata(BinaryValueWriter &out, const WaterAsset::Data &data) {
  return Sources(out, data.Sources) && Inputs(out, data.Inputs);
}

bool Metadata(BinaryValueReader &in, WaterAsset::Data &data) {
  return Sources(in, data.Sources) && Inputs(in, data.Inputs);
}

bool ValidMetadata(const WaterAsset::Data &data) noexcept {
  const auto bound = [&](const auto &record) {
    return record.Input == WaterAsset::NoInput || record.Input < data.Inputs.size();
  };
  return (data.Sources.empty() || data.Sources.size() == data.Tiles.size()) &&
         std::ranges::all_of(data.Sources, ValidSource) &&
         std::ranges::all_of(data.Inputs,
                             [&](const auto &input) {
                               return input.Source < data.Sources.size() &&
                                      input.Feature < data.Sources[input.Source].FeatureCount;
                             }) &&
         std::ranges::all_of(data.Surfaces, bound) && std::ranges::all_of(data.Courses, bound);
}
}
