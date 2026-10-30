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
  std::atomic<bool> ContentReady{false};
  std::atomic<bool> Joined{false};
  std::atomic<size_t> Builds{0};
  std::atomic<size_t> Duplicates{0};
};

AssetPublication Wait(const AssetPipeline::RequestTicket &ticket) {
  const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  do {
    auto result = ticket.Poll();
    if (result.Finished) { return result; }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  } while (std::chrono::steady_clock::now() < until);
  return {};
}

struct Discovery {
  std::shared_ptr<Control> State;

  [[nodiscard]] std::expected<AssetPipeline::LogicalStep, std::string>
  Step(const std::stop_token &stop) const {
    if (stop.stop_requested()) { return std::unexpected("discovery canceled"); }
    if (!State->InputsReady.load()) { return std::optional<AssetRecord>{}; }
    State->Joined.store(true);
    return AssetPipeline::ResolvedWork{
        .Key = AssetProductKey::Parse(kContent).value_or(AssetProductKey{}),
        .Produce = [state = State](const std::stop_token &)
            -> std::expected<std::optional<AssetRecord>, std::string> {
          ++state->Duplicates;
          return Record();
        }};
  }
};

void SharedDiscovery() {
  Tasks pool(1);
  auto pipeline =
      std::make_shared<AssetPipeline>(pool,
                                      AssetPipeline::Limits{.JobsMost = 2,
                                                            .ConsumersMost = 4,
                                                            .TicketsMost = 4,
                                                            .InputBytesMost = 512,
                                                            .ReceiptBytesMost = 2 * kReceipt});
  const auto control = std::make_shared<Control>();
  auto a = pipeline->OpenConsumer();
  auto b = pipeline->OpenConsumer();
  auto known = pipeline->OpenConsumer();
  CHECK(a && b && known, "logical and known-content callers share one bounded dispatcher");
  if (!a || !b || !known) { return; }
  auto discovery = std::make_shared<Discovery>();
  discovery->State = control;
  const std::weak_ptr<Discovery> retained = discovery;
  auto first =
      pipeline->RequestLogical(*a,
                               {.Address = kAddress,
                                .Generation = a->Generation(),
                                .InputBytes = 128,
                                .ConsumerBytes = 8,
                                .ReceiptBytesMost = kReceipt},
                               [discovery = std::move(discovery)](const std::stop_token &stop) {
                                 return discovery->Step(stop);
                               });
  auto second = pipeline->RequestLogical(
      *b,
      {.Address = kAddress,
       .Generation = b->Generation(),
       .ConsumerBytes = 8,
       .ReceiptBytesMost = kReceipt},
      [control](const std::stop_token &) -> std::expected<std::optional<AssetRecord>, std::string> {
        ++control->Duplicates;
        auto record = Record();
        record.RequestKey = kAddress;
        return record;
      });
  auto content = pipeline->RequestResumable(
      *known,
      {.Key = AssetProductKey::Parse(kContent).value_or(AssetProductKey{}),
       .Generation = known->Generation(),
       .InputBytes = 64,
       .ConsumerBytes = 8,
       .ReceiptBytesMost = kReceipt},
      [control](const std::stop_token &) -> std::expected<std::optional<AssetRecord>, std::string> {
        if (!control->ContentReady.load()) { return std::nullopt; }
        ++control->Builds;
        return Record();
      });
  CHECK(first && second && content, "all three callers fit exactly two admitted jobs");
  if (!first || !second || !content) { return; }
  CHECK(a->Advance({.Value = 2}), "the initiating caller advances before content discovery");
  control->InputsReady.store(true);
  const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (!control->Joined.load() && std::chrono::steady_clock::now() < until) {
    (void)second->Poll();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  const auto barrier = pool.Post([] {});
  CHECK(barrier != Tasks::kNoTask && pool.AwaitCompletion(barrier, 5),
        "one worker remains available while a logical job waits on its content job");
  if (barrier != Tasks::kNoTask) { (void)pool.TakeCompletion(barrier); }
  const auto pending = pipeline->Counts();
  CHECK(control->Joined.load() && pending.Jobs == 1 && pending.Running == 0 &&
            pending.Consumers == 3 && pending.Tickets == 3 && pending.InputBytes == 88,
        "discovery hands its admitted work to the known content job and releases lookup inputs");
  control->ContentReady.store(true);
  const auto result = Wait(*second);
  const auto direct = Wait(*content);
  CHECK(result.Status == AssetPublicationStatus::Ready && result.Receipt &&
            result.Receipt->Key == kContent && !result.Receipt->RequestKey &&
            result.RequestKey == AssetRequestKey::Parse(kAddress),
        "the surviving logical demand receives the discovered actual content");
  CHECK(direct.Status == AssetPublicationStatus::Ready && direct.Receipt &&
            direct.Receipt->Key == kContent && !direct.Receipt->RequestKey && !direct.RequestKey,
        "the direct content request has no logical address or mutable record annotation");
  CHECK(result.Receipt == direct.Receipt, "logical and direct callers share one immutable receipt");
  CHECK(control->Builds.load() == 1 && control->Duplicates.load() == 0 && retained.expired(),
        "only one content producer runs and terminal discovery releases its child demand");
  CHECK(Wait(*first).Status == AssetPublicationStatus::Cancelled &&
            pipeline->Counts().Consumers == 3 && pipeline->Counts().Tickets == 3,
        "obsolete demand stays canceled and the job-owned consumer retires independently");
}
}

int main() {
  SharedDiscovery();
  return Report();
}
