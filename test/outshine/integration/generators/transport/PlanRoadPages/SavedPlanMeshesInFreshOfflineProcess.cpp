#include "PreparedTransportPlans.h"
#include "ProfiledRoadMesher.h"
#include "TransportRoadPageMeshing.h"
#include "TransportPlanFixture.h"
#include "BinaryValueArchive.h"
#include "Sha256.h"
#include "Check.h"
#include <array>
#include <cerrno>
#include <cstddef>
#include <filesystem>
#include <print>
#include <spawn.h>
#include <span>
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

TransportPlanAddress Address() {
  return {.Definition = {.Core = {.Level = 16, .X = kFixtureCoreX, .Y = kFixtureCoreY}},
          .Inputs = TransportInputScopeId({})};
}

std::string MeshDigest(const TransportPlan &plan) {
  const std::array selected{
      RoadPagePartAppearance{.Part = {.Kind = RoadPagePartKind::Interval, .Index = 0},
                             .WearsLinear = {{1, 1, 1}}},
      RoadPagePartAppearance{.Part = {.Kind = RoadPagePartKind::Interval, .Index = 1},
                             .WearsLinear = {{1, 1, 1}}},
      RoadPagePartAppearance{.Part = {.Kind = RoadPagePartKind::Junction, .Index = 0},
                             .WearsLinear = {{1, 1, 1}}}};
  const ProfiledRoadMesher mesher;
  const auto page = MeshTransportRoadPage(mesher, plan, selected);
  if (!page || page->Mesh.Index.empty()) { return {}; }
  BinaryValueWriter out(kTransportFixtureLimits.PackageBytesMost);
  const auto &mesh = page->Mesh;
  if (!out.Array(std::span(mesh.PositionM)) || !out.Array(std::span(mesh.NormalM)) ||
      !out.Array(std::span(mesh.ColourRgba)) || !out.Array(std::span(mesh.Index)) ||
      !out(page->Swept.Pieces, page->Swept.Cuts, page->Swept.Refused)) {
    return {};
  }
  return Sha256Hex(out.Out.Bytes().data(), out.Out.Bytes().size());
}

TransportPlanLease Load(const std::string &directory) {
  auto cache = PreparedTransportPlans::Open(directory);
  if (!cache) { return {}; }
  auto loaded = (*cache)->LoadRequest(Address(), kTransportFixtureLimits);
  return loaded && *loaded ? (**loaded).Plan : TransportPlanLease{};
}

int Offline(const std::filesystem::path &directory, std::string_view expected) {
  const auto lease = Load(directory.string());
  if (!lease) { return 2; }
  const auto digest = MeshDigest(*lease);
  const bool same = !digest.empty() && digest == expected;
  std::println("TRANSPORT_PLAN_MESH_OFFLINE same={} digest={}", same, digest);
  return same ? 0 : 3;
}

bool Spawn(const std::string &executable, const std::string &directory, const std::string &digest) {
  std::array<std::string, 4> values{executable, "--offline", directory, digest};
  std::array<char *, 5> arguments{
      values[0].data(), values[1].data(), values[2].data(), values[3].data(), nullptr};
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

bool Prepare(const std::string &directory, const TransportPlan &plan) {
  auto cache = PreparedTransportPlans::Open(directory);
  if (!cache) { return false; }
  constexpr double extent = 2 * kEarthMeanRadiusM;
  const Box bounds{.Min = {{-extent, -extent, -extent}}, .Max = {{extent, extent, extent}}};
  if (!(*cache)->StoreComplete(plan, bounds, kTransportFixtureLimits)) { return false; }
  const auto loaded =
      (*cache)->LoadProduct(plan.Definition, plan.InputBinding, kTransportFixtureLimits);
  return loaded && *loaded && (*cache)->BindRequest(Address(), **loaded).has_value();
}
}

int main(int argc, char *const *argv) {
  if (argc == 4 && std::string_view(argv[1]) == "--offline") { return Offline(argv[2], argv[3]); }
  const auto directory = std::filesystem::temp_directory_path() /
                         ("outshine-engine-plan-mesh-" + std::to_string(getpid()));
  std::error_code error;
  std::filesystem::create_directories(directory, error);
  CHECK(!error, "the complete native plan test owns a temporary asset store");
  if (error) { return Report(); }
  const auto source = TransportPlanFixture();
  CHECK(source.has_value(), "the parent creates its source fixture before persistence");
  if (!source) { return Report(); }
  const auto digest = MeshDigest(*source);
  CHECK(!digest.empty(), "the existing native mesher creates the parent control geometry");
  CHECK(Prepare(directory.string(), *source),
        "the full plan commits and binds its logical request");
  CHECK(Spawn(argv[0], directory.string(), digest),
        "a fresh process recreates identical native mesh buffers from the stored lease after "
        "closing the cache");
  std::filesystem::remove_all(directory, error);
  CHECK(!error, "the private source-free mesh test store is removed");
  return Report();
}
