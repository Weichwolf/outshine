#include "AssetPipeline.h"
#include "Check.h"
#include <atomic>
#include <chrono>
#include <cstddef>
#include <expected>
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
constexpr size_t kInput = 128, kDemand = 64, kReceipt = 1024;
constexpr auto kDeadline = std::chrono::seconds(5);
constexpr auto kPoll = std::chrono::milliseconds(1);

AssetRecord Record(char key) {
  return {.Key = std::string(64, key),
          .Kind = "controlled-publication",
          .Bounds = {.Min = {{0, 0, 0}}, .Max = {{1, 1, 1}}},
          .Package = std::string(64, 'f'),
          .OffsetBytes = 0,
          .ByteCount = 3,
          .Level = 0,
          .Parent = {},
          .RequestKey = {}};
}

AssetPipeline::ResolvedRequest Request(const AssetPipeline::Consumer &consumer, char key) {
  return {.Key = *AssetProductKey::Parse(std::string(64, key)),
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
                            .TicketsMost = 4,
                            .InputBytesMost = kInput + 4 * kDemand,
                            .ReceiptBytesMost = kReceipt});
}

struct Control {
  std::binary_semaphore Started{0}, Release{0};
  std::atomic<size_t> Calls{0}, Stops{0};
};

AssetPipeline::Producer Held(const std::shared_ptr<Control> &control,
                             std::weak_ptr<AssetPipeline> pipeline,
                             std::shared_ptr<int> input) {
  return [control, pipeline, input = std::move(input)](
             std::stop_token stop) -> std::expected<AssetRecord, std::string> {
    const std::stop_callback callback(stop, [control, pipeline] {
      if (const auto live = pipeline.lock()) { (void)live->Counts(); }
      ++control->Stops;
    });
    ++control->Calls;
    control->Started.release();
    control->Release.acquire();
    if (!input || *input != 1) { return std::unexpected("missing captured input"); }
    return Record('a');
  };
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

bool Idle(const AssetPipeline &pipeline) {
  const auto until = std::chrono::steady_clock::now() + kDeadline;
  do {
    if (pipeline.Counts().Running == 0 && pipeline.Counts().ReceiptBytes == 0) { return true; }
    std::this_thread::sleep_for(kPoll);
  } while (std::chrono::steady_clock::now() < until);
  return false;
}

void CheckDraining() {
  Tasks pool(2);
  const auto pipeline = Pipeline(pool);
  auto a = pipeline->OpenConsumer();
  auto b = pipeline->OpenConsumer();
  CHECK(a && b && !pipeline->OpenConsumer(),
        "consumer admission is bounded without an unbounded history");
  if (!a || !b) { return; }
  const auto control = std::make_shared<Control>();
  auto input = std::make_shared<int>(1);
  const std::weak_ptr<int> retained = input;
  auto first =
      pipeline->RequestResolved(*a, Request(*a, 'a'), Held(control, pipeline, std::move(input)));
  CHECK(first.has_value(), "the real shared dispatcher admits its first controlled producer");
  if (!first) { return; }
  const bool started = control->Started.try_acquire_for(kDeadline);
  CHECK(started, "the producer owns its captured input while running");
  if (!started) {
    control->Release.release();
    return;
  }
  auto second =
      pipeline->RequestResolved(*b, Request(*b, 'a'), [](std::stop_token) { return Record('a'); });
  CHECK(second && pipeline->Counts().Jobs == 1 &&
            pipeline->Counts().InputBytes == kInput + 2 * kDemand,
        "independent tickets share one job and retain separately admitted consumer bytes");
  first->Cancel();
  CHECK(control->Stops.load() == 0,
        "canceling A leaves the still demanded native producer running");
  if (second) { second->Cancel(); }
  CHECK(control->Stops.load() == 1 && !retained.expired(),
        "the last cancellation calls back outside the state mutex and retains live captures");
  first = std::unexpected(AssetPublicationError::InvalidRequest);
  second = std::unexpected(AssetPublicationError::InvalidRequest);
  CHECK(pipeline->Counts().Jobs == 1 && pipeline->Counts().InputBytes == kInput &&
            pipeline->Counts().ReceiptBytes == kReceipt,
        "dropping canceled tickets releases no running job, captured input or output reservation");
  const auto repeat =
      pipeline->RequestResolved(*a, Request(*a, 'a'), [](std::stop_token) { return Record('a'); });
  const auto other =
      pipeline->RequestResolved(*b, Request(*b, 'b'), [](std::stop_token) { return Record('b'); });
  CHECK(!repeat && repeat.error() == AssetPublicationError::ResourceDeferred && !other,
        "later demand neither revives stopped work nor reuses its still occupied slot");
  control->Release.release();
  CHECK(Idle(*pipeline) && retained.expired() && pipeline->Counts().Jobs == 0 &&
            pipeline->Counts().InputBytes == 0 && pipeline->Counts().ReceiptBytes == 0,
        "actual worker completion releases captures and abandoned reservations");
}

void CheckLeases() {
  Tasks pool(1);
  const auto pipeline = Pipeline(pool);
  auto consumer = pipeline->OpenConsumer();
  CHECK(consumer.has_value(), "a scoped consumer opens");
  if (!consumer) { return; }
  auto ticket = pipeline->RequestResolved(
      *consumer, Request(*consumer, 'a'), [](std::stop_token) { return Record('a'); });
  CHECK(ticket.has_value(), "a publication is admitted");
  if (!ticket) { return; }
  auto result = Wait(*ticket);
  CHECK(result.Status == AssetPublicationStatus::Ready && result.Receipt &&
            result.Receipt->Key == std::string(64, 'a'),
        "poll returns an immutable valid receipt");
  ticket = std::unexpected(AssetPublicationError::InvalidRequest);
  CHECK(pipeline->Counts().Tickets == 0 && pipeline->Counts().Jobs == 0 &&
            pipeline->Counts().ReceiptBytes == kReceipt,
        "a receipt lease retains its reservation after the ticket and job index are gone");
  const auto blocked = pipeline->RequestResolved(
      *consumer, Request(*consumer, 'b'), [](std::stop_token) { return Record('b'); });
  CHECK(!blocked && blocked.error() == AssetPublicationError::ResourceDeferred,
        "held terminal outputs participate in admission");
  result.Receipt.reset();
  auto mismatch = pipeline->RequestResolved(
      *consumer, Request(*consumer, 'b'), [](std::stop_token) { return Record('c'); });
  CHECK(mismatch.has_value(), "last-lease release restores output capacity");
  if (!mismatch) { return; }
  const auto failed = Wait(*mismatch);
  CHECK(failed.Status == AssetPublicationStatus::Failed &&
            failed.Error == AssetPublicationError::InvalidReceipt,
        "an unrelated producer receipt cannot satisfy the requested content identity");
}

void CheckGenerations() {
  Tasks pool(1);
  const auto pipeline = Pipeline(pool);
  auto a = pipeline->OpenConsumer();
  auto b = pipeline->OpenConsumer();
  if (!a || !b) {
    CHECK(false, "two independent consumers open");
    return;
  }
  const auto control = std::make_shared<Control>();
  auto oldRequest = Request(*a, 'a');
  auto old =
      pipeline->RequestResolved(*a, oldRequest, Held(control, pipeline, std::make_shared<int>(1)));
  if (!old) {
    CHECK(false, "the old generation admits its producer");
    return;
  }
  const bool started = control->Started.try_acquire_for(kDeadline);
  CHECK(started, "a real job is running before a generation change");
  if (!started) {
    control->Release.release();
    return;
  }
  auto survivor =
      pipeline->RequestResolved(*b, Request(*b, 'a'), [](std::stop_token) { return Record('a'); });
  CHECK(survivor && a->Advance({.Value = 2}) && !a->Advance({.Value = 2}) &&
            control->Stops.load() == 0,
        "a new generation invalidates only A and preserves B's live shared demand");
  auto stale =
      pipeline->RequestResolved(*a, oldRequest, [](std::stop_token) { return Record('a'); });
  CHECK(!stale && stale.error() == AssetPublicationError::StaleGeneration,
        "old generation requests cannot become current again");
  auto current =
      pipeline->RequestResolved(*a, Request(*a, 'a'), [](std::stop_token) { return Record('a'); });
  CHECK(current.has_value(), "a current consumer can join a producer still valid for B");
  control->Release.release();
  CHECK(Wait(*old).Status == AssetPublicationStatus::Cancelled,
        "the old world binding receives no product after completion");
  if (!survivor || !current) { return; }
  const auto left = Wait(*survivor);
  const auto right = Wait(*current);
  CHECK(left.Status == AssetPublicationStatus::Ready &&
            right.Status == AssetPublicationStatus::Ready && left.Receipt == right.Receipt &&
            control->Calls.load() == 1,
        "current independent generations receive one shared immutable publication");
}
}

int main() {
  CheckDraining();
  CheckLeases();
  CheckGenerations();
  return outshine::Test::Report();
}
