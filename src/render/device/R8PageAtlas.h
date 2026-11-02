#ifndef OUTSHINE_RENDER_DEVICE_R8PAGEATLAS_H
#define OUTSHINE_RENDER_DEVICE_R8PAGEATLAS_H

#include "FieldPageTable.h"
#include "native/Device.h"
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace outshine::Render {
class UploadQueue;

struct R8PageAtlasOptions {
  FieldPageLayout Layout{.CoreTexels = 128, .Columns = 32, .Entries = 4096, .Probes = 16};
  uint32_t Slots = 1024;
  uint32_t EditsPerBatch = 64;
  uint32_t PendingBatches = 2;
};

struct R8PageAtlasStorage {
  uint64_t ResidentBytes = 0;
  uint64_t TransferBytes = 0;
  uint64_t CpuBytes = 0;
};

struct R8PageEdit {
  uint32_t Slot = 0;
  std::optional<FieldPageAddress> Address;
  std::span<const uint8_t> Pixels;
};

[[nodiscard]] Native::Result<R8PageAtlasStorage> R8PageAtlasStorageFor(R8PageAtlasOptions options);

class R8PageAtlas final {
public:
  R8PageAtlas(R8PageAtlas &&) noexcept = default;
  R8PageAtlas &operator=(R8PageAtlas &&) noexcept = default;
  R8PageAtlas(const R8PageAtlas &) = delete;
  R8PageAtlas &operator=(const R8PageAtlas &) = delete;

  [[nodiscard]] static Native::Result<R8PageAtlas> Create(Native::Device &device,
                                                          R8PageAtlasOptions options,
                                                          R8PageAtlasStorage limits,
                                                          UploadQueue *uploads = nullptr);

  // Call between frame submissions on the same queue; rejection preserves the published mapping.
  [[nodiscard]] Native::Result<std::optional<Native::Completion>>
  Update(std::span<const R8PageEdit> edits);
  [[nodiscard]] Native::Result<void> Collect();

  [[nodiscard]] std::optional<uint32_t> Find(FieldPageAddress address) const noexcept {
    return FindFieldPageSlot(Table_, Options_.Layout.Probes, address);
  }

  [[nodiscard]] const Native::Texture &Image() const noexcept { return *Image_; }

  [[nodiscard]] const Native::Sampler &Sampling() const noexcept { return *Sampling_; }

  [[nodiscard]] const Native::Buffer &Lookup() const noexcept { return *Lookup_; }

  [[nodiscard]] const R8PageAtlasOptions &Options() const noexcept { return Options_; }

  [[nodiscard]] const R8PageAtlasStorage &Storage() const noexcept { return Storage_; }

  [[nodiscard]] size_t PendingBatches() const noexcept { return Retired_.size(); }

  [[nodiscard]] std::optional<Native::Completion> LastUpload() const noexcept {
    return Retired_.empty() ? std::nullopt : std::optional{Retired_.back().Completion};
  }

private:
  friend Native::Result<R8PageAtlasStorage> R8PageAtlasStorageFor(R8PageAtlasOptions options);
  R8PageAtlas() = default;

  struct Retired {
    std::unique_ptr<Native::Buffer> Lookup;
    Native::Completion Completion;
  };

  [[nodiscard]] Native::Result<Native::Completion> Publish(std::span<const R8PageEdit> edits,
                                                           std::vector<FieldPageEntry> table);

  Native::Device *Device_ = nullptr;
  UploadQueue *Uploads_ = nullptr;
  R8PageAtlasOptions Options_;
  R8PageAtlasStorage Storage_;
  std::unique_ptr<Native::Texture> Image_;
  std::unique_ptr<Native::Sampler> Sampling_;
  std::unique_ptr<Native::Buffer> Lookup_;
  std::vector<std::optional<FieldPageAddress>> Slots_;
  std::vector<FieldPageEntry> Table_;
  std::vector<Retired> Retired_;
};
}
#endif
