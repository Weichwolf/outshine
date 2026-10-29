#include "CanonicalGroundFieldFixture.h"
#include "GroundPortPlan.h"
#include "PreparedTransportPlans.h"
#include "PreparedRoadPages.h"
#include "TransportRoadPageProducer.h"
#include "RoadPageQueue.h"
#include "RoadPublicationFixture.h"
#include "GeometryProductCodec.h"
#include "Sha256.h"
#include "BinaryValueArchive.h"
#include "ReadTextFile.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <ios>
#include <memory>
#include <optional>
#include <ratio>
#include <spawn.h>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <variant>
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

void BuildRoadPages(const TransportPlanLease &plan, const std::string &directory) {
  Geometry materials;
  CHECK(materials.addSurface("streets", Material{}), "a native material binds the offline page");
  const auto material = EncodeGeometryProduct(materials, kLimits.PackageBytesMost);
  const auto key = TransportPlanContentKey(plan->Definition, plan->InputBinding);
  auto opened = PreparedRoadPages::Open(directory);
  CHECK(material && key && opened && !plan->Paths.empty(),
        "the restored actual subset has selectable paths and an existing page store");
  if (!material || !key || !opened || plan->Paths.empty()) { return; }
  std::vector<RoadPagePartAppearance> parts;
  RoadPageAddress address{.PlanKey = *key,
                          .MaterialKey = Sha256Hex(material->data(), material->size()),
                          .MaterialIndex = 0,
                          .Anchor = plan->Anchor,
                          .Level = 0,
                          .Parts = {},
                          .Appearance = LegacyRoadPageV1{}};
  for (size_t index = 0; index < plan->Paths.size(); ++index) {
    const RoadPagePart part{.Kind = RoadPagePartKind::Interval,
                            .Index = static_cast<uint32_t>(index)};
    address.Parts.push_back(part);
    parts.push_back({.Part = part, .WearsLinear = {{.5f, .5f, .5f}}});
  }
  address.Appearance = RoadPageAppearanceV2{RoadPageAppearanceKey(parts)};
  CHECK(!TransportRoadPageProducer::Bind(plan, address, parts),
        "the actual persisted subset cannot bind a complete road page producer");
  auto bound = TransportRoadPageProducer::Bind(
      plan, address, parts, TransportRepresentation::FunctionalSubset);
  CHECK(bound.has_value(), "the actual persisted subset binds only its explicit page contract");
  if (!bound) { return; }
  auto producer = std::make_shared<const TransportRoadPageProducer>(std::move(*bound));
  const auto inputBytes = producer->InputCapacityBytes();
  const std::shared_ptr<PreparedRoadPages> pages(std::move(*opened));
  Tasks tasks(1);
  const auto publications = RoadPublications(tasks);
  RoadPageQueue queue(
      tasks, pages, {.PendingMost = 1, .PageBytesMost = kLimits.PackageBytesMost}, publications);
  const RoadPageRequest request{.Address = address,
                                .Parent = {},
                                .RequestKey = {},
                                .NativeBytesMost = kLimits.PackageBytesMost};
  const auto began = std::chrono::steady_clock::now();
  const auto admitted = queue.RequestRecord(request, producer);
  CHECK(admitted && *admitted, "the existing bounded queue admits the stored subset producer");
  if (!admitted || !*admitted) { return; }
  producer.reset();
  std::vector<RoadPageQueue::Completed> done;
  const auto deadline = began + std::chrono::seconds(5);
  do {
    done = queue.Collect();
    if (done.empty()) { std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
  } while (done.empty() && std::chrono::steady_clock::now() < deadline);
  CHECK(done.size() == 1 && done[0].Result && *done[0].Result,
        "the real page consumer completes after the caller releases its producer");
  if (done.size() != 1) { return; }
  const auto &result = done.front().Result;
  if (!result || !*result) { return; }
  const auto queueMs =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
  const auto *receipt = std::get_if<RoadPageQueue::Receipt>(&**result);
  CHECK(receipt && *receipt && (**receipt).Key == RoadPageKey(address),
        "bounded publication yields the exact subset page content address");
  const auto readBegan = std::chrono::steady_clock::now();
  const auto loaded = pages->Load(address, kLimits.PackageBytesMost);
  const auto readMs =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - readBegan)
          .count();
  CHECK(loaded && *loaded && !(**loaded).Page->Geometry.Mesh.Index.empty() &&
            (**loaded).Page->Geometry.Parts == address.Parts,
        "all selected actual body parts survive native meshing and page persistence");
  const auto warmBegan = std::chrono::steady_clock::now();
  const auto hit = pages->Prepare(request);
  const auto warmMs =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - warmBegan)
          .count();
  CHECK(hit && hit->Key == RoadPageKey(address),
        "the warm subset page needs neither a producer nor another plan operation");
  std::ofstream output(std::filesystem::path(directory) / "page-key", std::ios::binary);
  output << RoadPageKey(address);
  output.close();
  CHECK(output.good(), "only the exact page key passes to another source-free process");
  Note("subset_page_queue_elapsed_ms", queueMs, "ms");
  Note("subset_page_read_elapsed_ms", readMs, "ms");
  Note("subset_page_warm_prepare_elapsed_ms", warmMs, "ms");
  Note("subset_page_producer_input_capacity", static_cast<double>(inputBytes), "B");
  if (hit) { Note("subset_page_package_bytes", static_cast<double>(hit->ByteCount), "B"); }
}

int PageHit(const std::string &directory) {
  const auto key = ReadTextFile((std::filesystem::path(directory) / "page-key").string(), 64);
  auto pages = PreparedRoadPages::Open(directory);
  CHECK(key && pages, "a separate process opens only page metadata and the existing asset store");
  if (!key || !pages) { return Report(); }
  const auto page = (*pages)->LoadKey(*key, kLimits.PackageBytesMost);
  CHECK(page && *page && !(**page).Page->Geometry.Mesh.Index.empty() &&
            !(**page).Page->Geometry.Parts.empty(),
        "a fresh page hit needs no source, functional plan decode, or live planning job");
  return Report();
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
  BuildRoadPages((**hit).Plan, directory);
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
  const auto network = GroundPortPlanNetwork::Prepare(field);
  CHECK(network.has_value(), "the qualified native field network prepares before core export");
  if (!network) { return; }
  const auto product =
      BuildGroundPortPartialPlan(field, *network, *whole, core, recipes, omissions);
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

bool Spawn(const char *executable, const std::string &directory, const char *mode) {
  std::array<std::string, 3> values{executable, mode, directory};
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
  if (argc == 3 && std::string_view(argv[1]) == "--pages") { return PageHit(argv[2]); }
  const auto directory = std::filesystem::temp_directory_path() /
                         ("outshine-ground-subset-replay-" + std::to_string(getpid()));
  std::error_code error;
  std::filesystem::create_directories(directory, error);
  CHECK(!error, "the actual subset has a private asset directory");
  if (error) { return Report(); }
  Prepare(directory.string());
  CHECK(Spawn(argv[0], directory.string(), "--offline"),
        "a fresh process restores the actual subset without loading original fields");
  CHECK(Spawn(argv[0], directory.string(), "--pages"),
        "a second fresh process restores only the completed native road page");
  std::filesystem::remove_all(directory, error);
  CHECK(!error, "the private replay store is removed after all processes release");
  return Report();
}
