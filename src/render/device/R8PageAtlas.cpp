#include "R8PageAtlas.h"
#include "UploadQueue.h"
#include <bit>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <utility>

namespace outshine::Render {
Native::Result<R8PageAtlasStorage> R8PageAtlasStorageFor(R8PageAtlasOptions options) {
  const auto shape = options.Layout;
  constexpr uint32_t most = 65536;
  if (!std::has_single_bit(shape.CoreTexels) || shape.CoreTexels > most || shape.Columns == 0 ||
      shape.Columns > options.Slots || options.Slots > most ||
      !std::has_single_bit(shape.Entries) || shape.Entries > most ||
      shape.Entries < options.Slots || shape.Probes == 0 || shape.Probes > shape.Entries ||
      options.EditsPerBatch == 0 || options.EditsPerBatch > options.Slots ||
      options.PendingBatches == 0 || options.PendingBatches > 64) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "invalid bounded R8 page atlas shape"});
  }
  const uint64_t side = uint64_t{shape.CoreTexels} + 2;
  const uint64_t rows = (uint64_t{options.Slots} + shape.Columns - 1) / shape.Columns;
  const uint64_t table = sizeof(FieldPageLayout) + uint64_t{shape.Entries} * sizeof(FieldPageEntry);
  const uint64_t rowBytes = ((side + 255) / 256) * 256;
  return R8PageAtlasStorage{
      .ResidentBytes = side * side * shape.Columns * rows + table * (options.PendingBatches + 1u),
      .TransferBytes = (table + rowBytes * side * options.EditsPerBatch) * options.PendingBatches,
      .CpuBytes = 2 * (uint64_t{options.Slots} * sizeof(std::optional<FieldPageAddress>) +
                       uint64_t{shape.Entries} * sizeof(FieldPageEntry)) +
                  options.Slots + uint64_t{options.PendingBatches} * sizeof(R8PageAtlas::Retired)};
}

Native::Result<R8PageAtlas> R8PageAtlas::Create(Native::Device &device,
                                                R8PageAtlasOptions options,
                                                R8PageAtlasStorage limits,
                                                UploadQueue *uploads) {
  const auto storage = R8PageAtlasStorageFor(options);
  if (!storage) { return std::unexpected(storage.error()); }
  const uint64_t side = uint64_t{options.Layout.CoreTexels} + 2;
  const uint64_t width = side * options.Layout.Columns;
  const uint64_t height =
      side * ((uint64_t{options.Slots} + options.Layout.Columns - 1) / options.Layout.Columns);
  constexpr auto use = Native::TextureUse::Sampled | Native::TextureUse::CopySource |
                       Native::TextureUse::CopyDestination;
  const uint64_t tableBytes =
      sizeof(FieldPageLayout) + uint64_t{options.Layout.Entries} * sizeof(FieldPageEntry);
  if (storage->ResidentBytes > limits.ResidentBytes ||
      storage->TransferBytes > limits.TransferBytes || storage->CpuBytes > limits.CpuBytes ||
      width > device.Limits().MaxTextureExtent || height > device.Limits().MaxTextureExtent ||
      tableBytes > device.Limits().MaxStorageBufferBytes ||
      !device.Supports(Native::Format::R8Unorm, use, true) ||
      (uploads != nullptr && &uploads->Device() != &device)) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Invalid,
                      .Message = "R8 page atlas exceeds admitted device storage"});
  }
  auto image = device.CreateTexture({.Width = static_cast<uint32_t>(width),
                                     .Height = static_cast<uint32_t>(height),
                                     .Texels = Native::Format::R8Unorm,
                                     .Use = use});
  if (!image) { return std::unexpected(image.error()); }
  auto sampler = device.CreateSampler({.MaxLod = 0});
  if (!sampler) { return std::unexpected(sampler.error()); }
  R8PageAtlas atlas;
  atlas.Device_ = &device;
  atlas.Uploads_ = uploads;
  atlas.Options_ = options;
  atlas.Storage_ = *storage;
  atlas.Image_ = std::move(*image);
  atlas.Sampling_ = std::move(*sampler);
  atlas.Slots_.resize(options.Slots);
  atlas.Retired_.reserve(options.PendingBatches);
  auto table = BuildFieldPageTable(atlas.Slots_, options.Layout.Entries, options.Layout.Probes);
  if (!table) { return std::unexpected(table.error()); }
  auto initialized = atlas.Publish({}, std::move(*table));
  if (!initialized) { return std::unexpected(initialized.error()); }
  return atlas;
}
}
