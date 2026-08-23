#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>

#include "Check.h"

#include "SubjectResidency.h"

using outshine::Render::OwnedBuffer;
using outshine::Render::SubjectResidency;

namespace {

[[nodiscard]] std::vector<uint8_t> ReadBack(SDL_GPUDevice *device, SDL_GPUBuffer *buffer,
                                            uint32_t bytes) {
  std::vector<uint8_t> out(bytes, 0);
  SDL_GPUTransferBufferCreateInfo wanted{};
  wanted.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
  wanted.size = bytes;
  SDL_GPUTransferBuffer *landing = SDL_CreateGPUTransferBuffer(device, &wanted);
  if (landing == nullptr) { return out; }
  SDL_GPUCommandBuffer *commands = SDL_AcquireGPUCommandBuffer(device);
  SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(commands);
  const SDL_GPUBufferRegion from{buffer, 0, bytes};
  SDL_GPUTransferBufferLocation into{landing, 0};
  SDL_DownloadFromGPUBuffer(copy, &from, &into);
  SDL_EndGPUCopyPass(copy);
  SDL_GPUFence *fence = SDL_SubmitGPUCommandBufferAndAcquireFence(commands);
  SDL_WaitForGPUFences(device, true, &fence, 1);
  SDL_ReleaseGPUFence(device, fence);
  void *mapped = SDL_MapGPUTransferBuffer(device, landing, false);
  if (mapped != nullptr) {
    std::memcpy(out.data(), mapped, bytes);
    SDL_UnmapGPUTransferBuffer(device, landing);
  }
  SDL_ReleaseGPUTransferBuffer(device, landing);
  return out;
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
    Unprepared("the video subsystem did not start -- this proof needs the GPU");
    return Report();
  }
  SDL_GPUDevice *device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_MSL, false, nullptr);
  if (device == nullptr) {
    Unprepared("no Metal device -- this proof needs the GPU");
    return Report();
  }

  {
  SubjectResidency residency;
  residency.Device = device;
  std::string error;

  // 64 bytes of ring: room for two 16-byte crossings and their alignment, NOT for three
  CHECK(residency.OpenStaging(64, error), "the staging ring opens once, at establishment");

  std::vector<uint8_t> first(16), second(16), third(48);
  for (size_t at = 0; at < first.size(); ++at) { first[at] = (uint8_t)(at + 1); }
  for (size_t at = 0; at < second.size(); ++at) { second[at] = (uint8_t)(0x80 + at); }

  OwnedBuffer a, b;
  uint32_t heldA = 0, heldB = 0;
  const auto vertex = SDL_GPU_BUFFERUSAGE_VERTEX;
  SubjectResidency::Crossing one[] = {{&a, &heldA, vertex, first.data(), 16}};
  SubjectResidency::Crossing two[] = {{&b, &heldB, vertex, second.data(), 16}};
  CHECK(residency.Cross(one, 1, true, error), "the first crossing stages");
  CHECK(residency.Cross(two, 1, true, error),
        "and a SECOND crossing of the same frame stages beside it -- the shape SetPose "
        "takes every animated frame (streams, then the refit visibility)");

  SDL_GPUCommandBuffer *commands = SDL_AcquireGPUCommandBuffer(device);
  residency.FlushCrossings(commands);
  SDL_GPUFence *fence = SDL_SubmitGPUCommandBufferAndAcquireFence(commands);
  SDL_WaitForGPUFences(device, true, &fence, 1);
  SDL_ReleaseGPUFence(device, fence);

  const std::vector<uint8_t> landedA = ReadBack(device, a.Get(), 16);
  const std::vector<uint8_t> landedB = ReadBack(device, b.Get(), 16);
  CHECK(std::memcmp(landedA.data(), first.data(), 16) == 0,
        "**THE FIRST CROSSING'S BYTES ARRIVE INTACT BESIDE ITS NEIGHBOUR** -- the mid-frame "
        "regrow that destroyed its transfer buffer is gone (board:1738)");
  CHECK(std::memcmp(landedB.data(), second.data(), 16) == 0, "and the second's too");

  OwnedBuffer c;
  uint32_t heldC = 0;
  SubjectResidency::Crossing over[] = {{&c, &heldC, vertex, third.data(), 48}};
  (void)residency.Cross(one, 1, true, error);
  (void)residency.Cross(two, 1, true, error);
  CHECK(!residency.Cross(over, 1, true, error) &&
            error.find("64") != std::string::npos,
        "**A FRAME PAST THE OPENED CAPACITY REFUSES NAMING BOTH NUMBERS** -- never a "
        "regrow that orphans what is already staged (board:1738)");

  }
  SDL_DestroyGPUDevice(device);
  SDL_QuitSubSystem(SDL_INIT_VIDEO);

  Covers("V.8 a staged crossing survives its neighbour: the ring is opened once at "
         "establishment, two same-frame crossings land intact, and a frame past the "
         "capacity refuses with both numbers instead of orphaning the staged (board:1738)");
  return Report();
}
