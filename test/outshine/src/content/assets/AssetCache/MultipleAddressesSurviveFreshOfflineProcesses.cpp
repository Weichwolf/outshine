#include "AssetGeneration.h"
#include "Check.h"
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <print>
#include <optional>
#include <utility>
#include <spawn.h>
#include <span>
#include <sqlite3.h>
#include <string>
#include <string_view>
#include <sys/wait.h>
#include <unistd.h>
#ifdef __APPLE__
#include <crt_externs.h>
#endif

namespace {
using namespace outshine;
using namespace outshine::Test;
const std::string kProduct(64, 'a');
const std::string kFirst(64, 'b');
const std::string kSecond(64, 'c');
const std::string kOther(64, 'd');
const std::string kThird(64, 'e');
constexpr std::array<uint8_t, 4> kPayload{17, 23, 31, 47};

bool Publish(AssetCache &cache,
             const std::string &key,
             std::optional<std::string> request = std::nullopt) {
  const AssetRecord record{.Key = key,
                           .Kind = "native",
                           .Bounds = {.Min = {{0, 0, 0}}, .Max = {{1, 1, 1}}},
                           .Package = {},
                           .ByteCount = kPayload.size(),
                           .Parent = {},
                           .RequestKey = std::move(request)};
  return cache.Publish(std::span(&record, 1), kPayload).has_value();
}

bool PointsTo(const AssetCache &cache, const std::string &request, const std::string &key) {
  const auto found = cache.FindRequest(request);
  return found && *found && (**found).Key == key && (**found).RequestKey == request;
}

int Offline(const std::string &path) {
  auto opened = AssetCache::Open(path);
  if (!opened) { return 2; }
  auto &cache = **opened;
  const auto a = ResolveAssetRequest(cache, kFirst, 64, {});
  const auto b = ResolveAssetRequest(cache, kSecond, 64, {});
  const auto content = cache.Load(kProduct, 64);
  const auto metadata = cache.Find(kProduct);
  const auto selected = cache.Select({.Bounds = {.Min = {{0, 0, 0}}, .Max = {{1, 1, 1}}},
                                      .Kind = {},
                                      .Level = {},
                                      .Radius = {},
                                      .Planes = {}});
  const bool valid = a && b && a->Record().RequestKey == kFirst &&
                     b->Record().RequestKey == kSecond && a->Record().Key == kProduct &&
                     b->Record().Key == kProduct && a->Bytes().size() == kPayload.size() &&
                     b->Bytes().size() == kPayload.size() && a->Bytes().back() == kPayload.back() &&
                     b->Bytes().back() == kPayload.back() && content && *content &&
                     !(**content).Record().RequestKey && metadata && *metadata &&
                     !(**metadata).RequestKey && selected && selected->size() == 1 &&
                     !selected->front().RequestKey;
  std::println("ASSET_ALIAS_CHILD valid={}", valid);
  return valid ? 0 : 3;
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
  int status = 0;
  return waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

void Faults(const std::string &path) {
  auto opened = AssetCache::Open(path);
  CHECK(opened, "native store opens for payload failure injection");
  if (!opened) { return; }
  auto &cache = **opened;
  sqlite3 *database = nullptr;
  CHECK(sqlite3_open(path.c_str(), &database) == SQLITE_OK, "independent payload injector opens");
  if (database == nullptr) { return; }
  CHECK(sqlite3_exec(database, "UPDATE packages SET crc=crc+1", nullptr, nullptr, nullptr) ==
            SQLITE_OK,
        "stored package checksum is deliberately corrupted");
  auto first = cache.LoadRequest(kFirst, 64);
  auto second = cache.LoadRequest(kSecond, 64);
  CHECK(first && !*first && second && !*second && !ResolveAssetRequest(cache, kFirst, 64, {}),
        "neither alias turns a corrupt package into an offline cache hit");
  CHECK(Publish(cache, kProduct), "repair without an alias retains both bindings");
  CHECK(sqlite3_exec(database, "DELETE FROM packages", nullptr, nullptr, nullptr) == SQLITE_OK,
        "payload is removed independently of the metadata");
  first = cache.LoadRequest(kFirst, 64);
  second = cache.LoadRequest(kSecond, 64);
  CHECK(first && !*first && second && !*second && !ResolveAssetRequest(cache, kSecond, 64, {}),
        "neither alias turns a missing package into an offline cache hit");
  CHECK(sqlite3_close(database) == SQLITE_OK, "independent payload injector closes");
  CHECK(Publish(cache, kProduct) && PointsTo(cache, kFirst, kProduct) &&
            PointsTo(cache, kSecond, kProduct),
        "repair restores the original content with both independent bindings intact");
}

void Lifecycle(const std::string &path) {
  auto opened = AssetCache::Open(path);
  CHECK(opened, "native product store reopens after the separate offline process");
  if (!opened) { return; }
  auto &cache = **opened;
  const auto lease = cache.Load(kProduct, 64);
  CHECK(Publish(cache, kOther), "a separately addressable product is published");
  const auto rebound = cache.BindRequest(kOther, kFirst);
  CHECK(rebound && *rebound && PointsTo(cache, kFirst, kOther) &&
            PointsTo(cache, kSecond, kProduct),
        "rebinding one address preserves the independent address and product");
  CHECK(Publish(cache, kProduct) && PointsTo(cache, kSecond, kProduct),
        "publication without an alias retains existing request bindings");
  CHECK(Publish(cache, kProduct, kThird) && PointsTo(cache, kSecond, kProduct) &&
            PointsTo(cache, kThird, kProduct) && PointsTo(cache, kFirst, kOther),
        "publication adds only its alias without stealing other requests");
  CHECK(cache.Remove(kProduct) && !cache.FindRequest(kSecond)->has_value() &&
            !cache.FindRequest(kThird)->has_value() && PointsTo(cache, kFirst, kOther),
        "removing one product removes all its aliases and preserves unrelated bindings");
  CHECK(lease && *lease && (**lease).Bytes().size() == kPayload.size() &&
            (**lease).Bytes().front() == kPayload.front(),
        "removing product metadata does not revoke an owned immutable payload");
}
}

int main(int argc, char **argv) {
  if (argc == 3 && std::string_view(argv[1]) == "--offline") { return Offline(argv[2]); }
  const auto directory =
      std::filesystem::temp_directory_path() /
      ("outshine-asset-aliases-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(directory);
  const auto path = (directory / "assets.sqlite").string();
  {
    auto opened = AssetCache::Open(path);
    CHECK(opened, "native product store opens");
    if (!opened) { return Report(); }
    CHECK(Publish(**opened, kProduct, kFirst),
          "first address and immutable product publish together");
    const auto bound = (*opened)->BindRequest(kProduct, kSecond);
    CHECK(bound && *bound, "second resolved address binds the same product");
  }
  CHECK(Spawn(std::filesystem::absolute(argv[0]).string(), path),
        "fresh offline process resolves both aliases without any producer or source inputs");
  Faults(path);
  Lifecycle(path);
  std::filesystem::remove_all(directory);
  return Report();
}
