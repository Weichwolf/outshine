#include "AssetProduct.h"
#include <cstddef>
#include <cstdint>
#include <vector>
#include <string>
#include <utility>
#include <variant>

namespace outshine {
AssetProduct::AssetProduct(AssetRecord record, size_t consumerBytesMost)
    : Data_(std::move(record)), ConsumerBytesMost_(consumerBytesMost) {}

AssetProduct::AssetProduct(CachedAsset native, size_t consumerBytesMost)
    : Data_(std::move(native)), ConsumerBytesMost_(consumerBytesMost) {}

const AssetRecord &AssetProduct::Record() const noexcept {
  if (const auto *record = std::get_if<AssetRecord>(&Data_)) { return *record; }
  return std::get<CachedAsset>(Data_).Record();
}

const CachedAsset *AssetProduct::Native() const noexcept {
  return std::get_if<CachedAsset>(&Data_);
}

size_t AssetProduct::CapacityBytes() const noexcept {
  const auto &record = Record();
  size_t bytes = sizeof(AssetProduct) + record.Key.capacity() + record.Kind.capacity() +
                 record.Package.capacity() + record.Parent.capacity();
  if (record.RequestKey) { bytes += record.RequestKey->capacity(); }
  return bytes + BackingBytes();
}

size_t AssetProduct::BackingBytes() const noexcept {
  const auto *native = Native();
  if (native == nullptr) { return 0; }
  const auto *raw = std::get_if<std::vector<uint8_t>>(&native->Package_);
  return raw != nullptr ? raw->capacity() : std::get<ByteBlock>(native->Package_).CapacityBytes();
}

bool AssetProduct::RetainBacking(MemoryBudget::Reservation &source) {
  auto *native = std::get_if<CachedAsset>(&Data_);
  if (native == nullptr) { return true; }
  auto *raw = std::get_if<std::vector<uint8_t>>(&native->Package_);
  if (raw == nullptr) { return false; }
  MemoryBudget::Reservation retained;
  if (!source.TransferTo(retained, raw->capacity())) { return false; }
  auto lease = ByteBlock::Adopt(*raw, retained);
  if (!lease) {
    (void)retained.TransferTo(source, retained.Bytes());
    return false;
  }
  native->Package_ = std::move(*lease);
  return true;
}

void AssetProduct::SetRequestBinding(std::string request) {
  if (auto *record = std::get_if<AssetRecord>(&Data_)) {
    record->RequestKey = std::move(request);
  } else {
    std::get<CachedAsset>(Data_).Record_.RequestKey = std::move(request);
  }
}

void AssetProduct::ClearRequestBinding() noexcept {
  if (auto *record = std::get_if<AssetRecord>(&Data_)) {
    record->RequestKey.reset();
  } else {
    std::get<CachedAsset>(Data_).Record_.RequestKey.reset();
  }
}
}
