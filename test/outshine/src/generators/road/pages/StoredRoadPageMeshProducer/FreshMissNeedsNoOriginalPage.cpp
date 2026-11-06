#include "OpenAssetStore.h"
#include "PreparedRoadPageFixture.h"
#include "RoadPageMeshProducer.h"
#include "StoredRoadPageMeshProducer.h"
#include "Check.h"
#include <array>
#include <cerrno>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <spawn.h>
#include <string>
#include <sys/wait.h>
#include <system_error>
#include <unistd.h>
#include <utility>
#ifdef __APPLE__
#include <crt_externs.h>
#endif

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;
constexpr size_t kBytesMost = size_t{1024} * 1024;

RoadPageRequest Request() {
  return {.Address = {.PlanKey = std::string(64, 'a'),
                      .MaterialKey = std::string(64, 'b'),
                      .Anchor = {},
                      .Parts = {{.Kind = RoadPagePartKind::Interval, .Index = 7}},
                      .Clustered = true},
          .Parent = {},
          .RequestKey = {},
          .NativeBytesMost = kBytesMost};
}

bool Prepare(const std::filesystem::path &directory) {
  const auto cache = OpenAssetStore(directory);
  auto pages = PreparedRoadPages::Open(cache);
  if (!cache || !pages) { return false; }
  RoadPageRecipe recipe;
  recipe.Intervals.push_back({.Edge = 7,
                              .Lane = 3,
                              .Stations = {{.EastM = 0}, {.EastM = 1024}},
                              .Surface = {.HalfWidthM = 3,
                                          .Profile = RoadProfile::Simple,
                                          .WearsLinear = {{.2f, .3f, .4f}},
                                          .Form = RibbonForm::Surface}});
  const auto request = Request();
  const RoadPageMeshProducer producer(RoadPageKey(request.Address), std::move(recipe));
  const auto fine = PrepareAndLoadRoadPage(**pages, request, &producer);
  const auto input = cache->Find(RoadPageRecipeKey(request.Address));
  CHECK(fine && input && *input && (**input).Package != fine->Record.Package &&
            (**input).ByteCount < fine->Record.ByteCount,
        "the complete input occupies a smaller independent package than the original VBO");
  return fine && input && *input && cache->Remove(RoadPageKey(request.Address)).has_value();
}

void CheckFresh(const std::filesystem::path &directory) {
  const auto cache = OpenAssetStore(directory);
  auto opened = PreparedRoadPages::Open(cache);
  CHECK(cache && opened, "the fresh child opens only the existing asset store");
  if (!cache || !opened) { return; }
  const std::shared_ptr<PreparedRoadPages> pages = std::move(*opened);
  auto request = Request();
  const auto fine = cache->Find(RoadPageKey(request.Address));
  CHECK(fine && !*fine, "the original fine payload does not exist in the child");
  request.Address.PositionErrorM = .05;
  request.Address.DisplayPolicy = RoadDisplayPolicy::StoredSurfaceEnvelope;
  request.Address.Level = 1;
  request.SweepLimits = {.StationsMost = 128, .PoseQueriesMost = 256};
  const auto read = pages->PlanRecipeRead(request.Address, kBytesMost);
  CHECK(read && *read, "independent metadata admits the stored input before body IO");
  if (!read || !*read) { return; }
  auto producer = StoredRoadPageMeshProducer::Bind(pages, request.Address, **read);
  CHECK(producer && producer->InputCapacityBytes() > (**read).BufferBytesMost,
        "stored and decoded ownership are reserved before generation");
  if (!producer) { return; }
  const auto coarse = PrepareAndLoadRoadPage(*pages, request, &*producer);
  CHECK(coarse && coarse->Page->Geometry.Mesh.Index.size() == size_t{32} * 6 + 12 &&
            !coarse->Page->Geometry.Clusters.empty(),
        "the source-only miss directly emits the stored-surface product with ready clusters");
  CHECK(cache->Remove(RoadPageRecipeKey(request.Address)).has_value(),
        "the input is evicted after the product becomes ready");
  request.SweepLimits.reset();
  const auto hit = PrepareAndLoadRoadPage(*pages, request, &*producer);
  CHECK(hit && coarse &&
            hit->Page->Geometry.Mesh.PositionM == coarse->Page->Geometry.Mesh.PositionM &&
            hit->Page->Geometry.Mesh.Index == coarse->Page->Geometry.Mesh.Index,
        "the product hit uses neither its absent input nor a sampling budget");
  CHECK(!producer->Produce(request, {}), "a real later miss detects the absent source input");
}

bool Spawn(const std::string &executable, const std::filesystem::path &directory) {
  auto command = executable;
  auto folder = directory.string();
  std::array<char *, 3> args{command.data(), folder.data(), nullptr};
#ifdef __APPLE__
  char *const *const environment = *_NSGetEnviron();
#else
  char *const *const environment = environ;
#endif
  pid_t child = 0;
  if (posix_spawn(&child, command.c_str(), nullptr, nullptr, args.data(), environment) != 0) {
    return false;
  }
  int status = 0;
  pid_t waited = 0;
  do { waited = waitpid(child, &status, 0); } while (waited < 0 && errno == EINTR);
  return waited == child && WIFEXITED(status) && WEXITSTATUS(status) == 0;
}
}

int main(int argc, const char *const *argv) {
  if (argc == 2) {
    CheckFresh(argv[1]);
    return Report();
  }
  const auto directory = std::filesystem::temp_directory_path() /
                         ("outshine-stored-road-input-" + std::to_string(getpid()));
  std::filesystem::create_directories(directory);
  CHECK(Prepare(directory), "only the parent creates and persists the complete generator input");
  CHECK(Spawn(argv[0], directory), "a fresh offline process generates the new product");
  std::error_code error;
  std::filesystem::remove_all(directory, error);
  CHECK(!error, "the private input and render packages are removed after closing");
  return Report();
}
