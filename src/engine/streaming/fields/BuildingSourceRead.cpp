#include "BuildingSourceRead.h"
#include <expected>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

namespace outshine {
BuildingSourceRead::BuildingSourceRead(std::shared_ptr<Generators::PreparedBuildingAssets> assets,
                                       std::string key)
    : Assets_(std::move(assets)), Key_(std::move(key)) {}

std::expected<Generators::PreparedBuildingAssets::Base, Generators::StructureBakeError>
BuildingSourceRead::Load() {
  const std::scoped_lock lock(Lock_);
  if (Base_) { return Base_; }
  if (!Assets_) {
    return std::unexpected(Generators::StructureBakeErrorKind::ArtifactInvalidProduct);
  }
  auto loaded = Assets_->Load(Key_);
  if (!loaded) { return std::unexpected(loaded.error()); }
  Base_ = *loaded;
  return Base_;
}
}
