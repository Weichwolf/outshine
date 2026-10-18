#include "Check.h"
#include "SubjectResidency.h"
#include "test/harness/render/NativeGpuCheck.h"
#include "test/harness/render/NativeOffline.h"
#include <chrono>
#include <utility>

constexpr int kRepeatedSlots = 1000;

int main() {
  using namespace outshine::Render;
  using namespace outshine::Test;
  using namespace outshine::Render::Native;
  using namespace outshine::Test::Gpu;
  {
    auto device = Require(CreateDevice({.Validation = true}));
    SubjectResidency residency;
    Require(residency.StandsOn(device.get()));
    const SubjectTexture missing;
    const Texture *image = nullptr;
    const Sampler *sampler = nullptr;
    {
      const auto first =
          residency.Upload(missing, SubjectResidency::Transfer::Srgb, TexelKind::Value);
      CHECK(first && first->Image && first->Sample, "an unbound map has complete GPU resources");
      if (!first) { return Report(); }
      image = first->Image.get();
      sampler = first->Sample.get();
    }
    CHECK(residency.TakeUploadAttempts() == 1 && residency.TakeStagingAllocationAttempts() == 1,
          "the first default is uploaded once");
    for (int slot = 0; slot < kRepeatedSlots; ++slot) {
      const auto shared =
          residency.Upload(missing, SubjectResidency::Transfer::Srgb, TexelKind::Value);
      CHECK(shared && shared->Image.get() == image && shared->Sample.get() == sampler,
            "repeated defaults retain the same live image and sampler");
    }
    CHECK(residency.TakeUploadAttempts() == 0 && residency.TakeStagingAllocationAttempts() == 0,
          "repeated default maps allocate and submit no staging resources");
    SubjectTexture nearest = missing;
    nearest.Magnify = SubjectFilter::Nearest;
    const auto filtered = RetryOffline(
        [&] {
          return residency.Upload(nearest, SubjectResidency::Transfer::Srgb, TexelKind::Value);
        },
        std::chrono::seconds(5));
    const auto linear = RetryOffline(
        [&] {
          return residency.Upload(missing, SubjectResidency::Transfer::Linear, TexelKind::Value);
        },
        std::chrono::seconds(5));
    const auto normal = RetryOffline(
        [&] {
          return residency.Upload(
              missing, SubjectResidency::Transfer::Linear, TexelKind::Direction);
        },
        std::chrono::seconds(5));
    CHECK(filtered && filtered->Sample.get() != sampler,
          "a different sampler is not merged with the default sampler");
    CHECK(linear && linear->Image.get() != image,
          "linear and sRGB images retain their transfer formats");
    CHECK(normal && linear && normal->Image.get() != linear->Image.get(),
          "normal processing is not merged with scalar processing");
    CHECK(residency.TakeUploadAttempts() == 3,
          "three new processing or sampling configurations require three uploads");
    const SubjectTexture invalid{.Width = 2, .Height = 2, .Uv = {}};
    CHECK(!residency.Upload(invalid, SubjectResidency::Transfer::Srgb, TexelKind::Value),
          "a sized missing image cannot read past the default texel");
  }
  return Report();
}
