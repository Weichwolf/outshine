#include <cstdio>
#include <string>

#include <SDL3/SDL.h>

#include "Check.h"

namespace {

const char *const kProbe = R"(
#include <metal_stdlib>
using namespace metal;

kernel void visibility(device atomic_ulong *pixels [[buffer(0)]],
                       device const uint *fragments [[buffer(1)]],
                       uint index [[thread_position_in_grid]]) {
  uint depth = fragments[index * 2u];
  uint identity = fragments[index * 2u + 1u];
  ulong packed = (ulong(depth) << 32) | ulong(identity);
  atomic_fetch_max_explicit(&pixels[identity & 1023u], packed, memory_order_relaxed);
}
)";

const char *const kNarrow = R"(
#include <metal_stdlib>
using namespace metal;

kernel void visibility(device atomic_uint *pixels [[buffer(0)]],
                       device const uint *fragments [[buffer(1)]],
                       uint index [[thread_position_in_grid]]) {
  uint depth = fragments[index * 2u];
  uint identity = fragments[index * 2u + 1u];
  atomic_fetch_max_explicit(&pixels[identity & 1023u], depth, memory_order_relaxed);
}
)";

const char *const kExchange = R"(
#include <metal_stdlib>
using namespace metal;

kernel void visibility(device atomic_ulong *pixels [[buffer(0)]],
                       device const uint *fragments [[buffer(1)]],
                       uint index [[thread_position_in_grid]]) {
  ulong packed = (ulong(fragments[index * 2u]) << 32) | ulong(fragments[index * 2u + 1u]);
  ulong seen = atomic_load_explicit(&pixels[index & 1023u], memory_order_relaxed);
  while (packed > seen) {
    if (atomic_compare_exchange_weak_explicit(&pixels[index & 1023u], &seen, packed,
                                              memory_order_relaxed, memory_order_relaxed)) {
      break;
    }
  }
}
)";

bool Accepts(SDL_GPUDevice *device, const char *source, std::string &why) {
  SDL_GPUComputePipelineCreateInfo info{};
  info.code = reinterpret_cast<const Uint8 *>(source);
  info.code_size = SDL_strlen(source);
  info.entrypoint = "visibility";
  info.format = SDL_GPU_SHADERFORMAT_MSL;
  info.num_readwrite_storage_buffers = 1;
  info.num_readonly_storage_buffers = 1;
  info.threadcount_x = 64;
  info.threadcount_y = 1;
  info.threadcount_z = 1;
  SDL_GPUComputePipeline *pipeline = SDL_CreateGPUComputePipeline(device, &info);
  if (pipeline == nullptr) {
    const char *error = SDL_GetError();
    why = error != nullptr ? error : "no reason given";
    return false;
  }
  SDL_ReleaseGPUComputePipeline(device, pipeline);
  why.clear();
  return true;
}

}

int main() {
  using namespace outshine::Test;

  if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
    CHECK(false, "the video subsystem starts, which a shader case needs a device for");
    return Report();
  }
  SDL_GPUDevice *device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_MSL, false, nullptr);
  if (device == nullptr) {
    CHECK(false, "a Metal device is created");
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    return Report();
  }

  std::string why;
  const bool narrow = Accepts(device, kNarrow, why);
  if (!narrow) { std::printf("       32-bit control: %s\n", why.c_str()); }
  CHECK(narrow,
        "the same shader with a 32-bit word compiles, so a refusal of the 64-bit one is about the "
        "width and not about this probe");

  const bool atomicMax = Accepts(device, kProbe, why);
  const bool exchange = Accepts(device, kExchange, why);

  Note("64-bit atomic max compiles", atomicMax ? 1.0 : 0.0, "boolean");
  Note("64-bit compare-exchange compiles", exchange ? 1.0 : 0.0, "boolean");
  if (!atomicMax) {
    Note("so a packed depth-and-identity visibility buffer has no lock-free resolve on this path");
  }

  Covers("the 64-bit atomic a packed depth-and-identity visibility buffer is built on, "
         "exercised on this device through the same runtime path every shader in this tree takes");
  SDL_DestroyGPUDevice(device);
  SDL_QuitSubSystem(SDL_INIT_VIDEO);
  return Report();
}
