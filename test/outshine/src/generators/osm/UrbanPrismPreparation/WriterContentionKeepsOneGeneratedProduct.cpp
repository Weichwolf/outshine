#include "PrismPreparationFixture.h"
#include "Check.h"
#include "Sha256.h"
#include <atomic>
#include <chrono>
#include <cstddef>
#include <expected>
#include <memory>
#include <optional>
#include <sqlite3.h>
#include <stop_token>
#include <string>
#include <thread>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  using namespace outshine::Generators::Osm;
  namespace Fixture = PrismPreparationFixture;
  const Fixture::Cache directory;
  Tasks pool(1);
  const auto store = directory.Store();
  if (!store) { return Report(); }
  sqlite3 *raw = nullptr;
  const int opened = sqlite3_open(directory.Database().c_str(), &raw);
  const std::unique_ptr<sqlite3, decltype(&sqlite3_close)> writer(raw, sqlite3_close);
  CHECK(opened == SQLITE_OK, "an independent writer opens the actual asset database");
  if (opened != SQLITE_OK) { return Report(); }
  const int held = sqlite3_exec(writer.get(), "BEGIN IMMEDIATE", nullptr, nullptr, nullptr);
  CHECK(held == SQLITE_OK, "the external writer holds publication, not cached product reads");
  if (held != SQLITE_OK) { return Report(); }
  const auto request = Sha256Hex("prism/shared store/writer contention");
  UrbanPrismPreparation job(pool, store, request);
  size_t captures = 0;
  std::atomic_uint generations{0};
  const UrbanPrismPreparation::Capture capture =
      [&](const std::stop_token &) -> std::optional<UrbanPrismPreparation::Captured> {
    ++captures;
    return AssetFactory{[&](size_t most) {
      ++generations;
      return PackageUrbanPrisms(Fixture::Product(), request, {.BytesMost = most});
    }};
  };
  const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  auto heldUntil = until;
  bool generated = false;
  bool failed = false;
  bool early = false;
  while (std::chrono::steady_clock::now() < heldUntil) {
    const auto result = job.Advance(capture);
    failed = !result;
    early = result && *result;
    if (failed || early) { break; }
    if (!generated && generations == 1) {
      generated = true;
      heldUntil = std::chrono::steady_clock::now() + std::chrono::milliseconds(40);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  CHECK(!failed && !early && captures == 1 && generations == 1,
        "writer contention keeps exactly one generated product pending without permanent failure");
  CHECK(sqlite3_exec(writer.get(), "ROLLBACK", nullptr, nullptr, nullptr) == SQLITE_OK,
        "the external writer releases package admission");
  const auto restored = Fixture::Await(job, capture);
  CHECK(restored && *restored && captures == 1 && generations == 1,
        "publication resumes from the original product after writer release without regeneration");
  return Report();
}
