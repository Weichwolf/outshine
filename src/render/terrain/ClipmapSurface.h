#ifndef OUTSHINE_RENDER_TERRAIN_CLIPMAPSURFACE_H
#define OUTSHINE_RENDER_TERRAIN_CLIPMAPSURFACE_H
#include "ClipmapHeightCache.h"
#include "ClipmapNormalPass.h"
#include "ClipmapRenderer.h"
#include <cstddef>
#include <memory>
#include <span>
#include <vector>

namespace outshine::Render {
class ClipmapSurface {
public:
  [[nodiscard]] static Native::Result<std::shared_ptr<ClipmapSurface>>
  Make(Native::Device &device, ClipmapGrid grid, size_t bytesMost);

  [[nodiscard]] const ClipmapHeightCache &Heights() const noexcept { return Heights_; }

  [[nodiscard]] const ClipmapHeightCache &NormalHeights() const noexcept { return NormalHeights_; }

  [[nodiscard]] Native::Result<void> Publish(const ClipmapHeightUpdate &heights,
                                             const ClipmapHeightUpdate &normals,
                                             size_t stagingBytesMost);
  [[nodiscard]] Native::Result<void> Prepare(const ClipmapView &view);
  [[nodiscard]] ClipmapFields Fields() const noexcept;
  [[nodiscard]] bool Complete() const noexcept;
  [[nodiscard]] uint64_t HeldBytes() const noexcept;
  [[nodiscard]] uint32_t Triangles() const noexcept;

private:
  ClipmapSurface(Native::Device &device, ClipmapHeightCache heights, ClipmapHeightCache normals);
  [[nodiscard]] Native::Result<void> Allocate();

  Native::Device *Device_;
  ClipmapHeightCache Heights_, NormalHeights_;
  std::unique_ptr<Native::Texture> HeightImage_, NormalHeightImage_, NormalImage_;
  std::unique_ptr<Native::Buffer> RingBuffer_;
  ClipmapNormalPass NormalPass_;
  std::vector<ClipmapRing> Rings_;
};
}
#endif
