#include "ImpostorViewAssets.h"
#include "ImpostorAtlas.h"
#include "AssetCache.h"
#include "Check.h"
#include "Material.h"
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Content;
using namespace outshine::Test;
using ViewIndex = ImpostorViewAssets::ViewIndex;
constexpr size_t kLimit = size_t{1024} * 1024;
constexpr std::string_view kSource = "four-cells/material-field-1";
const Box kBounds{.Min = {{0, 0, 0}}, .Max = {{32, 32, 32}}};
const ImpostorViewAssets::Description kDescription{.Centre = {{1, 2, 3}},
                                                   .HalfExtentM = 16,
                                                   .Pixels = 4,
                                                   .Directions = {{{0, 0, 1}}, {{1, 0, 0}}}};

std::optional<ImpostorAtlas> Field(size_t view) {
  ImpostorAtlas::View samples{
      .TowardEye = kDescription.Directions.at(view), .Texels = {}, .Materials = {}};
  samples.Texels.resize(16);
  samples.Materials.resize(16);
  samples.Texels[0] = {.Normal = {{1, 0, 0}}, .Depth = .25f, .Surface = 1};
  samples.Texels[15] = {.Normal = {{0, 0, 1}}, .Depth = .75f, .Surface = 2};
  samples.Materials[0] = {.BaseColour = {{1, 0, 0}}, .Roughness = .125f, .Metalness = .25f};
  samples.Materials[15] = {.BaseColour = {{0, 1, 0}}, .Roughness = .75f, .Metalness = .5f};
  std::string error;
  return ImpostorAtlas::Create(4,
                               kDescription.Centre,
                               kDescription.HalfExtentM,
                               {Material{}, Material{}},
                               {std::move(samples)},
                               error);
}

bool MissingField(const ImpostorViewAssets &index, size_t view) {
  const auto field = index.LoadField({view}, kLimit);
  return field && !*field;
}

void InvalidStoredFields(AssetCache &cache,
                         const ImpostorViewAssets &index,
                         const ImpostorAtlas &first,
                         const ImpostorAtlas &second) {
  const auto existing = cache.Find(index.FieldKey({0}));
  CHECK(existing && *existing, "valid field supplies the current metadata parent");
  if (!existing || !*existing) { return; }
  const auto poison = [&](std::vector<uint8_t> bytes) {
    const AssetRecord record{.Key = index.FieldKey({1}),
                             .Kind = "impostor-material-view-1",
                             .Bounds = kBounds,
                             .Package = {},
                             .OffsetBytes = 0,
                             .ByteCount = bytes.size(),
                             .Level = 0,
                             .Parent = (**existing).Parent,
                             .RequestKey = {}};
    CHECK(cache.Publish(std::span(&record, 1), bytes), "stale fixture is stored atomically");
    CHECK(MissingField(index, 1), "stale or corrupt inner payload cannot become ready coverage");
  };
  std::string error;
  const auto wrongView = first.Encode(index.FieldKey({1}), error);
  const auto wrongSource = second.Encode("another-source", error);
  auto corrupt = second.Encode(index.FieldKey({1}), error);
  CHECK(wrongView && wrongSource && corrupt, "independent invalid payload controls encode");
  if (!wrongView || !wrongSource || !corrupt) { return; }
  poison(*wrongView);
  poison(*wrongSource);
  corrupt->back() ^= 1;
  poison(*corrupt);
  const ImpostorViewAssets::FieldFactory repair =
      [&](ViewIndex, size_t) -> std::expected<ImpostorAtlas, AssetCacheError> { return second; };
  CHECK(index.ResolveField({1}, kLimit, repair),
        "a real producer can replace invalid stored fields");
  CHECK(index.ResolveField({0}, kLimit, {}), "invalid neighbor payloads preserve an existing view");
}

void Cold(const std::string &path) {
  auto cache = AssetCache::Open(path);
  CHECK(cache, "native field store opens");
  if (!cache) { return; }
  auto index = ImpostorViewAssets::Publish(**cache, kSource, kDescription, kBounds);
  const auto field = Field(0);
  CHECK(index && field, "fixed metadata and contrasting unlit samples are valid");
  if (!index || !field) { return; }
  CHECK(index->FieldKey({0}) != index->FieldKey({1}) &&
            index->FieldKey({0}) != index->Key({.View = 0, .Mip = 0}) &&
            index->FieldKey({0}) != index->Key({.View = 0, .Mip = 1}),
        "raw views and reduced render cards have separate versioned identities");
  size_t generated = 0;
  const ImpostorViewAssets::FieldFactory factory =
      [&](ViewIndex view, size_t bytesMost) -> std::expected<ImpostorAtlas, AssetCacheError> {
    ++generated;
    CHECK(view.Value == 0 && bytesMost == kLimit,
          "producer receives only the selected view and budget");
    return *field;
  };
  CHECK(!index->ResolveField({0}, 0, factory) && generated == 0,
        "zero capacity is rejected before production");
  CHECK(!index->ResolveField({2}, kLimit, factory) && generated == 0,
        "invalid views are rejected before production");
  CHECK(!index->ResolveField({0}, kLimit, {}), "a missing required field needs a real producer");
  CHECK(index->ResolveField({0}, kLimit, factory) && generated == 1,
        "one miss generates and publishes the selected native field");
  const auto loaded = index->ResolveField({0}, kLimit, factory);
  CHECK(loaded && generated == 1, "a validated hit never calls the source producer");
  std::string error;
  const auto expected = field->Encode(index->FieldKey({0}), error);
  const auto actual = loaded ? loaded->Encode(index->FieldKey({0}), error) : std::nullopt;
  CHECK(expected && actual == expected,
        "normal/material correlation, empty samples, depth and material tables survive exactly");
  if (expected) {
    CHECK(field->Encode(index->FieldKey({0}), expected->size(), error) == expected,
          "an exact byte budget preserves the existing native encoding");
    CHECK(!field->Encode(index->FieldKey({0}), expected->size() - 1, error),
          "one byte less rejects serialization before allocation");
    CHECK(!index->LoadField({0}, expected->size() - 1), "payload admission precedes decode");
  }
  const auto other = Field(1);
  CHECK(other && !index->PublishField({0}, *other, kLimit),
        "a different capture direction cannot replace the selected field");
  CHECK(index->ResolveField({0}, kLimit, {}) && MissingField(*index, 1),
        "rejected production preserves the existing view and does not invent another");
  if (!other) { return; }
  CHECK(!index->PublishField({1}, *other, 1), "bounded encoding creates no oversized package");
  CHECK(MissingField(*index, 1), "failed encoding leaves the required view missing");
  const ImpostorViewAssets::FieldFactory failure =
      [](ViewIndex, size_t) -> std::expected<ImpostorAtlas, AssetCacheError> {
    return std::unexpected(AssetCacheError::Storage);
  };
  CHECK(!index->ResolveField({1}, kLimit, failure) && MissingField(*index, 1),
        "producer failure never publishes partial field coverage");
  InvalidStoredFields(**cache, *index, *field, *other);
}

void Warm(const std::string &path) {
  auto cache = AssetCache::Open(path);
  CHECK(cache, "a fresh store connection opens after the source owner is gone");
  if (!cache) { return; }
  auto opened = ImpostorViewAssets::Open(**cache, kSource);
  CHECK(opened && *opened, "bounded index metadata remains independently ready");
  if (!opened || !*opened) { return; }
  const auto &index = **opened;
  const auto selected = index.Select({{0, 0, 1}}, 1);
  CHECK(selected && selected->View == 0 && selected->Mip == 2,
        "screen demand chooses a view before field payload IO");
  if (!selected) { return; }
  auto field = index.ResolveField({selected->View}, kLimit, {});
  CHECK(field && field->Pixels() == 4 && field->Views().size() == 1,
        "a source-free hit retains raw samples instead of reducing before lighting");
  const Vec3f x{{1, 0, 0}};
  const Vec3f z{{0, 0, 1}};
  const Vec3f green{{0, 1, 0}};
  CHECK(field && field->Views()[0].Texels[0].Normal == x &&
            field->Views()[0].Materials[0].BaseColour == x &&
            field->Views()[0].Texels[15].Normal == z &&
            field->Views()[0].Materials[15].BaseColour == green,
        "contrasting source responses remain coupled for current-light evaluation");
  const auto stored = (*cache)->Find(index.FieldKey({0}));
  CHECK(stored && *stored && !(**stored).Parent.empty() && (**stored).Level == 0 &&
            (**stored).Bounds == kBounds,
        "raw payload retains its metadata parent, coverage and capture level");
}
}

int main() {
  const auto path =
      std::filesystem::temp_directory_path() /
      ("outshine-unlit-fields-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".sqlite");
  Cold(path.string());
  Warm(path.string());
  std::filesystem::remove(path);
  return outshine::Test::Report();
}
