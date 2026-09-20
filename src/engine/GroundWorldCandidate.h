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
  Ground::BuildingField Footprints;
  TilePieces Pieces;
  Geometry Ground;
  std::vector<float> PositionsM;
  std::vector<uint32_t> Indices;
  std::vector<float> ClassPalette;
  std::shared_ptr<const ClassStructure> ClassStructure;
  Material GroundMaterial;
  MaterialInstance GroundSurface;
  std::shared_ptr<const Path::Network> Network;
  size_t NetworkOfWays = 0;
  size_t RimsMissing = 0;
  TilePieces::Surfaces Surfaces;
};

class GroundWorldCandidate {
public:
  GroundWorldCandidate(Render::SceneRenderer &renderer,
                       const Surrounds &world,
                       const Ground::BuildingField &footprints)
      : Products_{.Sheets = world.Sheets,
                  .Footprints = footprints,
                  .Pieces = world.Pieces,
                  .Ground = {},
                  .PositionsM = {},
                  .Indices = {},
                  .ClassPalette = {},
                  .ClassStructure = {},
                  .GroundMaterial = {},
                  .GroundSurface = {},
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

  [[nodiscard]] Core::RuntimeScene &Scene() noexcept { return World_.Scene(); }

  [[nodiscard]] std::expected<void, std::string> Prepare(const Core::RuntimeScene &previous,
                                                         const Ui::Font *font) {
    if (auto prepared = World_.Prepare(previous, font); !prepared) { return prepared; }
    Products_.Sheets.Into(&World_.Renderer());
    Products_.Pieces.Into(&World_.Renderer());
    return {};
  }

  [[nodiscard]] std::expected<void, std::string>
  Publish(Surrounds &world,
          Ground::BuildingField &footprints,
          std::unique_ptr<Core::RuntimeScene> &published,
          const GroundRevision &revision) {
    if (!world.GroundPublished.CanPublish()) {
      return std::unexpected("a capture holds the published ground");
    }
    if (auto publishedWorld = World_.Publish(published); !publishedWorld) { return publishedWorld; }
    footprints = std::move(Products_.Footprints);
    world.Sheets = std::move(Products_.Sheets);
    world.GroundPositionsM = std::move(Products_.PositionsM);
    world.GroundIndex = std::move(Products_.Indices);
    world.Network = std::move(Products_.Network);
    world.NetworkOfWays = Products_.NetworkOfWays;
    world.RimsMissing = Products_.RimsMissing;
    world.Pieces = std::move(Products_.Pieces);
    world.Pieces.Wears(Products_.Surfaces);
    world.BindSceneResources(World_.Renderer());
    if (!world.GroundPublished.Publish(revision)) {
      return std::unexpected("a capture holds the published ground");
    }
    ++world.Relaid;
    return {};
  }

private:
  GroundBuildProducts Products_;
  Core::WorldCandidate World_;
  static_assert(std::is_nothrow_move_assignable_v<HeightSheets>);
  static_assert(std::is_nothrow_move_assignable_v<Ground::BuildingField>);
  static_assert(std::is_nothrow_move_assignable_v<TilePieces>);
  static_assert(std::is_nothrow_move_assignable_v<decltype(GroundBuildProducts::PositionsM)>);
  static_assert(std::is_nothrow_move_assignable_v<decltype(GroundBuildProducts::Indices)>);
  static_assert(std::is_nothrow_move_assignable_v<decltype(GroundBuildProducts::ClassPalette)>);
  static_assert(std::is_nothrow_move_assignable_v<decltype(GroundBuildProducts::ClassStructure)>);
  static_assert(std::is_nothrow_move_assignable_v<Material>);
  static_assert(std::is_nothrow_move_assignable_v<MaterialInstance>);
  static_assert(std::is_nothrow_move_assignable_v<decltype(GroundBuildProducts::Network)>);
};
}
#endif
