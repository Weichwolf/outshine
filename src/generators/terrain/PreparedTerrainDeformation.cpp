#include "PreparedTerrainDeformation.h"
#include "ByteArchive.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <tuple>
#include <type_traits>
#include <utility>

#include <cstddef>
#include <cstdint>
#include <vector>
#include <optional>
#include <string>
#include <span>

namespace outshine::Generators {
namespace {
constexpr uint32_t kFormat = 0x31445450;
constexpr uint32_t kPagesMost = 65536;

template <class Archive, class T> bool Scalar(Archive &archive, T &value) {
  if constexpr (std::is_floating_point_v<T>) {
    if constexpr (std::is_same_v<Archive, ByteWriter>) {
      if (!std::isfinite(value)) { return false; }
    }
    return archive.Number(value) && std::isfinite(value);
  } else {
    return archive.Number(value);
  }
}

template <class Archive, class... T> bool Scalars(Archive &archive, T &...values) {
  return (Scalar(archive, values) && ...);
}

bool Nodes(ByteWriter &out, const std::vector<float> &values) {
  if (values.size() > kTerrainDeformationBytesMost / sizeof(float) ||
      !out.Number(static_cast<uint32_t>(values.size())) ||
      !std::ranges::all_of(values, [](float value) { return std::isfinite(value); })) {
    return false;
  }
  if constexpr (std::endian::native == std::endian::little) {
    return out.Put(
        {reinterpret_cast<const uint8_t *>(values.data()), values.size() * sizeof(float)});
  }
  return std::ranges::all_of(values, [&out](float value) { return out.Number(value); });
}

bool Nodes(ByteReader &in, std::vector<float> &values, size_t &remaining) {
  uint32_t count = 0;
  if (!in.Number(count) || count > remaining / sizeof(float)) { return false; }
  const auto bytes = in.Take(static_cast<size_t>(count) * sizeof(float));
  if (!bytes) { return false; }
  remaining -= bytes->size();
  values.resize(count);
  if (!values.empty()) { std::memcpy(values.data(), bytes->data(), bytes->size()); }
  for (float &value : values) {
    value = LittleEndian(value);
    if (!std::isfinite(value)) { return false; }
  }
  return true;
}

template <class Archive> bool Effects(Archive &archive, EarthworkMetrics &value) {
  return Scalars(archive,
                 value.Stamps,
                 value.Unreached,
                 value.Nodes,
                 value.Contested,
                 value.AboveM,
                 value.BelowM,
                 value.UnfilledM,
                 value.WasAboveM,
                 value.WasBelowM);
}

template <class Archive> bool Effects(Archive &archive, PressedTerrain &value) {
  return Scalars(
             archive, value.Nodes, value.Structures, value.Held, value.DeepestM, value.RaisedM) &&
         Effects(archive, value.Pads) && Effects(archive, value.Corridors);
}

bool Page(ByteWriter &out, const Sheet &page) {
  return out.Number(page.Tile.Zoom) && out.Number(page.Tile.X) && out.Number(page.Tile.Y) &&
         out.Number(page.Side) && out.Number(page.Postings) &&
         out.Number(static_cast<uint8_t>(page.Virtual)) && out.Number(page.SourceZoom) &&
         Nodes(out, page.Nodes);
}

bool Page(ByteReader &in, Sheet &page, size_t &remaining) {
  uint8_t virtualPage = 0;
  if (!Scalars(in,
               page.Tile.Zoom,
               page.Tile.X,
               page.Tile.Y,
               page.Side,
               page.Postings,
               virtualPage,
               page.SourceZoom) ||
      page.Tile.Zoom < 0 || page.Tile.Zoom > Data::TileId::MaximumZoom ||
      page.Tile.X >= (uint32_t{1} << static_cast<unsigned>(page.Tile.Zoom)) ||
      page.Tile.Y >= (uint32_t{1} << static_cast<unsigned>(page.Tile.Zoom)) || page.Side < 0 ||
      page.Side > 1024 || page.Postings > 4096 || virtualPage > 1 || page.SourceZoom < -1 ||
      page.SourceZoom > Data::TileId::MaximumZoom) {
    return false;
  }
  page.Virtual = virtualPage != 0;
  return Nodes(in, page.Nodes, remaining);
}
}

std::optional<std::vector<uint8_t>> EncodeTerrainDeformation(const std::string &key,
                                                             std::span<const Sheet> pages,
                                                             const PressedTerrain &measures) {
  if (key.size() != 64 || pages.size() > kPagesMost) { return std::nullopt; }
  ByteWriter out(kTerrainDeformationBytesMost);
  size_t bytes = sizeof(PreparedTerrainDeformation);
  for (const Sheet &page : pages) { bytes += sizeof(Sheet) + page.Nodes.size() * sizeof(float); }
  out.Reserve(bytes + 512);
  if (!out.Number(kFormat) ||
      !out.Put({reinterpret_cast<const uint8_t *>(key.data()), key.size()}) ||
      !out.Number(static_cast<uint32_t>(pages.size()))) {
    return std::nullopt;
  }
  for (const Sheet &page : pages) {
    if (!Page(out, page)) { return std::nullopt; }
  }
  auto effects = measures;
  if (!Effects(out, effects)) { return std::nullopt; }
  return std::move(out).TakeBytes();
}

std::optional<std::vector<uint8_t>>
EncodeTerrainDeformation(const std::string &key, const PreparedTerrainDeformation &product) {
  return EncodeTerrainDeformation(key, product.Pages, product.Effects);
}

std::optional<PreparedTerrainDeformation> DecodeTerrainDeformation(const std::string &key,
                                                                   std::span<const uint8_t> bytes) {
  if (key.size() != 64 || bytes.size() > kTerrainDeformationBytesMost) { return std::nullopt; }
  ByteReader in(bytes);
  uint32_t format = 0;
  uint32_t count = 0;
  if (!in.Number(format) || format != kFormat) { return std::nullopt; }
  const auto identity = in.Take(key.size());
  if (!identity || !std::equal(identity->begin(), identity->end(), key.begin(), key.end()) ||
      !in.Number(count) || count > kPagesMost ||
      count > kTerrainDeformationBytesMost / sizeof(Sheet)) {
    return std::nullopt;
  }
  size_t remaining = kTerrainDeformationBytesMost - static_cast<size_t>(count) * sizeof(Sheet);
  PreparedTerrainDeformation product;
  product.Pages.resize(count);
  for (Sheet &page : product.Pages) {
    if (!Page(in, page, remaining)) { return std::nullopt; }
  }
  if (!Effects(in, product.Effects) || in.Remaining() != 0) { return std::nullopt; }
  return product;
}
}
