#include <Outshine.h>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <dlfcn.h>
#include <string>
#include <vector>
#include "Check.h"

namespace {
enum class Failure { None, Extent, Composition, Parameters, Texture };
Failure inject = Failure::None;
unsigned injected = 0;
unsigned releasedTextures = 0;
unsigned releasedWindows = 0;

template <typename Function> Function Original(const char *name) {
  const auto function = reinterpret_cast<Function>(dlsym(RTLD_NEXT, name));
  assert(function != nullptr);
  return function;
}

outshine::Scenario::Document TargetScenario(outshine::Extent extent) {
  outshine::Scenario::Document scene;
  scene.Render.Declared = true;
  scene.Render.Frame = extent;
  scene.Render.Outputs = {"surface"};
  outshine::Scenario::View view;
  view.Id = "target-contract";
  view.Person = "first";
  view.Sees.Placed = true;
  view.Sees.Stands.AtM = {{0, 0, 2}};
  view.Sees.setProjection(
      outshine::Scenario::Camera::Ortho{.XMagM = 2, .YMagM = 2, .NearM = 0.1, .FarM = 10});
  scene.Views.push_back(view);
  return scene;
}
} // namespace

extern "C" bool SDLCALL SDL_GetWindowSizeInPixels(SDL_Window *window, int *width, int *height) {
  if (inject == Failure::Extent) {
    ++injected;
    return SDL_SetError("injected window extent failure");
  }
  static const auto original =
      Original<decltype(&SDL_GetWindowSizeInPixels)>("SDL_GetWindowSizeInPixels");
  return original(window, width, height);
}

extern "C" bool SDLCALL SDL_WindowSupportsGPUSwapchainComposition(
    SDL_GPUDevice *device, SDL_Window *window, SDL_GPUSwapchainComposition composition) {
  if (inject == Failure::Composition) {
    ++injected;
    return false;
  }
  static const auto original = Original<decltype(&SDL_WindowSupportsGPUSwapchainComposition)>(
      "SDL_WindowSupportsGPUSwapchainComposition");
  return original(device, window, composition);
}

extern "C" bool SDLCALL SDL_SetGPUSwapchainParameters(SDL_GPUDevice *device,
                                                      SDL_Window *window,
                                                      SDL_GPUSwapchainComposition composition,
                                                      SDL_GPUPresentMode mode) {
  if (inject == Failure::Parameters) {
    ++injected;
    return SDL_SetError("injected swapchain configuration failure");
  }
  static const auto original =
      Original<decltype(&SDL_SetGPUSwapchainParameters)>("SDL_SetGPUSwapchainParameters");
  return original(device, window, composition, mode);
}

extern "C" SDL_GPUTexture *SDLCALL SDL_CreateGPUTexture(SDL_GPUDevice *device,
                                                        const SDL_GPUTextureCreateInfo *info) {
  if (inject == Failure::Texture) {
    ++injected;
    SDL_SetError("injected target allocation failure");
    return nullptr;
  }
  static const auto original = Original<decltype(&SDL_CreateGPUTexture)>("SDL_CreateGPUTexture");
  return original(device, info);
}

extern "C" void SDLCALL SDL_ReleaseGPUTexture(SDL_GPUDevice *device, SDL_GPUTexture *texture) {
  ++releasedTextures;
  static const auto original = Original<decltype(&SDL_ReleaseGPUTexture)>("SDL_ReleaseGPUTexture");
  original(device, texture);
}

extern "C" void SDLCALL SDL_ReleaseWindowFromGPUDevice(SDL_GPUDevice *device, SDL_Window *window) {
  ++releasedWindows;
  static const auto original =
      Original<decltype(&SDL_ReleaseWindowFromGPUDevice)>("SDL_ReleaseWindowFromGPUDevice");
  original(device, window);
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const bool initialized = SDL_Init(SDL_INIT_VIDEO);
  CHECK(initialized, "SDL video starts on the test's main thread");
  if (!initialized) { return Report(); }
  SDL_Window *first = SDL_CreateWindow("Outshine target contract", 32, 32, 0);
  SDL_Window *candidate = SDL_CreateWindow("Outshine target candidate", 48, 32, 0);
  CHECK(first != nullptr && candidate != nullptr,
        "two real SDL windows exist before their borrowers");
  if (first != nullptr && candidate != nullptr) {
    {
      Engine unconfigured;
      CHECK(!unconfigured.drawsInto(Extent{0, 32}), "invalid first configuration is rejected");
      auto target = unconfigured.swapChain();
      const auto begun = unconfigured.renderer().beginFrame(target);
      CHECK(!begun && target.extent().WidthPx == 0 && target.extent().HeightPx == 0,
            "a rejected first target leaves the Engine unconfigured with no drawable extent");
    }
    {
      Engine owner;
      Engine contender;
      CHECK(owner.drawsInto(first).has_value(), "the owner claims its window");
      const Extent original = owner.swapChain().extent();
      CHECK(!contender.drawsInto(first), "a second Engine cannot claim the same window");
      CHECK(owner.swapChain().presents() && !contender.swapChain().presents(),
            "refused claim preserves both owner identities");
      for (const Failure failure : {Failure::Extent, Failure::Composition, Failure::Parameters}) {
        const unsigned before = injected;
        const unsigned released = releasedWindows;
        inject = failure;
        const auto result = owner.drawsInto(candidate);
        inject = Failure::None;
        CHECK(injected > before && !result, "the actual SDL boundary rejects the candidate");
        CHECK(owner.swapChain().presents() &&
                  owner.swapChain().extent().WidthPx == original.WidthPx,
              "failed configuration preserves the old window and physical extent");
        const unsigned expectedReleases = failure == Failure::Extent ? 0u : 1u;
        CHECK(releasedWindows == released + expectedReleases,
              "only a successfully claimed candidate is released on failure");
        CHECK(!contender.drawsInto(first), "the original window remains claimed by its owner");
        CHECK(contender.drawsInto(candidate).has_value(),
              "the rejected candidate has no leaked claim");
        CHECK(contender.drawsInto(Extent{32, 32}).has_value(),
              "the contender releases the candidate");
        if (failure != Failure::Composition) {
          CHECK(!result && result.error().find("injected") != std::string::npos,
                "the public error retains the SDL failure at its origin");
        }
      }
      const bool ready =
          owner.declare(TargetScenario(original)) && owner.assemble() && owner.advance();
      CHECK(ready, "the retained window configures a real presentation plan and camera");
      if (!ready) { std::printf("presentation setup: %s\n", owner.error().c_str()); }
      auto target = owner.swapChain();
      auto renderer = owner.renderer();
      const bool began = ready && renderer.beginFrame(target).has_value();
      CHECK(began, "a frame opens on the retained window");
      if (began) {
        CHECK(!owner.drawsInto(candidate) && !owner.drawsInto(Extent{32, 32}),
              "both target overloads reject a change while a frame is open");
        CHECK(renderer.endFrame().has_value(), "the original window still presents that frame");
      }
      CHECK(owner.drawsInto(candidate).has_value(), "a closed frame permits a valid target switch");
      CHECK(contender.drawsInto(first).has_value(),
            "successful replacement releases the old window");
    }
    {
      Engine afterDestruction;
      CHECK(afterDestruction.drawsInto(first).has_value(),
            "Engine destruction releases its borrowed window");
      CHECK(afterDestruction.drawsInto(candidate).has_value(),
            "both former claims can be acquired again");
    }
    {
      Engine offscreen;
      CHECK(offscreen.drawsInto(Extent{32, 32}).has_value(), "offscreen target is configured");
      const bool ready = offscreen.declare(TargetScenario({32, 32})) && offscreen.assemble() &&
                         offscreen.advance();
      CHECK(ready, "the plan allocates an offscreen surface and camera");
      if (!ready) { std::printf("offscreen setup: %s\n", offscreen.error().c_str()); }
      if (ready) {
        auto renderer = offscreen.renderer();
        std::vector<uint8_t> before;
        CHECK(renderer.readPixels(before).has_value() && before.size() == 32u * 32u * 4u,
              "the original target produces a complete RGBA frame");
        const unsigned released = releasedTextures;
        const unsigned failures = injected;
        inject = Failure::Texture;
        const auto refused = offscreen.drawsInto(Extent{48, 32});
        inject = Failure::None;
        CHECK(injected == failures + 1 && !refused &&
                  refused.error().find("injected") != std::string::npos,
              "target allocation failure reaches the public caller");
        CHECK(releasedTextures == released && offscreen.swapChain().extent().WidthPx == 32,
              "failed allocation retains the existing texture and dimensions");
        std::vector<uint8_t> after;
        CHECK(renderer.readPixels(after).has_value() && after == before,
              "the retained offscreen target remains renderable with identical pixels");
        CHECK(!offscreen.drawsInto(Extent{-1, 32}) &&
                  !offscreen.drawsInto(static_cast<SDL_Window *>(nullptr)),
              "invalid inputs cannot replace the existing target");
        CHECK(offscreen.drawsInto(Extent{32, 32}).has_value(),
              "replacement succeeds after the injected failure");
        CHECK(offscreen.error().empty(), "successful recovery clears the previous operation error");
      }
    }
  }
  SDL_DestroyWindow(candidate);
  SDL_DestroyWindow(first);
  SDL_Quit();
  Covers("real SDL claims and presentation; atomic extent/claim/configuration/texture failures via "
         "link-time SDL interception; open-frame rejection; borrowed window lifetime; retained "
         "offscreen pixels; not minimized swapchain acquisition or full pipeline resize");
  return Report();
}
