#include "NativeGpuCheck.h"
#include "Check.h"
#include <SDL3/SDL.h>
#include <array>
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <print>
#include <ratio>
#include <string_view>
#include <thread>
#include <utility>

namespace {
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;
using Clock = std::chrono::steady_clock;
constexpr uint32_t kDelaySteps = 20000000;
constexpr uint32_t kMultiplier = 1664525;
constexpr uint32_t kIncrement = 1013904223;
constexpr int kFirstHeight = 96;
constexpr int kSecondWidth = 154;
constexpr int kSecondHeight = 112;
constexpr int kOccupiedWidth = 192;
double maxTargetMs = 0;
double maxAcquireMs = 0;
uint32_t targetCalls = 0;

Result<void> Target(Device &device, SDL_Window *window) {
  const auto start = Clock::now();
  auto result = device.SetPresentTarget(window);
  maxTargetMs = std::max(maxTargetMs,
                         std::chrono::duration<double, std::milli>(Clock::now() - start).count());
  ++targetCalls;
  return result;
}

using Window = std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)>;

struct Assets {
  std::unique_ptr<GraphicsPipeline> Raster;
  std::unique_ptr<ComputePipeline> Delay;
  std::unique_ptr<Buffer> Values;
  std::unique_ptr<Texture> Offscreen;
  Completion First;
};

std::unique_ptr<PresentFrame> Acquire(Device &device, SDL_Window *window) {
  const auto until = Clock::now() + std::chrono::seconds(5);
  while (Clock::now() < until) {
    SDL_PumpEvents();
    const auto start = Clock::now();
    auto result = device.AcquirePresentFrame();
    maxAcquireMs = std::max(
        maxAcquireMs, std::chrono::duration<double, std::milli>(Clock::now() - start).count());
    auto frame = Require(std::move(result));
    if (frame) {
      int width = 0;
      int height = 0;
      CHECK(SDL_GetWindowSizeInPixels(window, &width, &height),
            "the selected window has queryable actual pixels");
      CHECK(std::cmp_equal(frame->Image().Description().Width, width) &&
                std::cmp_equal(frame->Image().Description().Height, height),
            "the acquired image belongs to the requested window extent");
      return frame;
    }
    SDL_Delay(1);
  }
  CHECK(false, "a prepared presentation target supplies a frame");
  Failed();
}

Assets Prepare(Device &device, char *const *argv) {
  const auto vertex = Spirv(argv[1]);
  const auto fragment = Spirv(argv[2]);
  const std::array colours{ColourTarget{.Texels = Format::Bgra8Unorm, .Rgb = {}, .Alpha = {}}};
  Assets assets;
  assets.Raster = Require(device.CreateGraphicsPipeline({.Vertex = {.Spirv = vertex},
                                                         .Fragment = ShaderCode{.Spirv = fragment},
                                                         .Streams = {},
                                                         .Attributes = {},
                                                         .Colours = colours,
                                                         .Depth = {},
                                                         .Culling = Cull::None,
                                                         .Label = {}}));
  assets.Offscreen =
      Require(device.CreateTexture({.Width = 32,
                                    .Height = 16,
                                    .Texels = Format::Bgra8Unorm,
                                    .Use = TextureUse::ColourAttachment | TextureUse::CopySource}));
  assets.Delay = Require(device.CreateComputePipeline(Spirv(argv[3])));
  auto commands = Require(device.Begin());
  const std::array<uint32_t, 1> seed{7};
  assets.Values = Upload(device, *commands, seed);
  const std::array bindings{Storage(0, 0, *assets.Values)};
  Require(commands->Dispatch(*assets.Delay, bindings, {.X = 1, .Y = 1, .Z = 1}));
  assets.First = Require(device.Submit(std::move(commands)));
  return assets;
}

void Complete(Device &device, Completion done) {
  CHECK(Require(device.Wait(done, std::chrono::seconds(5))) == CompletionStatus::Ready,
        "accepted GPU work survives target changes");
}

void Pixels(Device &device, Buffer &readback, uint32_t width, uint32_t height) {
  const auto bytes = Require(device.MapRead(readback, {.Bytes = uint64_t{width} * height * 4}));
  uint64_t wrong = 0;
  for (uint32_t y = 0; y < height; ++y) {
    for (uint32_t x = 0; x < width; ++x) {
      const int band = static_cast<int>(x / 64 % 3);
      const std::array expected{
          band == 2 ? 204 : 26, band == 1 ? 204 : 26, band == 0 ? 204 : 26, 255};
      for (size_t channel = 0; channel < 4; ++channel) {
        const int actual =
            std::to_integer<int>(bytes[(static_cast<size_t>(y) * width + x) * 4 + channel]);
        const bool matches =
            channel == 3 ? actual == 255
                         : actual >= expected[channel] - 1 && actual <= expected[channel] + 1;
        if (!matches) { ++wrong; }
      }
    }
  }
  CHECK(wrong == 0,
        "the actual switched-to surface preserves every rendered colour/alpha component");
  Require(device.Unmap(readback));
}

Completion
Render(Device &device, Assets &assets, std::unique_ptr<PresentFrame> frame, bool delayed = false) {
  const auto &image = frame->Image();
  const auto size = image.Description();
  CHECK(size.Texels == Format::Bgra8Unorm, "the target exposes the actual shared BGRA format");
  auto commands = Require(device.Begin());
  if (delayed) {
    const std::array bindings{Storage(0, 0, *assets.Values)};
    Require(commands->Dispatch(*assets.Delay, bindings, {.X = 1, .Y = 1, .Z = 1}));
  }
  const std::array colours{ColourAttachment{.Image = {.Image = &image}, .Begin = Load::Clear}};
  Require(commands->BeginRenderPass({.Colours = colours, .Depth = {}}));
  Require(commands->Draw({.State = {.Pipeline = assets.Raster.get(),
                                    .Bindings = {},
                                    .Vertices = {},
                                    .Area = {.Width = static_cast<float>(size.Width),
                                             .Height = static_cast<float>(size.Height)},
                                    .Clip = {.Width = size.Width, .Height = size.Height}},
                          .Range = {.Count = 3}}));
  Require(commands->EndRenderPass());
  std::unique_ptr<Buffer> readback;
  const bool copyable =
      (static_cast<uint32_t>(size.Use) & static_cast<uint32_t>(TextureUse::CopySource)) != 0;
  if (!delayed && copyable) {
    readback = Require(device.CreateBuffer({.Bytes = uint64_t{size.Width} * size.Height * 4,
                                            .Use = BufferUse::CopyDestination,
                                            .Placement = Memory::Readback}));
    Require(commands->Copy(image,
                           *readback,
                           {.RowBytes = uint64_t{size.Width} * 4,
                            .Region = {.Width = size.Width, .Height = size.Height}}));
  }
  Require(commands->Present(std::move(frame)));
  const auto done = Require(device.Submit(std::move(commands)));
  if (!delayed) {
    Complete(device, done);
    if (readback) { Pixels(device, *readback, size.Width, size.Height); }
  }
  return done;
}

void Stale(Device &device, std::unique_ptr<PresentFrame> frame) {
  auto commands = Require(device.Begin());
  const auto invalid = commands->Present(std::move(frame));
  CHECK(!invalid && invalid.error().Kind == ErrorKind::Invalid,
        "a previously issued lease never becomes valid again after A to B to A");
}

void SwitchBack(Device &device, Assets &assets, SDL_Window *first, SDL_Window *second) {
  auto old = Acquire(device, first);
  auto open = Require(device.Begin());
  auto busy = Target(device, second);
  CHECK(!busy && busy.error().Kind == ErrorKind::Busy,
        "an open unsubmitted recording blocks target changes without consuming its lease");
  open.reset();
  Require(Target(device, second));
  auto other = Acquire(device, second);
  Require(Target(device, first));
  Stale(device, std::move(old));
  Stale(device, std::move(other));
  Render(device, assets, Acquire(device, first));
}

void Rollback(Device &device, Assets &assets, SDL_Window *current, SDL_Window *occupied) {
  auto competing = Require(CreateDevice({.Validation = true, .Window = occupied}));
  auto held = Acquire(device, current);
  const auto failed = Target(device, occupied);
  CHECK(!failed, "another live Vulkan swapchain prevents preparation on the occupied window");
  Render(device, assets, std::move(held));
  std::thread foreign([&] {
    const auto invalid = device.SetPresentTarget(nullptr);
    CHECK(!invalid && invalid.error().Kind == ErrorKind::Invalid,
          "target preparation rejects the wrong owner thread before touching SDL");
  });
  foreign.join();
  held = Acquire(device, current);
  Require(Target(device, current));
  Render(device, assets, std::move(held));
}

void Offscreen(Device &device, Assets &assets) {
  const auto size = assets.Offscreen->Description();
  const std::array colours{
      ColourAttachment{.Image = {.Image = assets.Offscreen.get()}, .Begin = Load::Clear}};
  auto commands = Require(device.Begin());
  Require(commands->BeginRenderPass({.Colours = colours, .Depth = {}}));
  Require(commands->Draw({.State = {.Pipeline = assets.Raster.get(),
                                    .Bindings = {},
                                    .Vertices = {},
                                    .Area = {.Width = static_cast<float>(size.Width),
                                             .Height = static_cast<float>(size.Height)},
                                    .Clip = {.Width = size.Width, .Height = size.Height}},
                          .Range = {.Count = 3}}));
  Require(commands->EndRenderPass());
  auto readback = Require(device.CreateBuffer({.Bytes = uint64_t{size.Width} * size.Height * 4,
                                               .Use = BufferUse::CopyDestination,
                                               .Placement = Memory::Readback}));
  Require(commands->Copy(*assets.Offscreen,
                         *readback,
                         {.RowBytes = uint64_t{size.Width} * 4,
                          .Region = {.Width = size.Width, .Height = size.Height}}));
  Complete(device, Require(device.Submit(std::move(commands))));
  Pixels(device, *readback, size.Width, size.Height);
}

void Values(Device &device, const Assets &assets) {
  auto commands = Require(device.Begin());
  auto readback = Require(device.CreateBuffer({.Bytes = sizeof(uint32_t),
                                               .Use = BufferUse::CopyDestination,
                                               .Placement = Memory::Readback}));
  Require(commands->Copy(*assets.Values, *readback, {.Bytes = sizeof(uint32_t)}));
  Complete(device, Require(device.Submit(std::move(commands))));
  const auto bytes = Require(device.MapRead(*readback, {.Bytes = sizeof(uint32_t)}));
  uint32_t actual = 0;
  std::memcpy(&actual, bytes.data(), sizeof(actual));
  uint32_t expected = 7;
  for (int dispatch = 0; dispatch < 2; ++dispatch) {
    for (uint32_t at = 0; at < kDelaySteps; ++at) {
      expected = (expected ^ (expected >> 13u)) * kMultiplier + kIncrement;
    }
  }
  CHECK(actual == expected,
        "the same pre-window compute pipeline and GPU buffer survive all target changes");
  Require(device.Unmap(*readback));
}

int Unavailable() {
  auto device = Require(CreateDevice({.Validation = true}));
  auto upload = Require(device->CreateBuffer(
      {.Bytes = sizeof(uint32_t), .Use = BufferUse::CopySource, .Placement = Memory::Upload}));
  constexpr uint32_t value = 0x12345678;
  auto write = Require(device->MapWrite(*upload, {.Bytes = sizeof(value)}));
  std::memcpy(write.data(), &value, sizeof(value));
  Require(device->Unmap(*upload));
  CHECK(SDL_Init(SDL_INIT_VIDEO), "the unavailable-target case uses an actual SDL window");
  Window window(SDL_CreateWindow("Outshine unavailable target", 64, 64, 0), SDL_DestroyWindow);
  CHECK(window, "an unavailable device is tested with a valid window");
  const auto unsupported = Target(*device, window.get());
  CHECK(!unsupported && unsupported.error().Kind == ErrorKind::Unsupported,
        "a device without prepared presentation capability preserves offscreen operation");
  Require(Target(*device, nullptr));
  auto readback = Require(device->CreateBuffer(
      {.Bytes = sizeof(value), .Use = BufferUse::CopyDestination, .Placement = Memory::Readback}));
  auto commands = Require(device->Begin());
  Require(commands->Copy(*upload, *readback, {.Bytes = sizeof(value)}));
  Complete(*device, Require(device->Submit(std::move(commands))));
  const auto read = Require(device->MapRead(*readback, {.Bytes = sizeof(value)}));
  uint32_t actual = 0;
  std::memcpy(&actual, read.data(), sizeof(actual));
  CHECK(actual == value, "the pre-existing buffer remains usable after unsupported attachment");
  Require(device->Unmap(*readback));
  std::println("TARGET_UNAVAILABLE device={}", device->Limits().Name);
  upload.reset();
  readback.reset();
  device.reset();
  window.reset();
  SDL_Quit();
  return Report();
}

}

int main(int argc, char *const *argv) {
  if (argc == 2 && std::string_view(argv[1]) == "--unavailable") { return Unavailable(); }
  if (argc != 4) { return 1; }
  CHECK(SDL_WasInit(SDL_INIT_VIDEO) == 0, "late attachment starts without initialized SDL video");
  auto device = Require(CreateDevice({.Validation = true}));
  auto assets = Prepare(*device, argv);
  CHECK(SDL_WasInit(SDL_INIT_VIDEO) == 0,
        "headless device preparation does not initialize SDL video");
  CHECK(SDL_Init(SDL_INIT_VIDEO), "the caller initializes SDL video later");
  Window first(SDL_CreateWindow("Outshine target A", 128, kFirstHeight, 0), SDL_DestroyWindow);
  Window second(SDL_CreateWindow("Outshine target B", kSecondWidth, kSecondHeight, 0),
                SDL_DestroyWindow);
  Window occupied(SDL_CreateWindow("Outshine occupied target", kOccupiedWidth, 128, 0),
                  SDL_DestroyWindow);
  CHECK(first && second && occupied, "target changes use existing ordinary SDL windows");
  if (!first || !second || !occupied) { Failed(); }
  Require(Target(*device, first.get()));
  Complete(*device, assets.First);
  Render(*device, assets, Acquire(*device, first.get()));
  SwitchBack(*device, assets, first.get(), second.get());
  const auto accepted = Render(*device, assets, Acquire(*device, first.get()), true);
  const auto before = Require(device->Poll(accepted));
  CHECK(before == CompletionStatus::Pending,
        "delayed real GPU work is pending before detaching its target");
  Require(Target(*device, nullptr));
  const auto unsupported = device->AcquirePresentFrame();
  CHECK(!unsupported && unsupported.error().Kind == ErrorKind::Unsupported,
        "successful offscreen detachment explicitly disables frame acquisition");
  first.reset();
  Complete(*device, accepted);
  Offscreen(*device, assets);
  Values(*device, assets);
  Require(Target(*device, second.get()));
  Rollback(*device, assets, second.get(), occupied.get());
  Require(Target(*device, nullptr));
  assets = {};
  device.reset();
  occupied.reset();
  second.reset();
  SDL_Quit();
  std::println("TARGET_HOST calls={} max_target_call_ms={:.6f} max_acquire_ms={:.6f}",
               targetCalls,
               maxTargetMs,
               maxAcquireMs);
  std::println("TARGET late_video=1 switched=1 stale_rejected=1 rollback=1 preserved_assets=1");
  return Report();
}
