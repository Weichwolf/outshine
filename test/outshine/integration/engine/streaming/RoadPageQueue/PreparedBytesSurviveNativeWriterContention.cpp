#include "test/harness/engine/RoadPublicationContention.h"
#include "Check.h"
#include "ControlledRoadPageProducer.h"
#include <cstddef>
#include <expected>
#include <stop_token>
#include <string>
#include <utility>
#include <variant>

namespace {
using namespace outshine;
using namespace outshine::Test;
using namespace outshine::Test::RoadPublication;

void CheckColdAndWarm() {
  Fixture fixture;
  CHECK(fixture.Open(), "native continuation opens its own SQLite asset store");
  if (!fixture.Cache) { return; }
  size_t cooks = 0;
  const auto producer =
      RoadProducer([&cooks](const Generators::RoadPageRequest &, const std::stop_token &) {
        ++cooks;
        return Triangle();
      });
  const auto request = Request('a', 'c');
  auto write = Pages::PreparePage(request);
  CHECK(write.has_value(), "valid bounded native page inputs create an owning continuation");
  if (!write) { return; }
  fixture.Hold();
  auto first = fixture.Cache->Prepare(*write, producer.get());
  CHECK(!first && Busy(first.error()) && write->PublicationBytes() > 0 && !write->NeedsProducer() &&
            cooks == 1,
        "the first Busy retains prepared native bytes after exactly one cook");
  const auto capacity = write->PublicationBytes();
  const auto metadataReads = write->Costs().MetadataReads;
  for (size_t step = 0; step < 5; ++step) {
    const auto busy = fixture.Cache->Prepare(*write);
    CHECK(!busy && Busy(busy.error()) && write->PublicationBytes() == capacity && cooks == 1 &&
              write->Costs().Preparations == 1 && write->Costs().MetadataReads == metadataReads &&
              write->Costs().Publications == step + 2,
          "later Busy attempts reuse the identical package without the original producer");
  }
  fixture.Release();
  const auto ready = fixture.Cache->Prepare(*write);
  CHECK(ready && ready->RequestKey == request.RequestKey && write->PublicationBytes() == 0,
        "successful publication binds atomically and releases the prepared package");
  CheckGeometry(*fixture.Cache, request);
  auto alias = request;
  alias.RequestKey.assign(64, 'd');
  auto warm = Pages::PreparePage(alias);
  CHECK(warm.has_value(), "an independent warm binding owns its continuation");
  if (!warm) { return; }
  fixture.Hold();
  const auto pending = fixture.Cache->Prepare(*warm);
  CHECK(!pending && Busy(pending.error()) && !warm->NeedsProducer() &&
            warm->PublicationBytes() == 0,
        "warm alias contention retains only validated metadata and needs no producer or package");
  const auto repeated = fixture.Cache->Prepare(*warm);
  CHECK(!repeated && Busy(repeated.error()) && cooks == 1 && warm->Costs().MetadataReads == 1 &&
            warm->Costs().Preparations == 0 && warm->Costs().Publications == 0,
        "warm retries keep their metadata and repeat neither lookup, generation nor publication");
  fixture.Release();
  const auto bound = fixture.Cache->Prepare(*warm);
  CHECK(bound && bound->RequestKey == alias.RequestKey && ready && bound->Package == ready->Package,
        "warm completion returns its own alias to the original immutable package");
  CheckGeometry(*fixture.Cache, alias);
  Note("held native publication capacity", static_cast<double>(capacity), "bytes");
}

void CheckCancelAndPermanentFailure() {
  Fixture fixture;
  CHECK(fixture.Open(), "cancellation and permanent errors use a private native store");
  if (!fixture.Cache) { return; }
  const auto producer = RoadProducer(
      [](const Generators::RoadPageRequest &, const std::stop_token &) { return Triangle(); });
  const auto request = Request();
  auto write = Pages::PreparePage(request);
  if (!write) {
    CHECK(false, "cancelable page request validates");
    return;
  }
  fixture.Hold();
  const auto pending = fixture.Cache->Prepare(*write, producer.get());
  CHECK(!pending && Busy(pending.error()),
        "cancellation starts from a real prepared pending package");
  std::stop_source stop;
  (void)stop.request_stop();
  const auto canceled = fixture.Cache->Prepare(*write, nullptr, stop.get_token());
  CHECK(!canceled && !Busy(canceled.error()),
        "a stopped continuation returns a terminal cancellation");
  write = std::unexpected(std::string("released canceled page"));
  fixture.Release();
  const auto absent = fixture.Cache->Load(request.Address, kBytesMost);
  CHECK(absent && !*absent, "cancellation never publishes the retained cold package later");
  CHECK(sqlite3_exec(fixture.Writer.get(),
                     "CREATE TRIGGER reject_page BEFORE INSERT ON assets BEGIN "
                     "SELECT RAISE(ABORT,'controlled permanent native publication failure'); END;",
                     nullptr,
                     nullptr,
                     nullptr) == SQLITE_OK,
        "the fixture installs a permanent native constraint independent of Busy");
  auto permanent = Pages::PreparePage(request);
  if (!permanent) {
    CHECK(false, "permanent failure request validates");
    return;
  }
  const auto failed = fixture.Cache->Prepare(*permanent, producer.get());
  CHECK(
      !failed && !Busy(failed.error()) && std::get_if<AssetCacheError>(&failed.error()) != nullptr,
      "permanent SQLite publication failure retains its typed classification instead of retrying");
}
}

int main() {
  CheckColdAndWarm();
  CheckCancelAndPermanentFailure();
  return outshine::Test::Report();
}
