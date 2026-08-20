#include <cstdio>
#include <string>

#include <SDL3/SDL.h>

#include "Check.h"

namespace {

const char *const kResolve = R"(
#include <metal_stdlib>
using namespace metal;

struct FsIn { float4 position [[position]]; };

fragment float4 resolve(FsIn in [[stage_in]],
                        texture2d<float> visibility [[texture(0)]],
                        sampler visibilitySampler [[sampler(0)]],
                        device const uint *indices [[buffer(0)]],
                        device const float *vertices [[buffer(1)]]) {
  float4 packed = visibility.sample(visibilitySampler, in.position.xy * 0.001);
  uint primitive = uint(packed.x);
  float2 bary = packed.yz;

  uint i0 = indices[primitive * 3u + 0u];
  uint i1 = indices[primitive * 3u + 1u];
  uint i2 = indices[primitive * 3u + 2u];

  /* Six floats a vertex: position then normal, which is the smallest layout that makes the
   * interpolation below a real one rather than a load. */
  float3 p0 = float3(vertices[i0 * 6u + 0u], vertices[i0 * 6u + 1u], vertices[i0 * 6u + 2u]);
  float3 p1 = float3(vertices[i1 * 6u + 0u], vertices[i1 * 6u + 1u], vertices[i1 * 6u + 2u]);
  float3 p2 = float3(vertices[i2 * 6u + 0u], vertices[i2 * 6u + 1u], vertices[i2 * 6u + 2u]);
  float3 n0 = float3(vertices[i0 * 6u + 3u], vertices[i0 * 6u + 4u], vertices[i0 * 6u + 5u]);
  float3 n1 = float3(vertices[i1 * 6u + 3u], vertices[i1 * 6u + 4u], vertices[i1 * 6u + 5u]);
  float3 n2 = float3(vertices[i2 * 6u + 3u], vertices[i2 * 6u + 4u], vertices[i2 * 6u + 5u]);

  float w = 1.0 - bary.x - bary.y;
  float3 position = p0 * w + p1 * bary.x + p2 * bary.y;
  float3 normal = normalize(n0 * w + n1 * bary.x + n2 * bary.y);
  return float4(position + normal, 1.0);
}
)";

const char *const kNarrow = R"(
#include <metal_stdlib>
using namespace metal;

struct FsIn { float4 position [[position]]; };

fragment float4 resolve(FsIn in [[stage_in]],
                        texture2d<float> visibility [[texture(0)]],
                        sampler visibilitySampler [[sampler(0)]]) {
  float4 packed = visibility.sample(visibilitySampler, in.position.xy * 0.001);
  float w = 1.0 - packed.y - packed.z;
  return float4(float3(w, packed.y, packed.z), 1.0);
}
)";

bool Accepts(SDL_GPUDevice *device, const char *source, Uint32 storageBuffers, std::string &why) {
  SDL_GPUShaderCreateInfo info{};
  info.code = reinterpret_cast<const Uint8 *>(source);
  info.code_size = SDL_strlen(source);
  info.entrypoint = "resolve";
  info.format = SDL_GPU_SHADERFORMAT_MSL;
  info.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
  info.num_samplers = 1;
  info.num_storage_buffers = storageBuffers;
  SDL_GPUShader *shader = SDL_CreateGPUShader(device, &info);
  if (shader == nullptr) {
    const char *error = SDL_GetError();
    why = error != nullptr ? error : "no reason given";
    return false;
  }
  SDL_ReleaseGPUShader(device, shader);
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
  const bool control = Accepts(device, kNarrow, 0, why);
  if (!control) { std::printf("       control without storage buffers: %s\n", why.c_str()); }
  CHECK(control,
        "the same fullscreen shader without the two storage buffers compiles, so a refusal of the "
        "fetching one is about the fetch and not about this probe");

  const bool refetch = Accepts(device, kResolve, 2, why);
  if (!refetch) { std::printf("       with two readonly storage buffers: %s\n", why.c_str()); }
  CHECK(refetch,
        "a fragment shader on this device indexes raw index and vertex data and interpolates a "
        "triangle itself, which is what lets a visibility buffer be one attachment instead of a "
        "G-buffer of several");

  Covers("a fullscreen pass re-fetches and re-interpolates the triangle a pixel covers, "
         "exercised on this device before a deferred material path is built on the assumption");
  SDL_DestroyGPUDevice(device);
  SDL_QuitSubSystem(SDL_INIT_VIDEO);
  return Report();
}
