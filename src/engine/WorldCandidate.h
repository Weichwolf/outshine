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

  [[nodiscard]] RuntimeScene &Scene() noexcept {
    assert(Scene_);
    return *Scene_;
  }

  [[nodiscard]] Render::SceneRenderer &Renderer() noexcept { return Renderer_; }

  [[nodiscard]] std::expected<void, std::string> Prepare(const RuntimeScene &previous,
                                                         const Ui::Font *font) {
    std::string error;
    if (!RuntimeScene::PreparesWorldReplacement(Renderer_, previous, font, Scene_, error)) {
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
  std::unique_ptr<RuntimeScene> Scene_;
  Render::SceneRenderer &Renderer_;
};
}
#endif
