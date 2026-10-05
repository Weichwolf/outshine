#include "Check.h"
#include "GpuOwned.h"
#include "Readback.h"
#include "SubjectResidency.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

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
    SubjectResidency resident;
    resident.StandsOn(device.Get());
    constexpr uint32_t blockBytes = 1024u * 1024u;
    constexpr uint32_t blocks = 64;
    constexpr uint32_t totalBytes = blockBytes * blocks;
    constexpr auto stream = SubjectResidency::Stream::Vertex;
    std::string error;
    CHECK(resident.Grow(stream, {.Usage = SDL_GPU_BUFFERUSAGE_VERTEX, .Bytes = totalBytes}, error),
          "all destination ranges have storage before deferred writes");
    std::vector<uint32_t> source(blockBytes / sizeof(uint32_t));
    uint64_t peak = 0;
    for (uint32_t block = 0; block < blocks; ++block) {
      std::ranges::fill(source, block + 1);
      SubjectResidency::Crossing crossing{.Which = stream,
                                          .Usage = SDL_GPU_BUFFERUSAGE_VERTEX,
                                          .From = source.data(),
                                          .Bytes = blockBytes,
                                          .Offset = block * blockBytes};
      CHECK(resident.Cross(std::span(&crossing, 1), true, error),
            "each deferred write is accepted");
      peak = std::max(peak, resident.Allocations().TransferBytes);
    }
    CHECK(resident.RecordedCrossings() > 0,
          "large deferred workloads submit work before the final explicit flush");
    CHECK(peak < totalBytes / 2,
          "transfer residency stays below half this workload instead of retaining every source");
    CHECK(resident.SubmitPendingUploads(error), "remaining writes submit");
    Readback read;
    CHECK(read.FromBuffer(device.Get(), resident.Buffer(stream).Get(), totalBytes) ==
              ReadState::Ready,
          "completed upload batches can be read independently");
    if (read.Rows() != nullptr) {
      const auto *words = reinterpret_cast<const uint32_t *>(read.Rows());
      bool identical = true;
      for (uint32_t at = 0; at < totalBytes / sizeof(uint32_t); ++at) {
        identical &= words[at] == at / source.size() + 1;
      }
      CHECK(identical, "every word of every source survives batching and transfer reuse");
    }
  }
  SDL_Quit();
  return Report();
}
