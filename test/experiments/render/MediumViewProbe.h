#ifndef OUTSHINE_TEST_MEDIUMVIEWPROBE_H
#define OUTSHINE_TEST_MEDIUMVIEWPROBE_H

#include "MediumView.h"
#include "NativeGpuCheck.h"
#include <array>
#include <cstdint>
#include <fstream>
#include <vector>
#include <span>
#include <memory>
#include <cstring>

namespace outshine::Test::MediumViewProbe {
using namespace outshine::Render;
using namespace outshine::Render::Native;
inline constexpr uint32_t kWidth = 43, kHeight = 31, kRowBytes = 768;
using Pixel = std::array<float, 4>;

struct Scenario {
  Lens Projection;
  CameraBasis Basis;
  SkyStanding Sky;
  Viewport Area;
  float AlongViewM;
};

struct GeometricRay {
  Vec3 OriginM;
  Vec3 Direction;
  double RadiusKm;
  double CosView;
  double CosSun;
  double ViewSun;
  double DistanceKm;
};

[[nodiscard]] std::vector<Scenario> Scenarios();
[[nodiscard]] std::vector<Scenario> GeometryScenarios();
[[nodiscard]] GeometricRay
ReferenceRay(const Scenario &scenario, Vec3 fragment, const Medium &medium);
[[nodiscard]] Scissor ViewClip(const Scenario &scenario);
[[nodiscard]] bool InViewport(const Scenario &scenario, size_t pixel);
[[nodiscard]] MediumViewParameters FrameFor(const Scenario &scenario, const Medium &medium);
void Finish(Device &device, std::unique_ptr<Commands> commands);
void UploadImage(Device &device,
                 Commands &commands,
                 Texture &texture,
                 std::span<const std::byte> bytes,
                 uint32_t rowBytes);
[[nodiscard]] std::vector<Pixel> ReadImage(Device &device, Render::Native::Buffer &readback);

struct ImageShape {
  uint32_t Width;
  uint32_t Height;
  uint32_t RowBytes;
};

[[nodiscard]] std::vector<Pixel>
ReadImage(Device &device, Render::Native::Buffer &readback, ImageShape shape);

template <typename Value, typename Function>
std::vector<std::byte> ImageBytes(ImageShape shape, Function value) {
  std::vector<std::byte> bytes(size_t{shape.RowBytes} * shape.Height);
  for (uint32_t y = 0; y < shape.Height; ++y) {
    for (uint32_t x = 0; x < shape.Width; ++x) {
      const Value pixel = value(x, y);
      std::memcpy(bytes.data() + size_t{y} * shape.RowBytes + size_t{x} * sizeof(Value),
                  &pixel,
                  sizeof(pixel));
    }
  }
  return bytes;
}

void Geometry(Device &device, const GraphicsPipeline &pipeline, const char *output);
void Opaque(Device &device, const GraphicsPipeline &pipeline, const char *output);
}
#endif
