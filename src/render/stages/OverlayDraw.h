#ifndef OUTSHINE_RENDER_STAGES_OVERLAYDRAW_H
#define OUTSHINE_RENDER_STAGES_OVERLAYDRAW_H

#include <cstdint>
#include <string>

#include "KernelShape.h"

#include "FrameContext.h"
#include "Gpu.h"
#include "GpuOwned.h"

namespace outshine::Render {

struct OverlayQuad {
  float LeftPx = 0, TopPx = 0, WidthPx = 0, HeightPx = 0;
  float U0 = 0, V0 = 0, U1 = 0, V1 = 0;
  float Red = 1, Green = 1, Blue = 1, Alpha = 1;
  float ClipLeftPx = 0, ClipTopPx = 0, ClipWidthPx = 0, ClipHeightPx = 0;
  float RadiusPx = 0, Opacity = 1, Pad0 = 0, Pad1 = 0;
};

inline constexpr size_t kMaxOverlayQuads = 16384;

class OverlayDraw {
public:
  [[nodiscard]] static std::string ShaderSource();
  [[nodiscard]] static std::string ShaderSource(std::string &error);
  static constexpr DrawShape ShaderShape{.VertexUniformBuffers = 1, .FragmentSamplers = 1};

  [[nodiscard]] bool Configure(const Gpu &gpu, SDL_GPUSampler *smooth,
                               SDL_GPUTextureFormat targetFormat, std::string &error);

  [[nodiscard]] bool SetAtlas(const Gpu &gpu, const uint8_t *rgba, int width, int height,
                              std::string &error);

  [[nodiscard]] bool SetQuads(const Gpu &gpu, const OverlayQuad *quads, size_t count,
                              std::string &error);

  void Bind(int widthPx, int heightPx) {
    WidthPx = widthPx;
    HeightPx = heightPx;
  }

  void Encode(const FrameContext &ctx, const PassRecording &into);

  [[nodiscard]] uint32_t Held() const { return Count; }

private:
  OwnedPipeline Pipe;
  OwnedBuffer Verts;
  OwnedTexture Atlas;
  SDL_GPUSampler *Smooth = nullptr;
  uint32_t Count = 0;
  uint32_t Capacity = 0;
  int WidthPx = 0, HeightPx = 0;
  bool Encodes = false;
};

}
#endif
