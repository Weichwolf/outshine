#include "RuntimeScene.h"
#include "Readback.h"
#include "Check.h"
#include <SDL3/SDL.h>
#include <array>
#include <cassert>
#include <cstring>
#include <dlfcn.h>
#include <memory>
#include <utility>
#include <vector>

namespace {
struct BufferRecord {
  SDL_GPUDevice *Device;
  SDL_GPUBuffer *Buffer;
  uint32_t Bytes;
  bool Released = false;
};

bool capture = false;
size_t pipelineCreations = 0;
std::vector<BufferRecord> buffers;

bool Contains(size_t first, const std::array<uint32_t, 4> &expected) {
  for (size_t at = first; at < buffers.size(); ++at) {
    const auto &record = buffers[at];
    if (record.Released || record.Bytes != sizeof(expected)) { continue; }
    outshine::Render::Readback read;
    if (read.FromBuffer(record.Device, record.Buffer, sizeof(expected)) !=
        outshine::Render::ReadState::Ready) {
      continue;
    }
    if (std::memcmp(read.Rows(), expected.data(), sizeof(expected)) == 0) { return true; }
  }
  return false;
}
}

extern "C" SDL_GPUBuffer *SDLCALL SDL_CreateGPUBuffer(SDL_GPUDevice *device,
                                                      const SDL_GPUBufferCreateInfo *info) {
  static const auto original =
      reinterpret_cast<decltype(&SDL_CreateGPUBuffer)>(dlsym(RTLD_NEXT, "SDL_CreateGPUBuffer"));
  assert(original != nullptr);
  SDL_GPUBuffer *buffer = original(device, info);
  if (capture && buffer) { buffers.push_back({device, buffer, info->size, false}); }
  return buffer;
}

extern "C" void SDLCALL SDL_ReleaseGPUBuffer(SDL_GPUDevice *device, SDL_GPUBuffer *buffer) {
  for (auto &record : buffers) {
    if (record.Device == device && record.Buffer == buffer) { record.Released = true; }
  }
  static const auto original =
      reinterpret_cast<decltype(&SDL_ReleaseGPUBuffer)>(dlsym(RTLD_NEXT, "SDL_ReleaseGPUBuffer"));
  assert(original != nullptr);
  original(device, buffer);
}

extern "C" SDL_GPUGraphicsPipeline *SDLCALL SDL_CreateGPUGraphicsPipeline(
    SDL_GPUDevice *device, const SDL_GPUGraphicsPipelineCreateInfo *info) {
  static const auto original = reinterpret_cast<decltype(&SDL_CreateGPUGraphicsPipeline)>(
      dlsym(RTLD_NEXT, "SDL_CreateGPUGraphicsPipeline"));
  assert(original != nullptr);
  SDL_GPUGraphicsPipeline *const pipeline = original(device, info);
  pipelineCreations += pipeline != nullptr ? 1u : 0u;
  return pipeline;
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes");
  {
    Render::SceneRenderer renderer;
    Core::Declaration declaration;
    declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
    declaration.Outputs = {"surface"};
    std::unique_ptr<Core::RuntimeScene> scene;
    std::string error;
    CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error),
          "initial world opens");
    if (scene) {
      const size_t framePipelines = pipelineCreations;
      const std::array<uint32_t, 4> classes{123, 456, 789, 1024};
      const std::array<float, 4> palette{0.125f, 0.25f, 0.5f, 1.0f};
      std::array<uint32_t, 4> paletteBits{};
      std::memcpy(paletteBits.data(), palette.data(), sizeof(palette));
      auto ownedClasses = std::make_shared<const std::array<uint32_t, 4>>(classes);
      auto ownedPalette = std::make_shared<const std::array<float, 4>>(palette);
      const std::weak_ptr<const std::array<uint32_t, 4>> classLifetime = ownedClasses;
      const std::weak_ptr<const std::array<float, 4>> paletteLifetime = ownedPalette;
      Render::GroundClassificationSource source{.Classes = {ownedClasses, ownedClasses->data()},
                                                .ClassWords = ownedClasses->size(),
                                                .Palette = {ownedPalette, ownedPalette->data()},
                                                .PaletteFloats = ownedPalette->size()};
      capture = true;
      CHECK(renderer.SetGroundClasses(std::move(source), error),
            "owned original classification uploads");
      capture = false;
      ownedClasses.reset();
      ownedPalette.reset();
      CHECK(!classLifetime.expired() && !paletteLifetime.expired(),
            "the renderer retains both immutable sources after their caller releases them");
      CHECK(!renderer.SetGroundClasses(Render::GroundClassificationSource{.ClassWords = 4}, error),
            "an unowned source cannot replace live classification");
      CHECK(buffers.size() == 2 && Contains(0, classes) && Contains(0, paletteBits),
            "original class and palette bytes reach separate GPU buffers");
      if (buffers.size() == 2) {
        std::unique_ptr<Core::RuntimeScene> candidate;
        const size_t restoredAt = buffers.size();
        capture = true;
        const bool prepared = Core::RuntimeScene::PreparesWorldReplacement(
            renderer, *scene, nullptr, candidate, error);
        capture = false;
        CHECK(prepared, "replacement world prepares");
        if (prepared) {
          CHECK(pipelineCreations == framePipelines,
                "an unchanged render plan reuses target-owned pipelines");
          CHECK(Contains(restoredAt, classes) && Contains(restoredAt, paletteBits),
                "replacement restores independent copies of both classification inputs");
          const std::array<uint32_t, 4> changed{9, 8, 7, 6};
          const std::array<float, 4> changedPalette{0.75f, 0.5f, 0.25f, 0.0f};
          auto pendingClasses = std::make_shared<const std::array<uint32_t, 4>>(changed);
          auto pendingPalette = std::make_shared<const std::array<float, 4>>(changedPalette);
          const std::weak_ptr<const std::array<uint32_t, 4>> pendingClassLifetime = pendingClasses;
          const std::weak_ptr<const std::array<float, 4>> pendingPaletteLifetime = pendingPalette;
          Render::GroundClassificationSource pending{
              .Classes = {pendingClasses, pendingClasses->data()},
              .ClassWords = pendingClasses->size(),
              .Palette = {pendingPalette, pendingPalette->data()},
              .PaletteFloats = pendingPalette->size()};
          CHECK(renderer.BeginGroundClasses(std::move(pending), error),
                "candidate accepts owned classification before publication");
          pendingClasses.reset();
          pendingPalette.reset();
          CHECK(!pendingClassLifetime.expired() && !pendingPaletteLifetime.expired(),
                "pending upload keeps both source buffers alive");
          bool complete = false;
          for (size_t step = 0; step < 2000 && !complete; ++step) {
            const auto advanced = renderer.AdvanceGroundClasses(4);
            CHECK(advanced.has_value(), "candidate upload advances in aligned ranges");
            if (!advanced) { break; }
            complete = *advanced;
            if (!complete) { SDL_Delay(1); }
          }
          CHECK(complete && !pendingClassLifetime.expired() && !pendingPaletteLifetime.expired(),
                "committed candidate keeps its original source without a CPU copy");
          CHECK(!buffers[0].Released && !buffers[1].Released,
                "candidate upload cannot retire either published buffer");
          candidate.reset();
          renderer.AbandonsWorldCandidate();
          CHECK(pendingClassLifetime.expired() && pendingPaletteLifetime.expired(),
                "discarded candidate releases its retained classification source");
          CHECK(!buffers[0].Released && !buffers[1].Released && Contains(0, classes) &&
                    Contains(0, paletteBits),
                "rejected candidate leaves original GPU payloads alive and unchanged");
          const size_t retryAt = buffers.size();
          capture = true;
          const bool retry = Core::RuntimeScene::PreparesWorldReplacement(
              renderer, *scene, nullptr, candidate, error);
          capture = false;
          CHECK(retry && Contains(retryAt, classes) && Contains(retryAt, paletteBits),
                "retry restores the original CPU snapshot, not rejected candidate data");
          if (retry) {
            CHECK(Core::RuntimeScene::PublishesPreparedWorld(renderer, scene, candidate, error),
                  "complete replacement publishes");
            CHECK(buffers[0].Released && buffers[1].Released && Contains(retryAt, classes) &&
                      Contains(retryAt, paletteBits),
                  "publication retires old buffers while retaining replacement payloads");
            const size_t freshAt = buffers.size();
            capture = true;
            const bool fresh =
                Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error);
            capture = false;
            CHECK(fresh && Contains(freshAt, {}) && !Contains(freshAt, classes) &&
                      !Contains(freshAt, paletteBits),
                  "a new declaration starts with empty classification, not the previous world");
            CHECK(pipelineCreations == framePipelines,
                  "a fresh world with the same plan keeps the target frame");
          }
        }
      }
    }
  }
  SDL_Quit();
  return Report();
}
