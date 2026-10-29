#include "AssetPipeline.h"
#include "AssetCache.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <csignal>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <spawn.h>
#include <span>
#include <sqlite3.h>
#include <stop_token>
#include <string>
#include <string_view>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <utility>
#ifdef __APPLE__
#include <crt_externs.h>
#endif

namespace {
using namespace outshine;
using namespace outshine::Test;
constexpr std::string_view kAddress =
    "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
const std::string kContent(64, 'a');
constexpr std::array<uint8_t, 4> kPayload{17, 23, 31, 47};

AssetPublication Wait(const AssetPipeline::RequestTicket &ticket) {
  const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  do {
    auto result = ticket.Poll();
    if (result.Finished) { return result; }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  } while (std::chrono::steady_clock::now() < until);
  return {};
}

int Offline(const std::string &path) {
  auto opened = AssetCache::Open(path);
  CHECK(opened.has_value(), "a fresh process opens the actual cache under a held writer");
  if (!opened) { return Report(); }
  const auto cache = std::shared_ptr<AssetCache>(std::move(*opened));
  Tasks pool(1);
  AssetPipeline pipeline(pool,
                         {.JobsMost = 1,
                          .ConsumersMost = 1,
                          .TicketsMost = 1,
                          .InputBytesMost = 16,
                          .ReceiptBytesMost = 2048});
  auto consumer = pipeline.OpenConsumer({.Identity = cache});
  CHECK(consumer.has_value(), "the lookup uses the real cache scope and shared dispatcher");
  if (!consumer) { return Report(); }
  std::atomic<size_t> lookups{0};
  auto ticket = pipeline.RequestLogical(
      *consumer,
      {.Address = kAddress, .Generation = consumer->Generation(), .ReceiptBytesMost = 2048},
      [cache, &lookups](
          const std::stop_token &) -> std::expected<std::optional<AssetRecord>, std::string> {
        ++lookups;
        auto found = cache->FindRequest(kAddress);
        if (!found) {
          if (found.error() == AssetCacheError::Busy) { return std::nullopt; }
          return std::unexpected("cached request lookup failed");
        }
        return std::move(*found);
      });
  CHECK(ticket.has_value(), "the pre-input address admits a bounded logical lookup");
  if (!ticket) { return Report(); }
  const auto result = Wait(*ticket);
  CHECK(result.Status == AssetPublicationStatus::Ready && result.Receipt &&
            result.Receipt->Key == kContent && result.Receipt->RequestKey == kAddress &&
            result.Receipt->Package != kAddress && lookups.load() == 1,
        "the persisted request returns its actual content receipt without key substitution");
  const auto loaded = cache->LoadRequest(kAddress, 1024);
  CHECK(loaded && *loaded && (**loaded).Record().Key == kContent &&
            std::ranges::equal((**loaded).Bytes(), kPayload),
        "the same alias resolves the real immutable payload without a producer");
  return Report();
}

bool Spawn(const std::string &executable, const std::string &path) {
  std::array<std::string, 3> values{executable, "--offline", path};
  std::array<char *, 4> arguments{values[0].data(), values[1].data(), values[2].data(), nullptr};
#ifdef __APPLE__
  char *const *const environment = *_NSGetEnviron();
#else
  char *const *const environment = environ;
#endif
  pid_t child = 0;
  if (posix_spawn(&child, executable.c_str(), nullptr, nullptr, arguments.data(), environment) !=
      0) {
    return false;
  }
  const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(10);
  int status = 0;
  while (std::chrono::steady_clock::now() < until) {
    const auto ended = waitpid(child, &status, WNOHANG);
    if (ended == child) { return WIFEXITED(status) && WEXITSTATUS(status) == 0; }
    if (ended < 0) { return false; }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  (void)kill(child, SIGKILL);
  (void)waitpid(child, &status, 0);
  return false;
}
}

int main(int argc, char *const *argv) {
  if (argc == 3 && std::string_view(argv[1]) == "--offline") { return Offline(argv[2]); }
  const auto directory =
      std::filesystem::temp_directory_path() /
      ("outshine-engine-logical-cache-346890006-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(directory);
  const auto path = (directory / "assets.sqlite").string();
  {
    auto opened = AssetCache::Open(path);
    CHECK(opened.has_value(), "the initial content store opens");
    if (!opened) { return Report(); }
    const AssetRecord record{.Key = kContent,
                             .Kind = "logical-cache-control",
                             .Bounds = {.Min = {{0, 0, 0}}, .Max = {{1, 1, 1}}},
                             .Package = {},
                             .ByteCount = kPayload.size(),
                             .Parent = {},
                             .RequestKey = std::string(kAddress)};
    CHECK((*opened)->Publish(std::span(&record, 1), kPayload).has_value(),
          "actual content and the early request binding persist together");
  }
  sqlite3 *writer = nullptr;
  CHECK(sqlite3_open(path.c_str(), &writer) == SQLITE_OK, "the independent writer opens");
  if (writer != nullptr) {
    CHECK(sqlite3_exec(writer, "BEGIN IMMEDIATE", nullptr, nullptr, nullptr) == SQLITE_OK,
          "a writer remains held throughout the fresh offline lookup");
    CHECK(Spawn(std::filesystem::absolute(argv[0]).string(), path),
          "a fresh logical hit and payload read need neither source work nor a write slot");
    CHECK(sqlite3_exec(writer, "ROLLBACK", nullptr, nullptr, nullptr) == SQLITE_OK,
          "the independent writer releases its transaction");
    CHECK(sqlite3_close(writer) == SQLITE_OK, "the independent writer closes");
  }
  std::filesystem::remove_all(directory);
  return Report();
}
