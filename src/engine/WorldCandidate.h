#ifndef OUTSHINE_ENGINE_WORLDCANDIDATE_H
#define OUTSHINE_ENGINE_WORLDCANDIDATE_H

#include "RuntimeScene.h"
#include <cassert>
#include <expected>
#include <memory>
#include <string>
#include <utility>

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
      Scene_.reset();
      Renderer_.AbandonsWorldCandidate();
    }
  }

  void Grounding(const Vec3 &albedo) { Scene().Grounding(albedo); }

  [[nodiscard]] const Render::SubjectEnvironment &AmbientStanding() {
    return Scene().AmbientStanding();
  }

  void GroundIs(int surface) { Scene().GroundIs(surface); }

  void Digests(bool enabled) { Scene().Digests(enabled); }

  [[nodiscard]] bool SetGeometry(Geometry geometry, size_t carried, std::string &error) {
    return Scene().SetGeometry(std::move(geometry), carried, error);
  }

  [[nodiscard]] bool
  SetGeometry(Geometry geometry, size_t carried, const Material &material, std::string &error) {
    return Scene().SetGeometry(std::move(geometry), carried, material, error);
  }

  [[nodiscard]] std::expected<void, std::string>
  BeginGeometryBuild(Geometry geometry, size_t drivenParts, Material material) {
    return Scene().BeginGeometryBuild(std::move(geometry), drivenParts, material);
  }

  [[nodiscard]] std::expected<void, std::string> BeginGeneratedGeometryBuild(
      Geometry geometry, MaterialInstance groundSurface, Material material) {
    return Scene().BeginGeneratedGeometryBuild(std::move(geometry), groundSurface, material);
  }

  [[nodiscard]] std::expected<bool, std::string> AdvanceGeometryBuild(size_t itemsMost) {
    return Scene().AdvanceGeometryBuild(itemsMost);
  }

  [[nodiscard]] std::expected<bool, std::string> AdvanceGroundResourceRestore(size_t &nextPage,
                                                                              size_t pagesMost) {
    return Scene().AdvanceGroundResourceRestore(nextPage, pagesMost);
  }

  [[nodiscard]] bool GeometryBuildActive() const noexcept { return Scene().GeometryBuildActive(); }

  [[nodiscard]] Render::SceneRenderer &Renderer() noexcept { return Renderer_; }

  [[nodiscard]] std::expected<void, std::string>
  Prepare(const RuntimeScene &previous,
          const Ui::Font *font,
          Render::SceneResources::PieceSources pieces = Render::SceneResources::PieceSources::Copy,
          SubjectGeometrySources geometry = SubjectGeometrySources::All,
          GroundResourceRestore ground = GroundResourceRestore::Immediate) {
    std::string error;
    if (!RuntimeScene::PreparesWorldReplacement(
            Renderer_, previous, font, Scene_, error, pieces, geometry, ground)) {
      return std::unexpected(std::move(error));
    }
    return {};
  }

  [[nodiscard]] std::expected<void, std::string> Publish(std::unique_ptr<RuntimeScene> &published) {
    assert(Scene_);
    std::string error;
    if (!RuntimeScene::PublishesPreparedWorld(Renderer_, published, Scene_, error)) {
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
};
}
#endif
