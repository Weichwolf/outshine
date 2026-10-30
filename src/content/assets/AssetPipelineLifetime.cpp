#include "AssetPipelineState.h"
#include <cstdint>
#include <cstddef>
#include <expected>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <utility>
#include <vector>

namespace outshine {
AssetPipeline::State::Reservation::~Reservation() {
  const std::scoped_lock lock(Owner->Lock);
  Owner->Held.ReceiptBytes -= Bytes;
  Owner->Held.InputBytes -= InputBytes;
}

std::optional<std::stop_source> AssetPipeline::State::Cancel(Demand &demand) {
  if (demand.Cancelled) { return std::nullopt; }
  demand.Cancelled = true;
  auto &job = *demand.Work;
  --job.Demands;
  if (job.Demands == 0 && !job.Finished && !job.StopRequested) {
    job.StopRequested = true;
    return job.Stop;
  }
  return std::nullopt;
}

void AssetPipeline::State::CancelTicket(uint64_t ticket) noexcept {
  std::optional<std::stop_source> stop;
  {
    const std::scoped_lock lock(Lock);
    const auto at = Tickets.find(ticket);
    if (at != Tickets.end()) { stop = Cancel(at->second); }
  }
  if (stop) {
    (void)stop->request_stop();
    WakeStopped();
  }
}

void AssetPipeline::State::Remove(const std::shared_ptr<Job> &job) {
  if (job->Finished && job->References == 0) {
    std::erase_if(Jobs, [&](const auto &entry) { return entry.second == job; });
    --Held.Jobs;
  }
}

void AssetPipeline::State::ReleaseTicket(uint64_t ticket) noexcept {
  std::optional<std::stop_source> stop;
  std::optional<Demand> released;
  std::optional<Owner> retired;
  {
    const std::scoped_lock lock(Lock);
    const auto at = Tickets.find(ticket);
    if (at == Tickets.end()) { return; }
    stop = Cancel(at->second);
    released.emplace(std::move(at->second));
    Tickets.erase(at);
    --Held.Tickets;
    Held.InputBytes -= released->Bytes;
    const auto owner = Consumers.find(released->Consumer);
    if (--owner->second.Tickets == 0 && !owner->second.Alive) {
      retired.emplace(std::move(owner->second));
      Consumers.erase(owner);
      --Held.Consumers;
    }
    PruneRequest(*released);
    --released->Work->References;
    Remove(released->Work);
  }
  if (stop) {
    (void)stop->request_stop();
    WakeStopped();
  }
}

bool AssetPipeline::State::Advance(AssetConsumerId consumer,
                                   AssetConsumerGeneration generation) noexcept {
  std::vector<std::stop_source> stops;
  {
    const std::scoped_lock lock(Lock);
    const auto owner = Consumers.find(consumer);
    if (owner == Consumers.end() || !owner->second.Alive ||
        generation <= owner->second.Generation) {
      return false;
    }
    owner->second.Generation = generation;
    for (auto &[id, demand] : Tickets) {
      if (demand.Consumer == consumer) {
        auto stop = Cancel(demand);
        if (stop) { stops.push_back(std::move(*stop)); }
      }
    }
  }
  for (auto &stop : stops) { (void)stop.request_stop(); }
  if (!stops.empty()) { WakeStopped(); }
  return true;
}

void AssetPipeline::State::Retire(AssetConsumerId consumer) noexcept {
  std::vector<std::stop_source> stops;
  std::optional<Owner> retired;
  {
    const std::scoped_lock lock(Lock);
    const auto owner = Consumers.find(consumer);
    if (owner == Consumers.end()) { return; }
    owner->second.Alive = false;
    for (auto &[id, demand] : Tickets) {
      if (demand.Consumer == consumer) {
        auto stop = Cancel(demand);
        if (stop) { stops.push_back(std::move(*stop)); }
      }
    }
    if (owner->second.Tickets == 0) {
      retired.emplace(std::move(owner->second));
      Consumers.erase(owner);
      --Held.Consumers;
    }
  }
  for (auto &stop : stops) { (void)stop.request_stop(); }
  if (!stops.empty()) { WakeStopped(); }
}

AssetPipeline::Consumer::Consumer(std::shared_ptr<State> state, AssetConsumerId id)
    : State_(std::move(state)), Id_(id) {}

AssetPipeline::Consumer::Consumer(Consumer &&other) noexcept
    : State_(std::move(other.State_)), Id_(std::exchange(other.Id_, {})) {}

AssetPipeline::Consumer &AssetPipeline::Consumer::operator=(Consumer &&other) noexcept {
  if (this != &other) {
    if (State_) { State_->Retire(Id_); }
    State_ = std::move(other.State_);
    Id_ = std::exchange(other.Id_, {});
  }
  return *this;
}

AssetPipeline::Consumer::~Consumer() {
  if (State_) { State_->Retire(Id_); }
}

AssetConsumerGeneration AssetPipeline::Consumer::Generation() const noexcept {
  if (!State_) { return {.Value = 0}; }
  const std::scoped_lock lock(State_->Lock);
  const auto at = State_->Consumers.find(Id_);
  return at != State_->Consumers.end() ? at->second.Generation
                                       : AssetConsumerGeneration{.Value = 0};
}

bool AssetPipeline::Consumer::Advance(AssetConsumerGeneration generation) noexcept {
  return State_ && State_->Advance(Id_, generation);
}

AssetPipeline::RequestTicket::RequestTicket(std::shared_ptr<State> state, uint64_t id)
    : State_(std::move(state)), Id_(id) {}

AssetPipeline::RequestTicket::RequestTicket(RequestTicket &&other) noexcept
    : State_(std::move(other.State_)), Id_(std::exchange(other.Id_, 0)) {}

AssetPipeline::RequestTicket &
AssetPipeline::RequestTicket::operator=(RequestTicket &&other) noexcept {
  if (this != &other) {
    if (State_) { State_->ReleaseTicket(Id_); }
    State_ = std::move(other.State_);
    Id_ = std::exchange(other.Id_, 0);
  }
  return *this;
}

AssetPipeline::RequestTicket::~RequestTicket() {
  if (State_) { State_->ReleaseTicket(Id_); }
}

void AssetPipeline::RequestTicket::Cancel() noexcept {
  if (State_) { State_->CancelTicket(Id_); }
}

std::expected<void, AssetPublicationError>
AssetPipeline::RequestTicket::SetConsumerBytes(size_t bytes) noexcept {
  if (!State_) { return std::unexpected(AssetPublicationError::InvalidRequest); }
  auto &state = *State_;
  const std::scoped_lock lock(state.Lock);
  const auto at = state.Tickets.find(Id_);
  if (at == state.Tickets.end() || at->second.Cancelled) {
    return std::unexpected(AssetPublicationError::Cancelled);
  }
  if (bytes > state.Budget.InputBytesMost) {
    return std::unexpected(AssetPublicationError::InvalidRequest);
  }
  const size_t others = state.Held.InputBytes - at->second.Bytes;
  if (bytes > state.Budget.InputBytesMost - others) {
    return std::unexpected(AssetPublicationError::ResourceDeferred);
  }
  state.Held.InputBytes = others + bytes;
  at->second.Bytes = bytes;
  return {};
}

AssetPublication AssetPipeline::RequestTicket::Poll() const noexcept {
  return State_ ? State_->Poll(Id_)
                : AssetPublication{.Status = AssetPublicationStatus::Cancelled,
                                   .Receipt = {},
                                   .Error = AssetPublicationError::InvalidRequest,
                                   .Message = {},
                                   .Native = {}};
}
}
