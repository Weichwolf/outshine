#include "Check.h"
#include "GpuOwned.h"
#include "Readback.h"
#include "SubjectResidency.h"
#include <SDL3/SDL.h>
#include <array>
#include <cstdint>
#include <cstring>
#include <string>

using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test;

namespace {
void Exercise(SDL_GPUDevice *device) {
  SubjectResidency residency;
  residency.StandsOn(device);
  std::array<uint32_t, 4> values{17, 23, 31, 47};
  constexpr auto stream = SubjectResidency::Stream::ClusterJobs;
  std::array<SubjectResidency::Crossing, 1> crossing{
      {{.Which = stream,
        .Usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ,
        .From = values.data(),
        .Bytes = sizeof(values)}}};
  std::string error;
  for (const bool deferred : {true, false, true}) {
    CHECK(residency.Cross(crossing, deferred, error), "native data uploads after scratch release");
    const auto capacity = residency.Allocations().TransferBytes;
    CHECK(capacity > 0, "upload allocates scratch storage");
    if (deferred) {
      residency.ReleaseUploadScratch();
      CHECK(residency.Allocations().TransferBytes == capacity,
            "unsubmitted crossings retain their upload storage");
      CHECK(residency.SubmitPendingUploads(error), "pending crossings survive the release attempt");
    }
    CHECK(SDL_WaitForGPUIdle(device), "GPU completes submitted uploads");
    const auto buffer = residency.Buffer(stream).Get();
    const auto resident = residency.Allocations().StreamBytes;
    residency.ReleaseUploadScratch();
    CHECK(residency.Allocations().TransferBytes == 0 &&
              residency.Allocations().StreamBytes == resident &&
              residency.Buffer(stream).Get() == buffer,
          "release removes scratch without replacing resident buffers");
    Readback read;
    CHECK(read.FromBuffer(device, buffer, sizeof(values)) == ReadState::Ready,
          "resident data remains readable after release");
    CHECK(read.Rows() != nullptr && std::memcmp(read.Rows(), values.data(), sizeof(values)) == 0,
          "resident bytes match the last successful upload");
    ++values[0];
  }
}
}

int main() {
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes");
  {
    const OwnedDevice device(SDL_CreateGPUDevice(
        SDL_GPU_SHADERFORMAT_MSL | SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL,
        false,
        nullptr));
    CHECK(device, "GPU device opens");
    if (device) { Exercise(device.Get()); }
  }
  SDL_Quit();
  return Report();
}
