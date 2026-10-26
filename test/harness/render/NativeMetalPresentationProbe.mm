#include "NativeGpuCheck.h"
#include "Check.h"
#include <SDL3/SDL.h>
#import <AppKit/AppKit.h>
#import <QuartzCore/CAMetalLayer.h>
#include <chrono>
#include <memory>

namespace {
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;
constexpr int kFirstHeight = 96;
constexpr int kSecondWidth = 144;
constexpr int kSecondHeight = 112;

CAMetalLayer *MetalLayer(NSView *view) {
  if ([view.layer isKindOfClass:[CAMetalLayer class]]) { return (CAMetalLayer *)view.layer; }
  for (NSView *child in view.subviews) {
    if (auto *const layer = MetalLayer(child)) { return layer; }
  }
  return nil;
}

void CheckWindow(Device &device, SDL_Window *window) {
  auto *const native = (__bridge NSWindow *)SDL_GetPointerProperty(
      SDL_GetWindowProperties(window), SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr);
  CHECK(native != nil, "the actual SDL window exposes its native Cocoa target");
  auto *const layer = MetalLayer(native.contentView);
  CHECK(layer != nil, "the production presentation owns a CAMetalLayer in that window");
  if (layer == nil) { Failed(); }
  CHECK(layer.pixelFormat == MTLPixelFormatBGRA8Unorm_sRGB,
        "the actual display attachment performs the SDR output transfer once");
  auto *const srgb = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
  CHECK(layer.colorspace != nullptr && CFEqual(layer.colorspace, srgb),
        "the compositor interprets the attachment bytes in the declared sRGB colour space");
  CGColorSpaceRelease(srgb);
  const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(3);
  while (std::chrono::steady_clock::now() < until) {
    SDL_PumpEvents();
    auto frame = Require(device.AcquirePresentFrame());
    if (frame) {
      CHECK(frame->Image().Description().Texels == Format::Bgra8Srgb,
            "the public frame format reports the actual sRGB attachment to the stage factory");
      return;
    }
    SDL_Delay(1);
  }
  CHECK(false, "the real window supplies an SDR drawable within the offline test budget");
}

}

int main() {
  @autoreleasepool {
    CHECK(SDL_Init(SDL_INIT_VIDEO), "SDL video starts for the actual display target");
    auto device = Require(CreateDevice({.Validation = true}));
    using Window = std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)>;
    Window first(SDL_CreateWindow("Outshine SDR", 128, kFirstHeight, 0), SDL_DestroyWindow);
    Window second(SDL_CreateWindow("Outshine SDR replacement", kSecondWidth, kSecondHeight, 0),
                  SDL_DestroyWindow);
    CHECK(first && second, "both ordinary SDL windows are available");
    if (!first || !second) { Failed(); }
    Require(device->SetPresentTarget(first.get()));
    CheckWindow(*device, first.get());
    Require(device->SetPresentTarget(second.get()));
    CheckWindow(*device, second.get());
    Require(device->SetPresentTarget(nullptr));
    device.reset();
    second.reset();
    first.reset();
    SDL_Quit();
  }
  return Report();
}
