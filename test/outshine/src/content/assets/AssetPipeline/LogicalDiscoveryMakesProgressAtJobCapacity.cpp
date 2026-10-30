#include "AssetPipeline.h"
#include "Check.h"
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <print>
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
constexpr std::array<std::string_view, 2> kAddresses{
    "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
    "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"};
const std::string kContent(64, 'a');
constexpr size_t kReceipt = 1024;

AssetRecord Record() {
  return {.Key = kContent,
          .Kind = "discovered-content-control",
          .Bounds = {.Min = {{0, 0, 0}}, .Max = {{1, 1, 1}}},
          .Package = std::string(64, 'd'),
          .ByteCount = 3,
          .Parent = {}};
}

struct Control {
  std::atomic<bool> InputsReady{false};
  std::atomic<unsigned> Seen{0}, Joined{0};
  std::atomic<size_t> Builds{0};
};

struct Discovery {
  std::shared_ptr<Control> State;
  unsigned Bit = 0;

  [[nodiscard]] std::expected<AssetPipeline::LogicalStep, std::string>
  Step(const std::stop_token &stop) const {
    if (stop.stop_requested()) { return std::unexpected("discovery canceled"); }
    State->Seen.fetch_or(Bit);
    if (!State->InputsReady.load()) { return std::optional<AssetRecord>{}; }
    State->Joined.fetch_or(Bit);
    return AssetPipeline::ResolvedWork{
        .Key = AssetProductKey::Parse(kContent).value_or(AssetProductKey{}),
        .Produce = [state = State](const std::stop_token &)
            -> std::expected<std::optional<AssetRecord>, std::string> {
          if (state->Joined.load() != 3) { return std::nullopt; }
          ++state->Builds;
          return Record();
        }};
  }
};

bool WaitBoth(const AssetPipeline::RequestTicket &first,
              const AssetPipeline::RequestTicket &second,
              std::array<AssetPublication, 2> &results) {
  const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  do {
    results = {first.Poll(), second.Poll()};
    if (results[0].Finished && results[1].Finished) { return true; }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  } while (std::chrono::steady_clock::now() < until);
  return false;
}

bool Drain(Tasks &pool) {
  const auto barrier = pool.Post([] {});
  if (barrier == Tasks::kNoTask || !pool.AwaitCompletion(barrier, 5)) { return false; }
  (void)pool.TakeCompletion(barrier);
  return true;
}

void Capacity(size_t jobsMost) {
  Tasks pool(1);
  const auto pipeline =
      std::make_shared<AssetPipeline>(pool,
                                      AssetPipeline::Limits{.JobsMost = jobsMost,
                                                            .ConsumersMost = 4,
                                                            .TicketsMost = 4,
                                                            .InputBytesMost = 1024,
                                                            .ReceiptBytesMost = 4 * kReceipt});
  const auto state = std::make_shared<Control>();
  std::array<std::weak_ptr<Discovery>, 2> retained;
  {
    auto a = pipeline->OpenConsumer();
    auto b = pipeline->OpenConsumer();
    CHECK(a && b, "two logical callers fit the declared consumer budget");
    if (!a || !b) { return; }
    const auto submit = [&](const AssetPipeline::Consumer &owner, size_t index) {
      auto discovery = std::make_shared<Discovery>();
      discovery->State = state;
      discovery->Bit = 1U << index;
      retained[index] = discovery;
      return pipeline->RequestLogical(
          owner,
          {.Address = kAddresses[index],
           .Generation = owner.Generation(),
           .InputBytes = 64,
           .ConsumerBytes = 8,
           .ReceiptBytesMost = kReceipt},
          [discovery = std::move(discovery)](const std::stop_token &stop) {
            return discovery->Step(stop);
          });
    };
    auto first = submit(*a, 0);
    auto second = submit(*b, 1);
    CHECK(first && second, "both logical discoveries are admitted before their content is known");
    if (!first || !second) { return; }
    CHECK(Drain(pool) && state->Seen.load() == 3 && pipeline->Counts().Jobs == 2,
          "both distinct logical jobs wait on inputs and leave the only worker available");
    state->InputsReady.store(true);
    std::array<AssetPublication, 2> results;
    const bool complete = WaitBoth(*first, *second, results);
    const auto live = pipeline->Counts();
    std::println("DISCOVERY_CAPACITY jobs_most={} jobs={} finished={} builds={}",
                 jobsMost,
                 live.Jobs,
                 complete ? 2U : 0U,
                 state->Builds.load());
    CHECK(complete && state->Builds.load() == 1,
          "accepted logical jobs discover new shared content and finish with exactly one build");
    if (complete) {
      CHECK(results[0].Receipt == results[1].Receipt,
            "different logical addresses share exactly one immutable content receipt");
      for (size_t index = 0; index < results.size(); ++index) {
        const auto &result = results[index];
        CHECK(result.Status == AssetPublicationStatus::Ready && result.Receipt &&
                  result.Receipt->Key == kContent && !result.Receipt->RequestKey &&
                  result.RequestKey == AssetRequestKey::Parse(kAddresses[index]),
              "each independent logical demand receives the actual content and its own address");
      }
    }
    CHECK(live.Jobs <= jobsMost && live.InputBytes <= 1024 && live.ReceiptBytes <= 4 * kReceipt &&
              Drain(pool),
          "discovery preserves its declared budgets and never occupies a waiting worker");
    results = {};
    first->Cancel();
    second->Cancel();
    CHECK(WaitBoth(*first, *second, results), "canceled accepted discoveries terminate promptly");
  }
  CHECK(Drain(pool), "all terminal cleanup runs before inspecting the shared dispatcher");
  const auto final = pipeline->Counts();
  CHECK(retained[0].expired() && retained[1].expired() && final.Jobs == 0 && final.Addresses == 0 &&
            final.Running == 0 && final.Consumers == 0 && final.Tickets == 0 &&
            final.InputBytes == 0 && final.ReceiptBytes == 0,
        "terminal discoveries release owners, tickets, reservations and their retained inputs");
}
}

int main() {
  Capacity(3);
  Capacity(2);
  return Report();
}
