#ifndef OUTSHINE_CONTENT_ANIMATION_ANIMATEDASSET_H
#define OUTSHINE_CONTENT_ANIMATION_ANIMATEDASSET_H

#include <expected>
#include <memory>
#include <span>
#include <string>
#include <string_view>

#include "scene/Camera.h"
#include "scene/Geometry.h"

namespace outshine {

class AnimatedAsset {
public:
  AnimatedAsset();
  ~AnimatedAsset();
  AnimatedAsset(AnimatedAsset &&) noexcept;
  AnimatedAsset &operator=(AnimatedAsset &&) noexcept;
  AnimatedAsset(const AnimatedAsset &) = delete;
  AnimatedAsset &operator=(const AnimatedAsset &) = delete;

  [[nodiscard]] std::expected<void, std::string> load(std::string_view path);
  [[nodiscard]] std::expected<void, std::string> selectMaterialVariant(std::string_view variant);
  [[nodiscard]] std::expected<void, std::string> selectAnimations(std::span<const int> animations);
  [[nodiscard]] std::expected<void, std::string> sampleAnimation(double seconds);
  [[nodiscard]] const Geometry &geometry() const;
  [[nodiscard]] int animationCount() const;
  [[nodiscard]] double durationS() const;
  [[nodiscard]] int cameraCount() const;
  [[nodiscard]] std::expected<Camera, std::string> camera(int index) const;

private:
  struct Held;
  std::unique_ptr<Held> Held_;
};

}

#endif
