#ifndef OUTSHINE_TEST_ASSETCACHE_RUNTIMEFIXTURE_H
#define OUTSHINE_TEST_ASSETCACHE_RUNTIMEFIXTURE_H
#include "AssetCache.h"
#include "Check.h"
#include "JobAdmission.h"
#include "Tasks.h"
#include <chrono>
#include <expected>
#include <filesystem>
#include <optional>
#include <semaphore>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>

namespace AssetRuntimeFixture {
using namespace outshine;
using namespace outshine::Test;
using namespace std::chrono_literals;
constexpr std::string_view kMissing =
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";

struct Directory {
  const std::filesystem::path Path =
      std::filesystem::temp_directory_path() /
      ("outshine-asset-runtime-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));

  ~Directory() {
    std::error_code error;
    std::filesystem::remove_all(Path, error);
  }
};

inline std::expected<std::optional<AssetRecord>, AssetCacheError> AwaitFind(Tasks &pool,
                                                                            AssetCache &cache) {
  const auto until = std::chrono::steady_clock::now() + 5s;
  for (;;) {
    std::expected<std::optional<AssetRecord>, AssetCacheError> result =
        std::unexpected(AssetCacheError::Storage);
    std::binary_semaphore done(0);
    const bool posted = pool.PostDetached([&] {
      result = cache.Find(kMissing);
      done.release();
    });
    CHECK(posted, "the worker accepts actual cache IO");
    if (!posted) { return result; }
    const bool completed = done.try_acquire_for(5s);
    CHECK(completed, "the actual cache operation finishes within its guard");
    if (!completed) { done.acquire(); }
    if (result || result.error() != AssetCacheError::Busy ||
        std::chrono::steady_clock::now() >= until) {
      return result;
    }
    std::this_thread::yield();
  }
}
}
#endif
