#ifndef OUTSHINE_ENGINE_WORLDCANDIDATE_H
#define OUTSHINE_ENGINE_WORLDCANDIDATE_H

#include "SceneLighting.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include <cassert>
#include <expected>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace outshine::Core {
class WorldCandidate {
public:
  explicit WorldCandidate(Render::SceneRenderer &renderer) noexcept : Renderer_(renderer) {}

  WorldCandidate(const WorldCandidate &) = delete;
  WorldCandidate &operator=(const WorldCandidate &) = delete;
  WorldCandidate(WorldCandidate &&) = delete;
  WorldCandidate &operator=(WorldCandidate &&) = delete;

  ~WorldCandidate() {
    if (Scene_) {
      const auto editing = Renderer_.EditWorldCandidate();
      Scene_.reset();
      Renderer_.AbandonsWorldCandidate();
    }
  }

  void Grounding(const Vec3 &albedo) {
    const auto editing = Renderer_.EditWorldCandidate();
    Scene().Grounding(albedo);
  }

  [[nodiscard]] const Vec3 &GroundAlbedo() {
    const auto editing = Renderer_.EditWorldCandidate();
    return Scene().GroundAlbedo();
  }

  [[nodiscard]] const Render::EnvironmentLighting &AmbientStanding() {
    const auto editing = Renderer_.EditWorldCandidate();
    return Scene().AmbientStanding();
  }

  void GroundIs(int surface) {
    const auto editing = Renderer_.EditWorldCandidate();
    Scene().GroundIs(surface);
  }

  void Digests(bool enabled) {
    const auto editing = Renderer_.EditWorldCandidate();
    Scene().Digests(enabled);
  }

  [[nodiscard]] bool SetGeometry(Geometry geometry, size_t carried, std::string &error) {
    const auto editing = Renderer_.EditWorldCandidate();
    return Scene().SetGeometry(std::move(geometry), carried, error);
  }

  [[nodiscard]] bool
  SetGeometry(Geometry geometry, size_t carried, const Material &material, std::string &error) {
    const auto editing = Renderer_.EditWorldCandidate();
    return Scene().SetGeometry(std::move(geometry), carried, material, error);
  }

  [[nodiscard]] std::expected<void, std::string>
  BeginGeometryBuild(Geometry geometry, size_t drivenParts, Material material) {
    const auto editing = Renderer_.EditWorldCandidate();
    return Scene().BeginGeometryBuild(std::move(geometry), drivenParts, material);
  }

  [[nodiscard]] std::expected<void, std::string> BeginGeneratedGeometryBuild(
      Geometry geometry, MaterialInstance groundSurface, Material material) {
    const auto editing = Renderer_.EditWorldCandidate();
    return Scene().BeginGeneratedGeometryBuild(std::move(geometry), groundSurface, material);
  }

  [[nodiscard]] std::expected<bool, std::string> AdvanceGeometryBuild(size_t itemsMost) {
    const auto editing = Renderer_.EditWorldCandidate();
    return Scene().AdvanceGeometryBuild(itemsMost);
  }

  [[nodiscard]] std::expected<bool, std::string> AdvancePieceResourceRestore(size_t &nextPiece,
                                                                             size_t piecesMost) {
    const auto editing = Renderer_.EditWorldCandidate();
    return Scene().AdvancePieceResourceRestore(nextPiece, piecesMost);
  }

  [[nodiscard]] std::expected<bool, std::string> AdvanceGroundResourceRestore(size_t &nextPage,
                                                                              size_t pagesMost) {
    const auto editing = Renderer_.EditWorldCandidate();
    return Scene().AdvanceGroundResourceRestore(nextPage, pagesMost);
  }

  [[nodiscard]] bool GeometryBuildActive() const noexcept { return Scene().GeometryBuildActive(); }

  [[nodiscard]] Render::SceneRenderer &Renderer() noexcept { return Renderer_; }

  // Ready declarations release their edit scope between candidate operations.
  [[nodiscard]] std::expected<void, std::string>
  PrepareDeclaration(const Declaration &declared,
                     const Ui::Font *font,
                     const Geometry *geometry,
                     std::vector<std::vector<Ui::Layout::Scrolled>> scrolled) {
    std::string error;
    if (!Renderer_.BeginsWorldCandidate(error)) { return std::unexpected(std::move(error)); }
    const auto failed = [&] {
      Scene_.reset();
      Renderer_.AbandonsWorldCandidate();
      return std::unexpected(std::move(error));
    };
    if (!RuntimeScene::Prepare(Renderer_, declared, font, Scene_, error)) { return failed(); }
    if (!scrolled.empty() && !Scene_->Scrolled(std::move(scrolled), error)) { return failed(); }
    if (geometry != nullptr && !Scene_->SetGeometry(geometry->clone(), 0, error)) {
      return failed();
    }
    Scene_->StopEditingCandidate();
    Publication_ = RuntimeScene::Publication::ReplaceDeclaration;
    return {};
  }

  [[nodiscard]] std::expected<void, std::string>
  Prepare(const RuntimeScene &previous,
          const Ui::Font *font,
          Render::SceneResources::PieceSources pieces = Render::SceneResources::PieceSources::Copy,
          GeometryRetention geometry = GeometryRetention::All,
          ResourceRestoreMode restore = ResourceRestoreMode::Immediate) {
    std::string error;
    if (!RuntimeScene::PreparesWorldReplacement(
            Renderer_, previous, font, Scene_, error, pieces, geometry, restore)) {
      return std::unexpected(std::move(error));
    }
    Publication_ = RuntimeScene::Publication::PreserveViewAndUi;
    return {};
  }

  [[nodiscard]] std::expected<void, std::string> Publish(std::unique_ptr<RuntimeScene> &published) {
    assert(Scene_);
    const auto editing = Renderer_.EditWorldCandidate();
    std::string error;
    if (!RuntimeScene::PublishesPreparedWorld(Renderer_, published, Scene_, error, Publication_)) {
      return std::unexpected(std::move(error));
    }
    return {};
  }

private:
  [[nodiscard]] const RuntimeScene &Scene() const noexcept {
    assert(Scene_);
    return *Scene_;
  }

  [[nodiscard]] RuntimeScene &Scene() noexcept {
    assert(Scene_);
    return *Scene_;
  }

  std::unique_ptr<RuntimeScene> Scene_;
  Render::SceneRenderer &Renderer_;
  RuntimeScene::Publication Publication_ = RuntimeScene::Publication::PreserveViewAndUi;
};
}
#endif
