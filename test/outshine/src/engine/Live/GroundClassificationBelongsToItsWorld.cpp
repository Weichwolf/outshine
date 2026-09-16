#include "Live.h"
#include "Readback.h"
#include "Check.h"
#include <SDL3/SDL.h>
#include <array>
#include <cassert>
#include <cstring>
#include <dlfcn.h>
#include <memory>
#include <vector>

namespace {
struct BufferRecord {
  SDL_GPUDevice *Device;
  SDL_GPUBuffer *Buffer;
  uint32_t Bytes;
  bool Released = false;
};

bool capture = false;
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

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes");
  {
    Render::SceneRenderer renderer;
    Core::Declaration declaration;
    declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
    declaration.Outputs = {"surface"};
    std::unique_ptr<Core::Live> scene;
    std::string error;
    CHECK(Core::Live::Open(renderer, declaration, nullptr, scene, error), "initial world opens");
    if (scene) {
      const std::array<uint32_t, 4> classes{123, 456, 789, 1024};
      const std::array<float, 4> palette{0.125f, 0.25f, 0.5f, 1.0f};
      std::array<uint32_t, 4> paletteBits{};
      std::memcpy(paletteBits.data(), palette.data(), sizeof(palette));
      capture = true;
      CHECK(scene->GroundClasses(classes, palette, error), "original classification uploads");
      capture = false;
      CHECK(buffers.size() == 2 && Contains(0, classes) && Contains(0, paletteBits),
            "original class and palette bytes reach separate GPU buffers");
      if (buffers.size() == 2) {
        std::unique_ptr<Core::Live> candidate;
        const size_t restoredAt = buffers.size();
        capture = true;
        const bool prepared =
            Core::Live::PreparesWorldReplacement(renderer, *scene, nullptr, candidate, error);
        capture = false;
        CHECK(prepared, "replacement world prepares");
        if (prepared) {
          CHECK(Contains(restoredAt, classes) && Contains(restoredAt, paletteBits),
                "replacement restores independent copies of both classification inputs");
          const std::array<uint32_t, 4> changed{9, 8, 7, 6};
          const std::array<float, 4> changedPalette{0.75f, 0.5f, 0.25f, 0.0f};
          CHECK(candidate->GroundClasses(changed, changedPalette, error),
                "candidate classification changes before publication");
          CHECK(!buffers[0].Released && !buffers[1].Released,
                "candidate upload cannot retire either published buffer");
          candidate.reset();
          renderer.AbandonsWorldCandidate();
          CHECK(!buffers[0].Released && !buffers[1].Released && Contains(0, classes) &&
                    Contains(0, paletteBits),
                "rejected candidate leaves original GPU payloads alive and unchanged");
          const size_t retryAt = buffers.size();
          capture = true;
          const bool retry =
              Core::Live::PreparesWorldReplacement(renderer, *scene, nullptr, candidate, error);
          capture = false;
          CHECK(retry && Contains(retryAt, classes) && Contains(retryAt, paletteBits),
                "retry restores the original CPU snapshot, not rejected candidate data");
          if (retry) {
            CHECK(Core::Live::PublishesPreparedWorld(renderer, scene, candidate, error),
                  "complete replacement publishes");
            CHECK(buffers[0].Released && buffers[1].Released && Contains(retryAt, classes) &&
                      Contains(retryAt, paletteBits),
                  "publication retires old buffers while retaining replacement payloads");
            const size_t freshAt = buffers.size();
            capture = true;
            const bool fresh = Core::Live::Open(renderer, declaration, nullptr, scene, error);
            capture = false;
            CHECK(fresh && Contains(freshAt, {}) && !Contains(freshAt, classes) &&
                      !Contains(freshAt, paletteBits),
                  "a new declaration starts with empty classification, not the previous world");
          }
        }
      }
    }
  }
  SDL_Quit();
  return Report();
}
