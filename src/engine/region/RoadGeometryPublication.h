#ifndef OUTSHINE_ENGINE_REGION_ROADGEOMETRYPUBLICATION_H
#define OUTSHINE_ENGINE_REGION_ROADGEOMETRYPUBLICATION_H

#include "GeometryPageResidency.h"
#include "MaterialRegistration.h"
#include "RoadPageManifest.h"
#include "SourceCoverage.h"
#include "StoredVertex.h"
#include "TangentFrame.h"
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace outshine {
class RoadGeometryPublication {
public:
  [[nodiscard]] static std::expected<std::unique_ptr<RoadGeometryPublication>, std::string>
  Begin(Render::SceneRenderer &renderer,
        Render::GeometryPageResidency &pages,
        const Generators::RoadPageManifest &manifest,
        const TangentFrame &frame);
  [[nodiscard]] std::expected<bool, std::string>
  Advance(const Generators::RoadPageAsset &page,
          const Generators::RoadPageReference &reference,
          std::chrono::steady_clock::time_point until);
  [[nodiscard]] std::expected<void, std::string> Finish();
  [[nodiscard]] size_t HeapBytes() const noexcept;

  [[nodiscard]] std::span<const uint32_t> PagesToLoad() const noexcept { return PagesToLoad_; }

  [[nodiscard]] size_t RetainedPages() const noexcept { return PageCount_ - PagesToLoad_.size(); }

  [[nodiscard]] double PackingMs() const noexcept { return PackingMs_; }

  [[nodiscard]] double PublicationMs() const noexcept { return PublicationMs_; }

private:
  [[nodiscard]] std::expected<void, std::string>
  RetainProducts(const Generators::RoadPageManifest &manifest);
  [[nodiscard]] std::expected<SourceCoverage, std::string>
  CoverageFor(const Generators::RoadPageReference &reference) const;

  RoadGeometryPublication(Render::GeometryPageResidency &pages,
                          SourceCoverage expected,
                          Render::MaterialRegistration material,
                          std::shared_ptr<const Mat4> placement,
                          Vec3 anchor);
  Render::GeometryPageResidency &Pages_;
  SourceCoverage Expected_;
  Render::MaterialRegistration Material_;
  std::shared_ptr<const Mat4> Placement_;
  Vec3 Anchor_;
  uint64_t SourceKey_;
  size_t PageCount_ = 0;
  double PackingMs_ = 0;
  double PublicationMs_ = 0;
  std::vector<uint32_t> PagesToLoad_;
  std::vector<std::string> Keys_;
  std::string PackingKey_;
  std::vector<StoredVertex> Packed_;
};
}
#endif
