#ifndef OUTSHINE_CONTENT_ASSETS_ASSETPRODUCT_H
#define OUTSHINE_CONTENT_ASSETS_ASSETPRODUCT_H
#include "AssetCache.h"
#include <cstddef>
#include <string>
#include <variant>

namespace outshine {
class AssetProduct {
public:
  explicit AssetProduct(AssetRecord record, size_t consumerBytesMost = 0);
  explicit AssetProduct(CachedAsset native, size_t consumerBytesMost = 0);
  [[nodiscard]] const AssetRecord &Record() const noexcept;
  [[nodiscard]] const CachedAsset *Native() const noexcept;
  // Object/string capacities and the whole retained package, excluding allocator overhead.
  [[nodiscard]] size_t CapacityBytes() const noexcept;
  [[nodiscard]] size_t BackingBytes() const noexcept;

  [[nodiscard]] size_t ConsumerBytesMost() const noexcept { return ConsumerBytesMost_; }

  void SetRequestBinding(std::string request);
  void ClearRequestBinding() noexcept;

private:
  std::variant<AssetRecord, CachedAsset> Data_;
  size_t ConsumerBytesMost_ = 0;
};
}
#endif
