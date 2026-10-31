#include "ImpostorViewJob.h"
#include "AssetPipeline.h"
#include "ImpostorAtlas.h"
#include "ImpostorSurface.h"
#include "Material.h"
#include "Check.h"
#include <atomic>
#include <chrono>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <semaphore>
#include <stop_token>
#include <string>
#include <thread>
#include <utility>
#include <type_traits>
#include <variant>

namespace {
using namespace outshine;
using namespace outshine::Test;
using Views = Content::ImpostorViewAssets;
constexpr size_t kBytes = size_t{1024} * 1024, kInput = 128, kReceipt = 2048;
constexpr auto kDeadline = std::chrono::seconds(5);
constexpr Views::Selection kSelected{.View = 0, .Mip = 1};

std::shared_ptr<AssetCache> Open(const std::filesystem::path &path) {
  auto opened = AssetCache::Open(path.string());
  CHECK(opened.has_value(), "the owned asset store opens");
  if (!opened) { return nullptr; }
  return {std::move(*opened)};
}

std::optional<Views> Metadata(AssetCache &cache, bool changed = false) {
  auto views = Views::Publish(
      cache,
      "job-source-1",
      {.Centre = {}, .HalfExtentM = changed ? 2.0 : 1.0, .Pixels = 8, .Directions = {{{0, 0, 1}}}},
      {.Min = {{-1, -1, -1}}, .Max = {{1, 1, 1}}});
  CHECK(views.has_value(), "immutable view metadata precedes source work");
  if (!views) { return std::nullopt; }
  return std::move(*views);
}

std::expected<ImpostorAtlas, std::string> FieldOf() {
  ImpostorAtlas::View view{.TowardEye = {{0, 0, 1}}, .Texels = {}, .Materials = {}};
  view.Texels.resize(64, {.Normal = {{0, 0, 1}}, .Depth = .5f, .Surface = 1});
  std::string error;
  auto atlas = ImpostorAtlas::Create(8, {}, 1, {Material{}}, {std::move(view)}, error);
  if (!atlas) { return std::unexpected(error); }
  return std::move(*atlas);
}

std::expected<Geometry, std::string> GeometryOf(Views::Selection selected) {
  auto atlas = FieldOf();
  if (!atlas) { return std::unexpected(atlas.error()); }
  auto geometry = Render::BuildFractionalImpostorSurface(*atlas, selected.View, selected.Mip);
  if (!geometry) { return std::unexpected("controlled source conversion failed"); }
  return std::move(*geometry);
}

template <bool Field>
using Product =
    std::conditional_t<Field, ImpostorViewJob::MaterialView, ImpostorViewJob::SurfaceView>;

template <bool Field> std::string Key(const Views &views) {
  if constexpr (Field) {
    return views.FieldKey({0});
  } else {
    return views.Key(kSelected);
  }
}

template <bool Field, class Factory> Product<Field> Requested(Factory factory) {
  if constexpr (Field) {
    return {.Selected = {0}, .Generate = std::move(factory)};
  } else {
    return {.Selected = kSelected, .Generate = std::move(factory)};
  }
}

template <bool Field, class Selection> auto Generated(Selection selected) {
  if constexpr (Field) {
    return FieldOf();
  } else {
    return GeometryOf(selected);
  }
}

AssetPublication Wait(const AssetPipeline::RequestTicket &ticket) {
  const auto until = std::chrono::steady_clock::now() + kDeadline;
  do {
    auto result = ticket.Poll();
    if (result.Finished || result.Status == AssetPublicationStatus::Cancelled) { return result; }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  } while (std::chrono::steady_clock::now() < until);
  return {};
}

AssetPipeline::ResolvedRequest Request(const AssetPipeline::Consumer &consumer,
                                       const std::string &key) {
  return {.Key = AssetProductKey::Parse(key).value_or(AssetProductKey{}),
          .Generation = consumer.Generation(),
          .InputBytes = kInput,
          .ConsumerBytes = 0,
          .ReceiptBytesMost = kReceipt};
}

struct Source {
  std::binary_semaphore Started{0}, Release{0};
  std::atomic<size_t> Calls{0};
  std::atomic<bool> Stopped{false}, OnWorker{false};
};

template <bool Field> void SharedRequests(const std::filesystem::path &path) {
  auto cache = Open(path);
  if (!cache) { return; }
  const auto views = Metadata(*cache);
  if (!views) { return; }
  const auto key = Key<Field>(*views);
  const auto main = std::this_thread::get_id();
  const auto source = std::make_shared<Source>();
  Tasks pool(2);
  AssetPipeline pipeline(pool,
                         {.JobsMost = 1,
                          .ConsumersMost = 2,
                          .TicketsMost = 3,
                          .InputBytesMost = kInput,
                          .ReceiptBytesMost = kReceipt});
  auto first = pipeline.OpenConsumer({.Identity = cache});
  auto second = pipeline.OpenConsumer({.Identity = cache});
  CHECK(first && second, "independent view consumers share one admitted producer");
  if (!first || !second) { return; }
  ImpostorViewJob job{.Cache = cache,
                      .Provenance = "job-source-1",
                      .ProductKey = key,
                      .BytesMost = kBytes,
                      .Product = Requested<Field>(
                          [source, main](auto selected, size_t, const std::stop_token &stop) {
                            ++source->Calls;
                            source->OnWorker = std::this_thread::get_id() != main;
                            source->Started.release();
                            source->Release.acquire();
                            source->Stopped = stop.stop_requested();
                            return Generated<Field>(selected);
                          })};
  const std::weak_ptr<AssetCache> retained = cache;
  auto a = pipeline.RequestResolved(*first, Request(*first, key), std::move(job));
  auto b = pipeline.RequestResolved(
      *second,
      Request(*second, key),
      [](const std::stop_token &) -> std::expected<AssetProduct, std::string> {
        return std::unexpected("a coalesced source must not run twice");
      });
  CHECK(a && b, "the same immutable product coalesces before a second factory is needed");
  if (!a || !b) {
    source->Release.release();
    return;
  }
  CHECK(source->Started.try_acquire_for(kDeadline), "the admitted producer starts off-thread");
  cache.reset();
  CHECK(!retained.expired(), "the queued job owns its store after the caller releases it");
  a->Cancel();
  source->Release.release();
  const auto result = Wait(*b);
  CHECK(result.Status == AssetPublicationStatus::Ready, result.Message.c_str());
  CHECK(result.Status == AssetPublicationStatus::Ready && result.Receipt &&
            result.Receipt->Key == key && source->Calls == 1 && source->OnWorker &&
            !source->Stopped,
        "one cancelled consumer preserves the other consumer's complete worker publication");
  CHECK(a->Poll().Status == AssetPublicationStatus::Cancelled,
        "the cancelled consumer cannot publish its sibling's receipt");
}

template <bool Field> void WarmAndStale(const std::filesystem::path &path) {
  const auto cache = Open(path);
  if (!cache) { return; }
  const auto opened = Views::Open(*cache, "job-source-1");
  CHECK(opened && *opened, "a fresh store session opens without source inputs");
  if (!opened || !*opened) { return; }
  ImpostorViewJob job{.Cache = cache,
                      .Provenance = "job-source-1",
                      .ProductKey = Key<Field>(**opened),
                      .BytesMost = kBytes,
                      .Product = Requested<Field>(decltype(Product<Field>::Generate){})};
  const auto warm = job({});
  CHECK(warm.has_value(),
        warm ? "a source-free warm job returns a valid stored receipt" : warm.error().c_str());
  std::stop_source cancelled;
  cancelled.request_stop();
  CHECK(!job(cancelled.get_token()), "a cancelled job returns no publication receipt");
  size_t calls = 0;
  std::get<Product<Field>>(job.Product).Generate =
      [&](auto selected, size_t, const std::stop_token &) {
        ++calls;
        return Generated<Field>(selected);
      };
  CHECK(Metadata(*cache, true).has_value(), "a new metadata revision changes product identity");
  CHECK(!job({}) && calls == 0,
        "a queued old key cannot reinterpret new metadata or acquire stale source products");
}

template <bool Field> void CancelLastConsumer(const std::filesystem::path &path) {
  const auto cache = Open(path);
  if (!cache) { return; }
  const auto views = Metadata(*cache);
  if (!views) { return; }
  const auto key = Key<Field>(*views);
  const auto source = std::make_shared<Source>();
  Tasks pool(1);
  AssetPipeline pipeline(pool,
                         {.JobsMost = 1,
                          .ConsumersMost = 1,
                          .TicketsMost = 1,
                          .InputBytesMost = kInput,
                          .ReceiptBytesMost = kReceipt});
  auto consumer = pipeline.OpenConsumer({.Identity = cache});
  CHECK(consumer.has_value(), "a bounded source consumer opens");
  if (!consumer) { return; }
  ImpostorViewJob job{
      .Cache = cache,
      .Provenance = "job-source-1",
      .ProductKey = key,
      .BytesMost = kBytes,
      .Product = Requested<Field>([source](auto selected, size_t, const std::stop_token &stop) {
        ++source->Calls;
        source->Started.release();
        source->Release.acquire();
        source->Stopped = stop.stop_requested();
        return Generated<Field>(selected);
      })};
  auto rejected = Request(*consumer, key);
  rejected.InputBytes = kInput + 1;
  CHECK(!pipeline.RequestResolved(*consumer, rejected, job) && source->Calls == 0,
        "refused admission starts no source acquisition");
  auto ticket = pipeline.RequestResolved(*consumer, Request(*consumer, key), std::move(job));
  CHECK(ticket.has_value(), "the admitted source job queues");
  if (!ticket) {
    source->Release.release();
    return;
  }
  CHECK(source->Started.try_acquire_for(kDeadline), "the worker reaches source production");
  ticket->Cancel();
  source->Release.release();
  const auto until = std::chrono::steady_clock::now() + kDeadline;
  while (pipeline.Counts().Running != 0 && std::chrono::steady_clock::now() < until) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  const auto record = cache->Find(key);
  CHECK(pipeline.Counts().Running == 0 && source->Calls == 1 && source->Stopped && record &&
            !*record,
        "cancelling the last consumer reaches the factory and prevents partial publication");
}
}

int main() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("outshine-view-jobs-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(root);
  const auto path = root / "assets.sqlite";
  SharedRequests<false>(path);
  WarmAndStale<false>(path);
  CancelLastConsumer<false>(root / "cancelled.sqlite");
  const auto fields = root / "fields.sqlite";
  SharedRequests<true>(fields);
  WarmAndStale<true>(fields);
  CancelLastConsumer<true>(root / "cancelled-fields.sqlite");
  std::filesystem::remove_all(root);
  return Report();
}
