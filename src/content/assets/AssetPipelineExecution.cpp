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
#include <variant>
#include <vector>

namespace outshine {
namespace {
constexpr size_t kErrorBytesMost = 256;

AssetPublication Failed(AssetPublicationError code, std::string message) {
  std::string bounded(message.data(), std::min(message.size(), kErrorBytesMost));
  return {.Status = AssetPublicationStatus::Failed,
          .Receipt = {},
          .Error = code,
          .Message = std::move(bounded),
          .Native = {}};
}

struct StoredReceipt {
  std::shared_ptr<void> Reservation;
  AssetProduct Product;
};
}

AssetPublication AssetPipeline::State::Receipt(const std::shared_ptr<Job> &job,
                                               std::expected<AssetProduct, std::string> result) {
  if (!result) { return Failed(AssetPublicationError::ProducerFailed, std::move(result.error())); }
  const auto &content = result->Record();
  const size_t backingBytes = result->BackingBytes();
  const auto key = AssetProductKey::Parse(content.Key);
  const auto request =
      content.RequestKey ? AssetProductKey::Parse(*content.RequestKey) : std::nullopt;
  const bool binding = job->Domain == KeyDomain::Request
                           ? request && *request == job->Key
                           : !content.RequestKey && key && *key == job->Key;
  if (!key || !binding || !AssetProductKey::Parse(content.Package) || content.ByteCount == 0 ||
      content.Kind.empty() || result->CapacityBytes() - backingBytes > job->Output->Bytes ||
      backingBytes > job->InputBytes) {
    return Failed(AssetPublicationError::InvalidReceipt,
                  "producer returned an invalid publication receipt");
  }
  result->ClearRequestBinding();
  const auto stored = std::make_shared<StoredReceipt>(
      StoredReceipt{.Reservation = job->Output, .Product = std::move(*result)});
  {
    const std::scoped_lock lock(job->Output->Owner->Lock);
    job->InputBytes -= backingBytes;
    job->Output->InputBytes = backingBytes;
  }
  std::shared_ptr<const AssetRecord> record(stored, &stored->Product.Record());
  std::shared_ptr<const CachedAsset> native;
  if (const auto *backing = stored->Product.Native()) { native = {stored, backing}; }
  return {.Status = AssetPublicationStatus::Ready,
          .Receipt = std::move(record),
          .Error = AssetPublicationError::InvalidRequest,
          .Message = {},
          .Native = std::move(native),
          .RequestHit = job->Domain == KeyDomain::Request};
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

std::expected<bool, AssetPublicationError> AssetPipeline::State::Execution::PrepareStep() {
  if (Work->Stop.stop_requested()) { return std::unexpected(AssetPublicationError::Cancelled); }
  auto admitted = Owner->AdmitInput(Work);
  if (!admitted || *admitted) { return admitted; }
  Ended = Owner->Defer(Work, std::move(Produce));
  if (!Ended) { return std::unexpected(AssetPublicationError::Cancelled); }
  return false;
}

void AssetPipeline::State::Execution::Run() {
  AssetPublication result;
  for (;;) {
    auto admitted = PrepareStep();
    if (!admitted) {
      result = Failed(admitted.error(), "asset stage cannot continue within active admission");
      break;
    }
    if (!*admitted) { return; }
    auto step = Produce(Work->Stop.get_token());
    if (!step) {
      result = Failed(AssetPublicationError::ProducerFailed, std::move(step.error()));
      break;
    }
    if (const auto *input = std::get_if<InputAdmission>(&*step)) {
      Work->NextInputBytes = input->Bytes;
      continue;
    }
    if (auto *resolved = std::get_if<ResolvedWork>(&*step)) {
      Produce = {};
      auto joined = Owner->Resolve(Work, *resolved);
      if (!joined) {
        result = Failed(joined.error(), "discovered content cannot use its admitted job");
        break;
      }
      if (*joined) {
        Joined = true;
        Ended = true;
        return;
      }
      Produce = std::move(resolved->Produce);
      continue;
    }
    auto record = std::get<std::optional<AssetProduct>>(std::move(*step));
    if (!record && Owner->Defer(Work, std::move(Produce))) {
      Ended = true;
      return;
    }
    if (Work->Stop.stop_requested() || !record) {
      result = Failed(AssetPublicationError::Cancelled, "asset publication canceled");
    } else {
      result = State::Receipt(Work, std::move(*record));
    }
    break;
  }
  Produce = {};
  Owner->Finish(Work, std::move(result));
  Ended = true;
}

std::expected<bool, AssetPublicationError>
AssetPipeline::State::AdmitInput(const std::shared_ptr<Job> &job) {
  const std::scoped_lock lock(Lock);
  if (!job->NextInputBytes) { return true; }
  const size_t wanted = *job->NextInputBytes;
  if (wanted > Budget.InputBytesMost) {
    return std::unexpected(AssetPublicationError::InvalidRequest);
  }
  const size_t others = Held.InputBytes - job->InputBytes;
  if (wanted > Budget.InputBytesMost - others) { return false; }
  Held.InputBytes = others + wanted;
  job->InputBytes = wanted;
  job->NextInputBytes.reset();
  return true;
}

AssetPipeline::State::Execution::~Execution() {
  if (Joined) { Owner->RetireJoined(Work); }
  if (!Ended) {
    Produce = {};
    Owner->Finish(
        Work,
        Failed(AssetPublicationError::DispatcherStopped, "asset publication dispatcher stopped"));
  }
}

bool AssetPipeline::State::Defer(const std::shared_ptr<Job> &job, LogicalProducer producer) {
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
            .Message = {},
            .Native = {}};
  }
  const auto &demand = at->second;
  auto result = demand.Work->Finished ? demand.Work->Result : AssetPublication{};
  result.RequestKey = demand.Address;
  const auto owner = Consumers.find(demand.Consumer);
  if (owner == Consumers.end() || !owner->second.Alive ||
      owner->second.Generation != demand.Generation) {
    result.Status = AssetPublicationStatus::Cancelled;
    result.Receipt.reset();
    result.Native.reset();
    result.Error = AssetPublicationError::StaleGeneration;
    result.Message.clear();
    result.Finished = demand.Work->Finished;
  } else if (demand.Cancelled) {
    result.Status = AssetPublicationStatus::Cancelled;
    result.Receipt.reset();
    result.Native.reset();
    result.Error = AssetPublicationError::Cancelled;
    result.Message.clear();
    result.Finished = demand.Work->Finished;
  }
  return result;
}
}
