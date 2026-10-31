#include "AssetPipeline.h"
#include "Check.h"
#include <atomic>
#include <chrono>
#include <cstddef>
#include <expected>
#include <initializer_list>
#include <memory>
#include <optional>
#include <semaphore>
#include <stop_token>
#include <string>
#include <thread>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Test;
constexpr auto kDeadline = std::chrono::seconds(5);
constexpr size_t kInput = 128, kDemand = 64, kReceipt = 1024;

AssetRecord Record() {
  return {.Key = std::string(64, 'a'),
          .Kind = "deferred-publication",
          .Bounds = {.Min = {{0, 0, 0}}, .Max = {{1, 1, 1}}},
          .Package = std::string(64, 'b'),
          .ByteCount = 3,
          .Parent = {}};
}

AssetPipeline::ResolvedRequest Request(const AssetPipeline::Consumer &consumer) {
  return {.Key = AssetProductKey::Parse(std::string(64, 'a')).value_or(AssetProductKey{}),
          .Generation = consumer.Generation(),
          .InputBytes = kInput,
          .ConsumerBytes = kDemand,
          .ReceiptBytesMost = kReceipt};
}

void Barrier(Tasks &pool) {
  const auto barrier = pool.Post([] {});
  CHECK(barrier != Tasks::kNoTask && pool.AwaitCompletion(barrier, 5),
        "an independent task finishes on the single worker while publication is deferred");
  if (barrier != Tasks::kNoTask) { (void)pool.TakeCompletion(barrier); }
}

AssetPublication Wait(const AssetPipeline::RequestTicket &ticket) {
  const auto until = std::chrono::steady_clock::now() + kDeadline;
  do {
    auto result = ticket.Poll();
    if (result.Finished) { return result; }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  } while (std::chrono::steady_clock::now() < until);
  return {};
}

void CheckSharing() {
  Tasks pool(1);
  AssetPipeline pipeline(pool,
                         {.JobsMost = 1,
                          .ConsumersMost = 2,
                          .TicketsMost = 2,
                          .InputBytesMost = kInput + 2 * kDemand,
                          .ReceiptBytesMost = kReceipt});
  auto a = pipeline.OpenConsumer();
  auto b = pipeline.OpenConsumer();
  CHECK(a && b, "two consumers open inside the unchanged admission bound");
  if (!a || !b) { return; }
  std::atomic<size_t> calls{0};
  std::atomic<size_t> duplicate{0};
  std::atomic<bool> ready{false};
  auto input = std::make_shared<int>(7);
  const std::weak_ptr<int> held = input;
  auto first = pipeline.RequestResumable(
      *a,
      Request(*a),
      [input = std::move(input), &calls, &ready](
          const std::stop_token &) -> std::expected<std::optional<AssetProduct>, std::string> {
        ++calls;
        if (!input) { return std::unexpected("lost retained input"); }
        if (!ready.load()) { return std::nullopt; }
        return AssetProduct(Record());
      });
  auto second = pipeline.RequestResolved(*b, Request(*b), [&duplicate](const std::stop_token &) {
    ++duplicate;
    return AssetProduct(Record());
  });
  CHECK(first && second, "a second consumer shares the existing deferred product");
  if (!first || !second) { return; }
  Barrier(pool);
  const auto initial = pipeline.Counts();
  CHECK(initial.Jobs == 1 && initial.Running == 0 && initial.Tickets == 2 &&
            initial.InputBytes == kInput + 2 * kDemand && !held.expired(),
        "deferred work keeps input, job and receipt reservations while freeing the worker");
  const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(50);
  while (std::chrono::steady_clock::now() < until) {
    CHECK(first->Poll().Status == AssetPublicationStatus::Pending,
          "unavailable publication remains pending during repeated observations");
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  Barrier(pool);
  CHECK(calls.load() > 1 && calls.load() < 20 && duplicate.load() == 0,
        "deadlines bound attempts and shared consumers never start another producer");
  first->Cancel();
  CHECK(first->Poll().Status == AssetPublicationStatus::Cancelled,
        "one consumer cancels independently without canceling the surviving demand");
  ready.store(true);
  const auto result = Wait(*second);
  CHECK(result.Status == AssetPublicationStatus::Ready && result.Receipt && held.expired(),
        "the survivor receives the real receipt and terminal execution releases retained inputs");
  CHECK(Wait(*first).Status == AssetPublicationStatus::Cancelled && duplicate.load() == 0,
        "the canceled consumer stays canceled through the shared terminal");
  Note("deferred producer attempts", static_cast<double>(calls.load()), "steps");
}

enum class Ending { Cancel, Generation, Retire, Release };

void CheckCancel(bool active, Ending ending) {
  Tasks pool(1);
  AssetPipeline pipeline(pool,
                         {.JobsMost = 1,
                          .ConsumersMost = 1,
                          .TicketsMost = 1,
                          .InputBytesMost = kInput + kDemand,
                          .ReceiptBytesMost = kReceipt});
  auto owner = pipeline.OpenConsumer();
  if (!owner) {
    CHECK(false, "the cancellation consumer opens");
    return;
  }
  std::binary_semaphore started{0};
  std::binary_semaphore release{0};
  auto input = std::make_shared<int>(3);
  const std::weak_ptr<int> held = input;
  auto ticket = pipeline.RequestResumable(
      *owner,
      Request(*owner),
      [input = std::move(input), &started, &release, active](
          const std::stop_token &) -> std::expected<std::optional<AssetProduct>, std::string> {
        if (!input) { return std::unexpected("lost canceled input"); }
        if (active) {
          started.release();
          release.acquire();
        }
        return std::nullopt;
      });
  CHECK(ticket.has_value(), "the cancelable publication is admitted");
  if (!ticket) { return; }
  if (active) {
    CHECK(started.try_acquire_for(kDeadline), "cancellation overlaps the actual producer step");
  } else {
    Barrier(pool);
    CHECK(pipeline.Counts().Running == 0, "the canceled job has already yielded its worker");
  }
  switch (ending) {
    case Ending::Cancel: ticket->Cancel(); break;
    case Ending::Generation:
      CHECK(owner->Advance({.Value = 2}), "a new generation retires the last old demand");
      break;
    case Ending::Retire: owner = std::unexpected(AssetPublicationError::InvalidRequest); break;
    case Ending::Release: ticket = std::unexpected(AssetPublicationError::InvalidRequest); break;
  }
  if (active) { release.release(); }
  Barrier(pool);
  CHECK(held.expired() && pipeline.Counts().Running == 0 &&
            pipeline.Counts().InputBytes == (ticket ? kDemand : 0),
        "active and idle cancellation release inputs on the pool without requiring another poll");
  if (ticket) {
    const auto result = ticket->Poll();
    CHECK(result.Status == AssetPublicationStatus::Cancelled && result.Finished,
          "the cancellation/worker boundary cannot strand deferred work");
  } else {
    const auto empty = pipeline.Counts();
    CHECK(empty.Jobs == 0 && empty.Tickets == 0 && empty.ReceiptBytes == 0,
          "releasing the last deferred ticket retires all job and output reservations");
  }
}
}

int main() {
  CheckSharing();
  for (const auto ending : {Ending::Cancel, Ending::Generation, Ending::Retire, Ending::Release}) {
    CheckCancel(false, ending);
    CheckCancel(true, ending);
  }
  return outshine::Test::Report();
}
