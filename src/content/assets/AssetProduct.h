#ifndef OUTSHINE_CONTENT_ASSETS_ASSETPRODUCT_H
#define OUTSHINE_CONTENT_ASSETS_ASSETPRODUCT_H
#include "AssetCache.h"
#include <cstddef>
#include <string>
#include <variant>

namespace outshine {
class AssetProduct {
public:
  explicit AssetProduct(AssetRecord record);
  explicit AssetProduct(CachedAsset native);
  [[nodiscard]] const AssetRecord &Record() const noexcept;
  [[nodiscard]] const CachedAsset *Native() const noexcept;
  // Object/string capacities and the whole retained package, excluding allocator overhead.
  [[nodiscard]] size_t CapacityBytes() const noexcept;
  [[nodiscard]] size_t BackingBytes() const noexcept;
  void SetRequestBinding(std::string request);
  void ClearRequestBinding() noexcept;

private:
  std::variant<AssetRecord, CachedAsset> Data_;
};
}
#endif
