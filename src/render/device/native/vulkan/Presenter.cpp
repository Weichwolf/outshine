#ifdef __linux__
#include "Presenter.h"
#include <SDL3/SDL.h>
#include <atomic>
#include <cstdint>
#include <system_error>
#include <expected>
#include <memory>
#include <mutex>
#include <utility>

namespace outshine::Render::Native::Vulkan {

Presenter::Presenter(std::shared_ptr<Context> context, SDL_Window *window)
    : Window_(window), State_(std::make_shared<PresentState>()) {
  State_->Owner = std::move(context);
}

void *Presenter::Entry(void *self) {
  RunPresenter(static_cast<Presenter *>(self)->State_);
  return nullptr;
}

Result<void> Presenter::Start() {
  const auto result = pthread_create(&Worker_, nullptr, Entry, this);
  if (result != 0) {
    return std::unexpected(
        Error{.Kind = ErrorKind::Driver,
              .Message = std::error_code(result, std::generic_category()).message()});
  }
  Started_ = true;
  return {};
}

Presenter::~Presenter() {
  Stop();
  DrainPresenter(State_);
}

void Presenter::Stop() {
  if (Started_) {
    State_->Stop.store(true);
    State_->Wake();
    pthread_join(Worker_, nullptr);
    Started_ = false;
  }
}

Result<std::unique_ptr<PresentFrame>> Presenter::Acquire() {
  int width = 0;
  int height = 0;
  if (!SDL_GetWindowSizeInPixels(Window_, &width, &height)) {
    return std::unexpected(Error{.Kind = ErrorKind::Invalid, .Message = SDL_GetError()});
  }
  if ((SDL_GetWindowFlags(Window_) & (SDL_WINDOW_HIDDEN | SDL_WINDOW_MINIMIZED)) != 0) {
    width = height = 0;
  }
  std::unique_lock lock(State_->Gate, std::try_to_lock);
  if (!lock.owns_lock()) { return std::unique_ptr<PresentFrame>{}; }
  const auto wantedWidth = width > 0 ? static_cast<uint32_t>(width) : 0u;
  const auto wantedHeight = height > 0 ? static_cast<uint32_t>(height) : 0u;
  if (wantedWidth != State_->Width || wantedHeight != State_->Height) {
    State_->Width = wantedWidth;
    State_->Height = wantedHeight;
    ++State_->Generation;
    State_->Wake();
  }
  if (State_->Failed) { return std::unexpected(*State_->Failed); }
  for (const auto &lease : State_->Slots) {
    if (!lease || lease->Target->Generation != State_->Generation) { continue; }
    auto before = PresentPhase::Ready;
    if (!lease->Phase.compare_exchange_strong(before, PresentPhase::Acquired)) { continue; }
    auto captured = lease;
    lock.unlock();
    return std::make_unique<VulkanPresentFrame>(std::move(captured));
  }
  return std::unique_ptr<PresentFrame>{};
}

void PresentState::Wake() noexcept {
  Revision.fetch_add(1);
  Revision.notify_one();
  Changed.notify_one();
}

void FailPresentation(PresentState &state, Error error) {
  const std::scoped_lock lock(state.Gate);
  if (!state.Failed) {
    state.Failed = std::move(error);
    ++state.Generation;
    state.Wake();
  }
}

void InvalidatePresentation(PresentState &state, uint64_t generation) {
  const std::scoped_lock lock(state.Gate);
  if (state.Generation == generation) { ++state.Generation; }
  state.Wake();
}

}
#endif
