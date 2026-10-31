#include "AssetPipeline.h"
#include "Check.h"
#include <array>
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
const std::string kContent(64, 'a');
constexpr std::array<std::string_view, 2> kAddresses{
    "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
    "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"};

AssetRecord Record() {
  return {.Key = kContent,
          .Kind = "resolved-scope-demand-control",
          .Bounds = {.Min = {{0, 0, 0}}, .Max = {{1, 1, 1}}},
          .Package = std::string(64, 'd'),
          .ByteCount = 3,
          .Parent = {}};
}

struct Control {
  std::atomic<bool> Ready{false};
  std::atomic<unsigned> Resolved{0};
  std::array<std::atomic_size_t, 2> Builds{};
};

struct Fixture {
  std::shared_ptr<const int> ScopeA = std::make_shared<const int>(0);
  std::shared_ptr<const int> ScopeB;
  Tasks Pool{1};
  AssetPipeline Pipeline{Pool,
                         {.JobsMost = 2,
                          .ConsumersMost = 2,
                          .TicketsMost = 2,
                          .InputBytesMost = 512,
                          .ReceiptBytesMost = 2048}};
  std::shared_ptr<Control> State = std::make_shared<Control>();
  std::array<std::weak_ptr<const int>, 2> Inputs;

  explicit Fixture(bool sharedScope)
      : ScopeB(sharedScope ? ScopeA : std::make_shared<const int>(0)) {}

  [[nodiscard]] std::expected<AssetPipeline::RequestTicket, AssetPublicationError>
  Submit(const AssetPipeline::Consumer &owner, size_t index) {
    auto input = std::make_shared<const int>(static_cast<int>(index));
    Inputs[index] = input;
    return Pipeline.RequestLogical(
        owner,
        {.Address = kAddresses[index],
         .Generation = owner.Generation(),
         .InputBytes = 64,
         .ConsumerBytes = 8,
         .ReceiptBytesMost = 1024},
        [state = State, index, input = std::move(input)](
            const std::stop_token &) -> std::expected<AssetPipeline::LogicalStep, std::string> {
          state->Resolved.fetch_or(1U << index);
          return AssetPipeline::ResolvedWork{
              .Key = AssetProductKey::Parse(kContent).value_or(AssetProductKey{}),
              .Produce = [state, index, input](const std::stop_token &stop)
                  -> std::expected<std::optional<AssetProduct>, std::string> {
                if (stop.stop_requested()) { return std::unexpected("resolved work canceled"); }
                if (!state->Ready.load()) { return std::nullopt; }
                CHECK(std::cmp_equal(*input, index),
                      "the actual producer retains its selected immutable input");
                ++state->Builds[index];
                return AssetProduct(Record());
              }};
        });
  }

  bool Drain() {
    const auto barrier = Pool.Post([] {});
    if (barrier == Tasks::kNoTask || !Pool.AwaitCompletion(barrier, 5)) { return false; }
    (void)Pool.TakeCompletion(barrier);
    return true;
  }
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

bool Resolved(Fixture &fixture,
              const AssetPipeline::RequestTicket &first,
              const AssetPipeline::RequestTicket &second,
              size_t jobs) {
  const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  do {
    (void)first.Poll();
    (void)second.Poll();
    if (!fixture.Drain()) { return false; }
    const auto state = fixture.Pipeline.Counts();
    if (fixture.State->Resolved.load() == 3 && state.Jobs == jobs && state.Running == 0) {
      return true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  } while (std::chrono::steady_clock::now() < until);
  return false;
}

struct CaseOptions {
  bool SharedScope = false;
  bool CancelFirst = false;
  bool CancelBoth = false;
};

void CompleteContent(Fixture &fixture,
                     const std::array<const AssetPipeline::RequestTicket *, 2> &tickets,
                     CaseOptions options) {
  fixture.State->Ready.store(true);
  const auto right = Wait(*tickets[1]);
  const auto left = Wait(*tickets[0]);
  CHECK(right.Status == AssetPublicationStatus::Ready && right.Receipt &&
            right.Receipt->Key == kContent && !right.Receipt->RequestKey &&
            right.RequestKey == AssetRequestKey::Parse(kAddresses[1]),
        "the surviving scoped caller receives correctly bound immutable content");
  if (options.CancelFirst) {
    CHECK(left.Status == AssetPublicationStatus::Cancelled && !left.Receipt,
          "shared late completion cannot revive the obsolete generation");
  } else {
    CHECK(left.Status == AssetPublicationStatus::Ready && left.Receipt &&
              left.RequestKey == AssetRequestKey::Parse(kAddresses[0]) &&
              ((left.Receipt == right.Receipt) == options.SharedScope),
          "only callers of the same store share the physical content receipt");
  }
  CHECK(fixture.State->Builds[0].load() + fixture.State->Builds[1].load() ==
            ((options.SharedScope || options.CancelFirst) ? 1U : 2U),
        "scoped demand cancellation preserves exactly the required content builds");
}

void ScopeAndDemand(CaseOptions options) {
  Fixture fixture(options.SharedScope);
  {
    auto a = fixture.Pipeline.OpenConsumer({.Identity = fixture.ScopeA});
    auto b = fixture.Pipeline.OpenConsumer({.Identity = fixture.ScopeB});
    CHECK(a && b, "both independent scoped consumers open");
    if (!a || !b) { return; }
    auto first = fixture.Submit(*a, 0);
    auto second = fixture.Submit(*b, 1);
    CHECK(first && second, "both logical requests fit their original two admitted jobs");
    if (!first || !second) { return; }
    CHECK(Resolved(fixture, *first, *second, options.SharedScope ? 1 : 2),
          "equal content keys merge exactly within the same store scope");
    CHECK(fixture.Inputs[0].expired() != fixture.Inputs[1].expired() || !options.SharedScope,
          "joining content releases only the unused producer input after worker cleanup");
    CHECK(options.SharedScope || (!fixture.Inputs[0].expired() && !fixture.Inputs[1].expired()),
          "different stores retain their own inputs even when content digests are equal");
    if (options.CancelFirst) {
      CHECK(a->Advance({.Value = 2}),
            "the first caller advances its meaning after content resolution");
      CHECK(fixture.Drain(), "obsolete demand cancellation finishes its queued cleanup");
      const auto stale = first->Poll();
      CHECK(stale.Status == AssetPublicationStatus::Cancelled && !stale.Receipt &&
                stale.Error == AssetPublicationError::StaleGeneration &&
                stale.RequestKey == AssetRequestKey::Parse(kAddresses[0]),
            "the stale ticket keeps its original address and exposes no product");
      CHECK(!second->Poll().Finished,
            "the surviving content job remains pending until its own input barrier opens");
    }
    if (options.CancelBoth) {
      first->Cancel();
      second->Cancel();
      const auto left = Wait(*first);
      const auto right = Wait(*second);
      CHECK(left.Status == AssetPublicationStatus::Cancelled && !left.Receipt &&
                right.Status == AssetPublicationStatus::Cancelled && !right.Receipt,
            "canceling every resolved demand terminates both stores without publishing");
      CHECK(fixture.State->Builds[0].load() == 0 && fixture.State->Builds[1].load() == 0,
            "last-demand cancellation releases deferred producers before any content build");
    } else {
      CompleteContent(fixture, {&*first, &*second}, options);
    }
  }
  CHECK(fixture.Drain(), "all scoped retirement reaches actual worker completion");
  const auto final = fixture.Pipeline.Counts();
  CHECK(fixture.Inputs[0].expired() && fixture.Inputs[1].expired() && final.Jobs == 0 &&
            final.Addresses == 0 && final.Running == 0 && final.Consumers == 0 &&
            final.Tickets == 0 && final.InputBytes == 0 && final.ReceiptBytes == 0,
        "every scope and generation releases its inputs, aliases and reservations");
}
}

int main() {
  ScopeAndDemand({.SharedScope = true, .CancelFirst = false});
  ScopeAndDemand({.SharedScope = false, .CancelFirst = false});
  ScopeAndDemand({.SharedScope = true, .CancelFirst = true});
  ScopeAndDemand({.SharedScope = false, .CancelFirst = true});
  ScopeAndDemand({.SharedScope = true, .CancelBoth = true});
  ScopeAndDemand({.SharedScope = false, .CancelBoth = true});
  return Report();
}
