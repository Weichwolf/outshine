#include "ImpostorViewAssets.h"
#include "Check.h"
#include <array>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Content;
using namespace outshine::Test;
constexpr std::string_view kSource = "field/world-frame";
constexpr Box kBounds{.Min = {{6377980, 100, -480}}, .Max = {{6378020, 150, -430}}};

Mat4 Frame() {
  Mat4 frame;
  frame[0] = frame[5] = 0;
  frame[1] = 1;
  frame[4] = -1;
  frame.SetTranslation({{6378000, 123.25, -456.5}});
  return frame;
}

ImpostorViewAssets::Description Description() {
  return {.Centre = {{1, 2, 3}},
          .HalfExtentM = 16,
          .Pixels = 32,
          .Directions = {{{0, 0, 1}}, {{1, 0, 0}}},
          .WorldFromField = Frame()};
}

std::string Publish(const std::string &path) {
  auto cache = AssetCache::Open(path);
  CHECK(cache, "the frame metadata store opens");
  if (!cache) { return {}; }
  const auto framed = ImpostorViewAssets::Publish(**cache, kSource, Description(), kBounds);
  CHECK(framed, "a rigid translated field frame publishes");
  if (!framed) { return {}; }
  const auto key = framed->FieldKey({0});
  auto translated = Description();
  auto frame = Frame();
  frame.SetTranslation({{6378000, 124.25, -456.5}});
  translated.WorldFromField = frame;
  const auto moved = ImpostorViewAssets::Publish(**cache, kSource, translated, kBounds);
  CHECK(moved && moved->FieldKey({0}) != key,
        "different intrinsic placements cannot alias field product keys");
  const auto restored = ImpostorViewAssets::Publish(**cache, kSource, Description(), kBounds);
  CHECK(restored && restored->FieldKey({0}) == key,
        "restoring the same intrinsic frame restores the same content address");
  auto local = Description();
  local.WorldFromField.reset();
  CHECK(ImpostorViewAssets::Publish(**cache, "field/local-only", local, kBounds),
        "local metadata remains usable without declaring a World frame");
  return key;
}

void Reopen(const std::filesystem::path &path, std::string_view key) {
  auto cache = AssetCache::Open(path.string());
  CHECK(cache, "a fresh cache connection needs no source owner");
  if (!cache) { return; }
  const auto field = ImpostorViewAssets::Open(**cache, kSource);
  CHECK(field && *field && (**field).Metadata().WorldFromField,
        "the intrinsic world transform survives storage and reopening");
  if (!field || !*field || !(**field).Metadata().WorldFromField) { return; }
  const auto &stored = *(**field).Metadata().WorldFromField;
  CHECK(stored.Column == Frame().Column && (**field).FieldKey({0}) == key,
        "all double coordinates and the content identity remain exact");
  const Vec3 local{{.001, 100000, 4}};
  const Vec3 expected{{6278000, 123.251, -452.5}};
  CHECK(Length(stored.TransformPoint(local) - expected) < 1e-9,
        "rotation and millimetre offsets retain their meaning at Earth-scale translation");
  const auto unframed = ImpostorViewAssets::Open(**cache, "field/local-only");
  CHECK(unframed && *unframed && !(**unframed).Metadata().WorldFromField,
        "unframed legacy metadata does not invent an identity World placement");
}

void InvalidFrames(const std::string &path) {
  auto cache = AssetCache::Open(path);
  CHECK(cache, "the negative frame store opens");
  if (!cache) { return; }
  auto scaled = Frame();
  scaled[1] = 2;
  auto reflected = Frame();
  reflected[10] = -1;
  auto perspective = Frame();
  perspective[3] = .01;
  auto sheared = Frame();
  sheared[8] = .01;
  auto nonfinite = Frame();
  nonfinite[12] = std::numeric_limits<double>::quiet_NaN();
  for (const auto &frame : std::array{scaled, reflected, perspective, sheared, nonfinite}) {
    auto description = Description();
    description.WorldFromField = frame;
    CHECK(!ImpostorViewAssets::Publish(**cache, "field/invalid", description, kBounds),
          "non-rigid or nonfinite frames cannot publish metadata");
  }
  const auto missing = ImpostorViewAssets::Open(**cache, "field/invalid");
  CHECK(missing && !*missing, "rejected frames leave no published index");
}

void MalformedStoredFrame(const std::string &path) {
  auto cache = AssetCache::Open(path);
  CHECK(cache, "the malformed metadata fixture opens");
  if (!cache) { return; }
  CHECK(ImpostorViewAssets::Publish(**cache, kSource, Description(), kBounds),
        "a valid frame supplies the independent stored counterexample");
  const auto rows = (*cache)->Select({.Bounds = kBounds,
                                      .Kind = "fractional-impostor-views-1",
                                      .Level = std::nullopt,
                                      .Radius = std::nullopt,
                                      .Planes = {}});
  CHECK(rows && rows->size() == 1, "the isolated fixture has one metadata record");
  if (!rows || rows->size() != 1) { return; }
  const auto &record = rows->front();
  const auto asset = (*cache)->Load(record.Key, 4096);
  CHECK(asset && *asset, "the complete framed payload is available");
  if (!asset || !*asset) { return; }
  const auto bytes = (**asset).Bytes();
  CHECK(bytes.size() >= sizeof(double), "the matrix fixture contains its homogeneous coordinate");
  if (bytes.size() < sizeof(double)) { return; }
  std::vector<uint8_t> changed(bytes.begin(), bytes.end());
  std::fill(changed.end() - sizeof(double), changed.end(), uint8_t{0});
  auto malformed = record;
  malformed.Package.clear();
  CHECK((*cache)->Publish(std::span(&malformed, 1), changed),
        "the malformed matrix has a valid outer package checksum");
  const auto opened = ImpostorViewAssets::Open(**cache, kSource);
  CHECK(opened && !*opened, "a non-affine stored frame cannot become ready metadata");
}
}

int main() {
  const auto path =
      std::filesystem::temp_directory_path() /
      ("outshine-field-frame-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".sqlite");
  const auto key = Publish(path.string());
  if (!key.empty()) { Reopen(path, key); }
  InvalidFrames(path.string());
  const auto malformed = path.string() + ".malformed";
  MalformedStoredFrame(malformed);
  std::filesystem::remove(malformed);
  std::filesystem::remove(path);
  return Report();
}
