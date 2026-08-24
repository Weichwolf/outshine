#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>

#include "Check.h"
#include "InterpolantPrecision.h"

#include "Readback.h"
#include "ShaderPrelude.h"

using outshine::Render::MslPrelude;

namespace {
[[nodiscard]] std::string Prelude() {
  std::string why;
  return MslPrelude(why);
}
} // namespace
using outshine::Render::Readback;
using outshine::Render::ReadState;
using outshine::Test::InterpolantErrorFor;
using outshine::Test::kInterpolantArithmeticError;
using outshine::Test::kSubpixelGrid;

namespace {

int DeviceErrors = 0;

bool Refused(const void *made, const char *what) {
  if (made) { return false; }
  ++DeviceErrors;
  std::printf("NOTE device refused %s: %s\n", what, SDL_GetError());
  return true;
}

constexpr uint32_t kSide = 512;

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

std::string TriangleShader() {
  return Prelude() + R"(
struct Corners { float4 clip[3]; };
struct Carried { float4 pos [[position]]; float3 basis; };

vertex Carried across(uint vid [[vertex_id]], constant Corners &corners [[buffer(0)]]) {
  Carried out;
  out.pos = corners.clip[vid];
  out.basis = float3(vid == 0 ? 1.0 : 0.0, vid == 1 ? 1.0 : 0.0, vid == 2 ? 1.0 : 0.0);
  return out;
}

fragment float4 carried(Carried in [[stage_in]]) {
  return float4(in.basis, 1.0);
}
)";
}

SDL_GPUShader *Stage(SDL_GPUDevice *device, const std::string &source, const char *entry,
                     SDL_GPUShaderStage stage, uint32_t uniforms) {
  SDL_GPUShaderCreateInfo wanted{};
  wanted.code = reinterpret_cast<const uint8_t *>(source.c_str());
  wanted.code_size = source.size();
  wanted.entrypoint = entry;
  wanted.format = SDL_GPU_SHADERFORMAT_MSL;
  wanted.stage = stage;
  wanted.num_uniform_buffers = uniforms;
  return SDL_CreateGPUShader(device, &wanted);
}

struct Rendered {
  std::vector<float> Rgba;
  bool Ready = false;
};

Rendered Raster(const Instrument &on, const float clip[12]) {
  Rendered out;
  const std::string source = TriangleShader();
  SDL_GPUShader *vertex = Stage(on.Device, source, "across", SDL_GPU_SHADERSTAGE_VERTEX, 1);
  if (Refused(vertex, "the vertex stage")) { return out; }
  SDL_GPUShader *fragment = Stage(on.Device, source, "carried", SDL_GPU_SHADERSTAGE_FRAGMENT, 0);
  if (Refused(fragment, "the fragment stage")) {
    SDL_ReleaseGPUShader(on.Device, vertex);
    return out;
  }

  SDL_GPUTextureCreateInfo wantedTarget{};
  wantedTarget.type = SDL_GPU_TEXTURETYPE_2D;
  wantedTarget.format = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
  wantedTarget.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
  wantedTarget.width = kSide;
  wantedTarget.height = kSide;
  wantedTarget.layer_count_or_depth = 1;
  wantedTarget.num_levels = 1;
  wantedTarget.sample_count = SDL_GPU_SAMPLECOUNT_1;
  SDL_GPUTexture *target = SDL_CreateGPUTexture(on.Device, &wantedTarget);

  SDL_GPUColorTargetDescription described{};
  described.format = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
  SDL_GPUGraphicsPipelineCreateInfo wantedPipeline{};
  wantedPipeline.vertex_shader = vertex;
  wantedPipeline.fragment_shader = fragment;
  wantedPipeline.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
  wantedPipeline.target_info.num_color_targets = 1;
  wantedPipeline.target_info.color_target_descriptions = &described;
  SDL_GPUGraphicsPipeline *pipeline = SDL_CreateGPUGraphicsPipeline(on.Device, &wantedPipeline);

  SDL_ReleaseGPUShader(on.Device, vertex);
  SDL_ReleaseGPUShader(on.Device, fragment);
  if (Refused(target, "the target") || Refused(pipeline, "the pipeline")) {
    if (target) { SDL_ReleaseGPUTexture(on.Device, target); }
    if (pipeline) { SDL_ReleaseGPUGraphicsPipeline(on.Device, pipeline); }
    return out;
  }

  SDL_GPUCommandBuffer *commands = SDL_AcquireGPUCommandBuffer(on.Device);
  if (!Refused(commands, "a command buffer")) {
    SDL_GPUColorTargetInfo attachment{};
    attachment.texture = target;

    attachment.clear_color = {0.0f, 0.0f, 0.0f, 0.0f};
    attachment.load_op = SDL_GPU_LOADOP_CLEAR;
    attachment.store_op = SDL_GPU_STOREOP_STORE;
    SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(commands, &attachment, 1, nullptr);
    SDL_BindGPUGraphicsPipeline(pass, pipeline);
    SDL_PushGPUVertexUniformData(commands, 0, clip, 12u * sizeof(float));
    SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
    SDL_EndGPURenderPass(pass);
    SDL_SubmitGPUCommandBuffer(commands);

    Readback read;
    if (read.FromTexture(on.Device, target, kSide, kSide, 16u) == ReadState::Ready) {
      const size_t components = (size_t)kSide * (size_t)kSide * 4u;
      out.Rgba.resize(components);
      std::memcpy(out.Rgba.data(), read.Rows(), components * sizeof(float));
      out.Ready = true;
    } else {
      ++DeviceErrors;
    }
  }
  SDL_ReleaseGPUGraphicsPipeline(on.Device, pipeline);
  SDL_ReleaseGPUTexture(on.Device, target);
  return out;
}

struct Deviation {
  size_t Covered = 0;
  double Worst = 0;
  double WorstAt[2] = {0, 0};
  double Sum = 0;
};

Deviation Against(const std::vector<float> &rgba, const float clip[12], int subpixelBits = 0) {
  Deviation found;
  double sx[3], sy[3], w[3];
  for (int corner = 0; corner < 3; ++corner) {
    const double cw = (double)clip[corner * 4 + 3];
    w[corner] = cw;
    const double ndcX = (double)clip[corner * 4 + 0] / cw;
    const double ndcY = (double)clip[corner * 4 + 1] / cw;
    sx[corner] = (ndcX * 0.5 + 0.5) * (double)kSide;
    sy[corner] = (0.5 - ndcY * 0.5) * (double)kSide;

    if (subpixelBits > 0) {
      const double grid = (double)(1u << (unsigned)subpixelBits);
      sx[corner] = std::round(sx[corner] * grid) / grid;
      sy[corner] = std::round(sy[corner] * grid) / grid;
    }
  }
  const double area = (sx[1] - sx[0]) * (sy[2] - sy[0]) - (sx[2] - sx[0]) * (sy[1] - sy[0]);
  if (area == 0.0) { return found; }

  for (uint32_t row = 0; row < kSide; ++row) {
    for (uint32_t column = 0; column < kSide; ++column) {
      const size_t at = ((size_t)row * (size_t)kSide + (size_t)column) * 4u;
      if (!(rgba[at + 3] > 0.5f)) { continue; }
      const double px = (double)column + 0.5, py = (double)row + 0.5;
      double lambda[3];
      lambda[0] = ((sx[1] - px) * (sy[2] - py) - (sx[2] - px) * (sy[1] - py)) / area;
      lambda[1] = ((sx[2] - px) * (sy[0] - py) - (sx[0] - px) * (sy[2] - py)) / area;
      lambda[2] = 1.0 - lambda[0] - lambda[1];
      double weight = 0;
      for (int corner = 0; corner < 3; ++corner) { weight += lambda[corner] / w[corner]; }
      if (!(weight > 0.0)) { continue; }
      ++found.Covered;
      for (int corner = 0; corner < 3; ++corner) {
        const double exact = (lambda[corner] / w[corner]) / weight;
        const double deviation = std::fabs((double)rgba[at + (size_t)corner] - exact);
        found.Sum += deviation;
        if (deviation > found.Worst) {
          found.Worst = deviation;
          found.WorstAt[0] = (double)column;
          found.WorstAt[1] = (double)row;
        }
      }
    }
  }
  return found;
}

void GeometryOf(const float clip[12], double &smallestHeight, double &widestWRatio) {
  double sx[3], sy[3], w[3];
  for (int corner = 0; corner < 3; ++corner) {
    const double cw = (double)clip[corner * 4 + 3];
    w[corner] = cw;
    sx[corner] = ((double)clip[corner * 4 + 0] / cw * 0.5 + 0.5) * (double)kSide;
    sy[corner] = (0.5 - (double)clip[corner * 4 + 1] / cw * 0.5) * (double)kSide;
  }
  const double twiceArea =
      std::fabs((sx[1] - sx[0]) * (sy[2] - sy[0]) - (sx[2] - sx[0]) * (sy[1] - sy[0]));
  double longest = 0;
  for (int corner = 0; corner < 3; ++corner) {
    const int other = (corner + 1) % 3;
    const double run = sx[other] - sx[corner], rise = sy[other] - sy[corner];
    longest = std::max(longest, std::sqrt(run * run + rise * rise));
  }
  smallestHeight = longest > 0.0 ? twiceArea / longest : 0.0;
  double low = w[0], high = w[0];
  for (int corner = 1; corner < 3; ++corner) {
    low = std::min(low, w[corner]);
    high = std::max(high, w[corner]);
  }
  widestWRatio = low > 0.0 ? high / low : 1.0;
}

void Report(const char *arm, const Deviation &found) {
  const std::string at = std::string(" (") + arm + ")";
  outshine::Test::Note(("pixels covered" + at).c_str(), (double)found.Covered, "px");
  outshine::Test::Note(("worst absolute deviation" + at).c_str(), found.Worst, "of a unit span");
  outshine::Test::Note(("mean absolute deviation" + at).c_str(),
                       found.Covered ? found.Sum / (3.0 * (double)found.Covered) : 0.0,
                       "of a unit span");
  outshine::Test::Note(("worst deviation at column" + at).c_str(), found.WorstAt[0], "px");
  outshine::Test::Note(("worst deviation at row" + at).c_str(), found.WorstAt[1], "px");
}

}

int main() {
  Instrument on;
  CHECK(on.Device != nullptr, "a device answers, so the interpolator can be probed at all");
  if (!on.Device) { return outshine::Test::Report(); }

  const float affine[12] = {-0.9f, -0.8f, 0.0f, 1.0f,
                             0.85f, -0.7f, 0.0f, 1.0f,
                            -0.1f,  0.9f, 0.0f, 1.0f};

  const float perspective[12] = {-0.9f * 0.5f, -0.8f * 0.5f, 0.0f, 0.5f,
                                  0.85f * 4.0f, -0.7f * 4.0f, 0.0f, 4.0f,
                                 -0.1f * 1.5f,  0.9f * 1.5f, 0.0f, 1.5f};

  int declaredBits = 0;
  for (double grid = kSubpixelGrid; grid < 1.0; grid *= 2.0) { ++declaredBits; }

  for (const auto &arm : {std::pair<const char *, const float *>{"affine, every w at 1", affine},
                          std::pair<const char *, const float *>{"perspective, w 0.5 to 4",
                                                                 perspective}}) {
    const Rendered drawn = Raster(on, arm.second);
    CHECK(drawn.Ready, "the triangle rasterises and reads back");
    if (!drawn.Ready) { continue; }

    const Deviation found = Against(drawn.Rgba, arm.second);
    Report(arm.first, found);
    CHECK(found.Covered > 50000u,
          "the triangle covers a population worth a maximum over, rather than a handful of pixels");

    double smallestHeight = 0, widestWRatio = 1;
    GeometryOf(arm.second, smallestHeight, widestWRatio);
    const double derived = InterpolantErrorFor(smallestHeight, widestWRatio);
    std::printf("NOTE %s: smallest height %.6g px, widest w ratio %.6g, derived term %.9g\n", arm.first,
                smallestHeight, widestWRatio, derived);

    int quietest = 0;
    double quietestWorst = 0;
    bool first = true;
    for (const int bits : {4, 6, 7, 8, 9, 10, 12}) {
      const Deviation swept = Against(drawn.Rgba, arm.second, bits);
      std::printf("NOTE %s: %2d subpixel bits -> worst %.9g\n", arm.first, bits, swept.Worst);
      if (first || swept.Worst < quietestWorst) {
        quietestWorst = swept.Worst;
        quietest = bits;
        first = false;
      }
    }
    CHECK(quietest == declaredBits,
          "the grid that collapses the residual is the one kSubpixelGrid declares, so the subpixel "
          "grid is measured on this device rather than assumed from another API");

    CHECK(quietestWorst <= kInterpolantArithmeticError * widestWRatio,
          "once the reference stands on the rasteriser's own grid the residual is within f32's four "
          "roundings, so the snap and the arithmetic are two terms and not one");

    CHECK(found.Worst <= derived,
          "this device interpolates within the error test/harness/shared/InterpolantPrecision.h "
          "DERIVES for this triangle, so the picture bound's interpolant term bounds the mechanism it "
          "names");
    std::printf("NOTE %s: measured is %.4gx under the derived term\n", arm.first,
                found.Worst > 0 ? derived / found.Worst : 0.0);
  }

  CHECK(DeviceErrors == 0, "the device reported no error over the probe");
  outshine::Test::Covers("the interpolant term of the picture bound: how exactly this device carries a "
         "per-vertex quantity across a triangle, measured rather than assumed");
  return outshine::Test::Report();
}
