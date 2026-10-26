#ifdef __linux__
#include "Presenter.h"
#include <SDL3/SDL.h>
#include <atomic>
#include <cstdint>
#include <expected>
#include <memory>
#include <mutex>
#include <utility>

namespace outshine::Render::Native::Vulkan {

Result<void> Presenter::SetTarget(SDL_Window *window) {
  if (window != nullptr && (!SDL_IsMainThread() || SDL_WasInit(SDL_INIT_VIDEO) == 0)) {
    return std::unexpected(
        Error{.Kind = ErrorKind::Invalid,
              .Message = "presentation target preparation requires SDL video on its main thread"});
  }
  if (Window_ == window) {
    const std::scoped_lock lock(State_->Gate);
    if (!State_->Failed &&
        (window == nullptr ||
         (State_->Surface && State_->Surface->WindowId == SDL_GetWindowID(window)))) {
      return {};
    }
  }
  auto request = std::make_shared<TargetRequest>();
  if (window != nullptr) {
    auto surface = PrepareSurface(State_->Owner, window);
    if (!surface) { return std::unexpected(surface.error()); }
    request->Surface = std::move(*surface);
    int width = 0;
    int height = 0;
    if (!SDL_GetWindowSizeInPixels(window, &width, &height)) {
      return std::unexpected(Error{.Kind = ErrorKind::Invalid, .Message = SDL_GetError()});
    }
    if ((SDL_GetWindowFlags(window) & (SDL_WINDOW_HIDDEN | SDL_WINDOW_MINIMIZED)) == 0) {
      request->Extent = {.width = width > 0 ? static_cast<uint32_t>(width) : 0,
                         .height = height > 0 ? static_cast<uint32_t>(height) : 0};
    }
  }
  if (auto started = Start(); !started) { return started; }
  {
    const std::scoped_lock lock(State_->Gate);
    State_->RequestedTarget = request;
    State_->Wake();
  }
  request->Done.wait(false);
  if (request->Failure) { return std::unexpected(std::move(*request->Failure)); }
  Window_ = window;
  State_->Owner->HasPresentTarget = window != nullptr;
  return {};
}

}
#endif
