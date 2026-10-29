#include "AssetPipelineState.h"
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace outshine {
namespace {
constexpr size_t kErrorBytesMost = 256;

size_t ReceiptBytes(const AssetRecord &record) {
  size_t bytes = sizeof(AssetRecord) + record.Key.capacity() + record.Kind.capacity() +
                 record.Package.capacity() + record.Parent.capacity();
  if (record.RequestKey) { bytes += record.RequestKey->capacity(); }
  return bytes;
}

AssetPublication Failed(AssetPublicationError code, std::string message) {
  std::string bounded(message.data(), std::min(message.size(), kErrorBytesMost));
  return {.Status = AssetPublicationStatus::Failed,
          .Receipt = {},
          .Error = code,
          .Message = std::move(bounded)};
}

struct StoredReceipt {
  AssetRecord Record;
  std::shared_ptr<void> Reservation;
};
}

AssetPublication AssetPipeline::State::Receipt(const std::shared_ptr<Job> &job,
                                               std::expected<AssetRecord, std::string> result) {
  if (!result) { return Failed(AssetPublicationError::ProducerFailed, std::move(result.error())); }
  const auto key = AssetProductKey::Parse(result->Key);
  const auto request =
      result->RequestKey ? AssetProductKey::Parse(*result->RequestKey) : std::nullopt;
  const bool binding = job->Domain == KeyDomain::Request
                           ? request && *request == job->Key
                           : !result->RequestKey && key && *key == job->Key;
  if (!key || !binding || !AssetProductKey::Parse(result->Package) || result->ByteCount == 0 ||
      result->Kind.empty() || ReceiptBytes(*result) > job->Output->Bytes) {
    return Failed(AssetPublicationError::InvalidReceipt,
                  "producer returned an invalid publication receipt");
  }
  const auto stored = std::make_shared<StoredReceipt>(
      StoredReceipt{.Record = std::move(*result), .Reservation = job->Output});
  std::shared_ptr<const AssetRecord> record(stored, &stored->Record);
  return {.Status = AssetPublicationStatus::Ready,
          .Receipt = std::move(record),
          .Error = AssetPublicationError::InvalidRequest,
          .Message = {}};
}

void AssetPipeline::State::Finish(const std::shared_ptr<Job> &job,
                                  AssetPublication result) noexcept {
  const std::scoped_lock lock(Lock);
  result.Finished = true;
  job->Result = std::move(result);
  job->Finished = true;
  job->Running = false;
  --Held.Running;
  Held.InputBytes -= job->InputBytes;
  job->InputBytes = 0;
  Remove(job);
}

void AssetPipeline::State::Execution::Run() {
  AssetPublication result;
  if (Work->Stop.stop_requested()) {
    result = Failed(AssetPublicationError::ProducerFailed, "asset publication canceled");
  } else {
    auto step = Produce(Work->Stop.get_token());
    if (!step) {
      result = Failed(AssetPublicationError::ProducerFailed, std::move(step.error()));
    } else if (!*step && Owner->Defer(Work, std::move(Produce))) {
      Ended = true;
      return;
    } else if (Work->Stop.stop_requested() || !*step) {
      result = Failed(AssetPublicationError::Cancelled, "asset publication canceled");
    } else {
      result = State::Receipt(Work, std::move(**step));
    }
  }
  Produce = {};
  Owner->Finish(Work, std::move(result));
  Ended = true;
}

AssetPipeline::State::Execution::~Execution() {
  if (!Ended) {
    Produce = {};
    Owner->Finish(
        Work,
        Failed(AssetPublicationError::DispatcherStopped, "asset publication dispatcher stopped"));
  }
}

bool AssetPipeline::State::Defer(const std::shared_ptr<Job> &job, ResumableProducer producer) {
  const std::scoped_lock lock(Lock);
  if (job->StopRequested) { return false; }
  job->Resume = std::move(producer);
  job->RetryAt = std::chrono::steady_clock::now() + job->Delay;
  job->Delay = std::min(job->Delay * 2, std::chrono::milliseconds(16));
  job->Running = false;
  --Held.Running;
  return true;
}

void AssetPipeline::State::Dispatch(const std::shared_ptr<Job> &job) {
  std::shared_ptr<Execution> execution;
  {
    const std::scoped_lock lock(Lock);
    if (job->Finished || job->Running || !job->Resume ||
        (!job->StopRequested && std::chrono::steady_clock::now() < job->RetryAt)) {
      return;
    }
    execution = std::make_shared<Execution>();
    execution->Owner = shared_from_this();
    execution->Work = job;
    execution->Produce = std::exchange(job->Resume, {});
    job->Running = true;
    ++Held.Running;
  }
  if (!Pool.PostDetached([execution] { execution->Run(); })) { execution.reset(); }
}

void AssetPipeline::State::WakeStopped() {
  std::vector<std::shared_ptr<Job>> stopped;
  {
    const std::scoped_lock lock(Lock);
    for (const auto &[key, job] : Jobs) {
      if (job->StopRequested && !job->Finished && !job->Running) { stopped.push_back(job); }
    }
  }
  for (const auto &job : stopped) { Dispatch(job); }
}

AssetPublication AssetPipeline::State::Poll(uint64_t ticket) noexcept {
  std::shared_ptr<Job> job;
  {
    const std::scoped_lock lock(Lock);
    const auto at = Tickets.find(ticket);
    if (at != Tickets.end()) { job = at->second.Work; }
  }
  if (job) { Dispatch(job); }
  const std::scoped_lock lock(Lock);
  const auto at = Tickets.find(ticket);
  if (at == Tickets.end()) {
    return {.Status = AssetPublicationStatus::Cancelled,
            .Receipt = {},
            .Error = AssetPublicationError::InvalidRequest,
            .Message = {}};
  }
  const auto &demand = at->second;
  const auto owner = Consumers.find(demand.Consumer);
  if (owner == Consumers.end() || !owner->second.Alive ||
      owner->second.Generation != demand.Generation) {
    return {.Status = AssetPublicationStatus::Cancelled,
            .Receipt = {},
            .Error = AssetPublicationError::StaleGeneration,
            .Message = {},
            .Finished = demand.Work->Finished};
  }
  if (demand.Cancelled) {
    return {.Status = AssetPublicationStatus::Cancelled,
            .Receipt = {},
            .Error = AssetPublicationError::Cancelled,
            .Message = {},
            .Finished = demand.Work->Finished};
  }
  if (!demand.Work->Finished) { return {}; }
  return demand.Work->Result;
}
}
