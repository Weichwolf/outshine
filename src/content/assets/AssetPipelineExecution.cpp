#include "AssetPipelineState.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

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
  if (!key || *key != job->Key || !AssetProductKey::Parse(result->Package) ||
      result->ByteCount == 0 || result->Kind.empty() || result->RequestKey ||
      ReceiptBytes(*result) > job->Output->Bytes) {
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
    result = State::Receipt(Work, Produce(Work->Stop.get_token()));
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

AssetPublication AssetPipeline::State::Poll(uint64_t ticket) const noexcept {
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
