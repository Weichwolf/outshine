#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>

#include "Check.h"

#include "Readback.h"
#include "ShaderPrelude.h"
#include "ShadowRay.h"
#include "TriangleBvh.h"

using outshine::BvhNode;
using outshine::BvhTriangle;
using outshine::Render::kMslPrelude;
using outshine::Span;
using outshine::TriangleBvh;
using outshine::Render::Readback;
using outshine::Render::ReadState;
using outshine::Render::ShadowRayMsl;

namespace {

int DeviceErrors = 0;

bool Refused(const void *made, const char *what) {
  if (made) { return false; }
  ++DeviceErrors;
  std::printf("NOTE device refused %s: %s\n", what, SDL_GetError());
  return true;
}

double Unit(uint32_t at) {
  uint32_t bits = at * 2654435761u + 1013904223u;
  bits ^= bits >> 15u;
  bits *= 2246822519u;
  bits ^= bits >> 13u;
  bits *= 3266489917u;
  bits ^= bits >> 16u;
  return (double)bits * 2.3283064365386963e-10;
}

struct Soup {
  std::vector<float> PositionsM;
  std::vector<uint32_t> Indices;
};

Soup Grown(uint32_t triangles) {
  Soup out;
  for (uint32_t at = 0; at < triangles; ++at) {
    const double shell = 0.3 + 0.7 * (double)(at % 5u) / 4.0;
    const double lon = 2.0 * 3.14159265358979323846 * Unit(at * 7u);
    const double lat = std::acos(2.0 * Unit(at * 7u + 1u) - 1.0);
    const double centre[3] = {shell * std::sin(lat) * std::cos(lon), shell * std::cos(lat),
                              shell * std::sin(lat) * std::sin(lon)};
    for (int corner = 0; corner < 3; ++corner) {
      out.Indices.push_back((uint32_t)(out.PositionsM.size() / 3u));
      for (int axis = 0; axis < 3; ++axis) {
        const double jitter = 0.08 * (Unit(at * 31u + (uint32_t)(corner * 3 + axis)) - 0.5);
        out.PositionsM.push_back((float)(centre[axis] + jitter));
      }
    }
  }
  return out;
}

constexpr uint32_t kRayFloats = 8;

struct Ray {
  float OriginM[3];
  float Direction[3];
  float NearM;
  float DistanceM;
};

std::vector<Ray> RaySet(const Soup &soup, uint32_t count) {
  std::vector<Ray> rays;
  const size_t triangles = soup.Indices.size() / 3u;
  for (uint32_t at = 0; at < count; ++at) {
    Ray ray;
    ray.NearM = 0.0f;
    const uint32_t population = at % 3u;
    if (population == 2u && triangles > 0) {
      const size_t tri = (size_t)(Unit(at * 17u) * (double)triangles) % triangles;
      double centre[3] = {0, 0, 0};
      for (int corner = 0; corner < 3; ++corner) {
        const uint32_t vertex = soup.Indices[tri * 3u + (size_t)corner];
        for (int axis = 0; axis < 3; ++axis) {
          centre[axis] += soup.PositionsM[(size_t)vertex * 3u + (size_t)axis] / 3.0;
        }
      }
      double length = 0.0;
      for (int axis = 0; axis < 3; ++axis) { length += centre[axis] * centre[axis]; }
      length = std::sqrt(std::max(length, 1.0e-12));
      for (int axis = 0; axis < 3; ++axis) {
        ray.OriginM[axis] = (float)centre[axis];
        ray.Direction[axis] = (float)(centre[axis] / length);
      }
      ray.NearM = 1.0e-4f;
      ray.DistanceM = 8.0f;
      rays.push_back(ray);
      continue;
    }
    const double lon = 2.0 * 3.14159265358979323846 * Unit(at * 13u);
    const double lat = std::acos(2.0 * Unit(at * 13u + 1u) - 1.0);
    const double origin[3] = {3.0 * std::sin(lat) * std::cos(lon), 3.0 * std::cos(lat),
                              3.0 * std::sin(lat) * std::sin(lon)};
    double aim[3] = {1.6 * (Unit(at * 13u + 2u) - 0.5), 1.6 * (Unit(at * 13u + 3u) - 0.5),
                     1.6 * (Unit(at * 13u + 4u) - 0.5)};
    if (population == 1u) {

      const int axis = (int)(Unit(at * 19u) * 3.0) % 3;
      for (int which = 0; which < 3; ++which) { aim[which] = origin[which]; }
      aim[axis] = origin[axis] > 0 ? origin[axis] - 6.0 : origin[axis] + 6.0;
    }
    double length = 0.0;
    for (int axis = 0; axis < 3; ++axis) {
      const double along = aim[axis] - origin[axis];
      length += along * along;
    }
    length = std::sqrt(length);
    for (int axis = 0; axis < 3; ++axis) {
      ray.OriginM[axis] = (float)origin[axis];
      ray.Direction[axis] = (float)((aim[axis] - origin[axis]) / length);
    }
    ray.DistanceM = (float)length;
    rays.push_back(ray);
  }
  return rays;
}

std::string TieShader(const std::string &traversal) {
  char stride[128];
  std::snprintf(stride, sizeof stride, "constant uint kRay = %uu;\n", kRayFloats);
  return std::string(kMslPrelude) + traversal + std::string(stride) + R"(
struct Span { uint rays; };

kernel void tie(uint3 id [[thread_position_in_grid]],
                constant Span &span [[buffer(0)]],
                device const float *rays [[buffer(1)]],
                device const BvhNode *nodes [[buffer(2)]],
                device const BvhTri *tris [[buffer(3)]],
                device float *results [[buffer(4)]]) {
  if (id.x >= span.rays) { return; }
  uint base = id.x * kRay;
  float3 originM = float3(rays[base], rays[base + 1u], rays[base + 2u]);
  float3 direction = float3(rays[base + 3u], rays[base + 4u], rays[base + 5u]);
  results[id.x] = bvhOccludes(nodes, tris, originM, direction, rays[base + 6u], rays[base + 7u])
                      ? 1.0 : 0.0;
}
)";
}

std::string WithoutTheEscapeLink(const std::string &traversal) {
  const std::string site = "if (enter > leave) { at = node.escape; continue; }";
  const size_t found = traversal.find(site);
  if (found == std::string::npos) { return std::string(); }
  return std::string(traversal).replace(found, site.size(),
                                        "if (enter > leave) { at = kBvhNoEscape; continue; }");
}

class Instrument {
public:
  ~Instrument() {
    if (Device) {
      SDL_DestroyGPUDevice(Device);
      SDL_QuitSubSystem(SDL_INIT_VIDEO);
    }
  }
  Instrument() {
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) { return; }
    Device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_MSL, false, nullptr);
    if (!Device) { SDL_QuitSubSystem(SDL_INIT_VIDEO); }
  }
  Instrument(const Instrument &) = delete;
  Instrument &operator=(const Instrument &) = delete;

  SDL_GPUDevice *Device = nullptr;
};

SDL_GPUBuffer *MakeBuffer(SDL_GPUDevice *device, SDL_GPUBufferUsageFlags usage, const void *from,
                          uint32_t bytes) {
  SDL_GPUBufferCreateInfo wanted{};
  wanted.usage = usage;
  wanted.size = bytes;
  SDL_GPUBuffer *buffer = SDL_CreateGPUBuffer(device, &wanted);
  if (Refused(buffer, "a storage buffer")) { return nullptr; }
  if (!from) { return buffer; }

  SDL_GPUTransferBufferCreateInfo staging{};
  staging.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
  staging.size = bytes;
  SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(device, &staging);
  if (Refused(transfer, "an upload buffer")) { return buffer; }
  std::memcpy(SDL_MapGPUTransferBuffer(device, transfer, false), from, bytes);
  SDL_UnmapGPUTransferBuffer(device, transfer);
  SDL_GPUCommandBuffer *commands = SDL_AcquireGPUCommandBuffer(device);
  SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(commands);
  SDL_GPUTransferBufferLocation source{transfer, 0};
  SDL_GPUBufferRegion into{buffer, 0, bytes};
  SDL_UploadToGPUBuffer(copy, &source, &into, false);
  SDL_EndGPUCopyPass(copy);
  SDL_SubmitGPUCommandBuffer(commands);
  SDL_ReleaseGPUTransferBuffer(device, transfer);
  return buffer;
}

std::vector<float> RunOnDevice(const Instrument &on, const std::string &msl,
                               const std::vector<float> &rays, const TriangleBvh &built,
                               size_t rayCount) {
  SDL_GPUComputePipelineCreateInfo wanted{};
  wanted.code = reinterpret_cast<const Uint8 *>(msl.c_str());
  wanted.code_size = msl.size();
  wanted.entrypoint = "tie";
  wanted.format = SDL_GPU_SHADERFORMAT_MSL;
  wanted.num_uniform_buffers = 1;
  wanted.num_readonly_storage_buffers = 3;
  wanted.num_readwrite_storage_buffers = 1;
  wanted.threadcount_x = 64;
  wanted.threadcount_y = 1;
  wanted.threadcount_z = 1;
  SDL_GPUComputePipeline *pipeline = SDL_CreateGPUComputePipeline(on.Device, &wanted);
  if (Refused(pipeline, "the tie's compute pipeline")) { return {}; }

  const uint32_t outputBytes = (uint32_t)(rayCount * sizeof(float));
  SDL_GPUBuffer *rayBuffer =
      MakeBuffer(on.Device, SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ, rays.data(),
                 (uint32_t)(rays.size() * sizeof(float)));
  SDL_GPUBuffer *nodeBuffer =
      MakeBuffer(on.Device, SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ, built.Nodes().Data(),
                 (uint32_t)built.Nodes().Bytes());
  SDL_GPUBuffer *triBuffer =
      MakeBuffer(on.Device, SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ, built.Triangles().Data(),
                 (uint32_t)built.Triangles().Bytes());
  SDL_GPUBuffer *results =
      MakeBuffer(on.Device, SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE, nullptr, outputBytes);

  std::vector<float> out;
  if (rayBuffer && nodeBuffer && triBuffer && results) {
    SDL_GPUCommandBuffer *commands = SDL_AcquireGPUCommandBuffer(on.Device);
    SDL_GPUStorageBufferReadWriteBinding written{};
    written.buffer = results;
    SDL_GPUComputePass *pass = SDL_BeginGPUComputePass(commands, nullptr, 0, &written, 1);
    SDL_BindGPUComputePipeline(pass, pipeline);
    SDL_GPUBuffer *bound[3] = {rayBuffer, nodeBuffer, triBuffer};
    SDL_BindGPUComputeStorageBuffers(pass, 0, bound, 3);
    const uint32_t count = (uint32_t)rayCount;
    SDL_PushGPUComputeUniformData(commands, 0, &count, sizeof count);
    SDL_DispatchGPUCompute(pass, (uint32_t)((rayCount + 63) / 64), 1, 1);
    SDL_EndGPUComputePass(pass);
    SDL_SubmitGPUCommandBuffer(commands);

    Readback read;
    if (read.FromBuffer(on.Device, results, outputBytes) == ReadState::Ready) {
      out.resize(rayCount);
      std::memcpy(out.data(), read.Rows(), outputBytes);
    } else {
      ++DeviceErrors;
    }
  }
  SDL_ReleaseGPUBuffer(on.Device, rayBuffer);
  SDL_ReleaseGPUBuffer(on.Device, nodeBuffer);
  SDL_ReleaseGPUBuffer(on.Device, triBuffer);
  SDL_ReleaseGPUBuffer(on.Device, results);
  SDL_ReleaseGPUComputePipeline(on.Device, pipeline);
  return out;
}

long Disagreements(const std::vector<float> &device, const std::vector<bool> &here) {
  long apart = 0;
  for (size_t at = 0; at < here.size() && at < device.size(); ++at) {
    if ((device[at] > 0.5f) != here[at]) { ++apart; }
  }
  return apart;
}

}

int main(void) {
  using namespace outshine::Test;

  constexpr uint32_t kTriangles = 4096;
  constexpr uint32_t kRays = 8192;
  const Soup soup = Grown(kTriangles);
  const TriangleBvh built =
      TriangleBvh::Over(Span<const float>(soup.PositionsM.data(), soup.PositionsM.size()),
                        Span<const uint32_t>(soup.Indices.data(), soup.Indices.size()));
  CHECK(!built.Empty(), "the subject the two halves are tied over builds");
  if (built.Empty()) { return Report(); }

  const std::vector<Ray> rays = RaySet(soup, kRays);
  std::vector<float> uploaded;
  uploaded.reserve(rays.size() * kRayFloats);
  std::vector<bool> here;
  here.reserve(rays.size());
  long occluded = 0;
  for (const Ray &ray : rays) {
    for (const float axis : ray.OriginM) { uploaded.push_back(axis); }
    for (const float axis : ray.Direction) { uploaded.push_back(axis); }
    uploaded.push_back(ray.NearM);
    uploaded.push_back(ray.DistanceM);
    const bool hit = built.Occludes(ray.OriginM, ray.Direction, ray.NearM, ray.DistanceM);
    here.push_back(hit);
    if (hit) { ++occluded; }
  }
  std::printf("SUBJECT %zu nodes, %zu triangles, depth %u; %u rays, %ld occluded on the processor\n",
              built.Nodes().Size(), built.Triangles().Size(), built.Depth(), kRays, occluded);
  CHECK(occluded > (long)kRays / 8 && occluded < (long)kRays - (long)kRays / 8,
        "the ray set is genuinely mixed, so agreement is not agreement with a constant");

  Instrument on;
  CHECK(on.Device != nullptr, "the device came up, so the emitted traversal can be run at all");
  if (!on.Device) { return Report(); }

  const std::string traversal = ShadowRayMsl();
  const std::vector<float> device = RunOnDevice(on, TieShader(traversal), uploaded, built, kRays);
  CHECK(device.size() == kRays, "the device answered every ray");
  const long apart = Disagreements(device, here);
  std::printf("TIE %ld disagreements over %u rays\n", apart, kRays);
  CHECK(apart == 0, "the emitted traversal gives the processor's answer on every ray");

  const std::string mutated = WithoutTheEscapeLink(traversal);
  CHECK(!mutated.empty(),
        "the negative control's site is still in the emitted traversal, so the mutation was applied");
  if (!mutated.empty()) {
    const std::vector<float> broken = RunOnDevice(on, TieShader(mutated), uploaded, built, kRays);
    const long brokenApart = Disagreements(broken, here);
    std::printf("CONTROL %ld disagreements with the escape link removed\n", brokenApart);
    CHECK(brokenApart > 0,
          "a traversal that stops at its first missed box disagrees, so this tie can see the "
          "traversal and not only the intersection test");
  }

  CHECK(DeviceErrors == 0, "the device refused nothing this tie depended on");
  return Report();
}
