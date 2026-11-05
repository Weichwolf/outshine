#include "StoredTransportRoadPageProducer.h"
#include "PreparedRoadPageFixture.h"
#include "TransportPlanFixture.h"
#include "OpenAssetStore.h"
#include "Check.h"
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <memory>
#include <spawn.h>
#include <stop_token>
#include <string>
#include <string_view>
#include <sys/wait.h>
#include <system_error>
#include <unistd.h>
#include <utility>
#include <vector>
#ifdef __APPLE__
#include <crt_externs.h>
#endif

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;
constexpr size_t kPageBytes = size_t{1024} * 1024;
const TransportPlanDefinition kDefinition{
    .Core = {.Level = 16, .X = kFixtureCoreX, .Y = kFixtureCoreY}};
const std::vector<RoadPagePartAppearance> kParts{
    {.Part = {.Kind = RoadPagePartKind::Interval, .Index = 1},
     .WearsLinear = {{0.2f, 0.3f, 0.4f}}}};

bool Prepare(const std::filesystem::path &directory) {
  const auto cache = OpenAssetStore(directory);
  auto plans = PreparedTransportPlans::Open(cache);
  const auto plan = TransportPlanFixture();
  if (!cache || !plans || !plan) { return false; }
  const auto stored = (*plans)->StoreComplete(
      *plan, {.Min = {{-1, -1, -1}}, .Max = {{1, 1, 1}}}, kTransportFixtureLimits);
  if (!stored) { return false; }
  const auto read = cache->PlanRead(stored->Key, kTransportFixtureLimits.PackageBytesMost);
  if (!read || !*read) { return false; }
  std::ofstream metadata(directory / "request.txt");
  metadata << std::setprecision(std::numeric_limits<double>::max_digits10) << stored->Key << '\n'
           << plan->Anchor.LongitudeDeg << ' ' << plan->Anchor.LatitudeDeg << '\n'
           << (**read).BufferBytesMost << '\n';
  for (const auto byte : plan->InputBinding) { metadata << static_cast<unsigned>(byte) << ' '; }
  return metadata.good();
}

void CheckMissAndHit(const std::filesystem::path &directory) {
  std::array<uint8_t, 32> inputs{};
  RoadPageRequest request{
      .Address = {.PlanKey = {},
                  .MaterialKey = std::string(64, 'b'),
                  .MaterialIndex = 0,
                  .Anchor = {},
                  .Level = 0,
                  .Parts = {kParts[0].Part},
                  .Appearance = RoadPageAppearanceV2{RoadPageAppearanceKey(kParts)},
                  .UpperModelErrorM = 1},
      .Parent = {},
      .RequestKey = {},
      .NativeBytesMost = kPageBytes,
      .SweepLimits = RoadDisplayLimits{.StationsMost = 8, .PoseQueriesMost = 256}};
  StoredTransportRoadPageLimits limits{.Plan = kTransportFixtureLimits};
  std::ifstream metadata(directory / "request.txt");
  metadata >> request.Address.PlanKey >> request.Address.Anchor.LongitudeDeg >>
      request.Address.Anchor.LatitudeDeg >> limits.ReadBufferBytesMost;
  for (auto &byte : inputs) {
    unsigned value = 0;
    metadata >> value;
    CHECK(value <= UINT8_MAX, "the private request descriptor contains byte-valued input binding");
    byte = static_cast<uint8_t>(value);
  }
  CHECK(metadata.good(), "the child reads only request metadata, never the source fixture");
  const auto cache = OpenAssetStore(directory);
  auto openedPlans = PreparedTransportPlans::Open(cache);
  auto pages = PreparedRoadPages::Open(cache);
  CHECK(openedPlans && pages, "the child opens the same native asset store in a fresh process");
  if (!openedPlans || !pages) { return; }
  const std::shared_ptr<PreparedTransportPlans> plans = std::move(*openedPlans);
  auto smallLimits = limits;
  smallLimits.ReadBufferBytesMost = 1;
  const auto small = StoredTransportRoadPageProducer::Bind(
      plans, kDefinition, inputs, request.Address, kParts, smallLimits);
  const auto refused =
      small ? (*pages)->Prepare(request, &*small)
            : std::expected<AssetRecord, std::string>(std::unexpected("no producer"));
  CHECK(!refused && refused.error() == "stored road plan exceeds its input capacity",
        "insufficient read memory fails before publishing any road page");
  const auto absent = (*pages)->Load(request.Address, kPageBytes);
  CHECK(absent && !*absent, "read admission failure leaves no replacement page in the cache");
  const auto producer = StoredTransportRoadPageProducer::Bind(
      plans, kDefinition, inputs, request.Address, kParts, limits);
  CHECK(producer && producer->InputCapacityBytes() >=
                        limits.ReadBufferBytesMost + limits.Plan.DecodedCapacityMost,
        "deferred IO and decoded input capacities are reserved before loading the plan");
  if (!producer) { return; }
  std::stop_source canceled;
  canceled.request_stop();
  CHECK(!producer->Produce(request, canceled.get_token()), "canceled misses do not read the plan");
  const auto built = PrepareAndLoadRoadPage(**pages, request, &*producer);
  CHECK(built && !built->Page->Geometry.Mesh.Index.empty() &&
            built->Page->Address.UpperModelErrorM == 1,
        "the fresh process directly builds the display miss from the saved functional plan");
  CHECK(cache->Remove(request.Address.PlanKey).has_value(),
        "the functional plan is evicted to expose accidental reads on a page hit");
  const auto rebound = StoredTransportRoadPageProducer::Bind(
      plans, kDefinition, inputs, request.Address, kParts, limits);
  CHECK(rebound.has_value(), "binding a deferred producer never loads even an absent plan");
  request.SweepLimits.reset();
  const auto hit = PrepareAndLoadRoadPage(**pages, request, rebound ? &*rebound : nullptr);
  CHECK(hit && built &&
            hit->Page->Geometry.Mesh.PositionM == built->Page->Geometry.Mesh.PositionM &&
            hit->Page->Geometry.Mesh.Index == built->Page->Geometry.Mesh.Index,
        "the native page hit needs neither its evicted plan nor sampling work");
  request.SweepLimits = RoadDisplayLimits{.StationsMost = 8, .PoseQueriesMost = 256};
  CHECK(!producer->Produce(request, {}), "a genuine later producer call detects the absent plan");
}

bool Spawn(const std::string &executable, const std::string &directory) {
  std::array<std::string, 3> values{executable, "--offline", directory};
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
  pid_t waited = 0;
  do { waited = waitpid(child, &status, 0); } while (waited < 0 && errno == EINTR);
  return waited == child && WIFEXITED(status) && WEXITSTATUS(status) == 0;
}
}

int main(int argc, char *const *argv) {
  if (argc == 3 && std::string_view(argv[1]) == "--offline") {
    CheckMissAndHit(argv[2]);
    return Report();
  }
  const auto directory = std::filesystem::temp_directory_path() /
                         ("outshine-stored-road-page-" + std::to_string(getpid()));
  std::error_code error;
  std::filesystem::create_directories(directory, error);
  CHECK(!error && Prepare(directory),
        "only the parent process constructs and stores source inputs");
  CHECK(Spawn(argv[0], directory.string()), "the source-free child validates display miss and hit");
  std::filesystem::remove_all(directory, error);
  CHECK(!error, "the private native asset store is removed after the child exits");
  return Report();
}
