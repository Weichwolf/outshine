#ifdef __linux__
#include "Recording.h"
#include <print>
#include <expected>
#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace outshine::Render::Native {

namespace Vulkan {

namespace {

struct Pending {
  std::unique_ptr<Recording> Recorded;
  VkFence Fence = VK_NULL_HANDLE;
  uint64_t Serial = 0;
};

struct CompletedTiming {
  uint64_t Serial = 0;
  GpuTimingResults Results;
  Result<SubmissionGpuTiming> Submission;
};

class VulkanDevice final : public Device {
public:
  VulkanDevice(std::shared_ptr<Context> context, std::unique_ptr<Presenter> presenter)
      : Context_(std::move(context)), Presenter_(std::move(presenter)) {}

  ~VulkanDevice() override {
    assert(OnOwner(*Context_));
    if (Presenter_) { Presenter_->Stop(); }
    if (!Pending_.empty() || FailedSubmission_ || Presenter_) {
      const auto result = vkDeviceWaitIdle(Context_->Logical);
      if (result != VK_SUCCESS) {
        std::println(stderr, "Vulkan shutdown: VkResult {}", static_cast<int>(result));
      }
    }
    for (const auto &pending : Pending_) {
      vkDestroyFence(Context_->Logical, pending.Fence, nullptr);
    }
    if (FailedSubmission_) { vkDestroyFence(Context_->Logical, FailedSubmission_->Fence, nullptr); }
    Pending_.clear();
    FailedSubmission_.reset();
    Presenter_.reset();
  }

  [[nodiscard]] const Capabilities &Limits() const noexcept override { return Context_->Limits; }

  [[nodiscard]] bool
  Supports(Format format, UseMask<TextureUse> use, bool filtered) const override {
    return Vulkan::Supports(*Context_, format, use, filtered);
  }

  [[nodiscard]] Result<std::unique_ptr<Buffer>>
  CreateBuffer(BufferDescription description, [[maybe_unused]] std::string_view label) override {
    if (auto ready = Reap(); !ready) { return std::unexpected(ready.error()); }
    auto state = AllocateBuffer(Context_, description);
    if (!state) { return std::unexpected(state.error()); }
    return std::make_unique<VulkanBuffer>(std::move(*state));
  }

  [[nodiscard]] Result<std::unique_ptr<Texture>>
  CreateTexture(TextureDescription description, [[maybe_unused]] std::string_view label) override {
    if (auto ready = Reap(); !ready) { return std::unexpected(ready.error()); }
    auto state = AllocateImage(Context_, description);
    if (!state) { return std::unexpected(state.error()); }
    return std::make_unique<VulkanTexture>(std::move(*state));
  }

  [[nodiscard]] Result<std::unique_ptr<Sampler>>
  CreateSampler(SamplerDescription description, [[maybe_unused]] std::string_view label) override {
    if (auto ready = Reap(); !ready) { return std::unexpected(ready.error()); }
    auto state = AllocateSampler(Context_, description);
    if (!state) { return std::unexpected(state.error()); }
    return std::make_unique<VulkanSampler>(std::move(*state));
  }

  [[nodiscard]] Result<std::unique_ptr<ComputePipeline>>
  CreateComputePipeline(std::span<const uint32_t> spirv, ComputeOptions options) override {
    if (auto ready = Reap(); !ready) { return std::unexpected(ready.error()); }
    return CreatePipeline(Context_, spirv, options.Entry);
  }

  [[nodiscard]] Result<std::unique_ptr<GraphicsPipeline>>
  CreateGraphicsPipeline(const GraphicsDescription &description) override {
    if (auto ready = Reap(); !ready) { return std::unexpected(ready.error()); }
    return CreateGraphics(Context_, description);
  }

  [[nodiscard]] Result<std::span<std::byte>> MapWrite(Buffer &buffer, ByteRange range) override {
    if (auto ready = Reap(); !ready) { return std::unexpected(ready.error()); }
    const auto native = BufferFrom(buffer, *Context_);
    if (!native) { return std::unexpected(native.error()); }
    return Vulkan::MapWrite(**native, range);
  }

  [[nodiscard]] Result<std::span<const std::byte>> MapRead(Buffer &buffer,
                                                           ByteRange range) override {
    if (auto ready = Reap(); !ready) { return std::unexpected(ready.error()); }
    const auto native = BufferFrom(buffer, *Context_);
    if (!native) { return std::unexpected(native.error()); }
    return Vulkan::MapRead(**native, range);
  }

  [[nodiscard]] Result<void> Unmap(Buffer &buffer) override {
    if (auto owner = CheckOwner(*Context_); !owner) { return owner; }
    const auto native = BufferFrom(buffer, *Context_);
    if (!native) { return std::unexpected(native.error()); }
    return Vulkan::Unmap(**native);
  }

  [[nodiscard]] Result<std::unique_ptr<Commands>> Begin() override {
    if (auto ready = Reap(); !ready) { return std::unexpected(ready.error()); }
    if (Pending_.size() >= kMaxPendingSubmissions || Context_->Recordings >= kMaxLiveRecordings) {
      return std::unexpected(Backpressure());
    }
    auto commands = std::make_unique<Recording>(Context_);
    if (auto begun = commands->Initialize(); !begun) { return std::unexpected(begun.error()); }
    return commands;
  }

  [[nodiscard]] Result<void> SetPresentTarget(SDL_Window *window) override {
    if (auto ready = Reap(); !ready) { return ready; }
    if (Context_->OpenRecordings != 0) {
      return std::unexpected(
          Error{.Kind = ErrorKind::Busy,
                .Message = "target changes require closed unsubmitted recordings"});
    }
    if (!Presenter_) {
      if (window == nullptr) { return {}; }
      return std::unexpected(
          Error{.Kind = ErrorKind::Unsupported,
                .Message = "Vulkan device has no prepared presentation capability"});
    }
    return Presenter_->SetTarget(window);
  }

  [[nodiscard]] Result<std::unique_ptr<PresentFrame>> AcquirePresentFrame() override {
    if (auto ready = Reap(); !ready) { return std::unexpected(ready.error()); }
    if (!Presenter_) {
      return std::unexpected(Error{.Kind = ErrorKind::Unsupported,
                                   .Message = "headless Vulkan device has no presentation"});
    }
    return Presenter_->Acquire();
  }

  [[nodiscard]] Result<Completion> Submit(std::unique_ptr<Commands> commands) override {
    if (auto ready = Reap(); !ready) { return std::unexpected(ready.error()); }
    if (Pending_.size() >= kMaxPendingSubmissions) { return std::unexpected(Backpressure()); }
    auto *const native = dynamic_cast<Recording *>(commands.get());
    if (native == nullptr || native->Owner() != Context_) {
      return std::unexpected(Error{.Kind = ErrorKind::Invalid,
                                   .Message = "commands belong to a different Vulkan device"});
    }
    if (auto ended = native->End(); !ended) { return std::unexpected(ended.error()); }
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkFence fence = VK_NULL_HANDLE;
    auto result = vkCreateFence(Context_->Logical, &fenceInfo, nullptr, &fence);
    if (result != VK_SUCCESS) { return std::unexpected(Failure("vkCreateFence", result)); }
    VkCommandBufferSubmitInfo buffer{};
    buffer.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    buffer.commandBuffer = native->Handle();
    VkSubmitInfo2 submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submit.commandBufferInfoCount = 1;
    submit.pCommandBufferInfos = &buffer;
    VkSemaphoreSubmitInfo wait{};
    VkSemaphoreSubmitInfo signal{};
    native->BindPresentation(submit, wait, signal);
    result = vkQueueSubmit2(Context_->Queue, 1, &submit, fence);
    auto held = std::unique_ptr<Recording>(static_cast<Recording *>(commands.release()));
    if (result != VK_SUCCESS) {
      FailedSubmission_ = Pending{.Recorded = std::move(held), .Fence = fence, .Serial = 0};
      Failed_ = Failure("vkQueueSubmit2", result);
      return std::unexpected(*Failed_);
    }
    const auto serial = ++Submitted_;
    native->CommitSubmission();
    Pending_.push_back({.Recorded = std::move(held), .Fence = fence, .Serial = serial});
    return Completion{.Device = Context_->Cookie, .Serial = serial};
  }

  [[nodiscard]] Result<CompletionStatus> Poll(Completion completion) override {
    if (auto valid = Validate(completion); !valid) { return std::unexpected(valid.error()); }
    if (auto reaped = Reap(); !reaped) { return std::unexpected(reaped.error()); }
    return completion.Serial <= Completed_ ? CompletionStatus::Ready : CompletionStatus::Pending;
  }

  [[nodiscard]] Result<CompletionStatus> Wait(Completion completion,
                                              std::chrono::nanoseconds budget) override {
    if (budget.count() < 0) {
      return std::unexpected(
          Error{.Kind = ErrorKind::Invalid, .Message = "negative completion budget"});
    }
    const auto polled = Poll(completion);
    if (!polled || *polled == CompletionStatus::Ready || budget.count() == 0) { return polled; }
    const auto held = std::ranges::find_if(Pending_, [completion](const Pending &pending) {
      return pending.Serial == completion.Serial;
    });
    if (held == Pending_.end()) {
      return std::unexpected(
          Error{.Kind = ErrorKind::Invalid, .Message = "unknown completion token"});
    }
    const auto result = vkWaitForFences(
        Context_->Logical, 1, &held->Fence, VK_TRUE, static_cast<uint64_t>(budget.count()));
    if (result == VK_TIMEOUT) { return CompletionStatus::Pending; }
    if (result != VK_SUCCESS) { return std::unexpected(Failure("vkWaitForFences", result)); }
    return Poll(completion);
  }

  [[nodiscard]] Result<GpuTimingResults> GpuTimings(Completion completion) override {
    if (auto valid = Validate(completion); !valid) { return std::unexpected(valid.error()); }
    if (!Context_->GpuTiming) {
      return std::unexpected(Error{.Kind = ErrorKind::Unsupported,
                                   .Message = "dispatch timing is disabled or unavailable"});
    }
    if (auto reaped = Reap(); !reaped) { return std::unexpected(reaped.error()); }
    if (completion.Serial > Completed_) { return GpuTimingResults{}; }
    const auto timing = std::ranges::find_if(Timings_, [completion](const CompletedTiming &entry) {
      return entry.Serial == completion.Serial;
    });
    if (timing == Timings_.end()) {
      return std::unexpected(
          Error{.Kind = ErrorKind::Invalid, .Message = "dispatch timing retention has expired"});
    }
    return timing->Results;
  }

  [[nodiscard]] Result<SubmissionGpuTiming> SubmissionGpuTime(Completion completion) override {
    const auto status = Poll(completion);
    if (!status) { return std::unexpected(status.error()); }
    if (!Context_->GpuTiming) {
      return std::unexpected(
          Error{.Kind = ErrorKind::Unsupported,
                .Message = "Vulkan submission timing is disabled or unavailable"});
    }
    if (*status == CompletionStatus::Pending) { return SubmissionGpuTiming{}; }
    const auto timing = std::ranges::find_if(Timings_, [completion](const CompletedTiming &entry) {
      return entry.Serial == completion.Serial;
    });
    if (timing == Timings_.end()) {
      return std::unexpected(Error{.Kind = ErrorKind::Invalid,
                                   .Message = "Vulkan submission timing retention has expired"});
    }
    return timing->Submission;
  }

private:
  [[nodiscard]] Result<void> Validate(Completion completion) const {
    if (auto owner = CheckOwner(*Context_); !owner) { return owner; }
    if (completion.Device != Context_->Cookie || completion.Serial == 0 ||
        completion.Serial > Submitted_) {
      return std::unexpected(
          Error{.Kind = ErrorKind::Invalid, .Message = "invalid completion token"});
    }
    return {};
  }

  [[nodiscard]] Result<void> Reap() {
    if (auto owner = CheckOwner(*Context_); !owner) { return owner; }
    if (Failed_) { return std::unexpected(*Failed_); }
    if (Context_->ValidationErrors.load(std::memory_order_relaxed) != 0) {
      return std::unexpected(
          Error{.Kind = ErrorKind::Driver, .Message = "Vulkan validation reported an error"});
    }
    while (!Pending_.empty()) {
      const auto result = vkGetFenceStatus(Context_->Logical, Pending_.front().Fence);
      if (result == VK_NOT_READY) { break; }
      if (result != VK_SUCCESS) {
        Failed_ = Failure("vkGetFenceStatus", result);
        return std::unexpected(*Failed_);
      }
      Completed_ = Pending_.front().Serial;
      if (Context_->GpuTiming) {
        if (Timings_.size() == kRetainedTimings) { Timings_.erase(Timings_.begin()); }
        Timings_.push_back({.Serial = Completed_,
                            .Results = Pending_.front().Recorded->Timings(),
                            .Submission = Pending_.front().Recorded->SubmissionTime()});
      }
      vkDestroyFence(Context_->Logical, Pending_.front().Fence, nullptr);
      Pending_.pop_front();
    }
    return {};
  }

  [[nodiscard]] static Error Backpressure() {
    return {.Kind = ErrorKind::Busy, .Message = "Vulkan in-flight submission budget exhausted"};
  }

  static constexpr std::size_t kMaxPendingSubmissions = 3;
  static constexpr uint32_t kMaxLiveRecordings = 8;
  static constexpr size_t kRetainedTimings = 8;
  std::shared_ptr<Context> Context_;
  std::unique_ptr<Presenter> Presenter_;
  std::deque<Pending> Pending_;
  std::vector<CompletedTiming> Timings_;
  std::optional<Pending> FailedSubmission_;
  std::optional<Error> Failed_;
  uint64_t Submitted_ = 0;
  uint64_t Completed_ = 0;
};

}
}

Result<std::unique_ptr<Device>> CreateDevice(DeviceOptions options) {
  auto context = Vulkan::CreateContext(options);
  if (!context) { return std::unexpected(context.error()); }
  std::unique_ptr<Vulkan::Presenter> presenter;
  if ((*context)->PresentCapable) {
    presenter = std::make_unique<Vulkan::Presenter>(*context);
    if (options.Window != nullptr) {
      if (auto attached = presenter->SetTarget(options.Window); !attached) {
        return std::unexpected(attached.error());
      }
    }
  }
  return std::make_unique<Vulkan::VulkanDevice>(std::move(*context), std::move(presenter));
}

}
#endif
