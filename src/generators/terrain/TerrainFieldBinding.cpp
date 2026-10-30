#include "TerrainFieldSet.h"
#include "HeightField.h"
#include "Address.h"
#include "BinaryValueArchive.h"
#include "Sha256.h"
#include "TerrainGrid.h"
#include "TileSourceIdentity.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <span>
#include <string_view>
#include <tuple>
#include <vector>

namespace outshine::Generators {
namespace {
using Error = TerrainFieldSet::BindingError;
using Entry = TerrainFieldSet::Entry;
constexpr size_t kMetadataBytesMost = size_t{256} * 1024u * 1024u;

std::expected<std::vector<size_t>, Error>
CanonicalFields(std::span<const Entry> fields, int zoom, bool unique) {
  const auto included = [zoom, unique](const Entry &entry) {
    return unique ? entry.first.Zoom == zoom : entry.first.Zoom >= 0 && entry.first.Zoom <= zoom;
  };
  std::vector<size_t> order;
  order.reserve(static_cast<size_t>(std::ranges::count_if(fields, included)));
  for (size_t at = 0; at < fields.size(); ++at) {
    if (included(fields[at])) { order.push_back(at); }
  }
  std::ranges::sort(order, [&](size_t left, size_t right) {
    const auto &a = fields[left].first;
    const auto &b = fields[right].first;
    return std::tie(a.Zoom, a.X, a.Y, left) < std::tie(b.Zoom, b.X, b.Y, right);
  });
  std::optional<Data::TileId> previous;
  size_t kept = 0;
  for (const size_t at : order) {
    const auto &[tile, field] = fields[at];
    if (previous == tile) {
      if (unique) { return std::unexpected(Error::InvalidRequest); }
      continue;
    }
    previous = tile;
    if (field && field->Meshable()) { order[kept++] = at; }
  }
  order.resize(kept);
  return order;
}

bool WriteSource(BinaryValueWriter &out, const Data::TileSourceIdentity &source) {
  if (!out(source.From,
           source.Kind,
           static_cast<int32_t>(source.Tile.Zoom),
           source.Tile.X,
           source.Tile.Y,
           source.NativeCell.has_value())) {
    return false;
  }
  if (source.NativeCell && !out(static_cast<int32_t>(source.NativeCell->SouthDeg),
                                static_cast<int32_t>(source.NativeCell->WestDeg))) {
    return false;
  }
  return out.Text(source.SourceId) && out.Text(source.Revision);
}

std::expected<std::array<uint8_t, 32>, Error>
RasterDigest(const ::outshine::Ground::TerrainField &field, size_t bytes) {
  if constexpr (std::endian::native == std::endian::little) {
    return Sha256Digest(field.Data(), bytes);
  } else {
    BinaryValueWriter out(bytes);
    out.Out.Reserve(bytes);
    for (const float value : std::span(field.Data(), bytes / sizeof(float))) {
      if (!out(value)) { return std::unexpected(Error::OverBudget); }
    }
    return Sha256Digest(out.Out.Bytes().data(), out.Out.Bytes().size());
  }
}
}

std::expected<TerrainFieldSet::Binding, TerrainFieldSet::BindingError>
TerrainFieldSet::BindingAt(int zoom) const {
  return BindAt(zoom, {}, {}, {});
}

std::expected<TerrainFieldSet::Binding, TerrainFieldSet::BindingError>
TerrainFieldSet::BilinearBindingAt(int zoom,
                                   std::string_view sourceDatum,
                                   std::string_view heightMapping) const {
  if (sourceDatum.empty() || heightMapping.empty()) {
    return std::unexpected(BindingError::InvalidRequest);
  }
  return BindAt(
      zoom, ::outshine::Ground::HeightField::BilinearSamplingSemantics, sourceDatum, heightMapping);
}

std::expected<TerrainFieldSet::Binding, TerrainFieldSet::BindingError>
TerrainFieldSet::BindAt(int zoom,
                        std::string_view sampling,
                        std::string_view sourceDatum,
                        std::string_view heightMapping) const {
  if (zoom < 0 || zoom > Data::TileId::MaximumZoom) {
    return std::unexpected(BindingError::InvalidRequest);
  }
  const auto order = CanonicalFields(Fields_, zoom, !sampling.empty());
  if (!order) { return std::unexpected(order.error()); }
  if (!sampling.empty() && order->empty()) { return std::unexpected(BindingError::InvalidRequest); }
  BinaryValueWriter out(kMetadataBytesMost);
  const auto *const schema =
      sampling.empty() ? "sourced-terrain-view-1" : "sourced-terrain-bilinear-view-1";
  if (!out.Text(schema) ||
      (!sampling.empty() &&
       (!out.Text(sampling) || !out.Text(sourceDatum) || !out.Text(heightMapping))) ||
      !out(static_cast<int32_t>(zoom), static_cast<uint64_t>(order->size()))) {
    return std::unexpected(BindingError::OverBudget);
  }
  Binding binding{};
  for (const size_t at : *order) {
    const auto &[tile, held] = Fields_[at];
    const auto &field = *held;
    if (field.Rows() > std::numeric_limits<size_t>::max() / sizeof(float) / field.Cols()) {
      return std::unexpected(BindingError::OverBudget);
    }
    const size_t bytes = static_cast<size_t>(field.Rows()) * field.Cols() * sizeof(float);
    if (bytes > std::numeric_limits<size_t>::max() - binding.RasterBytes) {
      return std::unexpected(BindingError::OverBudget);
    }
    const auto digest = RasterDigest(field, bytes);
    if (!digest ||
        !out(static_cast<int32_t>(tile.Zoom),
             tile.X,
             tile.Y,
             field.Rows(),
             field.Cols(),
             field.HasMissingBoundary(),
             *digest,
             static_cast<uint64_t>(field.Sources().size())) ||
        !std::ranges::all_of(field.Sources(),
                             [&](const auto &source) { return WriteSource(out, source); })) {
      return std::unexpected(BindingError::OverBudget);
    }
    ++binding.Fields;
    binding.RasterBytes += bytes;
    if constexpr (std::endian::native != std::endian::little) {
      binding.CopiedRasterBytes += bytes;
    }
  }
  binding.MetadataBytes = out.Out.Bytes().size();
  binding.Key = Sha256Digest(out.Out.Bytes().data(), out.Out.Bytes().size());
  return binding;
}
}
