#ifndef OUTSHINE_ENGINE_WORLDCANDIDATE_H
#define OUTSHINE_ENGINE_WORLDCANDIDATE_H

#include "Live.h"
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

  [[nodiscard]] Live &Scene() noexcept {
    assert(Scene_);
    return *Scene_;
  }

  [[nodiscard]] std::expected<void, std::string> Prepare(const Live &previous,
                                                         const Ui::Font *font) {
    std::string error;
    if (!Live::PreparesWorldReplacement(Renderer_, previous, font, Scene_, error)) {
      return std::unexpected(std::move(error));
    }
    return {};
  }

  [[nodiscard]] std::expected<void, std::string> Publish(std::unique_ptr<Live> &published) {
    assert(Scene_);
    std::string error;
    if (!Live::PublishesPreparedWorld(Renderer_, published, Scene_, error)) {
      return std::unexpected(std::move(error));
    }
    return {};
  }

private:
  std::unique_ptr<Live> Scene_;
  Render::SceneRenderer &Renderer_;
};
}
#endif
