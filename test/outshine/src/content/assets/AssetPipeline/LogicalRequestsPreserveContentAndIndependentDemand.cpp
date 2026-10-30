#include "AssetPipeline.h"
#include "Check.h"
#include <atomic>
#include <chrono>
#include <cstddef>
#include <expected>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Test;
constexpr std::string_view kAddress =
    "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
constexpr size_t kReceipt = 1024;

AssetRecord Record(char key, bool logical) {
  return {.Key = std::string(64, key),
          .Kind = "logical-request-control",
          .Bounds = {.Min = {{0, 0, 0}}, .Max = {{1, 1, 1}}},
          .Package = std::string(64, 'd'),
          .ByteCount = 3,
          .Parent = {},
          .RequestKey = logical ? std::optional(std::string(kAddress)) : std::nullopt};
}

AssetPublication Wait(const AssetPipeline::RequestTicket &ticket) {
  const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  do {
    auto result = ticket.Poll();
    if (result.Finished) { return result; }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  } while (std::chrono::steady_clock::now() < until);
  return {};
}

void Barrier(Tasks &pool) {
  const auto barrier = pool.Post([] {});
  CHECK(barrier != Tasks::kNoTask && pool.AwaitCompletion(barrier, 5),
        "an independent task completes on the only worker while inputs are pending");
  if (barrier != Tasks::kNoTask) { (void)pool.TakeCompletion(barrier); }
}

void Sharing() {
  Tasks pool(1);
  AssetPipeline pipeline(pool,
                         {.JobsMost = 2,
                          .ConsumersMost = 3,
                          .TicketsMost = 3,
                          .InputBytesMost = 512,
                          .ReceiptBytesMost = 2 * kReceipt});
  auto a = pipeline.OpenConsumer();
  auto b = pipeline.OpenConsumer();
  auto c = pipeline.OpenConsumer();
  CHECK(a && b && c, "three consumers share one admission owner");
  if (!a || !b || !c) { return; }
  std::string address(kAddress);
  const auto request = [&](const AssetPipeline::Consumer &consumer) {
    return AssetPipeline::LogicalRequest{.Address = address,
                                         .Generation = consumer.Generation(),
                                         .InputBytes = 128,
                                         .ConsumerBytes = 16,
                                         .ReceiptBytesMost = kReceipt};
  };
  std::atomic<size_t> calls{0};
  std::atomic<size_t> duplicates{0};
  std::atomic<bool> ready{false};
  auto input = std::make_shared<int>(7);
  const std::weak_ptr<int> retained = input;
  auto first = pipeline.RequestLogical(
      *a,
      request(*a),
      [input = std::move(input), &calls, &ready](
          const std::stop_token &stop) -> std::expected<std::optional<AssetRecord>, std::string> {
        ++calls;
        if (!input || stop.stop_requested()) { return std::unexpected("lost shared input"); }
        if (!ready.load()) { return std::nullopt; }
        return Record('a', true);
      });
  auto second = pipeline.RequestLogical(
      *b,
      request(*b),
      [&duplicates](
          const std::stop_token &) -> std::expected<std::optional<AssetRecord>, std::string> {
        ++duplicates;
        return Record('c', true);
      });
  CHECK(first && second, "equal early addresses share one actual resumable job");
  if (!first || !second) { return; }
  address.assign(64, 'c');
  const AssetPipeline::ResolvedRequest resolved{
      .Key = AssetProductKey::Parse(kAddress).value_or(AssetProductKey{}),
      .Generation = c->Generation(),
      .ConsumerBytes = 16,
      .ReceiptBytesMost = kReceipt};
  auto content = pipeline.RequestResolved(
      *c, resolved, [](const std::stop_token &) { return Record('b', false); });
  CHECK(content.has_value(), "the same digest in the content domain admits independent work");
  if (!content) { return; }
  auto direct = Wait(*content);
  Barrier(pool);
  const auto pending = pipeline.Counts();
  CHECK(direct.Status == AssetPublicationStatus::Ready && direct.Receipt &&
            direct.Receipt->Key == kAddress && !direct.Receipt->RequestKey && !direct.RequestKey,
        "resolved requests keep their strict real content receipt");
  CHECK(pending.Jobs == 2 && pending.Running == 0 && pending.Tickets == 3 &&
            pending.InputBytes == 128 + 3 * 16 && !retained.expired(),
        "pending inputs free the worker while retaining one input and all admitted demand");
  CHECK(a->Advance({.Value = 2}) && first->Poll().Status == AssetPublicationStatus::Cancelled,
        "advancing one generation cancels only that consumer");
  ready.store(true);
  auto result = Wait(*second);
  CHECK(
      result.Status == AssetPublicationStatus::Ready && result.Receipt &&
          result.Receipt->Key == std::string(64, 'a') && !result.Receipt->RequestKey &&
          result.RequestKey == AssetRequestKey::Parse(kAddress),
      "the survivor receives actual content annotated with the original borrowed request address");
  CHECK(calls.load() >= 2 && duplicates.load() == 0 && retained.expired(),
        "one producer resumes and releases its input without executing the duplicate");
  CHECK(Wait(*first).Status == AssetPublicationStatus::Cancelled,
        "late shared completion cannot revive an obsolete generation");
  direct.Receipt.reset();
  result.Receipt.reset();
  first = std::unexpected(AssetPublicationError::InvalidRequest);
  second = std::unexpected(AssetPublicationError::InvalidRequest);
  content = std::unexpected(AssetPublicationError::InvalidRequest);
  Barrier(pool);
  CHECK(pipeline.Counts().Jobs == 0 && pipeline.Counts().ReceiptBytes == 0,
        "both key domains retire independently after their final ticket and receipt");
  address.assign(kAddress);
  auto replacement = pipeline.RequestLogical(
      *b,
      request(*b),
      [](const std::stop_token &) -> std::expected<std::optional<AssetRecord>, std::string> {
        return Record('c', true);
      });
  CHECK(replacement.has_value(), "a retired logical address admits a fresh producer");
  if (replacement) {
    const auto fresh = Wait(*replacement);
    CHECK(fresh.Status == AssetPublicationStatus::Ready && fresh.Receipt &&
              fresh.Receipt->Key == std::string(64, 'c'),
          "logical job removal cannot expose a stale receipt from either domain");
  }
}
}

int main() {
  Sharing();
  return Report();
}
