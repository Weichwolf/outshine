#include "AssetProduct.h"
#include <cstddef>
#include <string>
#include <utility>
#include <variant>

namespace outshine {
AssetProduct::AssetProduct(AssetRecord record) : Data_(std::move(record)) {}

AssetProduct::AssetProduct(CachedAsset native) : Data_(std::move(native)) {}

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
  return native != nullptr ? native->Package_.capacity() : 0;
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
