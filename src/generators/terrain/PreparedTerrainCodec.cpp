#include "PreparedTerrainCodec.h"
#include "ByteArchive.h"
#include "TileSourceIdentity.h"
#include <math/Units.h>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <limits>
#include <string>
#include <utility>

namespace outshine::Generators {
namespace {

constexpr uint32_t kFormat = 0x31465450;
constexpr uint32_t kSourcesMost = 4096;
constexpr size_t kTextMost = 4096;

bool Text(ByteWriter &out, const std::string &value) {
  return value.size() <= kTextMost && out.Number(static_cast<uint32_t>(value.size())) &&
         out.Put({reinterpret_cast<const uint8_t *>(value.data()), value.size()});
}

bool Text(ByteReader &in, std::string &value, size_t &allocation) {
  uint32_t size = 0;
  if (!in.Number(size) || size > kTextMost || size > allocation) { return false; }
  const auto bytes = in.Take(size);
  if (!bytes) { return false; }
  allocation -= size;
  value.assign(reinterpret_cast<const char *>(bytes->data()), size);
  return true;
}

bool Source(ByteWriter &out, const Data::TileSourceIdentity &source) {
  return out.Number(static_cast<uint32_t>(source.From)) &&
         out.Number(static_cast<uint32_t>(source.Kind)) && out.Number(source.Tile.Zoom) &&
         out.Number(source.Tile.X) && out.Number(source.Tile.Y) &&
         out.Number(static_cast<uint8_t>(source.NativeCell.has_value())) &&
         (!source.NativeCell ||
          (out.Number(source.NativeCell->SouthDeg) && out.Number(source.NativeCell->WestDeg))) &&
         Text(out, source.SourceId) && Text(out, source.Revision);
}

bool Source(ByteReader &in, Data::TileSourceIdentity &source, size_t &allocation) {
  uint32_t origin = 0;
  uint32_t kind = 0;
  uint8_t native = 0;
  if (!in.Number(origin) ||
      origin > static_cast<uint32_t>(Data::TileSourceIdentity::Origin::Shaped) ||
      !in.Number(kind) || kind != static_cast<uint32_t>(Data::DataKind::Elevation) ||
      !in.Number(source.Tile.Zoom) || source.Tile.Zoom < 0 ||
      source.Tile.Zoom > Data::TileId::MaximumZoom || !in.Number(source.Tile.X) ||
      !in.Number(source.Tile.Y) ||
      source.Tile.X >= (uint32_t{1} << static_cast<unsigned>(source.Tile.Zoom)) ||
      source.Tile.Y >= (uint32_t{1} << static_cast<unsigned>(source.Tile.Zoom)) ||
      !in.Number(native) || native > 1) {
    return false;
  }
  if (native != 0) {
    Data::CellId cell;
    if (!in.Number(cell.SouthDeg) || !in.Number(cell.WestDeg) ||
        cell.SouthDeg < -kDegPerHalfTurn / 2 || cell.SouthDeg >= kDegPerHalfTurn / 2 ||
        cell.WestDeg < -kDegPerHalfTurn || cell.WestDeg >= kDegPerHalfTurn) {
      return false;
    }
    source.NativeCell = cell;
  }
  source.From = static_cast<Data::TileSourceIdentity::Origin>(origin);
  source.Kind = static_cast<Data::DataKind>(kind);
  return Text(in, source.SourceId, allocation) && !source.SourceId.empty() &&
         Text(in, source.Revision, allocation);
}

}

std::optional<std::vector<uint8_t>>
EncodePreparedTerrain(Data::TileId at, const ::outshine::Ground::TerrainField &field) {
  if (!field.Meshable() || field.Sources().empty() || field.Sources().size() > kSourcesMost) {
    return std::nullopt;
  }
  const size_t nodes = static_cast<size_t>(field.Rows()) * field.Cols();
  if (nodes > kPreparedTerrainBytesMost / sizeof(float) ||
      !std::ranges::all_of(std::span(field.Data(), nodes),
                           [](float value) { return std::isfinite(value); })) {
    return std::nullopt;
  }
  ByteWriter out(kPreparedTerrainBytesMost);
  out.Reserve(nodes * sizeof(float) + 256);
  if (!out.Number(kFormat) || !out.Number(at.Zoom) || !out.Number(at.X) || !out.Number(at.Y) ||
      !out.Number(field.Rows()) || !out.Number(field.Cols()) ||
      !out.Number(static_cast<uint8_t>(field.HasMissingBoundary())) ||
      !out.Number(static_cast<uint32_t>(field.Sources().size()))) {
    return std::nullopt;
  }
  for (const auto &source : field.Sources()) {
    if (!Source(out, source)) { return std::nullopt; }
  }
  if constexpr (std::endian::native == std::endian::little) {
    if (!out.Put({reinterpret_cast<const uint8_t *>(field.Data()), nodes * sizeof(float)})) {
      return std::nullopt;
    }
  } else {
    for (const float value : std::span(field.Data(), nodes)) {
      if (!out.Number(value)) { return std::nullopt; }
    }
  }
  return std::move(out).TakeBytes();
}

std::optional<::outshine::Ground::TerrainField>
DecodePreparedTerrain(Data::TileId at, std::span<const uint8_t> bytes, size_t residentBytesMost) {
  ByteReader in(bytes);
  uint32_t format = 0;
  Data::TileId stored;
  uint32_t rows = 0;
  uint32_t cols = 0;
  uint32_t count = 0;
  uint8_t missing = 0;
  if (!in.Number(format) || format != kFormat || !in.Number(stored.Zoom) || !in.Number(stored.X) ||
      !in.Number(stored.Y) || stored != at || !in.Number(rows) || !in.Number(cols) || rows < 2 ||
      cols < 2 || !in.Number(missing) || missing > 1 || !in.Number(count) || count == 0 ||
      count > kSourcesMost) {
    return std::nullopt;
  }
  const uint64_t nodes = static_cast<uint64_t>(rows) * cols;
  if (nodes > in.Remaining() / sizeof(float) || nodes > residentBytesMost / sizeof(float)) {
    return std::nullopt;
  }
  size_t allocation = residentBytesMost - static_cast<size_t>(nodes) * sizeof(float);
  if (count > allocation / sizeof(Data::TileSourceIdentity)) { return std::nullopt; }
  allocation -= count * sizeof(Data::TileSourceIdentity);
  ::outshine::Ground::TerrainField field(rows, cols);
  for (uint32_t index = 0; index < count; ++index) {
    Data::TileSourceIdentity source;
    if (!Source(in, source, allocation)) { return std::nullopt; }
    field.AddSource(std::move(source));
  }
  const auto samples = in.Take(static_cast<size_t>(nodes) * sizeof(float));
  if (!samples || in.Remaining() != 0) { return std::nullopt; }
  const auto view = field.Field();
  auto *data = view.data_handle();
  std::memcpy(data, samples->data(), samples->size());
  for (size_t index = 0; index < nodes; ++index) {
    data[index] = LittleEndian(data[index]);
    if (!std::isfinite(data[index])) { return std::nullopt; }
  }
  if (missing != 0) { field.MarkMissingBoundary(); }
  return field;
}

}
