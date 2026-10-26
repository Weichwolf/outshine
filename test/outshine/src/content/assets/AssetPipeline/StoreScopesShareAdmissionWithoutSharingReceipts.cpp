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
constexpr size_t kReceipt = 1024;
constexpr auto kDeadline = std::chrono::seconds(5);
constexpr auto kPoll = std::chrono::milliseconds(1);

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
          .InputBytes = 1,
          .ConsumerBytes = 1,
          .ReceiptBytesMost = kReceipt};
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
}

int main() {
  Tasks pool(1);
  AssetPipeline pipeline(pool,
                         {.JobsMost = 1,
                          .ConsumersMost = 2,
                          .TicketsMost = 2,
                          .InputBytesMost = 4,
                          .ReceiptBytesMost = kReceipt});
  auto oldStore = std::make_shared<int>(1);
  const std::weak_ptr<int> oldLifetime = oldStore;
  const auto newStore = std::make_shared<int>(2);
  auto old = pipeline.OpenConsumer({.Identity = oldStore});
  auto current = pipeline.OpenConsumer({.Identity = newStore});
  CHECK(old && current, "one production owner admits two independent store contexts");
  if (!old || !current) { return Report(); }

  struct Work {
    std::binary_semaphore Started{0}, Release{0};
    std::atomic<size_t> Calls{0};
  };

  const auto work = std::make_shared<Work>();
  auto first = pipeline.RequestResolved(*old, Request(*old), [work](const std::stop_token &) {
    ++work->Calls;
    work->Started.release();
    work->Release.acquire();
    return Record();
  });
  CHECK(first.has_value(), "the original store admits an actual queued job");
  if (!first) { return Report(); }
  const bool started = work->Started.try_acquire_for(kDeadline);
  CHECK(started, "the old store's worker remains active across the context switch");
  if (!started) {
    work->Release.release();
    return Report();
  }
  oldStore.reset();
  CHECK(oldLifetime.expired(), "a store identity does not pin its cache object or IO destructor");
  const auto second = pipeline.RequestResolved(
      *current, Request(*current), [](const std::stop_token &) { return Record(); });
  CHECK(!second && second.error() == AssetPublicationError::ResourceDeferred,
        "a new store neither shares the old store's receipt nor receives a new independent budget");
  first->Cancel();
  const auto draining = pipeline.RequestResolved(
      *current, Request(*current), [](const std::stop_token &) { return Record(); });
  CHECK(!draining && pipeline.Counts().Jobs == 1,
        "a store switch cannot bypass capacity still held by canceled running work");
  work->Release.release();
  CHECK(Wait(*first).Status == AssetPublicationStatus::Cancelled,
        "the old consumer receives no late result from its canceled job");
  first = std::unexpected(AssetPublicationError::InvalidRequest);
  const auto released = std::chrono::steady_clock::now() + kDeadline;
  while (pipeline.Counts().ReceiptBytes != 0 && std::chrono::steady_clock::now() < released) {
    std::this_thread::sleep_for(kPoll);
  }
  CHECK(pipeline.Counts().ReceiptBytes == 0,
        "physical completion releases the abandoned reservation");
  auto replacement =
      pipeline.RequestResolved(*current, Request(*current), [work](const std::stop_token &) {
        ++work->Calls;
        return Record();
      });
  CHECK(replacement.has_value(), "the same shared budget becomes available after real completion");
  if (replacement) {
    const auto result = Wait(*replacement);
    CHECK(result.Status == AssetPublicationStatus::Ready && work->Calls.load() == 2,
          "the new store produces its own receipt for the same content key");
  }
  return Report();
}
