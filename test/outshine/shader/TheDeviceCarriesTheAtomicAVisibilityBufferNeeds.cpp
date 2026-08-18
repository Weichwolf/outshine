/* CAN THIS DEVICE'S SHADING LANGUAGE DO A 64-BIT ATOMIC MAX? (board:1411)
 *
 * THIS IS A CAPABILITY QUESTION AND IT IS ASKED THE ONLY WAY A CAPABILITY CLAIM CAN BE: by exercising
 * it. `src/core/ClusterDag.h` opens by naming itself "Nanite half 1" and says of half two -- the
 * compute software rasteriser -- that it *cannot be* here, because *a shading language has no 64-bit
 * atomic*. That sentence closes a whole technique, and a sentence that closes a technique is worth one
 * test.
 *
 * THE PACKED DEPTH-AND-ID WORD IS WHY 64 BITS IS THE NUMBER. A software rasteriser resolves visibility
 * by writing `(depth << 32) | primitiveId` into a per-pixel word with an atomic max: the winner is the
 * nearest fragment and its identity arrives with it, in ONE operation with no lock and no second pass.
 * Split across two 32-bit words it is not atomic at all -- two threads can interleave and a pixel ends
 * up with one fragment's depth and another's identity.
 *
 * IT COMPILES THROUGH THE PATH THE ENGINE ACTUALLY USES, which is the whole reason this is a shader
 * case and not an offline compile. Every shader in this tree is MSL text handed to the driver at run
 * time; an offline toolchain would answer a question about a compiler that is not in the build.
 *
 * WHAT IS ASSERTED AND WHAT IS REPORTED, AND THE SPLIT IS THE POINT. The 32-bit control is asserted,
 * because a probe that is refused for its own reasons answers nothing about widths. The 64-bit answer
 * is REPORTED, in either direction: the engine does not depend on it today, so a red here would be a
 * permanent one about a capability nobody is waiting on -- and what this case is for is that the
 * answer stops being a sentence in a comment and becomes a number the suite re-takes on every run.
 *
 * [MEASURED] on this device, Metal through SDL_GPU: the compiler refuses `atomic_fetch_max_explicit`,
 * `atomic_load_explicit` and `atomic_compare_exchange_weak_explicit` on a `device atomic_ulong *`,
 * naming the trait `_valid_fetch_max_type<device unsigned long *, void>` as unsatisfied. So the
 * sentence in `ClusterDag.h` holds on the path this engine actually uses. Whether that is the GPU
 * family or the language version SDL_GPU requests is NOT established here and is the next question. */
#include <cstdio>
#include <string>

#include <SDL3/SDL.h>

#include "Check.h"

namespace {

/* The operation itself and nothing around it: a packed word, an atomic max, a read back. Written as a
 * compute shader because that is where a software rasteriser would live. */
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

/* THE CONTROL, AND WITHOUT IT THE ANSWER ABOVE DECIDES NOTHING. A shader can be refused for a reason
 * that has nothing to do with the width of its atomic -- a wrong entry point, a binding the pipeline
 * did not declare, a typo. This is the SAME shader with the word narrowed to 32 bits and nothing else
 * changed, so if it compiles then the only thing the wide one differs by is the width. */
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

/* A second spelling, in case the driver accepts the type and not the operation. `atomic_max` on a
 * 64-bit word is the one a rasteriser needs; a compare-exchange loop would work and would cost a loop
 * per fragment, so the two answers are different and both are worth having. */
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

} // namespace

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

  /* THE CONTROL FIRST, because it is what makes the other two answers mean anything. */
  std::string why;
  const bool narrow = Accepts(device, kNarrow, why);
  if (!narrow) { std::printf("       32-bit control: %s\n", why.c_str()); }
  CHECK(narrow,
        "the same shader with a 32-bit word compiles, so a refusal of the 64-bit one is about the "
        "width and not about this probe");

  const bool atomicMax = Accepts(device, kProbe, why);
  const bool exchange = Accepts(device, kExchange, why);

  /* REPORTED AND NOT ASSERTED, IN EITHER DIRECTION, and that is deliberate. The engine does not depend
   * on this today -- `ClusterDag.h` records the compute rasteriser as unbuildable for exactly this
   * reason -- so a red here would be a permanent one about a capability nobody is waiting on. What
   * this case is for is that the answer stops being a sentence in a comment and becomes a number the
   * suite re-takes on every run: the day a driver or an SDL version changes it, the note changes with
   * it and a closed technique reopens. */
  Note("64-bit atomic max compiles", atomicMax ? 1.0 : 0.0, "boolean");
  Note("64-bit compare-exchange compiles", exchange ? 1.0 : 0.0, "boolean");
  if (!atomicMax) {
    Note("so a packed depth-and-identity visibility buffer has no lock-free resolve on this path");
  }

  Covers("board:1411 the 64-bit atomic a packed depth-and-identity visibility buffer is built on, "
         "exercised on this device through the same runtime path every shader in this tree takes");
  SDL_DestroyGPUDevice(device);
  SDL_QuitSubSystem(SDL_INIT_VIDEO);
  return Report();
}
