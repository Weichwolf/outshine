#include "ImpostorCards.h"
#include "Sha256.h"
#include "BinaryValueArchive.h"
#include "content/GeometryAsset.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace outshine::Content {
namespace {
constexpr uint64_t kFormat = 0x0001314452434fULL;
constexpr double kUnitDirectionSquaredTolerance = 1e-12;
constexpr size_t kViewPrefixBytes = 3 * sizeof(double) + sizeof(uint32_t);

bool ValidDirection(const Vec3 &direction) {
  return std::ranges::all_of(direction, [](double value) { return std::isfinite(value); }) &&
         direction[1] == 0 &&
         std::abs(Dot(direction, direction) - 1.0) <= kUnitDirectionSquaredTolerance;
}

bool Valid(const ImpostorCards &cards) {
  return std::ranges::all_of(cards.Centre, [](double value) { return std::isfinite(value); }) &&
         std::isfinite(cards.HalfExtentM) && cards.HalfExtentM > 0 && !cards.Views.empty() &&
         cards.Views.size() <= 64 &&
         std::ranges::all_of(cards.Views, [](const ImpostorCards::View &view) {
           if (!ValidDirection(view.Direction) || view.Surface.parts() != 1 ||
               !view.Surface.wellFormed()) {
             return false;
           }
           const auto positions = view.Surface.positionsOf(0).size();
           return positions > 0 && view.Surface.normalsOf(0).size() == positions &&
                  view.Surface.textureOf(0).size() == positions / 3 * 2 &&
                  view.Surface.tangentsOf(0).size() == positions / 3 * 4;
         });
}
}

std::string ImpostorCards::AssetKey(std::string_view provenance) {
  return Sha256Hex("impostor-cards-2/" + std::string(provenance));
}

std::optional<std::vector<uint8_t>> ImpostorCards::Encode(std::string_view provenance,
                                                          size_t bytesMost) const {
  if (provenance.empty() || !Valid(*this)) { return std::nullopt; }
  BinaryValueWriter out(bytesMost);
  if (!out(kFormat) || !out.Text(provenance) ||
      !out(Centre[0], Centre[1], Centre[2], HalfExtentM, static_cast<uint32_t>(Views.size()))) {
    return std::nullopt;
  }
  for (const auto &view : Views) {
    if (!out(view.Direction[0], view.Direction[1], view.Direction[2])) { return std::nullopt; }
    auto geometry = EncodeGeometryAsset(view.Surface, bytesMost - out.Out.Bytes().size());
    if (!geometry || !out.Array(std::span<const uint8_t>(*geometry))) { return std::nullopt; }
  }
  return std::move(out.Out).TakeBytes();
}

std::optional<ImpostorCards> ImpostorCards::Decode(std::span<const uint8_t> bytes,
                                                   std::string_view provenance,
                                                   size_t bytesMost) {
  if (bytes.size() > bytesMost || provenance.empty()) { return std::nullopt; }
  BinaryValueReader in(bytes);
  ImpostorCards cards;
  uint64_t format = 0;
  uint32_t views = 0;
  std::string source;
  if (!in(format) || format != kFormat || !in.Text(source) || source != provenance ||
      !in(cards.Centre[0], cards.Centre[1], cards.Centre[2], cards.HalfExtentM, views) ||
      views == 0 || views > 64 || views > in.In.Remaining() / kViewPrefixBytes) {
    return std::nullopt;
  }
  cards.Views.reserve(views);
  for (uint32_t at = 0; at < views; ++at) {
    View view;
    uint32_t size = 0;
    if (!in(view.Direction[0], view.Direction[1], view.Direction[2], size)) { return std::nullopt; }
    const auto payload = in.In.Take(size);
    if (!payload) { return std::nullopt; }
    auto geometry = DecodeGeometryAsset(*payload, bytesMost);
    if (!geometry) { return std::nullopt; }
    view.Surface = std::move(*geometry);
    cards.Views.push_back(std::move(view));
  }
  if (in.In.Remaining() != 0 || !Valid(cards)) { return std::nullopt; }
  return cards;
}
}
