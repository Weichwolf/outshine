#include "Check.h"
#include "GpuOwned.h"
#include "SubjectResidency.h"
#include <SDL3/SDL.h>

int main() {
  using namespace outshine::Render;
  using namespace outshine::Test;
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes");
  {
    const OwnedDevice device(SDL_CreateGPUDevice(
        SDL_GPU_SHADERFORMAT_MSL | SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL,
        false,
        nullptr));
    CHECK(device, "GPU device opens");
    if (!device) { return Report(); }
    SubjectResidency residency;
    residency.StandsOn(device.Get());
    const SubjectTexture missing;
    SDL_GPUTexture *image = nullptr;
    SDL_GPUSampler *sampler = nullptr;
    {
      const auto first =
          residency.Upload(missing, SubjectResidency::Transfer::Srgb, TexelKind::Value);
      CHECK(first && first->Image && first->Sample, "an unbound map has complete GPU resources");
      if (!first) { return Report(); }
      image = first->Image.Get();
      sampler = first->Sample.Get();
    }
    CHECK(residency.TakeUploadAttempts() == 1 && residency.TakeStagingAllocationAttempts() == 1,
          "the first default is uploaded once");
    for (int slot = 0; slot < 1000; ++slot) {
      const auto shared =
          residency.Upload(missing, SubjectResidency::Transfer::Srgb, TexelKind::Value);
      CHECK(shared && shared->Image.Get() == image && shared->Sample.Get() == sampler,
            "repeated defaults retain the same live image and sampler");
    }
    CHECK(residency.TakeUploadAttempts() == 0 && residency.TakeStagingAllocationAttempts() == 0,
          "repeated default maps allocate and submit no staging resources");
    SubjectTexture nearest = missing;
    nearest.Magnify = SubjectFilter::Nearest;
    const auto filtered =
        residency.Upload(nearest, SubjectResidency::Transfer::Srgb, TexelKind::Value);
    const auto linear =
        residency.Upload(missing, SubjectResidency::Transfer::Linear, TexelKind::Value);
    const auto normal =
        residency.Upload(missing, SubjectResidency::Transfer::Linear, TexelKind::Direction);
    CHECK(filtered && filtered->Sample.Get() != sampler,
          "a different sampler is not merged with the default sampler");
    CHECK(linear && linear->Image.Get() != image,
          "linear and sRGB images retain their transfer formats");
    CHECK(normal && linear && normal->Image.Get() != linear->Image.Get(),
          "normal processing is not merged with scalar processing");
    CHECK(residency.TakeUploadAttempts() == 3,
          "three new processing or sampling configurations require three uploads");
    const SubjectTexture invalid{.Width = 2, .Height = 2};
    CHECK(!residency.Upload(invalid, SubjectResidency::Transfer::Srgb, TexelKind::Value),
          "a sized missing image cannot read past the default texel");
  }
  SDL_Quit();
  return Report();
}
