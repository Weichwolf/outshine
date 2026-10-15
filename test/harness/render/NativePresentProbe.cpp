#include "NativeGpuCheck.h"
#include "NativeSdr.h"
#include "Check.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <memory>
#include <print>
#include <ratio>
#include <thread>
#include <utility>

namespace {
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;
using Clock = std::chrono::steady_clock;

double maxAcquireMs = 0;

std::unique_ptr<PresentFrame> Acquire(Device &device) {
  const auto until = Clock::now() + std::chrono::seconds(3);
  while (Clock::now() < until) {
    SDL_PumpEvents();
    const auto start = Clock::now();
    auto next = device.AcquirePresentFrame();
    maxAcquireMs = std::max(
        maxAcquireMs, std::chrono::duration<double, std::milli>(Clock::now() - start).count());
    auto ready = Require(std::move(next));
    if (ready) { return ready; }
    SDL_Delay(1);
  }
  CHECK(false, "a visible window supplies a presentation frame within the bounded probe wait");
  Failed();
}

void Complete(Device &device, Completion completion) {
  CHECK(Require(device.Wait(completion, std::chrono::seconds(3))) == CompletionStatus::Ready,
        "the actual drawable rendering completes on the GPU");
}

void Render(Device &device, const GraphicsPipeline &pipeline, std::unique_ptr<PresentFrame> frame) {
  const auto &image = frame->Image();
  const auto description = image.Description();
  CHECK(description.Width > 0 && description.Height > 0 && IsSdr(description.Texels),
        "the acquired frame exposes its actual drawable dimensions and format");
  const std::array colours{
      ColourAttachment{.Image = {.Image = &image}, .Begin = Load::Clear, .Clear = {0, 0, 0, 1}}};
  auto commands = Require(device.Begin());
  Require(commands->BeginRenderPass({.Colours = colours, .Depth = {}}));
  Require(
      commands->Draw({.State = {.Pipeline = &pipeline,
                                .Bindings = {},
                                .Vertices = {},
                                .Area = {.Width = static_cast<float>(description.Width),
                                         .Height = static_cast<float>(description.Height)},
                                .Clip = {.Width = description.Width, .Height = description.Height}},
                      .Range = {.Count = 3}}));
  Require(commands->EndRenderPass());
  Require(commands->Present(std::move(frame)));
  Complete(device, Require(device.Submit(std::move(commands))));
}

void InvalidAndCancel(Device &device, Device &headless) {
  auto commands = Require(device.Begin());
  auto invalid = commands->Present(nullptr);
  CHECK(!invalid && invalid.error().Kind == ErrorKind::Invalid,
        "null presentation leases are rejected");
  auto frame = Acquire(device);
  auto other = Require(headless.Begin());
  invalid = other->Present(std::move(frame));
  CHECK(!invalid && invalid.error().Kind == ErrorKind::Invalid,
        "another device cannot consume the window's drawable");
  frame = Acquire(device);
  const std::array colours{ColourAttachment{.Image = {.Image = &frame->Image()}}};
  Require(commands->BeginRenderPass({.Colours = colours, .Depth = {}}));
  invalid = commands->Present(std::move(frame));
  CHECK(!invalid && invalid.error().Kind == ErrorKind::Invalid,
        "presentation cannot consume a frame during an open render pass");
  Require(commands->EndRenderPass());
  commands.reset();
  frame = Acquire(device);
  commands = Require(device.Begin());
  Require(commands->Present(std::move(frame)));
  invalid = commands->Present(nullptr);
  CHECK(!invalid && invalid.error().Kind == ErrorKind::Invalid,
        "a recording accepts at most one presentation frame");
  commands.reset();
  frame = Acquire(device);
  frame.reset();
}

void ResizeAndSuspend(Device &device, SDL_Window *window, const GraphicsPipeline &pipeline) {
  auto old = Acquire(device);
  CHECK(SDL_SetWindowSize(window, 448, 252), "the existing SDL window can be resized");
  CHECK(SDL_SyncWindow(window), "the asynchronous resize is applied before testing its generation");
  SDL_PumpEvents();
  auto fresh = Acquire(device);
  int width = 0;
  int height = 0;
  CHECK(SDL_GetWindowSizeInPixels(window, &width, &height), "actual resized pixels are queryable");
  CHECK(std::cmp_equal(fresh->Image().Description().Width, width) &&
            std::cmp_equal(fresh->Image().Description().Height, height),
        "resize discards stale mailbox results and returns the new drawable extent");
  auto commands = Require(device.Begin());
  auto invalid = commands->Present(std::move(old));
  CHECK(!invalid && invalid.error().Kind == ErrorKind::Invalid,
        "a drawable from the old window generation is rejected");
  Render(device, pipeline, std::move(fresh));
  auto pending = Acquire(device);
  Require(commands->Present(std::move(pending)));
  CHECK(SDL_HideWindow(window), "the SDL target can be hidden");
  SDL_PumpEvents();
  CHECK(!Require(device.AcquirePresentFrame()), "a hidden window returns no frame without waiting");
  auto stale = device.Submit(std::move(commands));
  CHECK(!stale && stale.error().Kind == ErrorKind::Invalid,
        "a recording invalidated by suspension is rejected before GPU submission");
  CHECK(SDL_ShowWindow(window), "the same window can resume without replacing the device");
  Render(device, pipeline, Acquire(device));
}

}

int main(int argc, char *const *argv) {
  if (argc != 3) { return 1; }
  CHECK(SDL_Init(SDL_INIT_VIDEO), "the native presentation probe initializes SDL video");
  constexpr int width = 384;
  constexpr int height = 216;
  std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)> window(
      SDL_CreateWindow("Outshine native presentation probe", width, height, 0), SDL_DestroyWindow);
  CHECK(window, "an ordinary SDL window needs no GPU-specific caller flags");
  if (!window) { Failed(); }
  auto headless = Require(CreateDevice({.Validation = true}));
  auto unsupported = headless->AcquirePresentFrame();
  CHECK(!unsupported && unsupported.error().Kind == ErrorKind::Unsupported,
        "headless devices explicitly reject window acquisition");
  auto device = Require(CreateDevice({.Validation = true, .Window = window.get()}));
  std::thread other([&] {
    auto invalid = device->AcquirePresentFrame();
    CHECK(!invalid && invalid.error().Kind == ErrorKind::Invalid,
          "acquisition rejects another thread before touching SDL or the mailbox");
  });
  other.join();
  const auto vertex = Spirv(argv[1]);
  const auto fragment = Spirv(argv[2]);
  auto formatFrame = Acquire(*device);
  const auto format = formatFrame->Image().Description().Texels;
  formatFrame.reset();
  const std::array targets{ColourTarget{.Texels = format, .Rgb = {}, .Alpha = {}}};
  auto pipeline = Require(device->CreateGraphicsPipeline({.Vertex = {.Spirv = vertex},
                                                          .Fragment = ShaderCode{.Spirv = fragment},
                                                          .Streams = {},
                                                          .Attributes = {},
                                                          .Colours = targets,
                                                          .Depth = {},
                                                          .Culling = Cull::None,
                                                          .Label = {}}));
  InvalidAndCancel(*device, *headless);
  for (int frame = 0; frame < 6; ++frame) { Render(*device, *pipeline, Acquire(*device)); }
  ResizeAndSuspend(*device, window.get(), *pipeline);
  auto outstanding = Acquire(*device);
  pipeline.reset();
  device.reset();
  outstanding.reset();
  headless.reset();
  window.reset();
  SDL_Quit();
  std::println("PRESENT frames=8 max_acquire_ms={:.6f}", maxAcquireMs);
  return Report();
}
