#ifndef OUTSHINE_ENGINE_GROUNDWORLDCANDIDATE_H
#define OUTSHINE_ENGINE_GROUNDWORLDCANDIDATE_H

#include "EngineHeld.h"
#include "WorldCandidate.h"
#include <expected>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace outshine {
struct GroundBuildProducts {
  HeightSheets Sheets;
  Geometry Ground;
  std::vector<float> PositionsM;
  std::vector<uint32_t> Indices;
  std::shared_ptr<const Path::Network> Network;
  size_t NetworkOfWays = 0;
  size_t RimsMissing = 0;
  TilePieces::Surfaces Surfaces;
};

class GroundWorldCandidate {
public:
  GroundWorldCandidate(Render::SceneRenderer &renderer, const Surrounds &world)
      : Products_{.Sheets = world.Sheets,
                  .Ground = {},
                  .PositionsM = {},
                  .Indices = {},
                  .Network = world.Network,
                  .NetworkOfWays = world.NetworkOfWays,
                  .RimsMissing = world.RimsMissing,
                  .Surfaces = {}},
        World_(renderer) {}

  GroundWorldCandidate(const GroundWorldCandidate &) = delete;
  GroundWorldCandidate &operator=(const GroundWorldCandidate &) = delete;
  GroundWorldCandidate(GroundWorldCandidate &&) = delete;
  GroundWorldCandidate &operator=(GroundWorldCandidate &&) = delete;

  [[nodiscard]] GroundBuildProducts &Products() noexcept { return Products_; }

  [[nodiscard]] Core::Live &Scene() noexcept { return World_.Scene(); }

  [[nodiscard]] std::expected<void, std::string> Prepare(const Core::Live &previous,
                                                         const Ui::Font *font) {
    if (auto prepared = World_.Prepare(previous, font); !prepared) { return prepared; }
    Products_.Sheets.Into(&World_.Scene());
    return {};
  }

  [[nodiscard]] std::expected<void, std::string> Publish(Surrounds &world,
                                                         std::unique_ptr<Core::Live> &published,
                                                         const GroundRevision &revision) {
    if (auto publishedWorld = World_.Publish(published); !publishedWorld) { return publishedWorld; }
    world.Sheets = std::move(Products_.Sheets);
    world.GroundPositionsM = std::move(Products_.PositionsM);
    world.GroundIndex = std::move(Products_.Indices);
    world.Network = std::move(Products_.Network);
    world.NetworkOfWays = Products_.NetworkOfWays;
    world.RimsMissing = Products_.RimsMissing;
    world.Pieces.Wears(Products_.Surfaces);
    world.BindLiveResources(*published);
    world.GroundPublished.Publish(revision);
    ++world.Relaid;
    return {};
  }

private:
  GroundBuildProducts Products_;
  Core::WorldCandidate World_;
  static_assert(std::is_nothrow_move_assignable_v<HeightSheets>);
  static_assert(std::is_nothrow_move_assignable_v<decltype(GroundBuildProducts::PositionsM)>);
  static_assert(std::is_nothrow_move_assignable_v<decltype(GroundBuildProducts::Indices)>);
  static_assert(std::is_nothrow_move_assignable_v<decltype(GroundBuildProducts::Network)>);
};
}
#endif
