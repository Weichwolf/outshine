#include "Check.h"
#include "ImpostorAtlas.h"
#include "ImpostorCards.h"
#include "ImpostorSurface.h"
#include "PreparedImpostorAssets.h"
#include "Sha256.h"
#include "Tasks.h"
#include "content/AssetCache.h"
#include "content/GeometryAsset.h"
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <thread>
#include <utility>

using namespace outshine;
using outshine::Test::Report;

namespace {
std::atomic_uint64_t attempts{0};

std::optional<Content::ImpostorCards> Prepare(const Content::ImpostorAtlas &atlas,
                                              std::string &error) {
  ++attempts;
  return Render::PrepareImpostorCards(atlas, error);
}

std::optional<Content::ImpostorCards> Refuse(const Content::ImpostorAtlas &, std::string &error) {
  ++attempts;
  error = "a ready hit must not call preparation";
  return std::nullopt;
}

std::optional<Content::PreparedImpostorAssets::Loaded> Read(Content::PreparedImpostorAssets &cache,
                                                            const std::string &provenance) {
  if (cache.Read(provenance) != Content::PreparedImpostorAssets::Request::Queued) {
    return std::nullopt;
  }
  const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (std::chrono::steady_clock::now() < until) {
    if (auto loaded = cache.Take()) { return loaded; }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return std::nullopt;
}

std::optional<Content::ImpostorAtlas> Atlas(std::string &error) {
  Material material;
  material.BaseColour = {{0.3f, 0.5f, 0.2f, 1}};
  material.Roughness = 0.7f;
  Content::ImpostorAtlas::View view;
  view.TowardEye = {{0, 0, 1}};
  view.Texels.resize(16 * 16);
  for (size_t at = 0; at < view.Texels.size(); ++at) {
    if (at % 16 < 3 || at / 16 < 2) { continue; }
    view.Texels[at] = {.Normal = {{0, 0, 1}}, .Depth = 0.5f, .Surface = 1};
  }
  return Content::ImpostorAtlas::Create(16, {{0, 1, 0}}, 1, {material}, {std::move(view)}, error);
}
}

int main() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("outshine-ready-cards-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  const std::string provenance = "ready-cards-fixture-1";
  std::string error;
  auto atlas = Atlas(error);
  CHECK(atlas, "capture fixture is valid");
  if (!atlas) { return Report(); }
  auto reference = Render::PrepareImpostorCards(*atlas, error);
  CHECK(reference, "reference uses the existing flat-card preparation");
  if (!reference) { return Report(); }
  const auto cardsBytes = reference->Encode(provenance, 1024 * 1024);
  const auto captureBytes = atlas->Encode(provenance, error);
  CHECK(cardsBytes && captureBytes && cardsBytes->size() < captureBytes->size(),
        "the existing RGBA8 card representation stores fewer bytes than the float capture");
  if (!cardsBytes || !captureBytes) { return Report(); }
  CHECK(!Content::ImpostorCards::Decode(*cardsBytes, provenance + "changed", 1024 * 1024) &&
            !Content::ImpostorCards::Decode(*cardsBytes, provenance, cardsBytes->size() - 1) &&
            !Content::ImpostorCards::Decode(
                std::span(*cardsBytes).first(cardsBytes->size() - 1), provenance, 1024 * 1024),
        "provenance changes, limits and truncation are rejected");
  Tasks tasks(1);
  const Content::PreparedImpostorAssets::Config config{.Directory = root.string()};
  {
    Content::PreparedImpostorAssets capture(tasks, config);
    CHECK(capture.Publish(*atlas, provenance, error), "legacy capture is retained independently");
  }
  {
    Content::PreparedImpostorAssets cache(tasks, config, Prepare);
    auto loaded = Read(cache, provenance);
    CHECK(loaded && loaded->Cards && !loaded->Atlas && attempts == 1 &&
              cache.Costs().Preparations == 1 && cache.Costs().Writes == 1,
          "a missing ready product is prepared once from the retained native capture");
  }
  {
    Content::PreparedImpostorAssets cache(tasks, config, Refuse);
    auto loaded = Read(cache, provenance);
    CHECK(loaded && loaded->Cards && !loaded->Atlas && attempts == 1 &&
              cache.Costs().Preparations == 0 && cache.Costs().Writes == 0 &&
              cache.Costs().ReadBytes == cardsBytes->size(),
          "a fresh owner reads only ready bytes and never prepares or rewrites the capture");
    if (loaded && loaded->Cards) {
      CHECK(loaded->Cards->Centre == reference->Centre &&
                loaded->Cards->Views[0].Direction == reference->Views[0].Direction &&
                EncodeGeometryAsset(loaded->Cards->Views[0].Surface, 1024 * 1024) ==
                    EncodeGeometryAsset(reference->Views[0].Surface, 1024 * 1024),
            "ready products preserve every geometry, material and texture byte");
    }
  }
  {
    auto database = AssetCache::Open((root / "prototypes.sqlite").string());
    CHECK(database, "fixture cache reopens for corruption control");
    if (database) {
      const std::array<uint8_t, 4> corrupt{};
      const AssetRecord record{.Key = Sha256Hex("impostor-cards-1/" + provenance),
                               .Kind = "impostor-cards",
                               .Bounds = {.Min = {{-1, 0, -1}}, .Max = {{1, 2, 1}}},
                               .Package = {},
                               .ByteCount = corrupt.size(),
                               .Level = 1,
                               .Parent = Sha256Hex(provenance)};
      CHECK((*database)->Publish(std::span(&record, 1), corrupt),
            "corrupt ready product is injected without touching the capture");
    }
    Content::PreparedImpostorAssets cache(tasks, config, Prepare);
    auto loaded = Read(cache, provenance);
    CHECK(loaded && loaded->Cards && attempts == 2 && cache.Costs().Preparations == 1,
          "corrupt ready products rebuild from the intact capture rather than becoming ready");
  }
  std::filesystem::remove_all(root);
  return Report();
}
