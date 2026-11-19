#ifndef OUTSHINE_ENGINE_REGION_ROADPAGERESTORATION_H
#define OUTSHINE_ENGINE_REGION_ROADPAGERESTORATION_H

#include "RoadPageQueue.h"
#include "RoadPageManifest.h"
#include <cstddef>
#include <chrono>
#include <cstdint>
#include <deque>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include <utility>

namespace outshine {
class RoadPageRestoration {
public:
  struct Limits {
    size_t PendingMost;
    size_t NativeBytesMost;
  };

  [[nodiscard]] static std::expected<std::unique_ptr<RoadPageRestoration>, std::string>
  Begin(Tasks &pool,
        std::shared_ptr<Generators::PreparedRoadPages> pages,
        std::shared_ptr<const Generators::RoadPageManifest> manifest,
        std::optional<std::span<const uint32_t>> selected = std::nullopt,
        std::string manifestKey = {},
        std::optional<Vec3> eye = std::nullopt,
        Limits limits = {.PendingMost = 4, .NativeBytesMost = size_t{8} * 1024u * 1024u});
  using Consumer =
      std::function<std::expected<bool, std::string>(const Generators::RoadPageAsset &,
                                                     const Generators::RoadPageReference &,
                                                     std::chrono::steady_clock::time_point)>;
  [[nodiscard]] std::expected<bool, std::string> Advance(const Consumer &consume);

  [[nodiscard]] bool Missing() const noexcept { return Missing_; }

  [[nodiscard]] size_t Pages() const noexcept { return Next_; }

  [[nodiscard]] size_t NativeBytes() const noexcept { return NativeBytes_; }

  [[nodiscard]] const RoadMeshingStats &Meshing() const noexcept { return Meshing_; }

private:
  struct Pending {
    uint32_t Index;
    std::optional<RoadPageQueue::Result> Result;
  };

  RoadPageRestoration(Tasks &pool,
                      std::shared_ptr<Generators::PreparedRoadPages> pages,
                      std::shared_ptr<const Generators::RoadPageManifest> manifest,
                      Limits limits);
  [[nodiscard]] bool OrderPages(const Vec3 &eye);
  [[nodiscard]] std::expected<void, std::string> Prefetch();
  [[nodiscard]] std::expected<void, std::string> CollectReady();
  [[nodiscard]] std::expected<void, std::string> Collect();
  void FinishPage();
  RoadPageQueue Queue_;
  std::shared_ptr<const Generators::RoadPageManifest> Manifest_;
  std::vector<uint32_t> Selected_;
  std::deque<Pending> Window_;
  Limits Limits_;
  RoadMeshingStats Meshing_;
  std::unique_ptr<Generators::RoadPageAsset> Loaded_;
  size_t Next_ = 0;
  size_t Requested_ = 0;
  size_t WindowBytes_ = 0;
  size_t NativeBytes_ = 0;
  std::string ManifestKey_;
  bool Missing_ = false;
};
}
#endif
