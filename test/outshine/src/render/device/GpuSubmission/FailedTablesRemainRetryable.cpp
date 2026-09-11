#include <SDL3/SDL.h>
#include <array>
#include <cassert>
#include <cstring>
#include <limits>
#include <dlfcn.h>
#include <string>
#include <vector>
#include "SubjectDraw.h"
#include "Readback.h"
#include "Check.h"

namespace {
enum class Failure { None, Map, Acquire, Pass, Submit };
Failure nextFailure = Failure::None;
unsigned skipFailures = 0;
unsigned failures = 0;
unsigned bufferAllocations = 0;

bool Reject(Failure point) {
  if (nextFailure != point) { return false; }
  if (skipFailures > 0) {
    --skipFailures;
    return false;
  }
  nextFailure = Failure::None;
  ++failures;
  SDL_SetError("injected subject upload failure");
  return true;
}

template <typename F> F Original(const char *name) {
  auto function = reinterpret_cast<F>(dlsym(RTLD_NEXT, name));
  assert(function != nullptr);
  return function;
}
}

extern "C" SDL_GPUBuffer *SDLCALL SDL_CreateGPUBuffer(SDL_GPUDevice *device,
                                                      const SDL_GPUBufferCreateInfo *info) {
  ++bufferAllocations;
  static const auto original = Original<decltype(&SDL_CreateGPUBuffer)>("SDL_CreateGPUBuffer");
  return original(device, info);
}

extern "C" void *SDLCALL SDL_MapGPUTransferBuffer(SDL_GPUDevice *device,
                                                  SDL_GPUTransferBuffer *buffer,
                                                  bool cycle) {
  if (Reject(Failure::Map)) { return nullptr; }
  static const auto original =
      Original<decltype(&SDL_MapGPUTransferBuffer)>("SDL_MapGPUTransferBuffer");
  return original(device, buffer, cycle);
}

extern "C" SDL_GPUCommandBuffer *SDLCALL SDL_AcquireGPUCommandBuffer(SDL_GPUDevice *device) {
  if (Reject(Failure::Acquire)) { return nullptr; }
  static const auto original =
      Original<decltype(&SDL_AcquireGPUCommandBuffer)>("SDL_AcquireGPUCommandBuffer");
  return original(device);
}

extern "C" SDL_GPUCopyPass *SDLCALL SDL_BeginGPUCopyPass(SDL_GPUCommandBuffer *commands) {
  if (Reject(Failure::Pass)) { return nullptr; }
  static const auto original = Original<decltype(&SDL_BeginGPUCopyPass)>("SDL_BeginGPUCopyPass");
  return original(commands);
}

extern "C" bool SDLCALL SDL_SubmitGPUCommandBuffer(SDL_GPUCommandBuffer *commands) {
  if (nextFailure == Failure::Submit && skipFailures == 0) {
    const bool consumed = SDL_CancelGPUCommandBuffer(commands);
    assert(consumed);
    (void)Reject(Failure::Submit);
    return false;
  }
  (void)Reject(Failure::Submit);
  static const auto original =
      Original<decltype(&SDL_SubmitGPUCommandBuffer)>("SDL_SubmitGPUCommandBuffer");
  return original(commands);
}

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test;

std::vector<uint32_t> Read(SDL_GPUDevice *device, SDL_GPUBuffer *buffer, size_t words) {
  if (buffer == nullptr) {
    CHECK(false, "GPU table buffer exists before readback");
    return {};
  }
  Readback reader;
  if (reader.FromBuffer(device, buffer, static_cast<uint32_t>(words * sizeof(uint32_t))) !=
      ReadState::Ready) {
    CHECK(false, "GPU table readback succeeds");
    return {};
  }
  std::vector<uint32_t> result(words);
  std::memcpy(result.data(), reader.Rows(), result.size() * sizeof(uint32_t));
  return result;
}

void DeferredRetry(SDL_GPUDevice *device) {
  SubjectResidency residency;
  residency.StandsOn(device, true);
  const std::vector<uint32_t> initial{0, 0, 0, 0};
  const std::vector<uint32_t> expected{17, 23, 31, 47};
  constexpr auto stream = SubjectResidency::Stream::ClusterJobs;
  std::array<SubjectResidency::Crossing, 1> crossing{
      {{.Which = stream,
        .Usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ,
        .From = initial.data(),
        .Bytes = 16}}};
  std::string error;
  CHECK(residency.Cross(crossing, false, error), "deferred fixture initializes GPU data");
  crossing[0].From = expected.data();
  CHECK(residency.Cross(crossing, true, error), "replacement is staged");
  auto *failed = SDL_AcquireGPUCommandBuffer(device);
  CHECK(failed != nullptr, "failed pass acquires commands");
  if (failed == nullptr) { return; }
  nextFailure = Failure::Pass;
  skipFailures = 0;
  CHECK(!residency.FlushCrossings(failed, error) && error.find("injected") != std::string::npos,
        "copy pass failure is reported without consuming pending uploads");
  nextFailure = Failure::None;
  CHECK(SDL_CancelGPUCommandBuffer(failed), "failed pass commands can be cancelled");
  auto *cancelled = SDL_AcquireGPUCommandBuffer(device);
  CHECK(cancelled != nullptr, "cancelled recording acquires commands");
  if (cancelled == nullptr) { return; }
  CHECK(residency.FlushCrossings(cancelled, error), "pending upload recording succeeds");
  CHECK(SDL_CancelGPUCommandBuffer(cancelled), "recorded upload is cancelled before submission");
  CHECK(Read(device, residency.Buffer(stream).Get(), initial.size()) == initial,
        "cancelled commands leave initial GPU contents intact");
  auto *retry = SDL_AcquireGPUCommandBuffer(device);
  CHECK(retry != nullptr, "retry acquires commands");
  if (retry == nullptr) { return; }
  CHECK(residency.FlushCrossings(retry, error), "pending upload recording succeeds");
  CHECK(SDL_SubmitGPUCommandBuffer(retry), "retry submission succeeds");
  residency.CommitCrossings();
  CHECK(Read(device, residency.Buffer(stream).Get(), expected.size()) == expected,
        "retry records the unacknowledged upload again");
}

void MixedUploads(SDL_GPUDevice *device) {
  SubjectResidency residency;
  residency.StandsOn(device, true);
  constexpr auto stream = SubjectResidency::Stream::ClusterJobs;
  const std::vector<uint32_t> initial{0, 0, 0, 0};
  const std::vector<uint32_t> staged{17, 23, 31, 47};
  const std::vector<uint32_t> latest{53, 59, 61, 67};
  std::array<SubjectResidency::Crossing, 1> crossing{
      {{.Which = stream,
        .Usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ,
        .From = initial.data(),
        .Bytes = 16}}};
  std::string error;
  CHECK(residency.Cross(crossing, false, error), "mixed fixture initializes data");
  crossing[0].From = staged.data();
  CHECK(residency.Cross(crossing, true, error), "earlier upload is deferred");
  crossing[0].From = latest.data();
  CHECK(residency.Cross(crossing, false, error), "later upload is immediate");
  auto *commands = SDL_AcquireGPUCommandBuffer(device);
  CHECK(commands != nullptr, "mixed upload recording acquires commands");
  if (commands == nullptr) { return; }
  CHECK(residency.FlushCrossings(commands, error), "pending upload recording succeeds");
  CHECK(SDL_SubmitGPUCommandBuffer(commands), "remaining deferred work submits");
  residency.CommitCrossings();
  CHECK(Read(device, residency.Buffer(stream).Get(), latest.size()) == latest,
        "earlier deferred data cannot overwrite a later immediate upload");
  crossing[0].From = staged.data();
  CHECK(residency.Cross(crossing, true, error), "growth has pending data to preserve");
  auto *original = residency.Buffer(stream).Get();
  const auto capacity = residency.HeldOf(stream);
  for (Failure point : {Failure::Acquire, Failure::Pass, Failure::Submit}) {
    nextFailure = point;
    skipFailures = 0;
    CHECK(
        !residency.Grow(stream,
                        {.Usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ, .Bytes = capacity * 2},
                        error),
        "pending submission failure prevents growth");
    CHECK(nextFailure == Failure::None, "growth reaches pending submission failure");
    nextFailure = Failure::None;
    CHECK(residency.Buffer(stream).Get() == original && residency.HeldOf(stream) == capacity,
          "failed pending submission preserves residency");
    CHECK(Read(device, original, latest.size()) == latest,
          "failed pending submission leaves the submitted GPU version intact");
  }
  CHECK(residency.Grow(stream,
                       {.Usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ, .Bytes = capacity * 2},
                       error),
        "growth retries pending uploads before copying");
  CHECK(Read(device, residency.Buffer(stream).Get(), staged.size()) == staged,
        "grown buffer contains pending values instead of the obsolete submitted version");
}

void Growth(SDL_GPUDevice *device) {
  constexpr auto stream = SubjectResidency::Stream::ClusterJobs;
  constexpr auto usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ;
  const std::vector<uint32_t> expected{17, 23, 31, 47};
  for (Failure point : {Failure::Acquire, Failure::Pass, Failure::Submit}) {
    SubjectResidency residency;
    residency.StandsOn(device, true);
    std::string error;
    std::array<SubjectResidency::Crossing, 1> crossing{
        {{.Which = stream,
          .Usage = usage,
          .From = expected.data(),
          .Bytes = static_cast<uint32_t>(expected.size() * sizeof(uint32_t))}}};
    CHECK(residency.Cross(crossing, false, error), "growth fixture uploads known words");
    auto *original = residency.Buffer(stream).Get();
    const auto capacity = residency.HeldOf(stream);
    const auto maximum = std::numeric_limits<uint32_t>::max();
    for (const auto invalid :
         {SubjectResidency::Crossing{.Which = SubjectResidency::Stream::Count},
          SubjectResidency::Crossing{.Which = stream,
                                     .Usage = usage,
                                     .From = expected.data(),
                                     .Bytes = 16,
                                     .Offset = maximum - 7},
          SubjectResidency::Crossing{
              .Which = stream, .Usage = usage, .From = expected.data(), .Bytes = maximum}}) {
      std::array<SubjectResidency::Crossing, 2> rejected{{{.Which = stream}, invalid}};
      const auto allocations = bufferAllocations;
      CHECK(!residency.Cross(rejected, false, error), "invalid upload is rejected before mutation");
      CHECK(bufferAllocations == allocations, "invalid upload performs no GPU allocation");
      CHECK(residency.Buffer(stream).Get() == original && residency.HeldOf(stream) == capacity,
            "invalid later crossing cannot release an earlier buffer");
      CHECK(Read(device, original, expected.size()) == expected,
            "preflight rejection preserves existing GPU data");
    }
    auto large = crossing[0];
    large.Bytes = (maximum / 2u) + 1u;
    std::array<SubjectResidency::Crossing, 2> oversized{{large, large}};
    for (const bool deferred : {false, true}) {
      const auto allocations = bufferAllocations;
      CHECK(!residency.Cross(oversized, deferred, error),
            "sum of individually representable uploads is checked");
      CHECK(bufferAllocations == allocations, "sum overflow performs no GPU allocation");
      CHECK(residency.Buffer(stream).Get() == original && residency.HeldOf(stream) == capacity,
            "sum overflow preserves residency");
    }
    for (unsigned attempt = 0; attempt < 2; ++attempt) {
      nextFailure = point;
      skipFailures = 0;
      const unsigned before = failures;
      CHECK(!residency.Grow(stream, {.Usage = usage, .Bytes = capacity * 2}, error),
            "failed preservation copy rejects growth");
      CHECK(failures == before + 1 && error.find("injected") != std::string::npos,
            "growth reports the injected copy failure");
      nextFailure = Failure::None;
      CHECK(residency.Buffer(stream).Get() == original && residency.HeldOf(stream) == capacity,
            "failed growth preserves buffer identity and capacity");
      CHECK(Read(device, original, expected.size()) == expected,
            "failed growth preserves original GPU contents");
    }
    CHECK(residency.Grow(stream, {.Usage = usage, .Bytes = capacity * 2}, error),
          "growth succeeds after repeated failures");
    CHECK(residency.HeldOf(stream) >= capacity * 2,
          "successful growth publishes requested capacity");
    CHECK(Read(device, residency.Buffer(stream).Get(), expected.size()) == expected,
          "successful growth copies all existing GPU words");
  }
}

void Tables(SDL_GPUDevice *device) {
  SubjectDraw draw;
  Gpu gpu{.Device = device,
          .HdrFormat = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT,
          .SurfaceFormat = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
          .Width = 32,
          .Height = 32};
  gpu.FiltersFloat32 = SDL_GPUTextureSupportsFormat(device,
                                                    SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT,
                                                    SDL_GPU_TEXTURETYPE_2D,
                                                    SDL_GPU_TEXTUREUSAGE_SAMPLER);
  CHECK(gpu.SceneColours.Add(Resource::SceneHdr), "one scene attachment is declared");
  std::string error;
  const bool configured = draw.Configure(gpu, error);
  CHECK(configured, error.c_str());
  if (!configured) { return; }
  const std::array<SubjectMaterial, 1> materials{};
  const bool materialReady = draw.SetMaterials(materials, error);
  CHECK(materialReady, error.c_str());
  if (!materialReady) { return; }
  const std::array<uint32_t, 1> slots{0};
  draw.WearPieces(slots);
  const std::array<StoredVertex, 3> vertices{StoredVertex::Of({{-1, -1, 0}}, {}, {{0, 0, 1}}),
                                             StoredVertex::Of({{1, -1, 0}}, {}, {{0, 0, 1}}),
                                             StoredVertex::Of({{0, 1, 0}}, {}, {{0, 0, 1}})};
  const std::array<uint32_t, 3> indices{0, 1, 2};
  const std::array<DagCluster, 1> clusters{{{.SelfRadius = 2, .ParentRadius = 2, .Count = 3}}};
  const PieceMesh piece{.Verts = vertices, .Indices = indices, .Clusters = clusters};
  const auto id = draw.PlacePiece(piece, error);
  CHECK(id != kNoPiece && draw.HandTables(error), "initial clustered piece publishes tables");
  if (id == kNoPiece) { return; }
  CHECK(draw.ClusterJobs() == 1, "fixture starts with one actual clustered job");
  for (Failure point : {Failure::Map, Failure::Acquire, Failure::Pass, Failure::Submit}) {
    for (unsigned after : {0u, 1u}) {
      const std::array<Mat4, 1> one{piece.Row};
      const std::array<Mat4, 2> two{piece.Row, piece.Row};
      CHECK(draw.SetPieceInstances(id, one, error) && draw.HandTables(error),
            "baseline has one instance");
      CHECK(draw.SetPieceInstances(id, two, error), "two instances require different tables");
      for (unsigned attempt = 0; attempt < 2; ++attempt) {
        const unsigned before = failures;
        nextFailure = point;
        skipFailures = after;
        error.clear();
        CHECK(!draw.HandTables(error), "failed table upload is reported on every attempt");
        CHECK(failures == before + 1 && error.find("injected") != std::string::npos,
              "retry reaches the failing upload and preserves its cause");
        CHECK(draw.ClusterJobs() == 0 && draw.ClusterBatchRows() == 0,
              "failed tables advertise no usable clustered jobs or arguments");
        nextFailure = Failure::None;
      }
      CHECK(draw.HandTables(error), "retry without another scene mutation succeeds");
      CHECK(draw.ClusterJobs() == 2 && draw.ClusterBatchRows() == 2,
            "both instance jobs are published only after recovery");
      const std::vector<uint32_t> expected{0, 0, 0, 3, 1, 1, 0, 3};
      CHECK(Read(device, draw.Resident().Buffer(SubjectResidency::Stream::ClusterJobs).Get(), 8) ==
                expected,
            "GPU jobs contain both triangles with distinct batch and sphere rows");
    }
  }
}
}

int main() {
  using namespace outshine::Render;
  using namespace outshine::Test;
  CHECK(SDL_SetHint(SDL_HINT_ASSERT, "abort"),
        "SDL assertions terminate instead of opening dialogs");
  CHECK(SDL_Init(SDL_INIT_VIDEO), "SDL video initializes");
#ifdef OUTSHINE_GPU_VALIDATION
  constexpr bool debug = true;
#else
  constexpr bool debug = false;
#endif
  {
    OwnedDevice device(SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_MSL | SDL_GPU_SHADERFORMAT_SPIRV |
                                               SDL_GPU_SHADERFORMAT_DXIL,
                                           debug,
                                           nullptr));
    CHECK(device.Get() != nullptr, "real GPU device is available");
    if (device) {
      Tables(device.Get());
      Growth(device.Get());
      DeferredRetry(device.Get());
      MixedUploads(device.Get());
    }
  }
  SDL_Quit();
  return Report();
}
