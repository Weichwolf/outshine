#ifndef OUTSHINE_ENGINE_REGION_ROADPAGECONSTRUCTION_H
#define OUTSHINE_ENGINE_REGION_ROADPAGECONSTRUCTION_H

#include "Corridors.h"
#include "RoadPageQueue.h"
#include "scene/Geometry.h"
#include <cstddef>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace outshine {
class RoadPageConstruction {
public:
  [[nodiscard]] static std::expected<std::unique_ptr<RoadPageConstruction>, std::string>
  Begin(Tasks &pool,
        std::shared_ptr<Generators::PreparedRoadPages> pages,
        const Generators::Corridors::Job &plan,
        const Generators::Corridors::Site &site,
        const std::string &planKey);
  [[nodiscard]] std::expected<bool, std::string> Advance(const Generators::Corridors::Job &plan,
                                                         const Generators::Corridors::Site &site,
                                                         Geometry &ground);

  [[nodiscard]] size_t Pages() const noexcept { return PageCount_; }

  [[nodiscard]] size_t MissRequests() const noexcept { return MissRequests_; }

  [[nodiscard]] const RoadMeshingStats &Meshing() const noexcept { return Meshing_; }

  [[nodiscard]] size_t NativeBytes() const noexcept { return NativeBytes_; }

private:
  enum class Stage : unsigned char {
    Request,
    Lookup,
    Generate,
    Generating,
    Positions,
    Normals,
    Colours,
    Triangles
  };
  RoadPageConstruction(Tasks &pool, std::shared_ptr<Generators::PreparedRoadPages> pages);
  [[nodiscard]] std::expected<bool, std::string> Collect(Geometry &ground);
  [[nodiscard]] std::expected<bool, std::string> Transfer(Geometry &ground);
  static constexpr size_t PageBytesMost = size_t{64} * 1024u * 1024u;
  static constexpr size_t PartsMost = 64;
  RoadPageQueue Queue_;
  Generators::RoadPageAddress Address_;
  std::vector<Generators::RoadPagePart> Parts_;
  std::shared_ptr<const Generators::RoadPageAsset> Loaded_;
  Material Material_;
  RoadMeshingStats Meshing_;
  std::optional<MaterialInstance> Surface_;
  std::string PendingKey_;
  size_t Next_ = 0;
  size_t PageCount_ = 0;
  size_t MissRequests_ = 0;
  size_t NativeBytes_ = 0;
  int Part_ = -1;
  Stage Stage_ = Stage::Request;
};
}
#endif
