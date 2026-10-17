#include "test/harness/render/NativeCommandsProxy.h"
#include "test/harness/render/NativeDeviceProxy.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include "test/harness/render/NativeOffline.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <print>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {
namespace Native = outshine::Render::Native;
namespace Render = outshine::Render;
using Clock = std::chrono::steady_clock;
using outshine::Test::Gpu::RetryOffline;
constexpr auto kTimeout = std::chrono::seconds(5);
constexpr size_t kSide = 35;
constexpr size_t kNodes = kSide * kSide;
constexpr size_t kBytes = kNodes * sizeof(float);
constexpr size_t kColumns = 16;
constexpr size_t kPerLayer = kColumns * kColumns;
constexpr size_t kBatch = 48;
constexpr size_t kMaximum = 65536;

[[noreturn]] void Fail(const std::string &message) {
  std::println(stderr, "{}", message);
  std::exit(2);
}

template <typename T> T Require(Native::Result<T> result) {
  if (!result) { Fail(result.error().Message); }
  return std::move(*result);
}

void Require(Native::Result<void> result) {
  if (!result) { Fail(result.error().Message); }
}

struct Counts {
  Native::Device *Device = nullptr;
  Native::Texture *Atlas = nullptr;
  Native::Completion Last{};
  uint64_t Allocations = 0;
  uint64_t UploadBytes = 0;
  uint64_t Submissions = 0;
  uint64_t TextureCopyBytes = 0;
};

class Commands final : public outshine::Test::Gpu::NativeCommandsProxy {
public:
  Commands(std::unique_ptr<Native::Commands> commands, Counts &counts)
      : NativeCommandsProxy(std::move(commands)), Counts_(counts) {}

  using NativeCommandsProxy::Copy;

  Native::Result<void> Copy(const Native::Texture &source,
                            Native::TextureRegion from,
                            Native::Texture &destination,
                            Native::TextureRegion into) override {
    auto copied = NativeCommandsProxy::Copy(source, from, destination, into);
    if (copied) { Counts_.TextureCopyBytes += uint64_t{from.Width} * from.Height * sizeof(float); }
    return copied;
  }

private:
  Counts &Counts_;
};

class Device final : public outshine::Test::Gpu::NativeDeviceProxy {
public:
  Device(std::unique_ptr<Native::Device> device, Counts &counts)
      : NativeDeviceProxy(*device), Device_(std::move(device)), Counts_(counts) {
    Counts_.Device = this;
  }

  static Native::Result<std::unique_ptr<Native::Device>> Create(void *context,
                                                                Native::DeviceOptions options) {
    auto device = Native::CreateDevice(options);
    if (!device) { return std::unexpected(device.error()); }
    return std::make_unique<Device>(std::move(*device), *static_cast<Counts *>(context));
  }

  Native::Result<std::unique_ptr<Native::Buffer>>
  CreateBuffer(Native::BufferDescription description, std::string_view label) override {
    auto buffer = Backing().CreateBuffer(description, label);
    if (buffer && description.Placement == Native::Memory::Upload) {
      ++Counts_.Allocations;
      Counts_.UploadBytes += description.Bytes;
    }
    return buffer;
  }

  Native::Result<std::unique_ptr<Native::Texture>>
  CreateTexture(Native::TextureDescription description, std::string_view label) override {
    auto texture = Backing().CreateTexture(description, label);
    if (texture && description.Texels == Native::Format::R32Float &&
        description.Type == Native::TextureType::Image2DArray) {
      Counts_.Atlas = texture->get();
    }
    return texture;
  }

  Native::Result<std::unique_ptr<Native::Commands>> Begin() override {
    auto commands = Backing().Begin();
    if (!commands) { return std::unexpected(commands.error()); }
    return std::make_unique<Commands>(std::move(*commands), Counts_);
  }

  Native::Result<Native::Completion> Submit(std::unique_ptr<Native::Commands> commands) override {
    auto submitted = Backing().Submit(static_cast<Commands &>(*commands).Take());
    if (submitted) {
      ++Counts_.Submissions;
      Counts_.Last = *submitted;
    }
    return submitted;
  }

private:
  std::unique_ptr<Native::Device> Device_;
  Counts &Counts_;
};

std::vector<float> Load(const char *path) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  const std::streamoff bytes = file ? file.tellg() : std::streampos(-1);
  if (bytes <= 0 || bytes % kBytes != 0 || static_cast<uint64_t>(bytes) / kBytes > kMaximum) {
    Fail("input must contain complete 35x35 float32 height pages within the atlas capacity");
  }
  std::vector<float> nodes(static_cast<size_t>(bytes) / sizeof(float));
  file.seekg(0);
  file.read(reinterpret_cast<char *>(nodes.data()), bytes);
  if (!file) { Fail("input could not be read completely"); }
  return nodes;
}

void Wait(Counts &counts) {
  if (Require(counts.Device->Wait(counts.Last, kTimeout)) != Native::CompletionStatus::Ready) {
    Fail("GPU completion exceeded five seconds");
  }
}

void Place(Render::SceneRenderer &renderer, std::span<const float> nodes) {
  const size_t pages = nodes.size() / kNodes;
#if OUTSHINE_HEIGHT_PAGE_BATCH
  for (size_t first = 0; first < pages; first += kBatch) {
    const size_t count = std::min(kBatch, pages - first);
    std::array<std::span<const float>, kBatch> batch{};
    for (size_t at = 0; at < count; ++at) {
      batch[at] = nodes.subspan((first + at) * kNodes, kNodes);
    }
    const auto handles = Require(RetryOffline(
        [&] { return renderer.PlaceHeightPages(std::span(batch).first(count)); }, kTimeout));
    for (size_t at = 0; at < count; ++at) {
      if (handles[at].Slot != first + at) { Fail("fresh arena page order differs"); }
    }
  }
#else
  for (size_t at = 0; at < pages; ++at) {
    const auto handle = Require(RetryOffline(
        [&] { return renderer.PlaceHeightPage(nodes.subspan(at * kNodes, kNodes)); }, kTimeout));
    if (handle.Slot != at) { Fail("fresh arena page order differs"); }
  }
#endif
}

void Verify(Counts &counts, std::span<const float> nodes) {
  if (counts.Atlas == nullptr) { Fail("the production R32 atlas was not observed"); }
  auto &device = *counts.Device;
  const size_t pages = nodes.size() / kNodes;
  auto readback = Require(device.CreateBuffer({.Bytes = nodes.size_bytes(),
                                               .Use = Native::BufferUse::CopyDestination,
                                               .Placement = Native::Memory::Readback}));
  for (size_t first = 0; first < pages; first += kPerLayer) {
    auto commands = Require(RetryOffline([&] { return device.Begin(); }, kTimeout));
    const size_t end = std::min(pages, first + kPerLayer);
    for (size_t at = first; at < end; ++at) {
      Require(
          commands->Copy(*counts.Atlas,
                         *readback,
                         {.BufferOffset = at * kBytes,
                          .RowBytes = kSide * sizeof(float),
                          .Region = {.Layer = static_cast<uint32_t>(at / kPerLayer),
                                     .X = static_cast<uint32_t>(at % kColumns * kSide),
                                     .Y = static_cast<uint32_t>(at % kPerLayer / kColumns * kSide),
                                     .Width = kSide,
                                     .Height = kSide}}));
    }
    Require(device.Submit(std::move(commands)));
    Wait(counts);
  }
  const auto mapped = Require(device.MapRead(*readback, {.Bytes = nodes.size_bytes()}));
  if (std::memcmp(mapped.data(), nodes.data(), nodes.size_bytes()) != 0) {
    Fail("the production GPU atlas differs from the complete original float32 input");
  }
  Require(device.Unmap(*readback));
}

void Run(const char *input) {
  const auto nodes = Load(input);
  outshine::Core::Declaration declaration;
  declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
  declaration.Outputs = {"surface"};
  Counts counts;
  Render::SceneRenderer renderer({.Context = &counts, .Create = Device::Create});
  std::unique_ptr<outshine::Core::RuntimeScene> scene;
  std::string error;
  if (!outshine::Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error) ||
      !renderer.Settle(error)) {
    Fail(error);
  }
  counts.Allocations = counts.UploadBytes = counts.Submissions = counts.TextureCopyBytes = 0;
  const auto began = Clock::now();
  Place(renderer, nodes);
  Wait(counts);
  const double hostMs = std::chrono::duration<double, std::milli>(Clock::now() - began).count();
  const auto measured = counts;
  Verify(counts, nodes);
  std::println("{{\"batch\":{},\"pages\":{},\"payload_bytes\":{},\"verified_floats\":{},"
               "\"host_completion_ms\":{:.3f},\"upload_allocations\":{},\"upload_bytes\":{},"
               "\"submissions\":{},\"atlas_copy_bytes\":{}}}",
               OUTSHINE_HEIGHT_PAGE_BATCH ? kBatch : 1,
               nodes.size() / kNodes,
               nodes.size() * sizeof(float),
               nodes.size(),
               hostMs,
               measured.Allocations,
               measured.UploadBytes,
               measured.Submissions,
               measured.TextureCopyBytes);
}
}

int main(int argc, char **argv) {
  if (argc != 2) { Fail("usage: height-page-batch <complete-height-pages.f32>"); }
  if (!SDL_Init(SDL_INIT_VIDEO)) { Fail(SDL_GetError()); }
  Run(argv[1]);
  SDL_Quit();
}
