#ifndef OUTSHINE_ENGINE_REGION_ROADPAGERESTORATION_H
#define OUTSHINE_ENGINE_REGION_ROADPAGERESTORATION_H

#include "RoadPageQueue.h"
#include "RoadPageManifest.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace outshine {
class RoadPageRestoration {
public:
  [[nodiscard]] static std::expected<std::unique_ptr<RoadPageRestoration>, std::string>
  Begin(Tasks &pool,
        std::shared_ptr<Generators::PreparedRoadPages> pages,
        std::shared_ptr<const Generators::RoadPageManifest> manifest,
        std::optional<std::span<const uint32_t>> selected = std::nullopt,
        std::string manifestKey = {});
  [[nodiscard]] std::expected<bool, std::string> Advance(Geometry &ground);

  [[nodiscard]] bool Missing() const noexcept { return Missing_; }

  [[nodiscard]] size_t Pages() const noexcept { return Next_; }

  [[nodiscard]] size_t NativeBytes() const noexcept { return NativeBytes_; }

private:
  enum class Stage : uint8_t { Material, Request, Waiting, Positions, Normals, Colours, Triangles };
  RoadPageRestoration(Tasks &pool,
                      std::shared_ptr<Generators::PreparedRoadPages> pages,
                      std::shared_ptr<const Generators::RoadPageManifest> manifest);
  [[nodiscard]] std::expected<bool, std::string> Collect(Geometry &ground);
  [[nodiscard]] std::expected<bool, std::string> Transfer(Geometry &ground);
  RoadPageQueue Queue_;
  std::shared_ptr<const Generators::RoadPageManifest> Manifest_;
  std::vector<uint32_t> Selected_;
  std::shared_ptr<const Generators::RoadPageAsset> Loaded_;
  std::optional<MaterialInstance> Surface_;
  size_t Next_ = 0;
  size_t NativeBytes_ = 0;
  int Part_ = -1;
  std::string ManifestKey_;
  bool Missing_ = false;
  Stage Stage_ = Stage::Material;
};
}
#endif
