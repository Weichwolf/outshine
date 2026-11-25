#include "GroundTerrainPrecision.h"
#include "BinaryValueArchive.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace outshine::Generators::Osm {
namespace {
constexpr double kStepM = 0.125;
constexpr uint32_t kFormat = 1;
constexpr int kPageSide = 33;
constexpr size_t kNodes = static_cast<size_t>(kPageSide + 2) * (kPageSide + 2);
constexpr size_t kWordBytes = sizeof(uint32_t);

template <typename Archive, typename Page> bool Fields(Archive &archive, Page &page) {
  return archive(
      page.Tile.Zoom, page.Tile.X, page.Tile.Y, page.Postings, page.Virtual, page.SourceZoom);
}

bool ValidFields(const Sheet &sheet) {
  const auto &tile = sheet.Tile;
  if (tile.Zoom < 0 || tile.Zoom > Data::TileId::MaximumZoom) { return false; }
  const auto width = uint32_t{1} << static_cast<unsigned>(tile.Zoom);
  return tile.X < width && tile.Y < width && sheet.Postings >= static_cast<uint32_t>(kPageSide) &&
         sheet.SourceZoom >= -1 && sheet.SourceZoom <= tile.Zoom;
}

bool EncodePage(BinaryValueWriter &base,
                const Sheet &sheet,
                std::vector<uint32_t> &words,
                double &errorM) {
  const auto nodes = sheet.Samples();
  if (sheet.Side != kPageSide || nodes.size() != kNodes || !ValidFields(sheet)) { return false; }
  std::array<int32_t, kNodes> encoded{};
  for (size_t at = 0; at < nodes.size(); ++at) {
    const float node = nodes[at];
    const double ticks = std::round(static_cast<double>(node) / kStepM);
    if (!std::isfinite(ticks) || ticks < std::numeric_limits<int32_t>::min() ||
        ticks > std::numeric_limits<int32_t>::max()) {
      return false;
    }
    encoded[at] = static_cast<int32_t>(ticks);
    const auto coarse = static_cast<float>(encoded[at] * kStepM);
    errorM = std::max(errorM, std::abs(static_cast<double>(coarse) - node));
    words.push_back(std::bit_cast<uint32_t>(node) ^ std::bit_cast<uint32_t>(coarse));
  }
  const auto [low, high] = std::ranges::minmax(encoded);
  const int64_t range = static_cast<int64_t>(high) - low;
  if (std::cmp_greater(range, std::numeric_limits<uint16_t>::max()) || !Fields(base, sheet) ||
      !base(low)) {
    return false;
  }
  const bool narrow = std::cmp_less_equal(range, std::numeric_limits<uint8_t>::max());
  if (!base(narrow)) { return false; }
  for (const auto ticks : encoded) {
    const auto relative = static_cast<uint16_t>(static_cast<int64_t>(ticks) - low);
    if (!(narrow ? base(static_cast<uint8_t>(relative)) : base(relative))) { return false; }
  }
  return true;
}

template <typename Tick> bool DecodeSamples(BinaryValueReader &in, Sheet &sheet, int32_t origin) {
  sheet.Nodes.reserve(kNodes);
  for (size_t at = 0; at < kNodes; ++at) {
    Tick ticks{};
    if (!in(ticks)) { return false; }
    sheet.Nodes.push_back(static_cast<float>((static_cast<double>(origin) + ticks) * kStepM));
  }
  return true;
}

bool DecodePage(BinaryValueReader &in, Sheet &sheet) {
  int32_t origin = 0;
  bool narrow = false;
  sheet.Side = kPageSide;
  return Fields(in, sheet) && ValidFields(sheet) && in(origin, narrow) &&
         (narrow ? DecodeSamples<uint8_t>(in, sheet, origin)
                 : DecodeSamples<uint16_t>(in, sheet, origin));
}
}

std::optional<GroundTerrainPrecision> EncodeGroundTerrainPrecision(std::span<const Sheet> sheets,
                                                                   std::span<const uint32_t> slots,
                                                                   size_t bytesMost) {
  BinaryValueWriter base(bytesMost);
  if (slots.empty() || slots.size() > 64 || !base(kFormat, static_cast<uint32_t>(slots.size()))) {
    return std::nullopt;
  }
  std::vector<uint32_t> words;
  words.reserve(slots.size() * kNodes);
  double errorM = 0;
  for (const uint32_t slot : slots) {
    if (!EncodePage(base, sheets[slot], words, errorM)) { return std::nullopt; }
  }
  constexpr size_t kResidualHeader = 2 * sizeof(uint32_t);
  const size_t residualBytes = kResidualHeader + words.size() * kWordBytes;
  if (residualBytes > bytesMost - base.Out.Bytes().size()) { return std::nullopt; }
  BinaryValueWriter residual(residualBytes);
  if (!residual(kFormat, static_cast<uint32_t>(words.size()))) { return std::nullopt; }
  std::vector<uint8_t> planes(words.size() * kWordBytes);
  for (size_t plane = 0; plane < kWordBytes; ++plane) {
    for (size_t at = 0; at < words.size(); ++at) {
      planes[plane * words.size() + at] = static_cast<uint8_t>(words[at] >> (plane * 8));
    }
  }
  if (!residual.Out.Put(planes)) { return std::nullopt; }
  return GroundTerrainPrecision{.Base = std::move(base.Out).TakeBytes(),
                                .Residual = std::move(residual.Out).TakeBytes(),
                                .ErrorM = errorM};
}

std::optional<std::vector<Sheet>> DecodeGroundTerrainBase(std::span<const uint8_t> bytes,
                                                          size_t expected) {
  BinaryValueReader in(bytes);
  uint32_t format = 0;
  uint32_t count = 0;
  if (!in(format, count) || format != kFormat || count != expected || count > 64) {
    return std::nullopt;
  }
  std::vector<Sheet> sheets(count);
  if (!std::ranges::all_of(sheets, [&](Sheet &sheet) { return DecodePage(in, sheet); }) ||
      in.In.Remaining() != 0) {
    return std::nullopt;
  }
  return sheets;
}

bool ApplyGroundTerrainResidual(std::span<const uint8_t> bytes, std::span<Sheet> sheets) {
  BinaryValueReader in(bytes);
  uint32_t format = 0;
  uint32_t count = 0;
  if (!in(format, count) || format != kFormat || count != sheets.size() * kNodes ||
      in.In.Remaining() != count * kWordBytes) {
    return false;
  }
  const auto planes = in.In.Take(count * kWordBytes);
  if (!planes) { return false; }
  size_t at = 0;
  for (Sheet &sheet : sheets) {
    for (float &node : sheet.Nodes) {
      auto word = std::bit_cast<uint32_t>(node);
      for (size_t plane = 0; plane < kWordBytes; ++plane) {
        word ^= static_cast<uint32_t>((*planes)[plane * count + at]) << (plane * 8);
      }
      node = std::bit_cast<float>(word);
      if (!std::isfinite(node)) { return false; }
      ++at;
    }
  }
  return at == count;
}
}
