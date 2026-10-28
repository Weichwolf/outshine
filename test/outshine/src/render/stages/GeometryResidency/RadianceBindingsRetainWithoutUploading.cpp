#include "GeometryResidency.h"
#include "RadianceFilter.h"
#include "Check.h"
#include "test/harness/render/NativeGpuCheck.h"
#include "test/harness/render/NativeRadiance.h"
#include "test/harness/render/NativeImageReadback.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <utility>

using namespace outshine::Render;
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;

int main() {
  const auto device = Require(CreateDevice({.Validation = true}));
  const auto source = RadianceSource(*device);
  std::shared_ptr<const RadianceImage> image;
  {
    RadianceFilter filter;
    CHECK(!filter.Apply(*device, *source, {.WidthPx = 0, .HeightPx = 1}),
          "invalid demand is rejected before producer configuration or allocation");
    image = Require(filter.Apply(*device, *source, {.WidthPx = 1, .HeightPx = 1}));
  }
  CHECK(image->Texture && image->Sampling && image->Device == device.get() &&
            image->Produced.Device != 0 && image->Produced.Serial != 0,
        "the field owns a complete producing-device lease after its filter is destroyed");
  GeometryResidency residency;
  Require(residency.StandsOn(device.get()));
  const auto held = Require(residency.Bind(*image, GeometryResidency::ImageUploadMode::Deferred));
  CHECK(held.Image == image->Texture && held.Sample == image->Sampling,
        "material residency uses the exact produced image and sampler");
  CHECK(residency.TakeUploadAttempts() == 0 && residency.TakeStagingAllocationAttempts() == 0 &&
            residency.TakeUploadBytes() == 0,
        "a resident binding performs no CPU image allocation or upload");
  auto invalid = *image;
  invalid.Device = nullptr;
  CHECK(!residency.Bind(invalid), "missing producing-device identity cannot fall back to pixels");
  invalid = *image;
  invalid.Produced = {};
  CHECK(!residency.Bind(invalid), "unsubmitted data cannot become a ready image binding");
  invalid = *image;
  invalid.Sampling.reset();
  CHECK(!residency.Bind(invalid), "a missing sampler is not silently substituted");
  const auto other = Require(CreateDevice());
  GeometryResidency foreign;
  Require(foreign.StandsOn(other.get()));
  CHECK(!foreign.Bind(*image), "a different render device cannot consume the field");
  const std::weak_ptr<Texture> lifetime = image->Texture;
  invalid = {};
  image.reset();
  CHECK(!lifetime.expired(), "the draw binding keeps the actual GPU backing alive");
  const auto pixels = ReadImage(*device, *held.Image, {.Width = 1, .Height = 1});
  std::array<uint16_t, 4> actual{};
  std::memcpy(actual.data(), pixels.Rows.data(), sizeof(actual));
  CHECK((actual == std::array<uint16_t, 4>{0x3400, 0x3800, 0x3000, 0x3700}),
        "ordered GPU production and readback retain HDR energy and fractional coverage exactly");
  return Report();
}
