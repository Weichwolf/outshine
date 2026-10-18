#ifdef __linux__
#include "Surface.h"
#include <SDL3/SDL.h>
#include <cstdint>
#include <expected>
#include <memory>

namespace outshine::Render::Native::Vulkan {

struct WindowClaim {
  uint64_t Device = 0;
};

namespace {
constexpr auto kWindowClaim = "outshine.vulkan.present.owner";

void SDLCALL DeleteClaim([[maybe_unused]] void *const userdata, void *value) {
  delete static_cast<std::weak_ptr<WindowClaim> *>(value);
}
}

Result<std::shared_ptr<WindowClaim>> ClaimWindow(const Context &context, SDL_Window *window) {
  if (!SDL_IsMainThread() || SDL_WasInit(SDL_INIT_VIDEO) == 0 || SDL_GetWindowID(window) == 0) {
    return std::unexpected(
        Error{.Kind = ErrorKind::Invalid,
              .Message = "Vulkan target needs a live SDL window on the main thread"});
  }
  const auto properties = SDL_GetWindowProperties(window);
  if (properties == 0) {
    return std::unexpected(Error{.Kind = ErrorKind::Driver, .Message = SDL_GetError()});
  }
  const auto *const previous = static_cast<std::weak_ptr<WindowClaim> *>(
      SDL_GetPointerProperty(properties, kWindowClaim, nullptr));
  if (previous != nullptr) {
    if (auto claim = previous->lock()) {
      if (claim->Device != context.Cookie) {
        return std::unexpected(Error{.Kind = ErrorKind::Busy,
                                     .Message = "Vulkan target belongs to another presentation"});
      }
      return claim;
    }
  }
  auto claim = std::make_shared<WindowClaim>(WindowClaim{.Device = context.Cookie});
  if (!SDL_SetPointerPropertyWithCleanup(
          properties, kWindowClaim, new std::weak_ptr<WindowClaim>(claim), DeleteClaim, nullptr)) {
    return std::unexpected(Error{.Kind = ErrorKind::Driver, .Message = SDL_GetError()});
  }
  return claim;
}

}
#endif
