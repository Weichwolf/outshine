#ifndef OUTSHINE_ENGINE_STREAMING_FIELDS_BUILDINGSOURCEREAD_H
#define OUTSHINE_ENGINE_STREAMING_FIELDS_BUILDINGSOURCEREAD_H
#include "PreparedBuildingAssets.h"
#include <expected>
#include <memory>
#include <mutex>
#include <string>

namespace outshine {
class BuildingSourceRead {
public:
  BuildingSourceRead(std::shared_ptr<Generators::PreparedBuildingAssets> assets, std::string key);

  [[nodiscard]] const std::string &Key() const noexcept { return Key_; }

  [[nodiscard]] std::expected<Generators::PreparedBuildingAssets::Base,
                              Generators::StructureBakeError>
  Load();

private:
  std::shared_ptr<Generators::PreparedBuildingAssets> Assets_;
  std::string Key_;
  std::mutex Lock_;
  Generators::PreparedBuildingAssets::Base Base_;
};
}
#endif
