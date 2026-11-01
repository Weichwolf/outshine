#include "AssetPipeline.h"
#include "Check.h"
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;
constexpr size_t kInput = 64, kReceipt = 2048;

struct Probe {
  std::thread::id Caller = std::this_thread::get_id();
  std::atomic<bool> Release{true}, CleanupEntered{false}, Freed{false}, OffCaller{false};
  std::atomic_bool Growing{false};
  std::atomic<unsigned> Computations{0};
  std::atomic<size_t> Admitted{0};
};

struct Input {
  explicit Input(std::shared_ptr<Probe> probe) : State(std::move(probe)) {}

  std::shared_ptr<Probe> State;
  std::vector<uint8_t> Bytes;

  ~Input() {
    State->CleanupEntered = true;
    while (!State->Release.load()) { std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
    std::vector<uint8_t>().swap(Bytes);
    State->OffCaller = std::this_thread::get_id() != State->Caller;
    State->Freed = true;
  }
};

struct ReleaseOnExit {
  const std::vector<std::shared_ptr<Probe>> &Probes;

  ~ReleaseOnExit() {
    for (const auto &probe : Probes) { probe->Release = true; }
  }
};

std::string Address(size_t index) {
  const std::string address(64, "fedcba9876543210"[index]);
  return address;
}

AssetRecord Record(const std::string &address) {
  return {.Key = std::string(64, 'a'),
          .Kind = "growth-control",
          .Bounds = {.Min = {{0, 0, 0}}, .Max = {{1, 1, 1}}},
          .Package = std::string(64, 'f'),
          .ByteCount = 1,
          .Parent = {},
          .RequestKey = address};
}

AssetPublication Wait(const AssetPipeline::RequestTicket &ticket) {
  const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  AssetPublication result;
  do {
    result = ticket.Poll();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  } while (!result.Finished && std::chrono::steady_clock::now() < until);
  return result;
}

AssetPipeline::LogicalProducer Produce(AssetPipeline &pipeline,
                                       std::atomic_bool &armed,
                                       std::shared_ptr<Probe> probe,
                                       std::string address,
                                       size_t bytes) {
  return [&pipeline,
          &armed,
          input = std::make_shared<Input>(std::move(probe)),
          address = std::move(address),
          bytes,
          stage = 0u](const std::stop_token &) mutable
             -> std::expected<AssetPipeline::LogicalStep, AssetPipeline::LogicalFailure> {
    if (!armed.load()) { return std::optional<AssetProduct>{}; }
    if (stage++ == 0) {
      input->Bytes = std::vector<uint8_t>(kInput);
      input->State->Growing = true;
      return AssetPipeline::InputAdmission{.Bytes = bytes};
    }
    input->State->Admitted = pipeline.Counts().InputBytes;
    ++input->State->Computations;
    std::vector<uint8_t>().swap(input->Bytes);
    input->Bytes = std::vector<uint8_t>(bytes);
    return std::optional(AssetProduct(Record(address)));
  };
}

void Retry(AssetPipeline &pipeline,
           const AssetPipeline::Consumer &owner,
           std::atomic_bool &armed,
           const std::string &address,
           size_t bytes) {
  const auto probe = std::make_shared<Probe>();
  auto retry = pipeline.RequestLogical(owner,
                                       {.Address = address,
                                        .Generation = owner.Generation(),
                                        .InputBytes = kInput,
                                        .ReceiptBytesMost = kReceipt},
                                       Produce(pipeline, armed, probe, address, bytes));
  CHECK(retry, "a yielded address can restart while its old failed tickets remain alive");
  if (!retry) { return; }
  AssetPublication retried;
  const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  do {
    retried = retry->Poll();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  } while (!retried.Finished && std::chrono::steady_clock::now() < until);
  CHECK(retried.Status == AssetPublicationStatus::Ready && probe->Computations == 1 &&
            retried.RequestKey == AssetRequestKey::Parse(address),
        "the repeated demand resolves anew and preserves its own request address");
}

struct Setup {
  size_t Jobs = 2;
  int Workers = 1;
  bool Reverse = false;
  bool HoldCleanup = false;
};

void Compete(const Setup &setup) {
  Tasks pool(setup.Workers);
  const size_t count = setup.Jobs;
  bool holdCleanup = setup.HoldCleanup;
  const size_t budget = count * kInput;
  const size_t peak = budget - kInput / 2;
  AssetPipeline pipeline(pool,
                         {.JobsMost = count + 1,
                          .ConsumersMost = count + 1,
                          .TicketsMost = count + 2,
                          .InputBytesMost = budget,
                          .ReceiptBytesMost = (count + 1) * kReceipt});
  std::atomic_bool armed{false};
  std::vector<std::shared_ptr<Probe>> probes;
  const ReleaseOnExit release{probes};
  std::vector<AssetPipeline::Consumer> owners;
  std::vector<AssetPipeline::RequestTicket> tickets;
  for (size_t i = 0; i <= count; ++i) {
    auto owner = pipeline.OpenConsumer();
    CHECK(owner, "original demands and their coalesced alias fit the consumer bound");
    if (!owner) { return; }
    owners.push_back(std::move(*owner));
    if (i == count) { break; }
    probes.push_back(std::make_shared<Probe>());
    auto ticket =
        pipeline.RequestLogical(owners.back(),
                                {.Address = Address(i),
                                 .Generation = owners.back().Generation(),
                                 .InputBytes = kInput,
                                 .ReceiptBytesMost = kReceipt},
                                Produce(pipeline, armed, probes.back(), Address(i), peak));
    CHECK(ticket, "each original input is admitted before any producer requests growth");
    if (!ticket) { return; }
    tickets.push_back(std::move(*ticket));
  }
  std::atomic_bool aliasInvoked{false};
  auto alias = pipeline.RequestLogical(
      owners.back(),
      {.Address = Address(count - 1),
       .Generation = owners.back().Generation(),
       .InputBytes = kInput,
       .ReceiptBytesMost = kReceipt},
      [&](const std::stop_token &)
          -> std::expected<AssetPipeline::LogicalStep, AssetPipeline::LogicalFailure> {
        aliasInvoked = true;
        return std::unexpected("coalesced producer must not run");
      });
  CHECK(alias, "another consumer joins the youngest job without another input reservation");
  if (!alias) { return; }
  probes.back()->Release = !holdCleanup;
  armed = true;
  const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  std::vector<AssetPublication> results(count);
  bool finished = false;
  do {
    finished = true;
    for (size_t n = 0; n < count; ++n) {
      const size_t i = setup.Reverse ? count - 1 - n : n;
      results[i] = tickets[i].Poll();
      finished = finished && results[i].Finished;
    }
    if (holdCleanup && probes.back()->CleanupEntered.load()) {
      CHECK(!probes.back()->Freed && pipeline.Counts().InputBytes == budget &&
                probes.front()->Computations == 0,
            "yielded input stays charged until its worker cleanup has actually released it");
      probes.back()->Release = true;
      holdCleanup = false;
    }
    CHECK(pipeline.Counts().InputBytes <= budget, "growth never raises the shared RAM bound");
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  } while (!finished && std::chrono::steady_clock::now() < until);
  CHECK(finished && results.front().Status == AssetPublicationStatus::Ready &&
            probes.front()->Computations == 1 && probes.front()->Admitted == peak,
        "the oldest producer finishes without caller cancellation or extra capacity");
  for (size_t i = 1; i < count; ++i) {
    CHECK(results[i].Status == AssetPublicationStatus::Failed &&
              results[i].Error == AssetPublicationError::ResourceDeferred &&
              probes[i]->Computations == 0 && probes[i]->Freed && probes[i]->OffCaller,
          "younger competing growth yields before allocation and frees inputs on the worker");
    Retry(pipeline, owners[i], armed, Address(i), peak);
  }
  CHECK(alias->Poll().Error == AssetPublicationError::ResourceDeferred && !aliasInvoked,
        "old coalesced tickets retain their yielded result after new requests succeed");
  alias = std::unexpected(AssetPublicationError::InvalidRequest);
  tickets.clear();
  results.clear();
  CHECK(pipeline.Counts().Jobs == 0 && pipeline.Counts().InputBytes == 0 &&
            pipeline.Counts().ReceiptBytes == 0 && pipeline.Counts().Addresses == 0,
        "original, aliased and retried work release every reservation and rendezvous entry");
}

void RetryRespectsWaitingGrowth() {
  Tasks pool(1);
  AssetPipeline pipeline(pool,
                         {.JobsMost = 3,
                          .ConsumersMost = 2,
                          .TicketsMost = 3,
                          .InputBytesMost = 128,
                          .ReceiptBytesMost = 3 * kReceipt});
  std::atomic_bool firstArmed{false};
  std::atomic_bool secondArmed{false};
  std::vector<std::shared_ptr<Probe>> probes;
  std::vector<AssetPipeline::Consumer> owners;
  std::vector<AssetPipeline::RequestTicket> tickets;
  for (size_t i = 0; i < 2; ++i) {
    auto owner = pipeline.OpenConsumer();
    CHECK(owner, "both independently controlled inputs fit the original RAM budget");
    if (!owner) { return; }
    owners.push_back(std::move(*owner));
    probes.push_back(std::make_shared<Probe>());
    auto ticket = pipeline.RequestLogical(
        owners.back(),
        {.Address = Address(i),
         .Generation = owners.back().Generation(),
         .InputBytes = kInput,
         .ReceiptBytesMost = kReceipt},
        Produce(pipeline, i == 0 ? firstArmed : secondArmed, probes.back(), Address(i), 96));
    CHECK(ticket, "both producers retain their smaller original input before either grows");
    if (!ticket) { return; }
    tickets.push_back(std::move(*ticket));
  }
  firstArmed = true;
  const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while ((!probes.front()->Growing || pipeline.Counts().Running != 0) &&
         std::chrono::steady_clock::now() < until) {
    (void)tickets.front().Poll();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  CHECK(probes.front()->Growing && probes.front()->Computations == 0,
        "the oldest input is waiting on its actual next-stage reservation");
  secondArmed = true;
  AssetPublication yielded;
  do {
    yielded = tickets.back().Poll();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  } while (!yielded.Finished && std::chrono::steady_clock::now() < until);
  CHECK(yielded.Error == AssetPublicationError::ResourceDeferred && probes.back()->Freed &&
            pipeline.Counts().InputBytes == kInput,
        "younger work releases its input while the older producer remains unpolled");
  auto retried = pipeline.RequestLogical(
      owners.back(),
      {.Address = Address(1),
       .Generation = owners.back().Generation(),
       .InputBytes = kInput,
       .ReceiptBytesMost = kReceipt},
      Produce(pipeline, secondArmed, std::make_shared<Probe>(), Address(1), 96));
  CHECK(!retried && retried.error() == AssetPublicationError::ResourceDeferred,
        "a repeated request cannot reclaim the bytes needed by the older waiting growth");
  retried = std::unexpected(AssetPublicationError::InvalidRequest);
  auto consumer = tickets.back().SetConsumerBytes(kInput);
  CHECK(!consumer && consumer.error() == AssetPublicationError::ResourceDeferred,
        "consumer retention obeys the same older growth priority as new jobs");
  CHECK(tickets.back().SetConsumerBytes(32),
        "capacity beyond the oldest growth need remains available for other consumers");
  AssetPublication completed;
  do {
    completed = tickets.front().Poll();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  } while (!completed.Finished && std::chrono::steady_clock::now() < until);
  CHECK(completed.Status == AssetPublicationStatus::Ready && probes.front()->Computations == 1,
        "the next oldest poll admits its growth even while allowed consumer bytes stay held");
}
}

namespace {
void PinnedConsumersDoNotCreateUnfillableHeadroom(size_t initial) {
  Tasks pool(1);
  AssetPipeline pipeline(pool,
                         {.JobsMost = 3,
                          .ConsumersMost = 1,
                          .TicketsMost = 3,
                          .InputBytesMost = 128,
                          .ReceiptBytesMost = 3 * kReceipt});
  auto owner = pipeline.OpenConsumer();
  CHECK(owner, "one consumer can retain a completed product while requesting others");
  if (!owner) { return; }
  auto pinned = pipeline.RequestLogical(
      *owner,
      {.Address = Address(0), .Generation = owner->Generation(), .ReceiptBytesMost = kReceipt},
      [](const std::stop_token &)
          -> std::expected<AssetPipeline::LogicalStep, AssetPipeline::LogicalFailure> {
        return std::optional(AssetProduct(Record(Address(0))));
      });
  CHECK(pinned, "the retained consumer has an independently completed receipt");
  if (!pinned) { return; }
  CHECK(Wait(*pinned).Status == AssetPublicationStatus::Ready && pinned->SetConsumerBytes(64),
        "external consumer bytes stay pinned independently of unfinished producer inputs");
  auto growing = pipeline.RequestLogical(
      *owner,
      {.Address = Address(1),
       .Generation = owner->Generation(),
       .InputBytes = initial,
       .ReceiptBytesMost = kReceipt},
      [stage = 0u](const std::stop_token &) mutable
          -> std::expected<AssetPipeline::LogicalStep, AssetPipeline::LogicalFailure> {
        if (stage++ == 0) { return AssetPipeline::InputAdmission{.Bytes = 96}; }
        return std::optional(AssetProduct(Record(Address(1))));
      });
  CHECK(growing, "the original smaller input fits beside the pinned consumer");
  if (!growing) { return; }
  const auto deferred = Wait(*growing);
  CHECK(deferred.Finished && deferred.Error == AssetPublicationError::ResourceDeferred &&
            pipeline.Counts().InputBytes == 64,
        "growth that cannot coexist with pinned data yields instead of reserving all free RAM");
  auto small = pipeline.RequestLogical(
      *owner,
      {.Address = Address(2),
       .Generation = owner->Generation(),
       .InputBytes = 32,
       .ReceiptBytesMost = kReceipt},
      [](const std::stop_token &)
          -> std::expected<AssetPipeline::LogicalStep, AssetPipeline::LogicalFailure> {
        return std::optional(AssetProduct(Record(Address(2))));
      });
  CHECK(small, "independent smaller work can use available RAM while external data remains pinned");
  if (small) {
    CHECK(Wait(*small).Status == AssetPublicationStatus::Ready, "small work completes");
  }
}
}

int main() {
  for (const size_t count : {size_t{2}, size_t{3}, size_t{8}}) {
    for (const int threads : {1, 2}) {
      Compete({.Jobs = count, .Workers = threads});
      Compete({.Jobs = count, .Workers = threads, .Reverse = true});
    }
  }
  Compete({.Jobs = 2, .Workers = 2, .Reverse = true, .HoldCleanup = true});
  RetryRespectsWaitingGrowth();
  PinnedConsumersDoNotCreateUnfillableHeadroom(0);
  PinnedConsumersDoNotCreateUnfillableHeadroom(64);
  return Report();
}
