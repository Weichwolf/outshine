#include "NativeGpuCheck.h"
#include "Check.h"
#include <SDL3/SDL.h>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <ios>
#include <memory>
#include <print>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;
using Clock = std::chrono::steady_clock;
constexpr int kWidth = 129;
constexpr int kHeight = 97;

std::unique_ptr<PresentFrame> Acquire(Device &device) {
  const auto until = Clock::now() + std::chrono::seconds(5);
  while (Clock::now() < until) {
    SDL_PumpEvents();
    auto frame = Require(device.AcquirePresentFrame());
    if (frame) { return frame; }
    SDL_Delay(1);
  }
  CHECK(false, "WSI supplies another frame after bounded cancellation or presentation");
  Failed();
}

std::unique_ptr<GraphicsPipeline>
Pipeline(Device &device, const char *vertex, const char *fragment) {
  const auto vs = Spirv(vertex);
  const auto fs = Spirv(fragment);
  const std::array targets{ColourTarget{.Texels = Format::Bgra8Unorm, .Rgb = {}, .Alpha = {}}};
  return Require(device.CreateGraphicsPipeline({.Vertex = {.Spirv = vs},
                                                .Fragment = ShaderCode{.Spirv = fs},
                                                .Streams = {},
                                                .Attributes = {},
                                                .Colours = targets,
                                                .Depth = {},
                                                .Culling = Cull::None,
                                                .Label = {}}));
}

void Draw(Commands &commands, const GraphicsPipeline &pipeline, const Texture &image) {
  const auto size = image.Description();
  const std::array colours{
      ColourAttachment{.Image = {.Image = &image}, .Begin = Load::Clear, .Clear = {0, 0, 0, 0}}};
  Require(commands.BeginRenderPass({.Colours = colours, .Depth = {}}));
  Require(commands.Draw({.State = {.Pipeline = &pipeline,
                                   .Bindings = {},
                                   .Vertices = {},
                                   .Area = {.Width = static_cast<float>(size.Width),
                                            .Height = static_cast<float>(size.Height)},
                                   .Clip = {.Width = size.Width, .Height = size.Height}},
                         .Range = {.Count = 3}}));
  Require(commands.EndRenderPass());
}

void Pixels(Device &device, const GraphicsPipeline &pipeline, const char *path) {
  auto frame = Acquire(device);
  const auto size = frame->Image().Description();
  CHECK(size.Texels == Format::Bgra8Unorm &&
            (static_cast<uint32_t>(size.Use) & static_cast<uint32_t>(TextureUse::CopySource)) != 0,
        "the measured RTX surface supplies the actual BGRA8 copyable image");
  const uint64_t bytes = uint64_t{size.Width} * size.Height * 4;
  auto readback = Require(device.CreateBuffer(
      {.Bytes = bytes, .Use = BufferUse::CopyDestination, .Placement = Memory::Readback}));
  auto commands = Require(device.Begin());
  Draw(*commands, pipeline, frame->Image());
  Require(commands->Copy(frame->Image(),
                         *readback,
                         {.RowBytes = uint64_t{size.Width} * 4,
                          .Region = {.Width = size.Width, .Height = size.Height}}));
  Require(commands->Present(std::move(frame)));
  const auto done = Require(device.Submit(std::move(commands)));
  CHECK(Require(device.Wait(done, std::chrono::seconds(5))) == CompletionStatus::Ready,
        "the real swapchain render and readback complete before inspection");
  const auto mapped = Require(device.MapRead(*readback, {.Bytes = bytes}));
  std::vector<unsigned char> rgb(static_cast<size_t>(size.Width) * size.Height * 3);
  uint64_t wrong = 0;
  for (uint32_t y = 0; y < size.Height; ++y) {
    for (uint32_t x = 0; x < size.Width; ++x) {
      const auto at = static_cast<size_t>(y) * size.Width + x;
      const int red = static_cast<int>(std::lround(255.0 * (x % 17) / 16));
      const int green = static_cast<int>(std::lround(255.0 * (y % 13) / 12));
      const int blue = ((x / 17 + y / 13) % 2) != 0 ? 255 : 0;
      const std::array expected{blue, green, red, 255};
      for (size_t channel = 0; channel < 4; ++channel) {
        if (std::abs(std::to_integer<int>(mapped[at * 4 + channel]) - expected[channel]) > 1) {
          ++wrong;
        }
      }
      for (size_t channel = 0; channel < 3; ++channel) {
        rgb[at * 3 + channel] = std::to_integer<unsigned char>(mapped[at * 4 + 2 - channel]);
      }
    }
  }
  std::println("VULKAN_WSI readback={}x{} pixels={} mismatched_components={}",
               size.Width,
               size.Height,
               uint64_t{size.Width} * size.Height,
               wrong);
  CHECK(wrong == 0, "every real swapchain pixel preserves asymmetric X/Y, channels and alpha");
  Require(device.Unmap(*readback));
  std::ofstream image(path, std::ios::binary);
  image << "P6\n" << size.Width << ' ' << size.Height << "\n255\n";
  image.write(reinterpret_cast<const char *>(rgb.data()), static_cast<std::streamsize>(rgb.size()));
  CHECK(image, "the actual readback is saved for personal visual inspection");
}

void Backpressure(Device &device) {
  std::vector<std::unique_ptr<PresentFrame>> held;
  held.push_back(Acquire(device));
  uint32_t empty = 0;
  for (int call = 0; call < 128; ++call) {
    auto next = Require(device.AcquirePresentFrame());
    if (next) {
      held.push_back(std::move(next));
    } else {
      ++empty;
    }
    CHECK(held.size() <= 3, "the WSI mailbox has a fixed outstanding-image bound");
    SDL_Delay(1);
  }
  CHECK(empty > 0, "finite acquire returns empty under actual driver backpressure");
  std::println("VULKAN_WSI held_images={} empty_acquires={}", held.size(), empty);
  held.front().reset();
  held.front() = Acquire(device);
}

void MissingLease(Device &device, const GraphicsPipeline &pipeline) {
  auto frame = Acquire(device);
  auto commands = Require(device.Begin());
  Draw(*commands, pipeline, frame->Image());
  auto invalid = device.Submit(std::move(commands));
  CHECK(!invalid && invalid.error().Kind == ErrorKind::Invalid,
        "a recorded swapchain image cannot submit without consuming its matching lease");
  frame.reset();
  frame = Acquire(device);
  commands = Require(device.Begin());
  Draw(*commands, pipeline, frame->Image());
  frame.reset();
  invalid = device.Submit(std::move(commands));
  CHECK(!invalid && invalid.error().Kind == ErrorKind::Invalid,
        "cancellation keeps recorded image views alive and rejects their later submission");
}

void AcceptedResize(Device &device, SDL_Window *window, const GraphicsPipeline &pipeline) {
  auto frame = Acquire(device);
  auto commands = Require(device.Begin());
  Draw(*commands, pipeline, frame->Image());
  Require(commands->Present(std::move(frame)));
  const auto done = Require(device.Submit(std::move(commands)));
  CHECK(SDL_SetWindowSize(window, 151, 109), "resize follows an accepted GPU submission");
  CHECK(SDL_SyncWindow(window), "the window manager applies the requested resize");
  SDL_PumpEvents();
  frame = Acquire(device);
  CHECK(Require(device.Wait(done, std::chrono::seconds(5))) == CompletionStatus::Ready,
        "a later WSI generation does not roll back the accepted GPU completion");
  auto cancelled = Require(device.Begin());
  Draw(*cancelled, pipeline, frame->Image());
  frame.reset();
  cancelled.reset();
}

}

int main(int argc, char *const *argv) {
  if (argc != 4) { return 1; }
  CHECK(SDL_Init(SDL_INIT_VIDEO), "the Vulkan WSI probe initializes SDL video");
  std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)> window(
      SDL_CreateWindow("Outshine Vulkan WSI pixel and lifetime probe", kWidth, kHeight, 0),
      SDL_DestroyWindow);
  CHECK(window, "the Vulkan adapter accepts an ordinary SDL window");
  if (!window) { Failed(); }
  auto device = Require(CreateDevice({.Validation = true, .Window = window.get()}));
  CHECK(device->Limits().Name.find("RTX 3060") != std::string::npos,
        "WSI runs on the declared RTX 3060");
  auto pipeline = Pipeline(*device, argv[1], argv[2]);
  Backpressure(*device);
  MissingLease(*device, *pipeline);
  Pixels(*device, *pipeline, argv[3]);
  AcceptedResize(*device, window.get(), *pipeline);
  auto frame = Acquire(*device);
  auto commands = Require(device->Begin());
  Draw(*commands, *pipeline, frame->Image());
  Require(commands->Present(std::move(frame)));
  const auto done = Require(device->Submit(std::move(commands)));
  CHECK(done.Serial != 0, "a final unpolled presentation was accepted");
  frame = Acquire(*device);
  commands = Require(device->Begin());
  Draw(*commands, *pipeline, frame->Image());
  pipeline.reset();
  device.reset();
  frame.reset();
  commands.reset();
  window.reset();
  SDL_Quit();
  std::println("VULKAN_WSI rendered_frames=3");
  return Report();
}
