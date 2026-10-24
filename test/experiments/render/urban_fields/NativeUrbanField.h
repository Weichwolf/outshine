#ifndef OUTSHINE_TEST_RENDER_NATIVEURBANFIELD_H
#define OUTSHINE_TEST_RENDER_NATIVEURBANFIELD_H
#include "BinaryValueArchive.h"
#include "Sha256.h"
#include "AssetGeneration.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <utility>
#include <expected>
#include <fstream>
#include <sstream>
#include <span>
#include <string>
#include <vector>

namespace UrbanControl {
using namespace outshine;
constexpr size_t kBytes = size_t{8} * 1024 * 1024;
constexpr uint64_t kFormat = 0x3144495247425255ULL;
constexpr size_t kHeaderBytes = sizeof(uint64_t) + 3 * sizeof(uint32_t) + 2 * sizeof(double);
constexpr size_t kCellsMost = (kBytes - kHeaderBytes) / sizeof(float);

inline bool DimensionsFit(uint32_t columns, uint32_t rows) {
  return columns > 0 && rows > 0 && columns <= kCellsMost / rows;
}

struct Request {
  std::string Database, SourceSha256, InputPath;
};

struct HeightGrid {
  uint32_t Columns = 0, Rows = 0;
  double CellM = 0, FillFraction = 0;
  std::vector<float> HeightsM;

  [[nodiscard]] bool Valid() const {
    return DimensionsFit(Columns, Rows) && std::isfinite(CellM) && CellM > 0 && CellM <= 1024 &&
           std::isfinite(FillFraction) && FillFraction > 0 && FillFraction <= 1 &&
           HeightsM.size() == static_cast<size_t>(Columns) * Rows &&
           std::ranges::all_of(HeightsM,
                               [](float h) { return std::isfinite(h) && h >= 0 && h <= 1024; });
  }
};

inline std::expected<std::vector<uint8_t>, std::string> EncodeInput(const Request &request) {
  std::ifstream input(request.InputPath, std::ios::binary | std::ios::ate);
  const std::streamoff bytes = input ? input.tellg() : std::streampos(-1);
  if (bytes <= 0 || std::cmp_greater(bytes, kBytes)) {
    return std::unexpected("missing or oversized cold field input");
  }
  std::string text(static_cast<size_t>(bytes), '\0');
  input.seekg(0);
  input.read(text.data(), bytes);
  if (!input || Sha256Hex(text) != request.SourceSha256) {
    return std::unexpected("cold field hash mismatch");
  }
  std::istringstream values(text);
  HeightGrid field;
  if (!(values >> field.Columns >> field.Rows >> field.CellM >> field.FillFraction) ||
      !DimensionsFit(field.Columns, field.Rows)) {
    return std::unexpected("invalid cold field header");
  }
  field.HeightsM.resize(static_cast<size_t>(field.Columns) * field.Rows);
  for (auto &height : field.HeightsM) {
    if (!(values >> height)) { return std::unexpected("truncated cold field heights"); }
  }
  std::string extra;
  if (values >> extra || !field.Valid()) { return std::unexpected("invalid cold field values"); }
  BinaryValueWriter out(kBytes);
  if (!out(kFormat, field.Columns, field.Rows, field.CellM, field.FillFraction) ||
      !out.Array<float>(field.HeightsM)) {
    return std::unexpected("native field encoding exceeds budget");
  }
  return std::move(out.Out).TakeBytes();
}

inline std::expected<HeightGrid, std::string> LoadHeightGrid(const Request &request) {
  const auto key = Sha256Hex("native-urban-control-1\n" + request.SourceSha256);
  auto cache = AssetCache::Open(request.Database);
  if (!cache) { return std::unexpected("asset database does not open"); }
  const auto loaded = ResolveAsset(
      **cache, key, kBytes, [&](size_t) -> std::expected<GeneratedAssetPackage, std::string> {
        auto bytes = EncodeInput(request);
        if (!bytes) { return std::unexpected(bytes.error()); }
        BinaryValueReader reader(*bytes);
        uint64_t format = 0;
        HeightGrid field;
        if (!reader(format, field.Columns, field.Rows, field.CellM, field.FillFraction) ||
            !reader.Array(field.HeightsM) || !field.Valid()) {
          return std::unexpected("cold native field is invalid");
        }
        const auto top = std::ranges::max(field.HeightsM);
        GeneratedAssetPackage package;
        package.Records.push_back(
            {.Key = key,
             .Kind = "urban-patch-control",
             .Bounds = {.Min = {{0, 0, 0}},
                        .Max = {{field.Columns * field.CellM, top, field.Rows * field.CellM}}},
             .Package = {},
             .OffsetBytes = 0,
             .ByteCount = bytes->size(),
             .Level = 0,
             .Parent = {},
             .RequestKey = {}});
        package.Bytes = std::move(*bytes);
        return package;
      });
  if (!loaded) { return std::unexpected(loaded.error()); }
  HeightGrid field;
  uint64_t format = 0;
  BinaryValueReader reader(loaded->Bytes());
  if (!reader(format, field.Columns, field.Rows, field.CellM, field.FillFraction) ||
      format != kFormat || !reader.Array(field.HeightsM) || reader.In.Remaining() != 0 ||
      !field.Valid()) {
    return std::unexpected("invalid native field asset");
  }
  return field;
}
}

#endif
