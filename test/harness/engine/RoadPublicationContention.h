#ifndef OUTSHINE_TEST_ENGINE_ROADPUBLICATIONCONTENTION_H
#define OUTSHINE_TEST_ENGINE_ROADPUBLICATIONCONTENTION_H
#include "PreparedRoadPages.h"
#include "ControlledRoadPageProducer.h"
#include "RoadPublicationFixture.h"
#include "Check.h"
#include <chrono>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <memory>
#include <sqlite3.h>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

namespace outshine::Test::RoadPublication {
using Pages = Generators::PreparedRoadPages;
constexpr size_t kBytesMost = 4096;
constexpr auto kDeadline = std::chrono::seconds(5);

inline Generators::RoadPageRequest Request(char plan = 'a', char alias = 0) {
  return {.Address = {.PlanKey = std::string(64, plan),
                      .MaterialKey = std::string(64, 'b'),
                      .Anchor = {.LongitudeDeg = 8, .LatitudeDeg = 49},
                      .Parts = {{.Kind = Generators::RoadPagePartKind::Interval, .Index = 7}}},
          .Parent = {},
          .RequestKey = alias != 0 ? std::string(64, alias) : std::string{},
          .NativeBytesMost = kBytesMost};
}

inline Generators::RoadPageGeometry Triangle() {
  return {.Mesh = {.PositionM = {0, 0, 0, 1, 0, 0, 0, 0, -1},
                   .NormalM = {0, 1, 0, 0, 1, 0, 0, 1, 0},
                   .ColourRgba = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
                   .Index = {0, 1, 2}},
          .Swept = {},
          .Diagnostics = {},
          .Parts = {{.Kind = Generators::RoadPagePartKind::Interval, .Index = 7}}};
}

struct Fixture {
  std::filesystem::path Directory =
      std::filesystem::temp_directory_path() /
      ("outshine-road-publication-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  Tasks Pool{1};
  std::shared_ptr<Pages> Cache;
  std::shared_ptr<AssetPipeline> Pipeline;
  std::unique_ptr<sqlite3, decltype(&sqlite3_close)> Writer{nullptr, sqlite3_close};

  ~Fixture() {
    Barrier();
    Writer.reset();
    Pipeline.reset();
    Cache.reset();
    std::error_code error;
    std::filesystem::remove_all(Directory, error);
    CHECK(!error, "the isolated road cache is removed after publication owners end");
  }

  bool Open() {
    std::filesystem::create_directories(Directory);
    auto opened = Pages::Open(Directory.string());
    if (!opened) { return false; }
    Cache = std::move(*opened);
    Pipeline = RoadPublications(Pool);
    sqlite3 *connection = nullptr;
    const int status = sqlite3_open((Directory / "assets.sqlite").c_str(), &connection);
    Writer.reset(connection);
    return status == SQLITE_OK;
  }

  void Barrier() {
    const auto barrier = Pool.Post([] {});
    CHECK(barrier != Tasks::kNoTask && Pool.AwaitCompletion(barrier, 5),
          "unrelated work completes on the same single worker despite the native writer lock");
    if (barrier != Tasks::kNoTask) { (void)Pool.TakeCompletion(barrier); }
  }

  void Hold() const {
    CHECK(sqlite3_exec(Writer.get(), "BEGIN IMMEDIATE", nullptr, nullptr, nullptr) == SQLITE_OK,
          "an independent SQLite connection holds the native cache writer");
  }

  void Release() const {
    CHECK(sqlite3_exec(Writer.get(), "ROLLBACK", nullptr, nullptr, nullptr) == SQLITE_OK,
          "the independent writer releases publication admission");
  }
};

inline bool Busy(const Pages::Failure &failure) {
  const auto *cache = std::get_if<AssetCacheError>(&failure);
  return cache != nullptr && *cache == AssetCacheError::Busy;
}

inline const AssetRecord *Record(const std::vector<RoadPageQueue::Completed> &done) {
  if (done.size() != 1 || !done.front().Result || !*done.front().Result) { return nullptr; }
  const auto *receipt = std::get_if<RoadPageQueue::Receipt>(&**done.front().Result);
  return receipt != nullptr && *receipt ? receipt->get() : nullptr;
}

inline std::vector<RoadPageQueue::Completed> Await(RoadPageQueue &queue) {
  const auto until = std::chrono::steady_clock::now() + kDeadline;
  do {
    auto done = queue.Collect();
    if (!done.empty()) { return done; }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  } while (std::chrono::steady_clock::now() < until);
  return {};
}

inline void CheckGeometry(Pages &cache, const Generators::RoadPageRequest &request) {
  const auto loaded = request.RequestKey.empty()
                          ? cache.Load(request.Address, kBytesMost)
                          : cache.LoadRequest(request.RequestKey, kBytesMost);
  const auto expected = Triangle();
  CHECK(loaded && *loaded && (**loaded).Page->Geometry.Mesh.PositionM == expected.Mesh.PositionM &&
            (**loaded).Page->Geometry.Mesh.NormalM == expected.Mesh.NormalM &&
            (**loaded).Page->Geometry.Mesh.ColourRgba == expected.Mesh.ColourRgba &&
            (**loaded).Page->Geometry.Mesh.Index == expected.Mesh.Index &&
            (**loaded).Page->Geometry.Parts == expected.Parts,
        "the native consumer restores every original mesh array and part identity");
}
}
#endif
