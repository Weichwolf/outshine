#include "RuntimeScene.h"
#include "ReadbackTransfer.h"
#include "Check.h"
#include "test/harness/render/NativeCommandsProxy.h"
#include "test/harness/render/NativeDeviceProxy.h"
#include <SDL3/SDL.h>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <expected>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
namespace Native = outshine::Render::Native;

struct BufferRecord {
  Native::Device *Device;
  Native::Buffer *Buffer;
  uint64_t Bytes;
  bool Released = false;
};

struct BufferCapture {
  bool Enabled = false;
  size_t PipelineCreations = 0;
  std::vector<BufferRecord> Buffers;

  [[nodiscard]] bool Contains(size_t first, const std::array<uint32_t, 4> &expected) const {
    for (size_t at = first; at < Buffers.size(); ++at) {
      const auto &record = Buffers[at];
      if (record.Released || record.Bytes != sizeof(expected)) { continue; }
      auto read = outshine::Render::ReadbackTransfer::Start(
          *record.Device, *record.Buffer, {.Bytes = sizeof(expected)});
      if (!read) { continue; }
      auto bytes = read->Wait(*record.Device, std::chrono::seconds(5));
      if (!bytes || !*bytes || (**bytes).size() != sizeof(expected)) { continue; }
      if (std::memcmp((**bytes).data(), expected.data(), sizeof(expected)) == 0) { return true; }
    }
    return false;
  }
};

class TrackedBuffer final : public Native::Buffer {
public:
  TrackedBuffer(std::unique_ptr<Native::Buffer> buffer, BufferCapture &capture, size_t index)
      : Buffer(buffer->Description()),
        Buffer_(std::move(buffer)),
        BufferCapture_(capture),
        Index_(index) {}

  ~TrackedBuffer() override { BufferCapture_.Buffers[Index_].Released = true; }

  [[nodiscard]] Native::Buffer &Backing() const { return *Buffer_; }

private:
  std::unique_ptr<Native::Buffer> Buffer_;
  BufferCapture &BufferCapture_;
  size_t Index_;
};

Native::Buffer *Unwrap(Native::Buffer *buffer) {
  const auto *tracked = dynamic_cast<const TrackedBuffer *>(buffer);
  return tracked != nullptr ? &tracked->Backing() : buffer;
}

const Native::Buffer *Unwrap(const Native::Buffer *buffer) {
  const auto *tracked = dynamic_cast<const TrackedBuffer *>(buffer);
  return tracked != nullptr ? &tracked->Backing() : buffer;
}

class TrackingCommands final : public outshine::Test::Gpu::NativeCommandsProxy {
public:
  using NativeCommandsProxy::Copy;
  using NativeCommandsProxy::NativeCommandsProxy;

  Native::Result<void> Copy(const Native::Buffer &source,
                            Native::Buffer &destination,
                            Native::BufferCopy region) override {
    return NativeCommandsProxy::Copy(*Unwrap(&source), *Unwrap(&destination), region);
  }
};

class TrackingDevice final : public outshine::Test::Gpu::NativeDeviceProxy {
public:
  TrackingDevice(std::unique_ptr<Native::Device> device, BufferCapture &capture)
      : NativeDeviceProxy(*device), Device_(std::move(device)), BufferCapture_(capture) {}

  static Native::Result<std::unique_ptr<Native::Device>> Create(void *context,
                                                                Native::DeviceOptions options) {
    auto device = Native::CreateDevice(options);
    if (!device) { return std::unexpected(device.error()); }
    return std::make_unique<TrackingDevice>(std::move(*device),
                                            *static_cast<BufferCapture *>(context));
  }

  Native::Result<std::unique_ptr<Native::Buffer>>
  CreateBuffer(Native::BufferDescription description, std::string_view label) override {
    auto buffer = Backing().CreateBuffer(description, label);
    if (!buffer || !BufferCapture_.Enabled || description.Placement != Native::Memory::Device) {
      return buffer;
    }
    const auto index = BufferCapture_.Buffers.size();
    BufferCapture_.Buffers.push_back(
        {.Device = &Backing(), .Buffer = buffer->get(), .Bytes = description.Bytes});
    return std::make_unique<TrackedBuffer>(std::move(*buffer), BufferCapture_, index);
  }

  Native::Result<std::unique_ptr<Native::GraphicsPipeline>>
  CreateGraphicsPipeline(const Native::GraphicsDescription &description) override {
    auto pipeline = Backing().CreateGraphicsPipeline(description);
    BufferCapture_.PipelineCreations += pipeline ? 1u : 0u;
    return pipeline;
  }

  Native::Result<std::unique_ptr<Native::Commands>> Begin() override {
    auto commands = Backing().Begin();
    if (!commands) { return std::unexpected(commands.error()); }
    return std::make_unique<TrackingCommands>(std::move(*commands));
  }

  Native::Result<Native::Completion> Submit(std::unique_ptr<Native::Commands> commands) override {
    return Backing().Submit(static_cast<TrackingCommands &>(*commands).Take());
  }

private:
  std::unique_ptr<Native::Device> Device_;
  BufferCapture &BufferCapture_;
};

auto ReplaceClassification(outshine::Render::SceneRenderer &renderer) {
  using namespace outshine::Test;
  std::string error;
  const std::array<uint32_t, 4> changed{9, 8, 7, 6};
  const std::array<float, 4> changedPalette{0.75f, 0.5f, 0.25f, 0.0f};
  auto pendingClasses = std::make_shared<const std::array<uint32_t, 4>>(changed);
  auto pendingPalette = std::make_shared<const std::array<float, 4>>(changedPalette);
  const std::weak_ptr<const std::array<uint32_t, 4>> pendingClassLifetime = pendingClasses;
  const std::weak_ptr<const std::array<float, 4>> pendingPaletteLifetime = pendingPalette;
  outshine::Render::GroundClassificationSource pending{
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
  constexpr size_t kUploadStepLimit = 2000;
  bool complete = false;
  for (size_t step = 0; step < kUploadStepLimit && !complete; ++step) {
    const auto advanced = renderer.AdvanceGroundClasses(4);
    CHECK(advanced.has_value(), "candidate upload advances in aligned ranges");
    if (!advanced) { break; }
    complete = *advanced;
    if (!complete) { SDL_Delay(1); }
  }
  CHECK(complete && !pendingClassLifetime.expired() && !pendingPaletteLifetime.expired(),
        "committed candidate keeps its original source without a CPU copy");
  return std::pair{pendingClassLifetime, pendingPaletteLifetime};
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes");
  {
    BufferCapture captureState;
    auto &capture = captureState.Enabled;
    const auto &pipelineCreations = captureState.PipelineCreations;
    const auto &buffers = captureState.Buffers;
    Render::SceneRenderer renderer({.Context = &captureState, .Create = TrackingDevice::Create});
    const auto uploadBudget = renderer.OfflineUploads(std::chrono::seconds(5));
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
      CHECK(!renderer.SetGroundClasses(
                Render::GroundClassificationSource{.Classes = {}, .ClassWords = 4, .Palette = {}},
                error),
            "an unowned source cannot replace live classification");
      CHECK(buffers.size() == 2 && captureState.Contains(0, classes) &&
                captureState.Contains(0, paletteBits),
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
          CHECK(captureState.Contains(restoredAt, classes) &&
                    captureState.Contains(restoredAt, paletteBits),
                "replacement restores independent copies of both classification inputs");
          const auto [pendingClassLifetime, pendingPaletteLifetime] =
              ReplaceClassification(renderer);
          CHECK(!buffers[0].Released && !buffers[1].Released,
                "candidate upload cannot retire either published buffer");
          candidate.reset();
          renderer.AbandonsWorldCandidate();
          CHECK(pendingClassLifetime.expired() && pendingPaletteLifetime.expired(),
                "discarded candidate releases its retained classification source");
          CHECK(!buffers[0].Released && !buffers[1].Released && captureState.Contains(0, classes) &&
                    captureState.Contains(0, paletteBits),
                "rejected candidate leaves original GPU payloads alive and unchanged");
          const size_t retryAt = buffers.size();
          capture = true;
          const bool retry = Core::RuntimeScene::PreparesWorldReplacement(
              renderer, *scene, nullptr, candidate, error);
          capture = false;
          CHECK(retry && captureState.Contains(retryAt, classes) &&
                    captureState.Contains(retryAt, paletteBits),
                "retry restores the original CPU snapshot, not rejected candidate data");
          if (retry) {
            CHECK(Core::RuntimeScene::PublishesPreparedWorld(renderer, scene, candidate, error),
                  "complete replacement publishes");
            CHECK(buffers[0].Released && buffers[1].Released &&
                      captureState.Contains(retryAt, classes) &&
                      captureState.Contains(retryAt, paletteBits),
                  "publication retires old buffers while retaining replacement payloads");
            const size_t freshAt = buffers.size();
            capture = true;
            const bool fresh =
                Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error);
            capture = false;
            CHECK(fresh && captureState.Contains(freshAt, {}) &&
                      !captureState.Contains(freshAt, classes) &&
                      !captureState.Contains(freshAt, paletteBits),
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
