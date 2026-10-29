#include "CanonicalGroundFieldFixture.h"
#include "GroundPortPlan.h"
#include "PreparedTransportPlans.h"
#include "BinaryValueArchive.h"
#include "ReadTextFile.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <memory>
#include <optional>
#include <spawn.h>
#include <span>
#include <string>
#include <string_view>
#include <sys/wait.h>
#include <system_error>
#include <unistd.h>
#include <vector>
#ifdef __APPLE__
#include <crt_externs.h>
#endif

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;
constexpr TransportPlanCodecLimits kLimits{.PackageBytesMost = 1 << 22,
                                           .DecodedCapacityMost = 1 << 24};

struct Replay {
  TransportPlanAddress Address;
  std::array<uint8_t, 32> Binding;
};

bool SaveRequest(const std::string &directory, const Replay &replay) {
  BinaryValueWriter bytes(128);
  const auto &core = replay.Address.Definition.Core;
  if (!bytes(static_cast<uint32_t>(core.Level),
             core.X,
             core.Y,
             replay.Address.Inputs.Bytes,
             replay.Binding)) {
    return false;
  }
  const auto values = bytes.Out.Bytes();
  std::ofstream file(std::filesystem::path(directory) / "request.bin", std::ios::binary);
  file.write(reinterpret_cast<const char *>(values.data()),
             static_cast<std::streamsize>(values.size()));
  return file.good();
}

std::optional<Replay> ReadRequest(const std::string &directory) {
  const auto bytes = ReadTextFile((std::filesystem::path(directory) / "request.bin").string(), 128);
  if (!bytes) { return {}; }
  BinaryValueReader input(
      std::span(reinterpret_cast<const uint8_t *>(bytes->data()), bytes->size()));
  uint32_t level = 0;
  uint32_t x = 0;
  uint32_t y = 0;
  std::array<uint8_t, 32> scope{};
  std::array<uint8_t, 32> binding{};
  if (!input(level, x, y, scope, binding) || input.In.Remaining() != 0 ||
      level > GeoCellId::MaximumLevel) {
    return {};
  }
  return Replay{
      .Address = {.Definition = {.Core = {.Level = static_cast<int>(level), .X = x, .Y = y},
                                 .Representation = TransportRepresentation::FunctionalSubset,
                                 .RepresentationVersion = 1},
                  .Inputs = TransportInputScopeId(scope)},
      .Binding = binding};
}

int Offline(const std::string &directory) {
  const auto request = ReadRequest(directory);
  auto cache = PreparedTransportPlans::Open(directory);
  CHECK(request && cache,
        "the fresh process opens only persisted request metadata and the existing asset store");
  if (!request || !cache) { return Report(); }
  const auto hit = (*cache)->LoadRequest(request->Address, kLimits);
  const auto product =
      (*cache)->LoadProduct(request->Address.Definition, request->Binding, kLimits);
  CHECK(hit && *hit && product && *product,
        "both logical request and resolved input binding restore the persisted real subset");
  if (!hit || !*hit || !product || !*product) { return Report(); }
  const auto &plan = *(**hit).Plan;
  CHECK(plan.Coverage == TransportCoverage::Partial && !plan.SourceEnds.empty() && plan.Network &&
            !plan.Bodies.empty() && !plan.Omissions.empty(),
        "actual functional bodies, complete owned source ends and omissions survive offline");
  CHECK(EncodeTransportPlan(plan, kLimits) == EncodeTransportPlan(*(**product).Plan, kLimits),
        "request and content lookup restore exactly the same native fields");
  CHECK(plan.Network && plan.Network->Metrics().SourceBytes == 0 &&
            plan.Network->Metrics().ParseMs == 0,
        "the restored native network contains no source decode work");
  auto full = request->Address;
  full.Definition.Representation = TransportRepresentation::Functional;
  full.Definition.RepresentationVersion = 2;
  const auto missing = (*cache)->LoadRequest(full, kLimits);
  CHECK(missing && !*missing,
        "the same scope and core do not turn a subset into a complete request hit");
  const auto refused = (*cache)->BindRequest(full, **hit);
  CHECK(!refused && refused.error() == TransportPlanAssetError::WrongProduct,
        "a valid subset receipt cannot bind the complete contract");
  return Report();
}

void Prepare(const std::string &directory) {
  auto fields = CanonicalGroundFixture::LoadFields();
  CHECK(fields.size() == 4, "the unchanged actual fields and both source publications load");
  if (fields.size() != 4 || !fields[2].Scope()) { return; }
  const auto &field = fields[2];
  const auto whole = BuildGroundPortBodies(field, {.ThicknessM = .3, .ApronM = 2, .ReliefM = 3});
  CHECK(whole.has_value(), "the actual accepted two-span field produces its original whole bodies");
  if (!whole) { return; }
  constexpr int level = 21;
  constexpr double count = uint32_t{1} << level;
  const auto anchor = field.Domain().Anchor;
  const GeoCellId core{.Level = level,
                       .X = static_cast<uint32_t>((anchor.LongitudeDeg + 180) / 360 * count),
                       .Y = static_cast<uint32_t>((anchor.LatitudeDeg + 90) / 180 * count)};
  std::vector<GroundPortPlanPathRecipe> recipes;
  for (const auto at : field.Source().SourceEdges) {
    recipes.push_back({.PathId = field.Source().Source->Topology().Edges()[at].Id.PathId,
                       .Material = "asphalt",
                       .Use = TransportUse::Road,
                       .SpeedMps = 13});
  }
  std::ranges::sort(recipes, {}, &GroundPortPlanPathRecipe::PathId);
  const auto duplicate = std::ranges::unique(recipes, {}, &GroundPortPlanPathRecipe::PathId);
  recipes.erase(duplicate.begin(), duplicate.end());
  const std::array omissions{
      TransportOmission{.SourceId = "actual-qualified-field/remaining-world",
                        .Reason = TransportOmissionReason::UnsupportedRepresentation}};
  const auto product = BuildGroundPortPartialPlan(field, *whole, core, recipes, omissions);
  CHECK(product &&
            product->Plan.Definition.Representation == TransportRepresentation::FunctionalSubset &&
            !product->Plan.SourceEnds.empty(),
        "the existing native producer declares its explicit qualified subset contract");
  if (!product) { return; }
  const Replay replay{.Address = {.Definition = product->Plan.Definition,
                                  .Inputs = TransportInputScopeId(product->Plan.InputBinding)},
                      .Binding = product->Plan.InputBinding};
  auto cache = PreparedTransportPlans::Open(directory);
  CHECK(cache.has_value(), "the qualified field uses the existing prepared transport asset cache");
  if (!cache) { return; }
  const auto before = (*cache)->LoadRequest(replay.Address, kLimits);
  CHECK(before && !*before, "a known qualified input scope misses before publication");
  const auto record = (*cache)->Store(product->Plan, product->BoundsEcef, kLimits);
  CHECK(record.has_value(),
        "the actual native subset is published within existing bounds and capacity admission");
  if (!record) { return; }
  Note("subset_package_bytes", static_cast<double>(record->ByteCount), "B");
  Note("subset_owned_capacity_bytes",
       static_cast<double>(TransportPlanCapacityBytes(product->Plan)),
       "B");
  const auto unbound = (*cache)->LoadRequest(replay.Address, kLimits);
  CHECK(unbound && !*unbound,
        "storing a valid subset does not publish an unvalidated request alias");
  const auto restored = (*cache)->LoadProduct(replay.Address.Definition, replay.Binding, kLimits);
  CHECK(restored && *restored,
        "resolved inputs reuse the stored subset without another field build");
  if (!restored || !*restored) { return; }
  Note("subset_restored_capacity_bytes",
       static_cast<double>(TransportPlanCapacityBytes(*(**restored).Plan)),
       "B");
  CHECK((*cache)->BindRequest(replay.Address, **restored) && SaveRequest(directory, replay),
        "the declared request binds the actual stored product and small replay metadata");
}

bool Spawn(const char *executable, const std::string &directory) {
  std::array<std::string, 3> values{executable, "--offline", directory};
  std::array<char *, 4> arguments{values[0].data(), values[1].data(), values[2].data(), nullptr};
#ifdef __APPLE__
  char *const *const environment = *_NSGetEnviron();
#else
  char *const *const environment = environ;
#endif
  pid_t child = 0;
  if (posix_spawn(&child, executable, nullptr, nullptr, arguments.data(), environment) != 0) {
    return false;
  }
  int status = 0;
  return waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0;
}
}

int main(int argc, char **argv) {
  if (argc == 3 && std::string_view(argv[1]) == "--offline") { return Offline(argv[2]); }
  const auto directory = std::filesystem::temp_directory_path() /
                         ("outshine-ground-subset-replay-" + std::to_string(getpid()));
  std::error_code error;
  std::filesystem::create_directories(directory, error);
  CHECK(!error, "the actual subset has a private asset directory");
  if (error) { return Report(); }
  Prepare(directory.string());
  CHECK(Spawn(argv[0], directory.string()),
        "a fresh process restores the actual subset without loading original fields");
  std::filesystem::remove_all(directory, error);
  CHECK(!error, "the private replay store is removed after all processes release");
  return Report();
}
