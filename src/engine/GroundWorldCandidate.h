#ifndef OUTSHINE_ENGINE_GROUNDWORLDCANDIDATE_H
#define OUTSHINE_ENGINE_GROUNDWORLDCANDIDATE_H

#include "EngineHeld.h"
#include "WorldCandidate.h"
#include <expected>
#include <limits>
#include <memory>
#include <span>
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

  [[nodiscard]] size_t OwnedHeapBytes() const noexcept {
    return Sheets.HeapBytes() + Footprints.HeapBytes() + Pieces.HeapBytes() +
           Ground.storageBytes() + PositionsM.capacity() * sizeof(float) +
           Indices.capacity() * sizeof(uint32_t) + ClassPalette.capacity() * sizeof(float);
  }
};

class GroundWorldCandidate {
public:
  GroundWorldCandidate(
      Render::SceneRenderer &renderer,
      const Surrounds &world,
      const Ground::BuildingField &footprints,
      Render::SceneResources::PieceSources pieces = Render::SceneResources::PieceSources::Copy)
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
        World_(renderer),
        PieceSources_(pieces) {}

  GroundWorldCandidate(const GroundWorldCandidate &) = delete;
  GroundWorldCandidate &operator=(const GroundWorldCandidate &) = delete;
  GroundWorldCandidate(GroundWorldCandidate &&) = delete;
  GroundWorldCandidate &operator=(GroundWorldCandidate &&) = delete;

  [[nodiscard]] GroundBuildProducts &Products() noexcept { return Products_; }

  void Grounding(const Vec3 &albedo) { World_.Grounding(albedo); }

  [[nodiscard]] const Render::SubjectEnvironment &AmbientStanding() {
    return World_.AmbientStanding();
  }

  void GroundIs(int surface) { World_.GroundIs(surface); }

  void Digests(bool enabled) { World_.Digests(enabled); }

  [[nodiscard]] bool SetGroundGeometry(Geometry geometry, size_t carried, std::string &error) {
    return World_.SetGeometry(std::move(geometry), carried, error);
  }

  [[nodiscard]] bool SetGroundGeometry(Geometry geometry,
                                       size_t carried,
                                       const Material &material,
                                       std::string &error) {
    return World_.SetGeometry(std::move(geometry), carried, material, error);
  }

  [[nodiscard]] std::expected<void, std::string>
  BeginGroundGeometryBuild(Geometry geometry, MaterialInstance groundSurface, Material material) {
    return World_.BeginGeneratedGeometryBuild(std::move(geometry), groundSurface, material);
  }

  [[nodiscard]] std::expected<bool, std::string> AdvanceGroundGeometryBuild(size_t itemsMost) {
    return World_.AdvanceGeometryBuild(itemsMost);
  }

  [[nodiscard]] bool GroundGeometryBuildActive() const noexcept {
    return World_.GeometryBuildActive();
  }

  [[nodiscard]] bool SetGroundClasses(std::span<const uint32_t> words,
                                      std::span<const float> palette,
                                      std::string &error) {
    return World_.Renderer().SetGroundClasses(words, palette, error);
  }

  [[nodiscard]] std::expected<void, std::string> Prepare(const Core::RuntimeScene &previous,
                                                         const Ui::Font *font) {
    for (;;) {
      auto prepared = AdvancePreparation(previous, font, std::numeric_limits<size_t>::max());
      if (!prepared) { return std::unexpected(std::move(prepared.error())); }
      if (*prepared) { return {}; }
    }
  }

  [[nodiscard]] std::expected<bool, std::string> AdvancePreparation(
      const Core::RuntimeScene &previous, const Ui::Font *font, size_t heightPagesMost) {
    if (Prepared_) { return true; }
    if (!WorldPrepared_) {
      auto prepared = World_.Prepare(previous,
                                     font,
                                     PieceSources_,
                                     Core::SubjectGeometrySources::Driven,
                                     Core::GroundResourceRestore::Deferred);
      if (!prepared) { return std::unexpected(std::move(prepared.error())); }
      WorldPrepared_ = true;
      return false;
    }
    auto ground = World_.AdvanceGroundResourceRestore(NextHeightPage_, heightPagesMost);
    if (!ground) { return std::unexpected(std::move(ground.error())); }
    if (!*ground) { return false; }
    Products_.Sheets.Into(&World_.Renderer());
    Products_.Pieces.Into(&World_.Renderer());
    if (PieceSources_ == Render::SceneResources::PieceSources::Omit) {
      Products_.Pieces.Clear();
      Products_.Pieces.Into(&World_.Renderer());
    }
    Prepared_ = true;
    return true;
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
  Render::SceneResources::PieceSources PieceSources_ = Render::SceneResources::PieceSources::Copy;
  size_t NextHeightPage_ = 0;
  bool WorldPrepared_ = false;
  bool Prepared_ = false;
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
