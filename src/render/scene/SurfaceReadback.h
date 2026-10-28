#ifndef OUTSHINE_RENDER_SCENE_SURFACEREADBACK_H
#define OUTSHINE_RENDER_SCENE_SURFACEREADBACK_H

#include "ReadbackTransfer.h"
#include "Vec3.h"
#include <array>
#include <cstddef>
#include <optional>
#include <span>

namespace outshine::Render {

struct SurfacePixel {
  Vec3f Normal;
  float Depth = 0;
  float Identity = 0;
  Vec3f Base;
  float Metalness = 0;
  float Roughness = 0;
};

class SurfaceReadback final {
public:
  SurfaceReadback() = default;
  SurfaceReadback(const SurfaceReadback &) = delete;
  SurfaceReadback &operator=(const SurfaceReadback &) = delete;
  SurfaceReadback(SurfaceReadback &&) = delete;
  SurfaceReadback &operator=(SurfaceReadback &&) = delete;

  [[nodiscard]] size_t Pixels() const noexcept { return Pixels_; }

  [[nodiscard]] size_t StorageBytes() const noexcept { return Bytes_.size(); }

  [[nodiscard]] SurfacePixel Read(size_t pixel) const noexcept;

private:
  friend class SceneRenderer;
  void Reset() noexcept;

  std::optional<ReadbackTransfer> Transfer_;
  std::span<const std::byte> Bytes_;
  std::array<ReadbackSlice, 5> Slices_;
  size_t Pixels_ = 0;
};

}
#endif
