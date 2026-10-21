#ifndef OUTSHINE_RENDER_STAGES_OVERLAYDRAW_H
#define OUTSHINE_RENDER_STAGES_OVERLAYDRAW_H

#include <cstdint>

#include "Extent.h"
#include "KernelShape.h"

#include "FrameContext.h"
#include "Gpu.h"
#include <memory>
#include <cstddef>

namespace outshine::Render {

struct OverlayQuad {
  float LeftPx = 0, TopPx = 0, WidthPx = 0, HeightPx = 0;
  float U0 = 0, V0 = 0, U1 = 0, V1 = 0;
  float Red = 1, Green = 1, Blue = 1, Alpha = 1;
  float ClipLeftPx = 0, ClipTopPx = 0, ClipWidthPx = 0, ClipHeightPx = 0;
  float RadiusPx = 0, Opacity = 1, Pad0 = 0, Pad1 = 0;
};

inline constexpr size_t kOverlayQuadBytes = 80;
inline constexpr size_t kOverlayClipOffset = 48;
static_assert(sizeof(OverlayQuad) == kOverlayQuadBytes && alignof(OverlayQuad) == alignof(float));
static_assert(offsetof(OverlayQuad, LeftPx) == 0 && offsetof(OverlayQuad, U0) == 16);
static_assert(offsetof(OverlayQuad, Red) == 32 &&
              offsetof(OverlayQuad, ClipLeftPx) == kOverlayClipOffset);
static_assert(offsetof(OverlayQuad, RadiusPx) == 64);

inline constexpr size_t kMaxOverlayQuads = 16384;

class OverlayDraw {
public:
  [[nodiscard]] Native::Result<void>
  SetAtlas(const Gpu &gpu, const uint8_t *rgba, int width, int height);

  [[nodiscard]] Native::Result<void>
  SetQuads(const Gpu &gpu, const OverlayQuad *quads, size_t count);

  struct AtlasPixels {
    const uint8_t *Rgba = nullptr;
    int Width = 0;
    int Height = 0;
  };

  [[nodiscard]] Native::Result<void>
  Replace(const Gpu &gpu, const OverlayQuad *quads, size_t count, const AtlasPixels *atlas);

  [[nodiscard]] Native::Result<void> EnsureAtlas(const Gpu &gpu);

  [[nodiscard]] uint32_t Held() const { return Count; }

  [[nodiscard]] Native::Buffer *Vertices() const { return Verts.get(); }

  [[nodiscard]] Native::Texture *Atlas() const { return Atlas_.get(); }

private:
  std::unique_ptr<Native::Buffer> Verts;
  std::unique_ptr<Native::Buffer> SpareVerts;
  std::unique_ptr<Native::Texture> Atlas_;
  Native::Device *Device_ = nullptr;
  [[nodiscard]] Native::Result<void> Update(const Gpu &gpu,
                                            const OverlayQuad *quads,
                                            size_t count,
                                            const AtlasPixels *atlas,
                                            bool changeQuads);
  uint32_t Count = 0;
};

class OverlayPipeline {
public:
  static constexpr DrawShape ShaderShape{.VertexUniformBuffers = 1, .FragmentSamplers = 1};

  [[nodiscard]] Native::Result<void>
  Configure(const Gpu &gpu, Native::Sampler *smooth, Native::Format targetFormat);

  void Bind(Extent frame) {
    WidthPx = frame.WidthPx;
    HeightPx = frame.HeightPx;
  }

  [[nodiscard]] Native::Result<void>
  Encode(const OverlayDraw &overlay, const FrameContext &ctx, const PassRecording &into);

private:
  std::unique_ptr<Native::GraphicsPipeline> Pipe;
  Native::Sampler *Smooth = nullptr;
  int WidthPx = 0, HeightPx = 0;
  bool Encodes = false;
};

}
#endif
