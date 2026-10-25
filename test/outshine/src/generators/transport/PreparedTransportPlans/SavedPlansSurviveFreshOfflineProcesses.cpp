#include "PreparedTransportPlans.h"
#include "TransportPlanFixture.h"
#include "Check.h"
#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <print>
#include <spawn.h>
#include <string>
#include <string_view>
#include <sys/wait.h>
#include <system_error>
#include <unistd.h>
#ifdef __APPLE__
#include <crt_externs.h>
#endif

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;

TransportPlanAddress Address(uint8_t scope) {
  std::array<uint8_t, 32> inputs{};
  inputs[0] = scope;
  return {.Definition = {.Core = {.Level = 16, .X = kFixtureCoreX, .Y = kFixtureCoreY}},
          .Inputs = TransportInputScopeId(inputs)};
}

int Offline(const std::string &directory) {
  auto cache = PreparedTransportPlans::Open(directory);
  if (!cache) { return 2; }
  const auto a = (*cache)->LoadRequest(Address(0), kTransportFixtureLimits);
  const auto b = (*cache)->LoadRequest(Address(1), kTransportFixtureLimits);
  const auto unknown = (*cache)->LoadRequest(Address(2), kTransportFixtureLimits);
  if (!a || !*a || !b || !*b || !unknown || *unknown) { return 3; }
  const auto &first = **a;
  const auto &second = **b;
  const std::shared_ptr<const World::TransportNetworkSnapshot> consumer = first.Plan->Network;
  constexpr uint64_t sourcePath = 42;
  const auto *route = consumer ? consumer->FindRoute("fixture-circuit") : nullptr;
  const auto *edge = consumer ? consumer->Topology().FindEdge({.PathId = sourcePath}) : nullptr;
  const bool native = consumer && consumer.get() == first.Plan->Network.get() && route != nullptr &&
                      edge != nullptr && route->EdgeIds.size() == 2 &&
                      route->SourceRouteId == sourcePath * 2 &&
                      consumer->Topology().OutgoingFrom(7).size() == 2 &&
                      edge->Allows(World::TransportMode::Motor) &&
                      consumer->Metrics().SourceBytes == 0 && consumer->Metrics().ReadMs == 0 &&
                      consumer->Metrics().ParseMs == 0 && consumer->Metrics().GraphMs == 0;
  const bool valid = native && first.Record.Key == second.Record.Key &&
                     first.Plan->Coverage == TransportCoverage::Complete &&
                     first.Plan->TerrainContacts.size() == 1 &&
                     first.Plan->TerrainContacts[0].Profile &&
                     first.Plan->TerrainContacts[0].HoleRingsEastNorthM.size() == 1 &&
                     first.Plan->Ports.size() == 1 && first.Plan->Bodies.size() == 1 &&
                     first.Plan->Paths.size() == 2 &&
                     EncodeTransportPlan(*first.Plan, kTransportFixtureLimits) ==
                         EncodeTransportPlan(*second.Plan, kTransportFixtureLimits);
  std::println(
      "TRANSPORT_PLAN_OFFLINE valid={} independent_aliases=2 native_snapshot={} source_modules=0",
      valid,
      native);
  return valid ? 0 : 3;
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
  return waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

void Prepare(const std::string &directory) {
  auto cache = PreparedTransportPlans::Open(directory);
  const auto plan = TransportPlanFixture();
  CHECK(cache && plan, "the native source plan and existing asset store are available");
  if (!cache || !plan) { return; }
  const auto before = (*cache)->LoadRequest(Address(0), kTransportFixtureLimits);
  CHECK(before && !*before, "the declared logical request misses before any product exists");
  const auto bounds = Box{.Min = {{-1, -1, -1}}, .Max = {{1, 1, 1}}};
  const auto stored = (*cache)->StoreComplete(*plan, bounds, kTransportFixtureLimits);
  CHECK(stored && !stored->RequestKey, "product publication cannot bind a caller's generation");
  const auto unbound = (*cache)->LoadRequest(Address(0), kTransportFixtureLimits);
  CHECK(unbound && !*unbound, "an unbound completed cook is not a published request result");
  const auto product =
      (*cache)->LoadProduct(plan->Definition, plan->InputBinding, kTransportFixtureLimits);
  CHECK(product && *product, "resolved inputs find their content before any second cook");
  if (!product || !*product) { return; }
  CHECK((*cache)->BindRequest(Address(0), **product) &&
            (*cache)->BindRequest(Address(1), **product),
        "two independently validated request aliases reuse the same complete product");
}
}

int main(int argc, char **argv) {
  if (argc == 3 && std::string_view(argv[1]) == "--offline") { return Offline(argv[2]); }
  const auto directory = std::filesystem::temp_directory_path() /
                         ("outshine-engine-transport-plan-" + std::to_string(getpid()));
  std::error_code error;
  std::filesystem::create_directories(directory, error);
  CHECK(!error, "the test has an isolated system temporary directory");
  if (error) { return Report(); }
  Prepare(directory.string());
  CHECK(Spawn(argv[0], directory.string()),
        "a fresh process restores both complete plans without the source fixture or planner");
  std::filesystem::remove_all(directory, error);
  CHECK(!error, "the private test store is removed after its process exits");
  return Report();
}
