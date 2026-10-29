#include "AssetPipelineState.h"
#include <cstdint>
#include <expected>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <utility>

namespace outshine {
AssetPipeline::AssetPipeline(Tasks &pool, Limits limits)
    : State_(std::make_shared<State>(pool, limits)) {}

std::expected<AssetPipeline::Consumer, AssetPublicationError>
AssetPipeline::OpenConsumer(AssetStoreScope scope) {
  const std::scoped_lock lock(State_->Lock);
  if (State_->Held.Consumers >= State_->Budget.ConsumersMost) {
    return std::unexpected(AssetPublicationError::ResourceDeferred);
  }
  if (State_->NextConsumer == std::numeric_limits<uint64_t>::max()) {
    return std::unexpected(AssetPublicationError::InvalidRequest);
  }
  const AssetConsumerId id{State_->NextConsumer++};
  State::Owner owner;
  owner.Scope = std::move(scope);
  State_->Consumers.emplace(id, std::move(owner));
  ++State_->Held.Consumers;
  return Consumer(State_, id);
}

std::expected<AssetPipeline::RequestTicket, AssetPublicationError> AssetPipeline::RequestResolved(
    const Consumer &consumer, const ResolvedRequest &request, Producer producer) {
  if (!producer) { return std::unexpected(AssetPublicationError::InvalidRequest); }
  return RequestResumable(consumer,
                          request,
                          [producer = std::move(producer)](std::stop_token stop)
                              -> std::expected<std::optional<AssetRecord>, std::string> {
                            auto record = producer(std::move(stop));
                            if (!record) { return std::unexpected(std::move(record.error())); }
                            return std::move(*record);
                          });
}

std::expected<AssetPipeline::RequestTicket, AssetPublicationError> AssetPipeline::RequestResumable(
    const Consumer &consumer, const ResolvedRequest &request, ResumableProducer producer) {
  return Submit(consumer,
                {.Key = request.Key,
                 .Domain = KeyDomain::Content,
                 .Generation = request.Generation,
                 .InputBytes = request.InputBytes,
                 .ConsumerBytes = request.ConsumerBytes,
                 .ReceiptBytesMost = request.ReceiptBytesMost},
                std::move(producer));
}

std::expected<AssetPipeline::RequestTicket, AssetPublicationError> AssetPipeline::RequestLogical(
    const Consumer &consumer, const LogicalRequest &request, ResumableProducer producer) {
  const auto key = AssetProductKey::Parse(request.Address);
  if (!key) { return std::unexpected(AssetPublicationError::InvalidRequest); }
  return Submit(consumer,
                {.Key = *key,
                 .Domain = KeyDomain::Request,
                 .Generation = request.Generation,
                 .InputBytes = request.InputBytes,
                 .ConsumerBytes = request.ConsumerBytes,
                 .ReceiptBytesMost = request.ReceiptBytesMost},
                std::move(producer));
}

std::expected<AssetPipeline::RequestTicket, AssetPublicationError> AssetPipeline::Submit(
    const Consumer &consumer, const Submission &request, ResumableProducer producer) {
  if (consumer.State_ != State_ || !producer || request.ReceiptBytesMost == 0) {
    return std::unexpected(AssetPublicationError::InvalidRequest);
  }
  std::shared_ptr<State::Execution> execution;
  uint64_t ticket = 0;
  {
    const std::scoped_lock lock(State_->Lock);
    const auto owner = State_->Consumers.find(consumer.Id_);
    if (owner == State_->Consumers.end() || !owner->second.Alive ||
        owner->second.Generation != request.Generation) {
      return std::unexpected(AssetPublicationError::StaleGeneration);
    }
    if (State_->Held.Tickets >= State_->Budget.TicketsMost ||
        request.ConsumerBytes > State_->Budget.InputBytesMost - State_->Held.InputBytes) {
      return std::unexpected(AssetPublicationError::ResourceDeferred);
    }
    if (State_->NextTicket == std::numeric_limits<uint64_t>::max()) {
      return std::unexpected(AssetPublicationError::InvalidRequest);
    }
    const State::ScopedKey address{
        .Scope = owner->second.Scope, .Domain = request.Domain, .Key = request.Key};
    const auto found = State_->Jobs.find(address);
    std::shared_ptr<State::Job> job;
    if (found != State_->Jobs.end()) {
      job = found->second;
      if (job->StopRequested || request.ReceiptBytesMost < job->Output->Bytes) {
        return std::unexpected(AssetPublicationError::ResourceDeferred);
      }
    } else {
      const auto inputRoom =
          State_->Budget.InputBytesMost - State_->Held.InputBytes - request.ConsumerBytes;
      if (State_->Held.Jobs >= State_->Budget.JobsMost || request.InputBytes > inputRoom ||
          request.ReceiptBytesMost > State_->Budget.ReceiptBytesMost - State_->Held.ReceiptBytes) {
        return std::unexpected(AssetPublicationError::ResourceDeferred);
      }
      job = std::make_shared<State::Job>();
      job->Key = request.Key;
      job->Domain = request.Domain;
      job->Scope = address.Scope;
      job->InputBytes = request.InputBytes;
      job->Output = std::make_shared<State::Reservation>();
      job->Output->Owner = State_;
      job->Output->Bytes = request.ReceiptBytesMost;
      State_->Jobs.emplace(address, job);
      ++State_->Held.Jobs;
      ++State_->Held.Running;
      State_->Held.InputBytes += request.InputBytes;
      State_->Held.ReceiptBytes += request.ReceiptBytesMost;
      execution = std::make_shared<State::Execution>();
      execution->Owner = State_;
      execution->Work = job;
      execution->Produce = std::move(producer);
    }
    ticket = State_->NextTicket++;
    State_->Tickets.emplace(ticket,
                            State::Demand{.Consumer = consumer.Id_,
                                          .Generation = request.Generation,
                                          .Bytes = request.ConsumerBytes,
                                          .Cancelled = false,
                                          .Work = job});
    ++job->References;
    ++job->Demands;
    ++owner->second.Tickets;
    ++State_->Held.Tickets;
    State_->Held.InputBytes += request.ConsumerBytes;
  }
  if (execution && !State_->Pool.PostDetached([execution] { execution->Run(); })) {
    execution.reset();
  }
  return RequestTicket(State_, ticket);
}

AssetPipeline::Snapshot AssetPipeline::Counts() const noexcept {
  const std::scoped_lock lock(State_->Lock);
  return State_->Held;
}
}
