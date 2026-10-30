#include "StoreRoadManifest.h"
#include "Earth.h"
#include "PreparedRoadPages.h"
#include "RoadPageMeshProducer.h"
#include "TransportRoadPageProducer.h"
#include "TransportPlanCodec.h"
#include "TransportPlanFixture.h"
#include "RoadPageRestoration.h"
#include "GeometryProductCodec.h"
#include "Sha256.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <print>
#include <unistd.h>
#ifdef __APPLE__
#include <crt_externs.h>
#endif
#include <filesystem>
#include <memory>
#include <optional>
#include <spawn.h>
#include <string>
#include <string_view>
#include <span>
#include <sys/wait.h>
#include <system_error>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;
constexpr size_t kLimit = size_t{1024} * 1024;
constexpr double kSpanM = 100;
constexpr LongitudeLatitude kAnchor{.LongitudeDeg = 8, .LatitudeDeg = 49};

struct Fixture {
  std::string Manifest;
  std::string Request;
  std::string Payload;
};

RoadPageRecipe Recipe() {
  RoadPageRecipe recipe;
  recipe.Intervals.push_back({.Edge = 7,
                              .Lane = 0,
                              .Stations = {{.EastM = 0, .NorthM = 0, .GradeM = 0, .Node = 1},
                                           {.EastM = kSpanM, .NorthM = 0, .GradeM = 0, .Node = 2}},
                              .Surface = {.HalfWidthM = 4,
                                          .Profile = RoadProfile::Kerbed,
                                          .WearsLinear = {{0.5f, 0.5f, 0.5f}},
                                          .Crossfall = 0,
                                          .Form = RibbonForm::Surface,
                                          .EndPorts = {}}});
  return recipe;
}

std::optional<Fixture> Seed(const std::filesystem::path &directory) {
  auto opened = PreparedRoadPages::Open(directory.string());
  if (!opened) { return std::nullopt; }
  auto &pages = **opened;
  RoadPageManifest manifest;
  manifest.PlanKey.assign(64, 'a');
  manifest.Anchor = kAnchor;
  if (!manifest.Materials.addSurface("streets", Material{})) { return std::nullopt; }
  const auto material = EncodeGeometryProduct(manifest.Materials, 4096);
  if (!material) { return std::nullopt; }
  manifest.MaterialKey = Sha256Hex(material->data(), material->size());
  const RoadPageRequest request{
      .Address = {.PlanKey = manifest.PlanKey,
                  .MaterialKey = manifest.MaterialKey,
                  .MaterialIndex = 0,
                  .Anchor = manifest.Anchor,
                  .Level = 0,
                  .Parts = {{.Kind = RoadPagePartKind::Interval, .Index = 7}}},
      .Parent = {},
      .RequestKey = std::string(64, 'd'),
      .NativeBytesMost = kLimit};
  const RoadPageMeshProducer producer(RoadPageKey(request.Address), Recipe());
  auto wrong = request;
  wrong.Address.Parts.front().Index = 8;
  if (producer.Produce(wrong, {})) { return std::nullopt; }
  auto cold = pages.Prepare(request, &producer);
  if (!cold || cold->ByteCount == 0) { return std::nullopt; }
  manifest.Pages.push_back({.Key = cold->Key,
                            .Bounds = cold->Bounds,
                            .ByteCount = cold->ByteCount,
                            .Level = 0,
                            .Parts = request.Address.Parts});
  auto key = ::outshine::Test::StoreRoadManifest(pages, manifest);
  if (!key) { return std::nullopt; }
  return Fixture{
      .Manifest = std::move(*key), .Request = request.RequestKey, .Payload = cold->Package};
}

std::optional<Fixture> SeedBound(const std::filesystem::path &directory) {
  auto source = outshine::Test::TransportPlanFixture();
  auto opened = PreparedRoadPages::Open(directory.string());
  if (!source || !opened) { return std::nullopt; }
  const auto plan = std::make_shared<const TransportPlan>(std::move(*source));
  const auto planKey = TransportPlanContentKey(plan->Definition, plan->InputBinding);
  if (!planKey) { return std::nullopt; }
  RoadPageManifest manifest;
  manifest.PlanKey = *planKey;
  manifest.Anchor = plan->Anchor;
  if (!manifest.Materials.addSurface("streets", Material{})) { return std::nullopt; }
  const auto material = EncodeGeometryProduct(manifest.Materials, 4096);
  if (!material) { return std::nullopt; }
  manifest.MaterialKey = Sha256Hex(material->data(), material->size());
  const std::vector<RoadPagePartAppearance> selected{
      {.Part = {.Kind = RoadPagePartKind::Interval, .Index = 1},
       .WearsLinear = {{0.5f, 0.5f, 0.5f}}}};
  const RoadPageRequest request{
      .Address = {.PlanKey = manifest.PlanKey,
                  .MaterialKey = manifest.MaterialKey,
                  .MaterialIndex = 0,
                  .Anchor = manifest.Anchor,
                  .Level = 0,
                  .Parts = {selected[0].Part},
                  .Appearance = RoadPageAppearanceV2{RoadPageAppearanceKey(selected)}},
      .Parent = {},
      .RequestKey = std::string(64, 'd'),
      .NativeBytesMost = kLimit};
  const auto producer = TransportRoadPageProducer::Bind(plan, request.Address, selected);
  if (!producer) { return std::nullopt; }
  const auto cold = (*opened)->Prepare(request, &*producer);
  if (!cold || cold->ByteCount == 0) { return std::nullopt; }
  manifest.Pages.push_back({.Key = cold->Key,
                            .Bounds = cold->Bounds,
                            .ByteCount = cold->ByteCount,
                            .Level = 0,
                            .Parts = request.Address.Parts,
                            .Appearance = request.Address.Appearance});
  manifest.Appearance = RoadPageAppearanceV2{RoadPageManifestAppearanceKey(manifest.Pages)};
  auto key = ::outshine::Test::StoreRoadManifest(**opened, manifest);
  if (!key) { return std::nullopt; }
  return Fixture{
      .Manifest = std::move(*key), .Request = request.RequestKey, .Payload = cold->Package};
}

bool Finish(RoadPageRestoration &restore, Geometry &world) {
  const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(10);
  while (std::chrono::steady_clock::now() < until) {
    const auto ready = restore.Advance(world);
    if (!ready || restore.Missing()) { return false; }
    if (*ready) { return true; }
    std::this_thread::yield();
  }
  return false;
}

void CheckAppearance(Tasks &pool,
                     const std::shared_ptr<PreparedRoadPages> &pages,
                     const std::shared_ptr<const RoadPageManifest> &manifest) {
  using namespace outshine::Test;
  const auto foreign =
      std::make_shared<RoadPageManifest>(RoadPageManifest{.PlanKey = manifest->PlanKey,
                                                          .MaterialKey = manifest->MaterialKey,
                                                          .Anchor = manifest->Anchor,
                                                          .Materials = manifest->Materials.clone(),
                                                          .Pages = manifest->Pages,
                                                          .Appearance = manifest->Appearance});
  const bool legacy = std::holds_alternative<LegacyRoadPageV1>(manifest->Appearance);
  foreign->Pages[0].Appearance =
      legacy ? RoadPageAppearance(RoadPageAppearanceV2{std::string(64, 'f')})
             : RoadPageAppearance(LegacyRoadPageV1{});
  CHECK(!RoadPageRestoration::Begin(pool, pages, foreign),
        "mixed appearance versions are rejected before page IO");
  if (legacy) { return; }
  foreign->Pages[0].Appearance = RoadPageAppearanceV2{std::string(64, 'f')};
  auto rejected = RoadPageRestoration::Begin(pool, pages, foreign);
  Geometry world;
  CHECK(rejected && !Finish(**rejected, world) && world.parts() == 0,
        "a valid cached leaf cannot replace the appearance declared by its reference");
}

int ChildResult() {
  using namespace outshine::Test;
  std::println("ROADPAGE_CHILD checked={} failures={}", Checks.Value(), Failures.Value());
  return Checks.Value() > 0 && Failures.Value() == 0 && Unprepareds.Value() == 0 ? 0 : 1;
}

int Offline(const char *directory, const Fixture &fixture) {
  using namespace outshine::Test;
  auto opened = PreparedRoadPages::Open(directory);
  CHECK(opened, "a fresh process opens only the finished native cache");
  if (!opened) { return ChildResult(); }
  const std::shared_ptr<PreparedRoadPages> pages = std::move(*opened);
  auto manifest = pages->LoadManifest(fixture.Manifest);
  auto hit = pages->LoadRequest(fixture.Request, kLimit);
  CHECK(manifest && *manifest && hit && *hit,
        "stored request and manifest resolve without source, plan, recipe or producer");
  if (!manifest || !*manifest || !hit || !*hit) { return ChildResult(); }
  const auto bytes = EncodeRoadPageAsset(*(**hit).Page, kLimit);
  CHECK(bytes && Sha256Hex(bytes->data(), bytes->size()) == fixture.Payload,
        "the fresh hit retains every original native product word");
  Tasks pool(1);
  auto restore = RoadPageRestoration::Begin(pool, pages, **manifest);
  Geometry world;
  CHECK(restore && Finish(**restore, world), "the actual reader binds the product into the world");
  const auto &mesh = (**hit).Page->Geometry.Mesh;
  CHECK(world.wellFormed() && world.parts() == 1 &&
            std::ranges::equal(world.positionsOf(0), mesh.PositionM) &&
            std::ranges::equal(world.normalsOf(0), mesh.NormalM) &&
            std::ranges::equal(world.coloursOf(0), mesh.ColourRgba) &&
            std::ranges::equal(world.trianglesOf(0), mesh.Index),
        "world positions, normals, colours and indices equal the finished native lease");
  CheckAppearance(pool, pages, **manifest);
  std::println("ROADPAGE_OFFLINE pages={} native_bytes={} world_parts={}",
               restore ? (*restore)->Pages() : 0,
               (**hit).NativeBytes,
               world.parts());
  return ChildResult();
}

bool Spawn(const std::string &executable, const std::string &directory, const Fixture &fixture) {
  std::array<std::string, 6> arguments{
      executable, "--offline", directory, fixture.Manifest, fixture.Request, fixture.Payload};
  std::array<char *, 7> argv{};
  std::ranges::transform(arguments, argv.begin(), [](std::string &value) { return value.data(); });
#ifdef __APPLE__
  char *const *const environment = *_NSGetEnviron();
#else
  char *const *const environment = environ;
#endif
  pid_t child = 0;
  if (posix_spawn(&child, executable.c_str(), nullptr, nullptr, argv.data(), environment) != 0) {
    return false;
  }
  int status = 0;
  return waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0;
}
}

int main(int argc, char **argv) {
  const std::span arguments(argv, static_cast<size_t>(argc));
  if (argc == 6 && std::string_view(arguments[1]) == "--offline") {
    return Offline(arguments[2],
                   {.Manifest = arguments[3], .Request = arguments[4], .Payload = arguments[5]});
  }
  using namespace outshine::Test;
  const auto directory =
      std::filesystem::temp_directory_path() /
      ("outshine-roadpage-product-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(directory);
  constexpr std::array versions{false, true};
  for (const bool bound : versions) {
    const auto path = directory / (bound ? "v2" : "v1");
    std::filesystem::create_directories(path);
    const auto fixture = bound ? SeedBound(path) : Seed(path);
    CHECK(fixture,
          "the generator-owned typed producer commits a complete native page and manifest");
    CHECK(fixture &&
              Spawn(std::filesystem::absolute(arguments[0]).string(), path.string(), *fixture),
          "a separate offline process restores and binds the committed product without generation");
  }
  std::error_code error;
  std::filesystem::remove_all(directory, error);
  CHECK(!error, "the completed process-boundary fixture is removed");
  return Report();
}
