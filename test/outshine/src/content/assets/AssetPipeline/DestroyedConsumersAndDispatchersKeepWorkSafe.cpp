#include "AssetPipeline.h"
#include "Check.h"
#include <atomic>
#include <chrono>
#include <cstddef>
#include <expected>
#include <memory>
#include <semaphore>
#include <stop_token>
#include <string>
#include <thread>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Test;
constexpr size_t kReceipt = 1024, kInput = 128, kDemand = 64;
constexpr auto kDeadline = std::chrono::seconds(5);
constexpr auto kPoll = std::chrono::milliseconds(1);

struct Work {
  std::binary_semaphore Started{0}, Release{0};
  std::atomic<size_t> Stops{0}, Calls{0};
};

AssetRecord Record() {
  return {.Key = std::string(64, 'a'),
          .Kind = "controlled-publication",
          .Bounds = {.Min = {{0, 0, 0}}, .Max = {{1, 1, 1}}},
          .Package = std::string(64, 'b'),
          .OffsetBytes = 0,
          .ByteCount = 3,
          .Level = 0,
          .Parent = {},
          .RequestKey = {}};
}

AssetPipeline::ResolvedRequest Request(const AssetPipeline::Consumer &consumer) {
  return {.Key = AssetProductKey::Parse(std::string(64, 'a')).value_or(AssetProductKey{}),
          .Generation = consumer.Generation(),
          .InputBytes = kInput,
          .ConsumerBytes = kDemand,
          .ReceiptBytesMost = kReceipt};
}

std::shared_ptr<AssetPipeline> Pipeline(Tasks &pool) {
  return std::make_shared<AssetPipeline>(
      pool,
      AssetPipeline::Limits{.JobsMost = 1,
                            .ConsumersMost = 2,
                            .TicketsMost = 2,
                            .InputBytesMost = kInput + 2 * kDemand,
                            .ReceiptBytesMost = kReceipt});
}

AssetPublication Wait(const AssetPipeline::RequestTicket &ticket) {
  const auto until = std::chrono::steady_clock::now() + kDeadline;
  do {
    auto result = ticket.Poll();
    if (result.Status != AssetPublicationStatus::Pending) { return result; }
    std::this_thread::sleep_for(kPoll);
  } while (std::chrono::steady_clock::now() < until);
  return {};
}

void CheckConsumer() {
  Tasks pool(1);
  const auto pipeline = Pipeline(pool);
  auto a = pipeline->OpenConsumer();
  auto b = pipeline->OpenConsumer();
  CHECK(a && b, "two scoped consumers own independent lifetimes");
  if (!a || !b) { return; }
  const auto work = std::make_shared<Work>();
  auto first = pipeline->RequestResolved(*a, Request(*a), [work](const std::stop_token &stop) {
    const std::stop_callback callback(stop, [work] { ++work->Stops; });
    ++work->Calls;
    work->Started.release();
    work->Release.acquire();
    return std::expected<AssetProduct, std::string>(AssetProduct(Record()));
  });
  CHECK(first.has_value(), "the first publication is admitted");
  if (!first) { return; }
  const bool started = work->Started.try_acquire_for(kDeadline);
  CHECK(started, "the actual worker is held while its initial owner is destroyed");
  if (!started) {
    work->Release.release();
    return;
  }
  auto second = pipeline->RequestResolved(
      *b, Request(*b), [](const std::stop_token &) { return AssetProduct(Record()); });
  a = std::unexpected(AssetPublicationError::InvalidRequest);
  CHECK(second && work->Stops.load() == 0 && pipeline->Counts().Consumers == 2,
        "destruction returns before worker release and retains the retired consumer's held ticket");
  work->Release.release();
  CHECK(Wait(*first).Status == AssetPublicationStatus::Cancelled,
        "a retired owner receives no late world binding");
  if (!second) { return; }
  const auto ready = Wait(*second);
  CHECK(ready.Status == AssetPublicationStatus::Ready && ready.Receipt && work->Calls.load() == 1,
        "the surviving owner receives one completed immutable publication");
  first = std::unexpected(AssetPublicationError::InvalidRequest);
  CHECK(pipeline->Counts().Consumers == 1,
        "last retired-ticket release removes its consumer index without retained history");
}

void CheckDispatcher() {
  auto pool = std::make_unique<Tasks>(1);
  const auto block = std::make_shared<Work>();
  CHECK(pool->PostDetached([block] {
    block->Started.release();
    block->Release.acquire();
  }),
        "a separate real task holds the only dispatcher worker");
  const bool blocked = block->Started.try_acquire_for(kDeadline);
  CHECK(blocked, "the tested production remains queued rather than running");
  if (!blocked) {
    block->Release.release();
    return;
  }
  const auto pipeline = Pipeline(*pool);
  auto consumer = pipeline->OpenConsumer();
  if (!consumer) {
    block->Release.release();
    CHECK(false, "the consumer opens");
    return;
  }
  auto input = std::make_shared<int>(1);
  const std::weak_ptr<int> retained = input;
  auto ticket = pipeline->RequestResolved(*consumer,
                                          Request(*consumer),
                                          [input = std::move(input)](const std::stop_token &)
                                              -> std::expected<AssetProduct, std::string> {
                                            if (!input) { return std::unexpected("missing input"); }
                                            return AssetProduct(Record());
                                          });
  CHECK(ticket && !retained.expired(), "queued dispatch owns admitted inputs before execution");
  if (!ticket) {
    block->Release.release();
    return;
  }
  std::jthread shutdown([closing = std::move(pool)] mutable { closing.reset(); });
  const auto result = Wait(*ticket);
  CHECK(result.Status == AssetPublicationStatus::Failed &&
            result.Error == AssetPublicationError::DispatcherStopped && retained.expired() &&
            pipeline->Counts().Running == 0 && pipeline->Counts().InputBytes == kDemand,
        "discarded queued execution reaches a real terminal and releases its captures during "
        "dispatcher teardown");
  block->Release.release();
  shutdown.join();
  ticket = std::unexpected(AssetPublicationError::InvalidRequest);
  consumer = std::unexpected(AssetPublicationError::InvalidRequest);
  const auto empty = pipeline->Counts();
  CHECK(empty.Jobs == 0 && empty.Consumers == 0 && empty.Tickets == 0 && empty.InputBytes == 0 &&
            empty.ReceiptBytes == 0,
        "teardown leaves no unbounded jobs, tickets, consumers or budget history");
}
}

int main() {
  CheckConsumer();
  CheckDispatcher();
  return outshine::Test::Report();
}
