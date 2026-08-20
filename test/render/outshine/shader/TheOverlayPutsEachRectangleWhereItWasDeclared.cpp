#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>

#include "Check.h"
#include "Gpu.h"
#include "OverlayDraw.h"
#include "Readback.h"

using namespace outshine::Test;
using outshine::Render::Gpu;
using outshine::Render::OverlayDraw;
using outshine::Render::OverlayQuad;
using outshine::Render::PassRecording;
using outshine::Render::ReadState;
using outshine::Render::Readback;

namespace {

constexpr uint32_t kWidth = 64;
constexpr uint32_t kHeight = 48;

class Instrument {
public:
  ~Instrument() {
    if (Device) {
      SDL_DestroyGPUDevice(Device);
      SDL_QuitSubSystem(SDL_INIT_VIDEO);
    }
  }
  Instrument() {
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) { return; }
    Device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_MSL, false, nullptr);
    if (!Device) { SDL_QuitSubSystem(SDL_INIT_VIDEO); }
  }
  Instrument(const Instrument &) = delete;
  Instrument &operator=(const Instrument &) = delete;
  SDL_GPUDevice *Device = nullptr;
};

struct Frame {
  std::vector<uint8_t> Texels;
  bool Held = false;
};

struct Rgba {
  int R = 0, G = 0, B = 0, A = 0;
  bool operator==(const Rgba &other) const {
    return R == other.R && G == other.G && B == other.B && A == other.A;
  }
};

Rgba At(const Frame &frame, uint32_t x, uint32_t y) {
  const size_t at = ((size_t)y * kWidth + x) * 4u;
  if (at + 3 >= frame.Texels.size()) { return {}; }
  return {frame.Texels[at], frame.Texels[at + 1], frame.Texels[at + 2], frame.Texels[at + 3]};
}

Frame Drawn(Instrument &on, const std::vector<OverlayQuad> &quads) {
  Frame frame;
  Gpu handles{};
  handles.Device = on.Device;
  handles.SurfaceFormat = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;

  SDL_GPUSamplerCreateInfo wantedSampler{};
  wantedSampler.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
  wantedSampler.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
  wantedSampler.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
  wantedSampler.min_filter = SDL_GPU_FILTER_LINEAR;
  wantedSampler.mag_filter = SDL_GPU_FILTER_LINEAR;
  wantedSampler.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
  SDL_GPUSampler *sampler = SDL_CreateGPUSampler(on.Device, &wantedSampler);

  SDL_GPUTextureCreateInfo wantedTarget{};
  wantedTarget.type = SDL_GPU_TEXTURETYPE_2D;
  wantedTarget.format = handles.SurfaceFormat;
  wantedTarget.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
  wantedTarget.width = kWidth;
  wantedTarget.height = kHeight;
  wantedTarget.layer_count_or_depth = 1;
  wantedTarget.num_levels = 1;
  SDL_GPUTexture *target = SDL_CreateGPUTexture(on.Device, &wantedTarget);

  std::string error;
  OverlayDraw overlay;
  if (sampler == nullptr || target == nullptr) {
    std::printf("       the device refused a sampler or a target: %s\n", SDL_GetError());
  } else if (!overlay.Configure(handles, sampler, handles.SurfaceFormat, error)) {
    std::printf("       %s\n", error.c_str());
  } else if (!overlay.SetQuads(handles, quads.data(), quads.size(), error)) {
    std::printf("       %s\n", error.c_str());
  } else {
    overlay.Bind((int)kWidth, (int)kHeight);
    SDL_GPUCommandBuffer *commands = SDL_AcquireGPUCommandBuffer(on.Device);
    SDL_GPUColorTargetInfo attachment{};
    attachment.texture = target;
    attachment.load_op = SDL_GPU_LOADOP_CLEAR;
    attachment.store_op = SDL_GPU_STOREOP_STORE;
    attachment.clear_color = {0.0f, 0.0f, 0.0f, 1.0f};
    SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(commands, &attachment, 1, nullptr);
    const PassRecording into{commands, pass};
    overlay.Encode(outshine::Render::FrameContext{}, into);
    SDL_EndGPURenderPass(pass);
    SDL_SubmitGPUCommandBuffer(commands);

    Readback read;
    if (read.FromTexture(on.Device, target, kWidth, kHeight, 4) == ReadState::Ready) {
      frame.Texels.resize((size_t)kWidth * kHeight * 4u);
      for (uint32_t y = 0; y < kHeight; ++y) {
        std::memcpy(frame.Texels.data() + (size_t)y * kWidth * 4u, read.Rows() + (size_t)y * read.RowBytes(),
                    (size_t)kWidth * 4u);
      }
      frame.Held = true;
    } else {
      std::printf("       the target did not come back off the device\n");
    }
  }
  if (target) { SDL_ReleaseGPUTexture(on.Device, target); }
  if (sampler) { SDL_ReleaseGPUSampler(on.Device, sampler); }
  return frame;
}

OverlayQuad Solid(float x, float y, float w, float h, float r, float g, float b, float a) {
  OverlayQuad quad;
  quad.LeftPx = x;
  quad.TopPx = y;
  quad.WidthPx = w;
  quad.HeightPx = h;
  quad.Red = r;
  quad.Green = g;
  quad.Blue = b;
  quad.Alpha = a;
  quad.ClipLeftPx = 0;
  quad.ClipTopPx = 0;
  quad.ClipWidthPx = (float)kWidth;
  quad.ClipHeightPx = (float)kHeight;
  return quad;
}

}

int main(void) {
  Instrument on;
  if (on.Device == nullptr) {
    Skip("no SDL_GPU device with an MSL backend on this host");
    return Report();
  }

  {
    const Frame frame = Drawn(on, {Solid(8, 6, 16, 12, 1, 0, 0, 1)});
    CHECK(frame.Held, "the overlay drew and the target came back");
    if (frame.Held) {
      CHECK(At(frame, 8, 6) == (Rgba{255, 0, 0, 255}), "its first pixel is the declared colour");
      CHECK(At(frame, 23, 17) == (Rgba{255, 0, 0, 255}), "and so is its last");
      CHECK(At(frame, 7, 6) == (Rgba{0, 0, 0, 255}), "one pixel to its left is the clear");
      CHECK(At(frame, 24, 17) == (Rgba{0, 0, 0, 255}), "and one past its right edge is too");
      CHECK(At(frame, 8, 5) == (Rgba{0, 0, 0, 255}), "one row above it is the clear");
      CHECK(At(frame, 8, 18) == (Rgba{0, 0, 0, 255}), "and one row below it is too");
    }
  }

  {
    OverlayQuad clipped = Solid(0, 0, 40, 40, 0, 1, 0, 1);
    clipped.ClipLeftPx = 0;
    clipped.ClipTopPx = 0;
    clipped.ClipWidthPx = 20;
    clipped.ClipHeightPx = 20;
    const Frame frame = Drawn(on, {clipped});
    CHECK(frame.Held, "the clipped rectangle drew");
    if (frame.Held) {
      CHECK(At(frame, 10, 10) == (Rgba{0, 255, 0, 255}), "inside the clip the colour is there");
      CHECK(At(frame, 30, 10) == (Rgba{0, 0, 0, 255}),
            "past the clip's right edge nothing was written, though the rectangle covers it");
      CHECK(At(frame, 10, 30) == (Rgba{0, 0, 0, 255}), "and past its bottom edge nothing was either");
    }
  }

  {
    const Frame frame = Drawn(on, {Solid(0, 0, 32, 32, 1, 1, 1, 0.5f),
                                   Solid(16, 0, 32, 32, 1, 1, 1, 0.5f)});
    CHECK(frame.Held, "the two translucent rectangles drew");
    if (frame.Held) {
      const Rgba single = At(frame, 4, 4);
      const Rgba doubled = At(frame, 20, 4);
      CHECK(single.R >= 127 && single.R <= 128,
            "one half-opaque white over black is half the range");
      CHECK(doubled.R >= 190 && doubled.R <= 192,
            "and a second over the first reaches three quarters -- which is what says the blend is "
            "premultiplied rather than darkening every seam");
    }
  }

  {
    OverlayQuad rounded = Solid(8, 8, 24, 24, 0, 0, 1, 1);
    rounded.RadiusPx = 8;
    const Frame frame = Drawn(on, {rounded});
    CHECK(frame.Held, "the rounded rectangle drew");
    if (frame.Held) {
      CHECK(At(frame, 20, 20) == (Rgba{0, 0, 255, 255}), "its middle is filled");
      CHECK(At(frame, 8, 8).A == 0 || At(frame, 8, 8) == (Rgba{0, 0, 0, 255}),
            "and its corner texel is not -- the radius reached the pixel rather than the comment");
    }
  }

  {
    Gpu handles{};
    handles.Device = on.Device;
    handles.SurfaceFormat = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    OverlayDraw overlay;
    std::string error;
    const std::vector<OverlayQuad> tooMany(outshine::Render::kMaxOverlayQuads + 3);
    const bool took = overlay.SetQuads(handles, tooMany.data(), tooMany.size(), error);
    CHECK(!took, "a list past the bound is refused rather than cut");
    CHECK(error.find("3 past the bound") != std::string::npos,
          "and the refusal names the overage, so a consumer learns what it asked for");
  }

  return Report();
}
