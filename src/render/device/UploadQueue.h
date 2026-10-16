#ifndef OUTSHINE_RENDER_DEVICE_UPLOADQUEUE_H
#define OUTSHINE_RENDER_DEVICE_UPLOADQUEUE_H
#include "native/Device.h"
#include <chrono>
#include <expected>
#include <memory>
#include <optional>
#include <utility>

namespace outshine::Render {
class UploadQueue {
public:
  class WaitScope {
  public:
    WaitScope(std::chrono::nanoseconds &target, std::chrono::nanoseconds budget)
        : Target_(target), Previous_(std::exchange(target, budget)) {}

    ~WaitScope() { Target_ = Previous_; }

    WaitScope(const WaitScope &) = delete;
    WaitScope &operator=(const WaitScope &) = delete;

  private:
    std::chrono::nanoseconds &Target_;
    std::chrono::nanoseconds Previous_;
  };

  UploadQueue(Native::Device &device, std::chrono::nanoseconds &budget)
      : Device_(device), Budget_(budget) {}

  [[nodiscard]] Native::Result<std::unique_ptr<Native::Commands>> Begin() {
    return Begin([this] { return Device_.Begin(); });
  }

  template <class Acquire>
  [[nodiscard]] Native::Result<std::unique_ptr<Native::Commands>> Begin(Acquire acquire) {
    auto commands = acquire();
    if (commands || commands.error().Kind != Native::ErrorKind::Busy || Budget_.count() <= 0 ||
        !Last_) {
      return commands;
    }
    const auto ready = Device_.Wait(*Last_, Budget_);
    if (!ready) { return std::unexpected(ready.error()); }
    if (*ready == Native::CompletionStatus::Pending) { return commands; }
    return acquire();
  }

  [[nodiscard]] Native::Result<Native::Completion>
  Submit(std::unique_ptr<Native::Commands> commands) {
    auto submitted = Device_.Submit(std::move(commands));
    if (submitted) { Submitted(*submitted); }
    return submitted;
  }

  void Submitted(Native::Completion completion) { Last_ = completion; }

  [[nodiscard]] Native::Device &Device() const { return Device_; }

private:
  Native::Device &Device_;
  const std::chrono::nanoseconds &Budget_;
  std::optional<Native::Completion> Last_;
};

inline Native::Result<std::unique_ptr<Native::Commands>> BeginUpload(Native::Device &device,
                                                                     UploadQueue *queue) {
  if (queue != nullptr && &queue->Device() != &device) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "upload queue belongs to a different device"});
  }
  return queue != nullptr ? queue->Begin() : device.Begin();
}

inline Native::Result<Native::Completion> SubmitUpload(Native::Device &device,
                                                       UploadQueue *queue,
                                                       std::unique_ptr<Native::Commands> commands) {
  if (queue != nullptr && &queue->Device() != &device) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "upload queue belongs to a different device"});
  }
  return queue != nullptr ? queue->Submit(std::move(commands)) : device.Submit(std::move(commands));
}
}
#endif
