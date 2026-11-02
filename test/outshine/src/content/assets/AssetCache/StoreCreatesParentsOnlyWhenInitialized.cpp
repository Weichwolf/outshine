#include "RuntimeFixture.h"
#include "AssetCache.h"
#include "Check.h"
#include "JobAdmission.h"
#include "Tasks.h"
#include <expected>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const AssetRuntimeFixture::Directory directory;
  const auto directPath = directory.Path / "direct" / "nested" / "assets.sqlite";
  CHECK(!std::filesystem::exists(directPath.parent_path()),
        "synchronous initialization starts without an existing store directory");
  const auto direct = AssetCache::Open(directPath.string());
  CHECK(direct && std::filesystem::exists(directPath),
        "the generic store owns parent creation for synchronous initialization");

  Tasks worker(1);
  const auto connections = std::make_shared<JobAdmission>(1);
  const auto deferredPath = directory.Path / "deferred" / "nested" / "assets.sqlite";
  const auto deferred =
      AssetCache::Open(deferredPath.string(), {.Workers = &worker, .Connections = connections});
  CHECK(deferred && !std::filesystem::exists(deferredPath.parent_path()) &&
            connections->Active() == 0,
        "binding a deferred store creates no directories or SQLite connections");
  if (deferred) {
    const auto found = AssetRuntimeFixture::AwaitFind(worker, **deferred);
    CHECK(found && !*found && std::filesystem::exists(deferredPath) && connections->Active() == 1,
          "first admitted worker IO creates and initializes the deferred store");
  }

  const auto obstacle = directory.Path / "ordinary-file";
  {
    std::ofstream file(obstacle);
    file << "not a directory";
    CHECK(file.good(), "the invalid store parent is a real ordinary file");
  }
  const auto rejected = AssetCache::Open((obstacle / "assets.sqlite").string());
  CHECK(!rejected && rejected.error() == AssetCacheError::Storage &&
            std::filesystem::is_regular_file(obstacle),
        "an invalid parent fails without replacing caller-owned filesystem content");
  return Report();
}
