#include "ImpostorFieldLoad.h"
#include "Check.h"
#include "Material.h"
#include <chrono>
#include <atomic>
#include <cstddef>
#include <expected>
#include <memory>
#include <stop_token>
#include <string>
#include <filesystem>
#include <optional>
#include <semaphore>
#include <thread>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Test;
using Views = Content::ImpostorViewAssets;
constexpr size_t kBytes = size_t{1024} * 1024;
constexpr auto kDeadline = std::chrono::seconds(5);

struct Fixture {
  Tasks Workers{1};
  AssetPipeline Pipeline{Workers,
                         {.JobsMost = 1,
                          .ConsumersMost = 1,
                          .TicketsMost = 1,
                          .InputBytesMost = 4096,
                          .ReceiptBytesMost = 4096}};
  std::shared_ptr<AssetCache> Cache;
  std::optional<AssetPipeline::Consumer> Consumer;
  std::string Key;

  explicit Fixture(const std::filesystem::path &path) {
    auto opened = AssetCache::Open(path.string());
    CHECK(opened.has_value(), "the field store opens");
    if (!opened) { return; }
    Cache = std::move(*opened);
    auto views = Metadata();
    if (!views) { return; }
    ImpostorAtlas::View view{.TowardEye = {{0, 0, 1}}, .Texels = {}, .Materials = {}};
    view.Texels.resize(64, {.Normal = {{0, 0, 1}}, .Depth = .5f, .Surface = 1});
    std::string error;
    auto field = ImpostorAtlas::Create(8, {}, 1, {Material{}}, {std::move(view)}, error);
    CHECK(field.has_value(), "the controlled CPU field is valid");
    if (!field) { return; }
    CHECK(views->PublishField({0}, *field, kBytes).has_value(),
          "a complete field precedes loading");
    Key = views->FieldKey({0});
    auto consumer = Pipeline.OpenConsumer({.Identity = Cache});
    CHECK(consumer.has_value(), "the field consumer opens");
    if (consumer) { Consumer.emplace(std::move(*consumer)); }
  }

  [[nodiscard]] std::optional<Views> Metadata(double halfExtent = 1) const {
    auto views = Views::Publish(
        *Cache,
        "field-load-1",
        {.Centre = {}, .HalfExtentM = halfExtent, .Pixels = 8, .Directions = {{{0, 0, 1}}}},
        {.Min = {{-1, -1, -1}}, .Max = {{1, 1, 1}}});
    CHECK(views.has_value(), "the immutable field metadata publishes");
    if (!views) { return std::nullopt; }
    return std::move(*views);
  }

  std::optional<AssetPipeline::RequestTicket> ReadyTicket() {
    if (!Consumer) { return std::nullopt; }
    const auto product = AssetProductKey::Parse(Key);
    if (!product) { return std::nullopt; }
    auto ticket = Pipeline.RequestResolved(*Consumer,
                                           {.Key = *product,
                                            .Generation = Consumer->Generation(),
                                            .InputBytes = 0,
                                            .ConsumerBytes = 0,
                                            .ReceiptBytesMost = 4096},
                                           [cache = Cache, key = Key](const std::stop_token &)
                                               -> std::expected<AssetRecord, std::string> {
                                             auto found = cache->Find(key);
                                             if (!found || !*found) {
                                               return std::unexpected("field receipt unavailable");
                                             }
                                             return std::move(**found);
                                           });
    CHECK(ticket.has_value(), "the field receipt request admits");
    if (!ticket) { return std::nullopt; }
    const auto until = std::chrono::steady_clock::now() + kDeadline;
    while (!ticket->Poll().Finished && std::chrono::steady_clock::now() < until) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    CHECK(ticket->Poll().Status == AssetPublicationStatus::Ready, "the field receipt completes");
    return std::move(*ticket);
  }
};

std::expected<std::shared_ptr<const ImpostorAtlas>, std::string>
Await(const ImpostorFieldLoad &load) {
  const auto until = std::chrono::steady_clock::now() + kDeadline;
  do {
    auto result = load.Poll();
    if (!result || *result) { return result; }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  } while (std::chrono::steady_clock::now() < until);
  return std::unexpected("field load deadline");
}

void Retention(const std::filesystem::path &path) {
  Fixture fixture(path);
  auto ticket = fixture.ReadyTicket();
  if (!ticket) { return; }
  std::binary_semaphore started{0};
  std::binary_semaphore release{0};
  CHECK(fixture.Workers.PostDetached([&] {
    started.release();
    release.acquire();
  }),
        "the worker can be held before field IO");
  CHECK(started.try_acquire_for(kDeadline), "the worker reaches the controlled hold");
  auto pending = ImpostorFieldLoad::Start(
      fixture.Workers, fixture.Cache, "field-load-1", {0}, kBytes, std::move(*ticket));
  CHECK(pending.has_value(), "a ready receipt starts a detached field read");
  if (!pending) {
    release.release();
    return;
  }
  std::optional<ImpostorFieldLoad> load(std::move(*pending));
  const auto waiting = load->Poll();
  CHECK(waiting && !*waiting, "polling while the worker is held performs no inline decode");
  release.release();
  auto ready = Await(*load);
  CHECK(ready && *ready && (**ready).Pixels() == 8 && (**ready).Views()[0].Texels.size() == 64,
        "only the complete decoded CPU field becomes ready");
  load.reset();
  CHECK(fixture.Pipeline.Counts().Tickets == 1,
        "a returned CPU lease retains the publication after its loader retires");
  ready = std::shared_ptr<const ImpostorAtlas>{};
  const auto until = std::chrono::steady_clock::now() + kDeadline;
  while (fixture.Pipeline.Counts().Tickets != 0 && std::chrono::steady_clock::now() < until) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  CHECK(fixture.Pipeline.Counts().Tickets == 0, "the final CPU consumer releases its ticket");
}

void BudgetAndAbandonment(const std::filesystem::path &path) {
  Fixture fixture(path);
  auto ticket = fixture.ReadyTicket();
  if (!ticket) { return; }
  CHECK(!ImpostorFieldLoad::Start(
            fixture.Workers, fixture.Cache, "field-load-1", {0}, 1, std::move(*ticket)),
        "oversized encoded fields are refused before posting IO");
  auto next = fixture.ReadyTicket();
  if (!next) { return; }
  std::binary_semaphore started{0};
  std::binary_semaphore release{0};
  std::binary_semaphore drained{0};
  CHECK(fixture.Workers.PostDetached([&] {
    started.release();
    release.acquire();
  }),
        "the lifetime control holds the worker");
  CHECK(started.try_acquire_for(kDeadline), "the held worker cannot start field IO");
  std::atomic_size_t preparations{0};
  auto pending = ImpostorFieldLoad::Start(
      fixture.Workers,
      fixture.Cache,
      "field-load-1",
      {0},
      kBytes,
      std::move(*next),
      [&preparations](const ImpostorAtlas &) -> std::expected<void, std::string> {
        ++preparations;
        return {};
      });
  CHECK(pending.has_value(), "the queued field owns its source store");
  const std::weak_ptr<AssetCache> retained = fixture.Cache;
  fixture.Cache.reset();
  pending = std::unexpected("abandoned");
  CHECK(!retained.expired(), "dropping a queued loader does not destroy its worker state");
  release.release();
  CHECK(fixture.Workers.PostDetached([&] { drained.release(); }),
        "the queue drains without waiting in the loader");
  CHECK(drained.try_acquire_for(kDeadline), "the cancelled detached field job retires");
  CHECK(retained.expired() && fixture.Pipeline.Counts().Tickets == 0 && preparations == 0,
        "abandoned work releases the store and ticket after its worker exits");
}

void StaleAndCancelled(const std::filesystem::path &path) {
  Fixture fixture(path);
  auto ticket = fixture.ReadyTicket();
  if (!ticket) { return; }
  CHECK(fixture.Metadata(2).has_value(), "source metadata changes before field IO");
  auto stale = ImpostorFieldLoad::Start(
      fixture.Workers, fixture.Cache, "field-load-1", {0}, kBytes, std::move(*ticket));
  CHECK(stale && !Await(*stale), "a stale publication cannot load a reinterpreted material view");
  stale = std::unexpected("retired");
  auto next = fixture.ReadyTicket();
  if (!next) { return; }
  auto cancelled = ImpostorFieldLoad::Start(
      fixture.Workers, fixture.Cache, "field-load-1", {0}, kBytes, std::move(*next));
  if (!cancelled) {
    CHECK(false, "the cancellation control starts");
    return;
  }
  if (!fixture.Consumer) {
    CHECK(false, "the load consumer remains available");
    return;
  }
  CHECK(fixture.Consumer->Advance({2}), "the consumer generation advances");
  CHECK(!cancelled->Poll(), "obsolete consumers receive no ready field");
  cancelled->Cancel();
  CHECK(!cancelled->Poll(), "explicit cancellation is terminal");
}

void Preparation(const std::filesystem::path &path) {
  Fixture fixture(path);
  auto ticket = fixture.ReadyTicket();
  if (!ticket) { return; }
  std::binary_semaphore entered{0};
  std::binary_semaphore release{0};
  const auto caller = std::this_thread::get_id();
  std::thread::id worker;
  size_t samples = 0;
  auto load =
      ImpostorFieldLoad::Start(fixture.Workers,
                               fixture.Cache,
                               "field-load-1",
                               {0},
                               kBytes,
                               std::move(*ticket),
                               [&](const ImpostorAtlas &field) -> std::expected<void, std::string> {
                                 worker = std::this_thread::get_id();
                                 samples = field.Views()[0].Texels.size();
                                 entered.release();
                                 if (!release.try_acquire_for(kDeadline)) {
                                   return std::unexpected("preparation hold deadline");
                                 }
                                 return {};
                               });
  const bool held = load && entered.try_acquire_for(kDeadline);
  CHECK(held, "decoded preparation reaches its worker hold");
  if (!load) { return; }
  if (!held) {
    release.release();
    const auto retired = Await(*load);
    CHECK(retired, "the delayed preparation drains after releasing its hold");
    return;
  }
  const auto pending = load->Poll();
  CHECK(pending && !*pending && worker != caller && samples == 64,
        "decoded fields remain pending until off-thread preparation finishes");
  release.release();
  auto ready = Await(*load);
  CHECK(ready && *ready, "one completed CPU lease publishes after preparation");
  load = std::unexpected("retired");
  if (ready) { ready->reset(); }
  auto next = fixture.ReadyTicket();
  if (!next) { return; }
  auto failed =
      ImpostorFieldLoad::Start(fixture.Workers,
                               fixture.Cache,
                               "field-load-1",
                               {0},
                               kBytes,
                               std::move(*next),
                               [](const ImpostorAtlas &) -> std::expected<void, std::string> {
                                 return std::unexpected("owned preparation rejection");
                               });
  if (!failed) { return; }
  const auto rejected = Await(*failed);
  CHECK(!rejected && rejected.error() == "owned preparation rejection",
        "a preparation error cannot publish a partially ready field");
}
}

int main() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("outshine-field-load-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(root);
  Retention(root / "retention.sqlite");
  StaleAndCancelled(root / "stale.sqlite");
  BudgetAndAbandonment(root / "abandoned.sqlite");
  Preparation(root / "prepared.sqlite");
  std::filesystem::remove_all(root);
  return Report();
}
