#ifndef OUTSHINE_ENGINE_STREAMING_GROUNDPATCHPREPARATION_H
#define OUTSHINE_ENGINE_STREAMING_GROUNDPATCHPREPARATION_H

#include "GroundSnapshot.h"
#include "PreparedTerrainAssets.h"
#include "Tasks.h"
#include <expected>
#include <functional>
#include <memory>
#include <stop_token>
#include <string>

namespace outshine {
class GroundPatchPreparation {
public:
  GroundPatchPreparation(Tasks &pool,
                         std::shared_ptr<Generators::PreparedTerrainAssets> assets,
                         std::string key,
                         Generators::Tile region,
                         int side);
  ~GroundPatchPreparation();
  GroundPatchPreparation(const GroundPatchPreparation &) = delete;
  GroundPatchPreparation &operator=(const GroundPatchPreparation &) = delete;

  [[nodiscard]] const std::string &Key() const noexcept { return Key_; }

  [[nodiscard]] std::expected<std::shared_ptr<const Generators::GroundPatch>, std::string>
  Advance(const GroundQuery &heights, Generators::Snapped *how);

private:
  using Result = std::expected<std::shared_ptr<const Generators::GroundPatch>, std::string>;
  enum class Phase { Reading, Generating, Storing, Ready, Failed };
  struct Work;
  void Post(std::function<Result()> operation);

  Tasks &Pool_;
  std::shared_ptr<Generators::PreparedTerrainAssets> Assets_;
  std::string Key_;
  Generators::Tile Region_;
  int Side_;
  Phase Phase_ = Phase::Reading;
  std::shared_ptr<Work> Work_;
  std::stop_source Stop_;
  Result Result_;
};
}
#endif
