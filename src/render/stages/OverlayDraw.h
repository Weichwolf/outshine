#ifndef OUTSHINE_RENDER_STAGES_OVERLAYDRAW_H
#define OUTSHINE_RENDER_STAGES_OVERLAYDRAW_H

#include <cstdint>

#include "Extent.h"
#include "UiPaint.h"
#include "PipelineShape.h"

#include "FrameContext.h"
#include "Gpu.h"
#include <memory>
#include <cstddef>

namespace outshine::Render {

inline constexpr size_t kOverlayQuadBytes = 80;
inline constexpr size_t kOverlayClipOffset = 48;
static_assert(sizeof(Ui::PaintQuad) == kOverlayQuadBytes &&
              alignof(Ui::PaintQuad) == alignof(float));
static_assert(offsetof(Ui::PaintQuad, LeftPx) == 0 && offsetof(Ui::PaintQuad, U0) == 16);
static_assert(offsetof(Ui::PaintQuad, Red) == 32 &&
              offsetof(Ui::PaintQuad, ClipLeftPx) == kOverlayClipOffset);
static_assert(offsetof(Ui::PaintQuad, RadiusPx) == 64);

inline constexpr size_t kMaxOverlayQuads = 16384;

class OverlayDraw {
public:
  [[nodiscard]] Native::Result<void>
  SetAtlas(const Gpu &gpu, const uint8_t *rgba, int width, int height);

  [[nodiscard]] Native::Result<void>
  SetQuads(const Gpu &gpu, const Ui::PaintQuad *quads, size_t count);

  [[nodiscard]] Native::Result<void> Replace(const Gpu &gpu,
                                             const Ui::PaintQuad *quads,
                                             size_t count,
                                             const Ui::PaintAtlasView *atlas);

  [[nodiscard]] uint32_t Held() const { return Count; }

  [[nodiscard]] Native::Buffer *Vertices() const { return Verts.get(); }

  [[nodiscard]] Native::Texture *Atlas() const { return Atlas_.get(); }

private:
  std::unique_ptr<Native::Buffer> Verts;
  std::unique_ptr<Native::Buffer> SpareVerts;
  std::unique_ptr<Native::Texture> Atlas_;
  Native::Device *Device_ = nullptr;
  [[nodiscard]] Native::Result<void> Update(const Gpu &gpu,
                                            const Ui::PaintQuad *quads,
                                            size_t count,
                                            const Ui::PaintAtlasView *atlas,
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
