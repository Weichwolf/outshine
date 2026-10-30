#include "PreparedRoadPages.h"
#include "TransportRoadPageProducer.h"
#include "TransportPlanCodec.h"
#include "TransportPlanFixture.h"
#include "GeometryProductCodec.h"
#include "Sha256.h"
#include "Check.h"
#include "ControlledRoadPageProducer.h"
#include <array>
#include <cerrno>
#include <cstddef>
#include <filesystem>
#include <expected>
#include <stop_token>
#include <memory>
#include <optional>
#include <print>
#include <spawn.h>
#include <string>
#include <string_view>
#include <sys/wait.h>
#include <system_error>
#include <unistd.h>
#include <utility>
#include <variant>
#include <vector>
#ifdef __APPLE__
#include <crt_externs.h>
#endif

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;
constexpr size_t kLimit = size_t{1024} * 1024;
constexpr size_t kAppearanceBytes = 4 + 64;

std::optional<RoadPageManifest> Prepare(const std::string &directory) {
  auto source = TransportPlanFixture();
  auto pages = PreparedRoadPages::Open(directory);
  if (!source || !pages) { return {}; }
  const auto plan = std::make_shared<const TransportPlan>(std::move(*source));
  const auto planKey = TransportPlanContentKey(plan->Definition, plan->InputBinding);
  if (!planKey) { return {}; }
  RoadPageManifest manifest;
  manifest.PlanKey = *planKey;
  manifest.Anchor = plan->Anchor;
  if (!manifest.Materials.addSurface("streets", Material{})) { return {}; }
  const auto material = EncodeGeometryProduct(manifest.Materials, kLimit);
  if (!material) { return {}; }
  manifest.MaterialKey = Sha256Hex(material->data(), material->size());
  const std::vector<RoadPagePartAppearance> selected{
      {.Part = {.Kind = RoadPagePartKind::Interval, .Index = 0}, .WearsLinear = {{1, 1, 1}}}};
  const RoadPageAddress address{.PlanKey = manifest.PlanKey,
                                .MaterialKey = manifest.MaterialKey,
                                .MaterialIndex = 0,
                                .Anchor = manifest.Anchor,
                                .Level = 0,
                                .Parts = {selected[0].Part},
                                .Appearance =
                                    RoadPageAppearanceV2{RoadPageAppearanceKey(selected)}};
  const auto producer = TransportRoadPageProducer::Bind(plan, address, selected);
  if (!producer) { return {}; }
  const auto record = (*pages)->Prepare(
      {.Address = address, .Parent = {}, .RequestKey = {}, .NativeBytesMost = kLimit}, &*producer);
  if (!record) { return {}; }
  manifest.Pages.push_back({.Key = record->Key,
                            .Bounds = record->Bounds,
                            .ByteCount = record->ByteCount,
                            .Level = 0,
                            .Parts = address.Parts,
                            .Appearance = address.Appearance});
  manifest.Appearance = RoadPageAppearanceV2{RoadPageManifestAppearanceKey(manifest.Pages)};
  return manifest;
}

void CheckManifest(const RoadPageManifest &manifest) {
  const auto bytes = EncodeRoadPageManifest(manifest, kLimit);
  auto decoded = bytes ? DecodeRoadPageManifest(*bytes, kLimit) : std::nullopt;
  CHECK(decoded && decoded->Appearance == manifest.Appearance &&
            decoded->Pages[0].Appearance == manifest.Pages[0].Appearance,
        "root and exact per-page appearance bindings survive the native manifest");
  if (!decoded) { return; }
  auto &changed = *decoded;
  changed.Appearance = RoadPageAppearanceV2{};
  CHECK(!EncodeRoadPageManifest(changed, kLimit), "v2 has no missing-root wildcard state");
  changed.Appearance = manifest.Appearance;
  changed.Pages[0].Appearance = manifest.Pages[0].Appearance;
  changed.Pages[0].Appearance = LegacyRoadPageV1{};
  CHECK(!EncodeRoadPageManifest(changed, kLimit), "v2 cannot consume a legacy reference");
  changed.Appearance = manifest.Appearance;
  changed.Pages[0].Appearance = manifest.Pages[0].Appearance;
  changed.Pages[0].Appearance = RoadPageAppearanceV2{std::string(64, 'b')};
  CHECK(!EncodeRoadPageManifest(changed, kLimit),
        "changed per-page binding invalidates root and page identity");
  changed.Appearance = manifest.Appearance;
  changed.Pages[0].Appearance = manifest.Pages[0].Appearance;
  changed.Appearance = LegacyRoadPageV1{};
  CHECK(!EncodeRoadPageManifest(changed, kLimit), "legacy cannot consume a v2 reference");
  if (bytes) {
    auto missing = *bytes;
    missing.resize(8 + 2 * (4 + 64));
    CHECK(!DecodeRoadPageManifest(missing, kLimit),
          "a v2 packet missing its appearance is rejected");
  }
}

std::optional<std::string> StoreLegacyTwin(PreparedRoadPages &pages,
                                           const RoadPageManifest &bound) {
  const auto loaded = pages.LoadKey(bound.Pages[0].Key, kLimit);
  if (!loaded || !*loaded) { return {}; }
  const auto &page = *(**loaded).Page;
  auto address = page.Address;
  address.Appearance = LegacyRoadPageV1{};
  const auto producer = RoadProducer([&page](const RoadPageRequest &, const std::stop_token &) {
    return std::expected<RoadPageGeometry, std::string>(page.Geometry);
  });
  const auto record =
      pages.Prepare({.Address = address, .Parent = {}, .RequestKey = {}, .NativeBytesMost = kLimit},
                    producer.get());
  if (!record) { return {}; }
  RoadPageManifest legacy;
  legacy.PlanKey = bound.PlanKey;
  legacy.MaterialKey = bound.MaterialKey;
  legacy.Anchor = bound.Anchor;
  if (!legacy.Materials.addSurface("streets", Material{})) { return {}; }
  legacy.Pages.push_back({.Key = record->Key,
                          .Bounds = record->Bounds,
                          .ByteCount = record->ByteCount,
                          .Level = 0,
                          .Parts = address.Parts});
  const auto previous = EncodeRoadPageManifest(legacy, kLimit);
  const auto current = EncodeRoadPageManifest(bound, kLimit);
  CHECK(previous && current && current->size() == previous->size() + 2 * kAppearanceBytes,
        "a one-page manifest adds only its root and per-reference appearance hashes");
  const auto stored = pages.StoreManifest(legacy);
  return stored ? std::optional(*stored) : std::nullopt;
}

bool Hit(PreparedRoadPages &pages, const std::string &key, bool version2) {
  const auto loaded = pages.LoadManifest(key);
  if (!loaded || !*loaded || (**loaded)->Pages.empty() ||
      std::holds_alternative<RoadPageAppearanceV2>((**loaded)->Appearance) != version2) {
    return false;
  }
  for (const auto &reference : (**loaded)->Pages) {
    const auto page = pages.LoadKey(reference.Key, kLimit);
    if (!page || !*page || (**page).Page->Address.Appearance != reference.Appearance ||
        (**page).Page->Geometry.Mesh.Index.empty()) {
      return false;
    }
    const auto hit = pages.Prepare({.Address = (**page).Page->Address,
                                    .Parent = {},
                                    .RequestKey = {},
                                    .NativeBytesMost = kLimit});
    if (!hit || hit->Key != reference.Key) { return false; }
  }
  return true;
}

int Offline(const std::filesystem::path &directory,
            const std::string &key,
            const std::string &legacyKey) {
  auto pages = PreparedRoadPages::Open(directory.string());
  const bool valid = pages && Hit(**pages, key, true) && Hit(**pages, legacyKey, false);
  std::println("ROAD_OFFLINE v1_and_v2={} producer=absent plan_asset=absent source_modules=absent",
               valid);
  return valid ? 0 : 2;
}

bool Spawn(const std::string &executable,
           const std::string &directory,
           const std::string &key,
           const std::string &legacyKey) {
  std::array<std::string, 5> values{executable, "--offline", directory, key, legacyKey};
  std::array<char *, 6> arguments{values[0].data(),
                                  values[1].data(),
                                  values[2].data(),
                                  values[3].data(),
                                  values[4].data(),
                                  nullptr};
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
  if (argc == 5 && std::string_view(argv[1]) == "--offline") {
    return Offline(argv[2], argv[3], argv[4]);
  }
  const auto directory = std::filesystem::temp_directory_path() /
                         ("outshine-engine-appearance-" + std::to_string(getpid()));
  std::error_code error;
  std::filesystem::create_directories(directory, error);
  CHECK(!error, "the asset experiment owns its temporary directory");
  const auto manifest = Prepare(directory.string());
  CHECK(manifest.has_value(), "the native immutable producer persists its v2 page");
  if (manifest) {
    CheckManifest(*manifest);
    std::string key;
    std::string legacyKey;
    {
      auto pages = PreparedRoadPages::Open(directory.string());
      if (!pages) { return 1; }
      const auto stored = (*pages)->StoreManifest(*manifest);
      CHECK(stored.has_value(), "the fully bound manifest persists after its dependency");
      if (stored) { key = *stored; }
      const auto legacy = StoreLegacyTwin(**pages, *manifest);
      if (legacy) { legacyKey = *legacy; }
    }
    CHECK(!key.empty() && !legacyKey.empty() && Spawn(argv[0], directory.string(), key, legacyKey),
          "fresh offline process has no producer, plan asset, source or fine-region dependency");
  }
  std::filesystem::remove_all(directory, error);
  CHECK(!error, "the original experimental inputs are removed after all consumers close");
  return Report();
}
