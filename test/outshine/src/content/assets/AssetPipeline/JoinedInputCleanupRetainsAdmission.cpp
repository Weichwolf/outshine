#include "AssetPipeline.h"
#include "Check.h"
#include <atomic>
#include <chrono>
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
const std::string kContent(64, 'a');
constexpr std::string_view kAddress =
    "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";

AssetRecord Record() {
  return {.Key = kContent,
          .Kind = "joined-cleanup-control",
          .Bounds = {.Min = {{0, 0, 0}}, .Max = {{1, 1, 1}}},
          .Package = std::string(64, 'd'),
          .ByteCount = 3,
          .Parent = {}};
}

struct Control {
  std::atomic<bool> Armed{false};
  std::atomic<bool> Entered{false};
  std::atomic<bool> Release{false};
  std::atomic<bool> Freed{false};
};

struct Input {
  std::shared_ptr<Control> State;

  explicit Input(std::shared_ptr<Control> state) : State(std::move(state)) {}

  ~Input() {
    if (!State->Armed.load()) {
      State->Freed.store(true);
      return;
    }
    State->Entered.store(true);
    while (!State->Release.load()) { std::this_thread::yield(); }
    State->Freed.store(true);
  }
};

struct ReleaseOnExit {
  std::shared_ptr<Control> State;

  ~ReleaseOnExit() { State->Release.store(true); }
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

bool Drain(Tasks &pool) {
  const auto barrier = pool.Post([] {});
  if (barrier == Tasks::kNoTask || !pool.AwaitCompletion(barrier, 5)) { return false; }
  (void)pool.TakeCompletion(barrier);
  return true;
}

void CleanupAdmission() {
  Tasks pool(1);
  AssetPipeline pipeline(pool,
                         {.JobsMost = 2,
                          .ConsumersMost = 2,
                          .TicketsMost = 2,
                          .InputBytesMost = 128,
                          .ReceiptBytesMost = 2048});
  const auto state = std::make_shared<Control>();
  const ReleaseOnExit release{state};
  {
    auto directOwner = pipeline.OpenConsumer();
    auto queryOwner = pipeline.OpenConsumer();
    CHECK(directOwner && queryOwner, "the content owner and logical caller open independently");
    if (!directOwner || !queryOwner) { return; }
    auto direct = pipeline.RequestResolved(
        *directOwner,
        {.Key = AssetProductKey::Parse(kContent).value_or(AssetProductKey{}),
         .Generation = directOwner->Generation(),
         .ConsumerBytes = 8,
         .ReceiptBytesMost = 1024},
        [](const std::stop_token &) -> std::expected<AssetProduct, std::string> {
          return AssetProduct(Record());
        });
    CHECK(direct.has_value(), "the direct content job is admitted first");
    if (!direct) { return; }
    const auto known = Wait(*direct);
    CHECK(known.Status == AssetPublicationStatus::Ready && known.Receipt,
          "the known immutable content is ready before discovery begins");
    auto input = std::make_shared<Input>(state);
    auto query = pipeline.RequestLogical(
        *queryOwner,
        {.Address = kAddress,
         .Generation = queryOwner->Generation(),
         .InputBytes = 64,
         .ConsumerBytes = 8,
         .ReceiptBytesMost = 1024},
        [input = std::move(input)](
            const std::stop_token &) -> std::expected<AssetPipeline::LogicalStep, std::string> {
          input->State->Armed.store(true);
          return AssetPipeline::ResolvedWork{
              .Key = AssetProductKey::Parse(kContent).value_or(AssetProductKey{}),
              .Produce = [input](const std::stop_token &)
                  -> std::expected<std::optional<AssetProduct>, std::string> {
                return std::unexpected("the joined unused producer must never run");
              }};
        });
    CHECK(query.has_value(), "the logical resolver owns a separate admitted input");
    if (!query) { return; }
    const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!state->Entered.load() && std::chrono::steady_clock::now() < until) {
      (void)query->Poll();
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    CHECK(state->Entered.load() && !state->Freed.load(),
          "the unused resolved input is paused inside its real worker destructor");
    const auto ready = query->Poll();
    CHECK(ready.Status == AssetPublicationStatus::Ready && ready.Receipt == known.Receipt &&
              ready.RequestKey == AssetRequestKey::Parse(kAddress),
          "the shared product is usable while independent discovery cleanup is still running");
    const auto held = pipeline.Counts();
    CHECK(held.Jobs == 2 && held.Running == 1 && held.InputBytes == 80 && held.ReceiptBytes == 2048,
          "joined input, worker and reservations stay admitted until actual cleanup finishes");
    state->Release.store(true);
    CHECK(Drain(pool) && state->Freed.load(),
          "the real input destructor completes before retirement");
    const auto retired = pipeline.Counts();
    CHECK(retired.Jobs == 1 && retired.Running == 0 && retired.InputBytes == 16 &&
              retired.ReceiptBytes == 1024,
          "completed cleanup releases only the joined admission and preserves both live demands");
  }
  CHECK(Drain(pool), "final consumer cleanup completes without any retained input");
  const auto final = pipeline.Counts();
  CHECK(final.Jobs == 0 && final.Addresses == 0 && final.Consumers == 0 && final.Tickets == 0 &&
            final.InputBytes == 0 && final.ReceiptBytes == 0,
        "the shared content retires after its last ticket and receipt");
}
}

int main() {
  CleanupAdmission();
  return Report();
}
