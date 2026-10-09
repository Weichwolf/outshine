#include "PreparedGroundPatchCodec.h"
#include "BinaryValueArchive.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace outshine::Generators {
namespace {
constexpr uint32_t kFormat = 0x31504754;

bool ValidGrid(Data::TileId at, int side) {
  return at.Zoom >= 0 && at.Zoom <= Data::TileId::MaximumZoom &&
         at.X < (uint32_t{1} << static_cast<unsigned>(at.Zoom)) &&
         at.Y < (uint32_t{1} << static_cast<unsigned>(at.Zoom)) && side >= 2 &&
         side <= kPreparedGroundPatchSideMost;
}
}

std::optional<std::vector<uint8_t>> EncodePreparedGroundPatch(Data::TileId at,
                                                              const GroundPatch &patch) {
  if (!ValidGrid(at, patch.Side()) || patch.Region() != at) { return std::nullopt; }
  const Tile region(at.Zoom, static_cast<int>(at.X), static_cast<int>(at.Y));
  const double steps = patch.Side() - 1.0;
  if (patch.SpacingEm() != region.SpanEm() / steps ||
      patch.SpacingNm() != region.SpanNm() / steps ||
      !std::ranges::all_of(patch.HeightsAslM(), GroundSample::HeightIsOnEarth)) {
    return std::nullopt;
  }
  BinaryValueWriter encoded(kPreparedGroundPatchBytesMost);
  if (!encoded(kFormat, at.Zoom, at.X, at.Y, patch.Side()) || !encoded.Array(patch.HeightsAslM())) {
    return std::nullopt;
  }
  return std::move(encoded.Out).TakeBytes();
}

std::shared_ptr<const GroundPatch>
DecodePreparedGroundPatch(Data::TileId at, int side, std::span<const uint8_t> bytes) {
  if (!ValidGrid(at, side) || bytes.size() > kPreparedGroundPatchBytesMost) { return nullptr; }
  BinaryValueReader encoded(bytes);
  uint32_t format = 0;
  Data::TileId stored;
  int storedSide = 0;
  uint32_t count = 0;
  const size_t nodes = static_cast<size_t>(side) * static_cast<size_t>(side);
  if (!encoded(format, stored.Zoom, stored.X, stored.Y, storedSide, count) || format != kFormat ||
      stored != at || storedSide != side || count != nodes ||
      encoded.In.Remaining() != nodes * sizeof(double)) {
    return nullptr;
  }
  std::vector<GroundPatch::Posting> postings(nodes);
  for (auto &posting : postings) {
    double height = 0;
    if (!encoded(height) || !GroundSample::HeightIsOnEarth(height)) { return nullptr; }
    posting.Height = GroundSample::At(height);
  }
  return GroundPatch::Complete(
      Tile(at.Zoom, static_cast<int>(at.X), static_cast<int>(at.Y)), side, postings);
}
}
