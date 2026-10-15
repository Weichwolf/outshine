#include "MetalInternal.h"
#include <algorithm>

namespace outshine::Render::Native::Metal {

void MetalDevice::Collect() {
  while (!Pending_.empty()) {
    {
      const auto &submission = Pending_.front();
      const std::scoped_lock lock(submission->Mutex);
      if (!submission->Done) { return; }
      if (submission->Failed && !Failed_) { Failed_ = submission->Failed; }
      Retired_ = submission->Serial;
    }
    Pending_.pop_front();
  }
}

Result<Completion> MetalDevice::Submit(std::unique_ptr<Commands> commands) {
  if (!OnOwner()) {
    return Failure(ErrorKind::Invalid, "Metal operation belongs to another thread");
  }
  @autoreleasepool {
    Collect();
    if (Failed_) { return std::unexpected(*Failed_); }
    auto *const metal = dynamic_cast<MetalCommands *>(commands.get());
    if (metal == nullptr || metal->Device != Identity_ || metal->Render) {
      return Failure(ErrorKind::Invalid, "Metal submission requires its device and closed passes");
    }
    metal->Finish();
    const auto submission = std::make_shared<Submission>();
    submission->Serial = NextSerial_;
    submission->Commands = metal->Native;
    const auto held = std::make_shared<SubmissionResources>();
    held->Buffers = std::move(metal->Held);
    held->Permit = std::move(metal->Permit);
    [metal->Native addCompletedHandler:^(id<MTLCommandBuffer> finished) {
      held->Buffers.clear();
      held->Permit.reset();
      {
        const std::scoped_lock lock(submission->Mutex);
        if (finished.status != MTLCommandBufferStatusCompleted) {
          const char *const message = finished.error.localizedDescription.UTF8String;
          submission->Failed =
              Error{.Kind = ErrorKind::DeviceLost,
                    .Message = message != nullptr ? message : "Metal command failed"};
        }
        submission->Done = true;
      }
      submission->Changed.notify_all();
    }];
    Pending_.push_back(submission);
    ++NextSerial_;
    [metal->Native commit];
    return Completion{.Device = Identity_, .Serial = submission->Serial};
  }
}

Result<CompletionStatus> MetalDevice::Poll(Completion completion) {
  if (!OnOwner()) {
    return Failure(ErrorKind::Invalid, "Metal operation belongs to another thread");
  }
  if (completion.Device != Identity_ || completion.Serial == 0 ||
      completion.Serial >= NextSerial_) {
    return Failure(ErrorKind::Invalid, "invalid Metal completion token");
  }
  Collect();
  if (Failed_) { return std::unexpected(*Failed_); }
  return completion.Serial <= Retired_ ? CompletionStatus::Ready : CompletionStatus::Pending;
}

Result<CompletionStatus> MetalDevice::Wait(Completion completion, std::chrono::nanoseconds budget) {
  if (!OnOwner()) {
    return Failure(ErrorKind::Invalid, "Metal operation belongs to another thread");
  }
  const auto now = std::chrono::steady_clock::now();
  if (budget.count() < 0 || budget > std::chrono::steady_clock::time_point::max() - now) {
    return Failure(ErrorKind::Invalid, "invalid or overflowing Metal wait budget");
  }
  auto status = Poll(completion);
  if (!status || *status == CompletionStatus::Ready || budget.count() == 0) { return status; }
  const auto found = std::ranges::find_if(
      Pending_, [&](const auto &one) { return one->Serial == completion.Serial; });
  if (found == Pending_.end()) { return Failure(ErrorKind::Invalid, "unknown Metal submission"); }
  const auto &submission = *found;
  {
    std::unique_lock lock(submission->Mutex);
    submission->Changed.wait_until(lock, now + budget, [&] { return submission->Done; });
  }
  return Poll(completion);
}

}
