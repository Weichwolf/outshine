#include "CopernicusRaster.h"

#include <algorithm>
#include <bit>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <expected>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <tiffio.h>

namespace outshine::Data {
namespace {

constexpr uint64_t kMostObjectBytes = uint64_t{128} * 1024 * 1024;
constexpr size_t kMostParts = 64, kMostLevels = 8;
constexpr uint32_t kMostAxisSamples = 3600, kMostBlocks = 64;
constexpr uint64_t kMostEncodedBlockBytes = uint64_t{8} * 1024 * 1024;
constexpr tmsize_t kMostAllocationBytes = tmsize_t{8} * 1024 * 1024;
constexpr tmsize_t kMostTiffBytes = tmsize_t{16} * 1024 * 1024;
constexpr uint32_t kPixelScale = 33550, kTiePoint = 33922, kGeoKeys = 34735;
constexpr uint32_t kMostTagValues = 256;
constexpr uint16_t kGeographicModel = 2, kPointRaster = 2, kWgs84 = 4326;
constexpr uint16_t kModelTypeKey = 1024, kRasterTypeKey = 1025, kGeographicTypeKey = 2048;
constexpr uint16_t kAngularUnitsKey = 2054, kVerticalTypeKey = 4096, kVerticalUnitsKey = 4099;
constexpr uint16_t kAngularDegrees = 9102, kEgm2008 = 3855, kMeters = 9001;
constexpr double kLongitudeLimitDeg = 180, kLatitudeLimitDeg = 90;
constexpr double kDegreeTolerance = 1e-8;

struct Reader {
  CopernicusObject Original;
  uint64_t Position = 0;
  std::optional<ByteRange> Needed = std::nullopt;

  [[nodiscard]] CopernicusFailure Failure() const {
    return {.Problem = Needed ? CopernicusProblem::MissingBytes : CopernicusProblem::InvalidRaster,
            .Needed = Needed};
  }
};

std::expected<void, CopernicusFailure> Validate(CopernicusObject original) {
  if (original.TotalBytes == 0 || original.TotalBytes > kMostObjectBytes ||
      original.Parts.size() > kMostParts) {
    return std::unexpected(CopernicusFailure{.Problem = CopernicusProblem::CapacityRefused});
  }
  if (original.ObjectKey.empty() || original.ObjectKey.size() > 4096 ||
      original.EntityTag.size() < 2 || original.EntityTag.size() > 1024 ||
      original.EntityTag.front() != '"' || original.EntityTag.back() != '"') {
    return std::unexpected(CopernicusFailure{});
  }
  uint64_t end = 0;
  for (const auto &part : original.Parts) {
    const uint64_t first = part.Origin.Bytes.First;
    if (part.Bytes.empty() || first < end || first > original.TotalBytes ||
        part.Bytes.size() > original.TotalBytes - first ||
        part.Bytes.size() != part.Origin.Bytes.Length ||
        part.Origin.TotalBytes != original.TotalBytes ||
        part.Origin.EntityTag != original.EntityTag || part.ObjectKey != original.ObjectKey) {
      return std::unexpected(CopernicusFailure{});
    }
    end = first + part.Bytes.size();
  }
  return {};
}

tmsize_t Read(Reader &reader, std::span<uint8_t> destination) {
  if (destination.empty()) { return 0; }
  const auto bytes = static_cast<uint64_t>(destination.size());
  if (reader.Position > reader.Original.TotalBytes ||
      bytes > reader.Original.TotalBytes - reader.Position || bytes > kMostEncodedBlockBytes) {
    return 0;
  }
  const uint64_t end = reader.Position + bytes;
  uint64_t covered = reader.Position;
  for (const auto &part : reader.Original.Parts) {
    if (part.Origin.Bytes.First > covered) { break; }
    const uint64_t partEnd = part.Origin.Bytes.First + part.Bytes.size();
    covered = std::max(covered, partEnd);
    if (covered >= end) { break; }
  }
  if (covered < end) {
    if (!reader.Needed) { reader.Needed = ByteRange{.First = covered, .Length = end - covered}; }
    return 0;
  }
  auto *out = destination.data();
  for (const auto &part : reader.Original.Parts) {
    const uint64_t first = part.Origin.Bytes.First;
    if (first >= end) { break; }
    const uint64_t begin = std::max(reader.Position, first);
    const uint64_t finish = std::min(end, first + part.Bytes.size());
    if (begin < finish) {
      std::memcpy(out + (begin - reader.Position),
                  part.Bytes.data() + (begin - first),
                  static_cast<size_t>(finish - begin));
    }
  }
  reader.Position = end;
  return static_cast<tmsize_t>(destination.size());
}

struct FileSeek {
  toff_t Offset = 0;
  int Whence = SEEK_SET;
};

toff_t Seek(Reader &reader, FileSeek by) {
  const auto [offset, whence] = by;
  uint64_t position = offset;
  if (whence != SEEK_SET) {
    if (whence != SEEK_CUR && whence != SEEK_END) { return std::numeric_limits<toff_t>::max(); }
    const uint64_t origin = whence == SEEK_CUR ? reader.Position : reader.Original.TotalBytes;
    if (std::bit_cast<int64_t>(offset) < 0) {
      const uint64_t distance = uint64_t{0} - offset;
      if (distance > origin) { return std::numeric_limits<toff_t>::max(); }
      position = origin - distance;
    } else {
      if (origin > reader.Original.TotalBytes || offset > reader.Original.TotalBytes - origin) {
        return std::numeric_limits<toff_t>::max();
      }
      position = origin + offset;
    }
  }
  if (position > reader.Original.TotalBytes) { return std::numeric_limits<toff_t>::max(); }
  reader.Position = position;
  return position;
}

int Close([[maybe_unused]] thandle_t source) {
  return 0;
}

toff_t Size(thandle_t handle) {
  return static_cast<Reader *>(handle)->Original.TotalBytes;
}

using RasterHandle = std::unique_ptr<TIFF, decltype(&TIFFClose)>;

RasterHandle Open(Reader &reader) {
  const std::unique_ptr<TIFFOpenOptions, decltype(&TIFFOpenOptionsFree)> options(
      TIFFOpenOptionsAlloc(), &TIFFOpenOptionsFree);
  if (!options) { return {nullptr, &TIFFClose}; }
  TIFFOpenOptionsSetMaxSingleMemAlloc(options.get(), kMostAllocationBytes);
  TIFFOpenOptionsSetMaxCumulatedMemAlloc(options.get(), kMostTiffBytes);
  TIFFOpenOptionsSetWarnAboutUnknownTags(options.get(), 0);
  constexpr auto quiet = +[]([[maybe_unused]] TIFF *raster,
                             [[maybe_unused]] void *context,
                             [[maybe_unused]] const char *const unit,
                             [[maybe_unused]] const char *format,
                             [[maybe_unused]] va_list arguments) { return 1; };
  TIFFOpenOptionsSetErrorHandlerExtR(options.get(), quiet, nullptr);
  TIFFOpenOptionsSetWarningHandlerExtR(options.get(), quiet, nullptr);
  return {TIFFClientOpenExt(
              "copernicus-original",
              "rm",
              &reader,
              +[](void *const source, void *bytes, tmsize_t count) -> tmsize_t {
                if (count <= 0) { return 0; }
                return Read(*static_cast<Reader *>(source),
                            {static_cast<uint8_t *>(bytes), static_cast<size_t>(count)});
              },
              +[]([[maybe_unused]] void *const source,
                  [[maybe_unused]] void *bytes,
                  [[maybe_unused]] tmsize_t count) -> tmsize_t { return 0; },
              +[](thandle_t source, const toff_t offset, int whence) {
                return Seek(*static_cast<Reader *>(source), {.Offset = offset, .Whence = whence});
              },
              &Close,
              &Size,
              nullptr,
              nullptr,
              options.get()),
          &TIFFClose};
}

template <class T> std::span<const T> Custom(TIFF *raster, uint32_t tag, TIFFDataType type) {
  const TIFFField *field = TIFFFindField(raster, tag, TIFF_ANY);
  if (field == nullptr || TIFFFieldDataType(field) != type ||
      TIFFFieldReadCount(field) != TIFF_VARIABLE2 || TIFFFieldPassCount(field) != 1) {
    return {};
  }
  uint32_t count = 0;
  const T *values = nullptr;
  if (TIFFGetField(raster, tag, &count, &values) != 1 || count == 0 || count > kMostTagValues ||
      values == nullptr) {
    return {};
  }
  return {values, count};
}

std::optional<uint16_t> GeoKey(std::span<const uint16_t> keys, uint16_t wanted) {
  for (size_t at = 4; at + 3 < keys.size(); at += 4) {
    if (keys[at] == wanted && keys[at + 1] == 0 && keys[at + 2] == 1) { return keys[at + 3]; }
  }
  return {};
}

std::expected<CopernicusLevel, CopernicusFailure> Level(TIFF *raster, uint64_t totalBytes) {
  CopernicusLevel level;
  uint16_t bits = 0;
  uint16_t samples = 0;
  uint16_t format = 0;
  uint16_t compression = 0;
  uint16_t predictor = 0;
  uint16_t planar = 0;
  if (TIFFGetField(raster, TIFFTAG_IMAGEWIDTH, &level.Columns) != 1 ||
      TIFFGetField(raster, TIFFTAG_IMAGELENGTH, &level.Rows) != 1 ||
      TIFFGetField(raster, TIFFTAG_TILEWIDTH, &level.BlockColumns) != 1 ||
      TIFFGetField(raster, TIFFTAG_TILELENGTH, &level.BlockRows) != 1 ||
      TIFFGetFieldDefaulted(raster, TIFFTAG_BITSPERSAMPLE, &bits) != 1 ||
      TIFFGetFieldDefaulted(raster, TIFFTAG_SAMPLESPERPIXEL, &samples) != 1 ||
      TIFFGetFieldDefaulted(raster, TIFFTAG_SAMPLEFORMAT, &format) != 1 ||
      TIFFGetFieldDefaulted(raster, TIFFTAG_COMPRESSION, &compression) != 1 ||
      TIFFGetFieldDefaulted(raster, TIFFTAG_PREDICTOR, &predictor) != 1 ||
      TIFFGetFieldDefaulted(raster, TIFFTAG_PLANARCONFIG, &planar) != 1 ||
      TIFFIsTiled(raster) == 0 || bits != 32 || samples != 1 || format != SAMPLEFORMAT_IEEEFP ||
      (compression != COMPRESSION_ADOBE_DEFLATE && compression != COMPRESSION_DEFLATE) ||
      predictor != PREDICTOR_FLOATINGPOINT || planar != PLANARCONFIG_CONTIG || level.Columns == 0 ||
      level.Rows == 0 || level.BlockColumns == 0 || level.BlockRows == 0) {
    return std::unexpected(CopernicusFailure{});
  }
  if (level.Columns > kMostAxisSamples || level.Rows > kMostAxisSamples ||
      level.BlockColumns > 1024 || level.BlockRows > 1024) {
    return std::unexpected(CopernicusFailure{.Problem = CopernicusProblem::CapacityRefused});
  }
  const uint64_t across = (level.Columns + level.BlockColumns - 1) / level.BlockColumns;
  const uint64_t down = (level.Rows + level.BlockRows - 1) / level.BlockRows;
  const uint64_t blocks = across * down;
  if (blocks > kMostBlocks) {
    return std::unexpected(CopernicusFailure{.Problem = CopernicusProblem::CapacityRefused});
  }
  const uint64_t *offsets = nullptr;
  const uint64_t *counts = nullptr;
  if (TIFFNumberOfTiles(raster) != blocks ||
      TIFFGetField(raster, TIFFTAG_TILEOFFSETS, &offsets) != 1 ||
      TIFFGetField(raster, TIFFTAG_TILEBYTECOUNTS, &counts) != 1 || offsets == nullptr ||
      counts == nullptr) {
    return std::unexpected(CopernicusFailure{});
  }
  level.Blocks.reserve(static_cast<size_t>(blocks));
  for (size_t block = 0; block < blocks; ++block) {
    if (counts[block] == 0 || counts[block] > kMostEncodedBlockBytes ||
        offsets[block] > totalBytes || counts[block] > totalBytes - offsets[block]) {
      return std::unexpected(CopernicusFailure{});
    }
    level.Blocks.push_back({.First = offsets[block], .Length = counts[block]});
  }
  return level;
}

std::expected<void, CopernicusFailure> Locate(TIFF *raster, CopernicusLevel &level) {
  const auto scale = Custom<double>(raster, kPixelScale, TIFF_DOUBLE);
  const auto tie = Custom<double>(raster, kTiePoint, TIFF_DOUBLE);
  const auto keys = Custom<uint16_t>(raster, kGeoKeys, TIFF_SHORT);
  if (scale.size() != 3 || tie.size() != 6 || keys.size() < 4 || keys[0] != 1 ||
      keys.size() != 4u + static_cast<size_t>(keys[3]) * 4u ||
      GeoKey(keys, kModelTypeKey) != kGeographicModel ||
      GeoKey(keys, kRasterTypeKey) != kPointRaster || GeoKey(keys, kGeographicTypeKey) != kWgs84 ||
      !std::ranges::all_of(scale, [](double x) { return std::isfinite(x); }) ||
      !std::ranges::all_of(tie, [](double x) { return std::isfinite(x); }) || !(scale[0] > 0) ||
      !(scale[1] > 0)) {
    return std::unexpected(CopernicusFailure{});
  }
  const auto angular = GeoKey(keys, kAngularUnitsKey);
  const auto vertical = GeoKey(keys, kVerticalTypeKey);
  const auto units = GeoKey(keys, kVerticalUnitsKey);
  if ((angular && *angular != kAngularDegrees) || (vertical && *vertical != kEgm2008) ||
      (units && *units != kMeters)) {
    return std::unexpected(CopernicusFailure{});
  }
  level.ColumnStepDeg = scale[0];
  level.RowStepDeg = scale[1];
  level.WestDeg = tie[3] - tie[0] * scale[0];
  level.NorthDeg = tie[4] + tie[1] * scale[1];
  if (std::abs(level.Columns * scale[0] - 1.0) > kDegreeTolerance ||
      std::abs(level.Rows * scale[1] - 1.0) > kDegreeTolerance ||
      level.WestDeg < -kLongitudeLimitDeg || level.WestDeg + 1 > kLongitudeLimitDeg ||
      level.NorthDeg > kLatitudeLimitDeg || level.NorthDeg - 1 < -kLatitudeLimitDeg) {
    return std::unexpected(CopernicusFailure{});
  }
  return {};
}

std::expected<std::optional<float>, CopernicusFailure> NoData(TIFF *raster) {
  const TIFFField *field = TIFFFindField(raster, TIFFTAG_GDAL_NODATA, TIFF_ANY);
  if (field == nullptr) { return std::optional<float>{}; }
  if (TIFFFieldDataType(field) != TIFF_ASCII || TIFFFieldReadCount(field) != TIFF_VARIABLE2 ||
      TIFFFieldPassCount(field) != 1) {
    return std::unexpected(CopernicusFailure{});
  }
  uint32_t count = 0;
  const char *value = nullptr;
  if (TIFFGetField(raster, TIFFTAG_GDAL_NODATA, &count, &value) != 1) {
    return std::optional<float>{};
  }
  if (count == 0 || count > 64 || value == nullptr || value[count - 1] != '\0') {
    return std::unexpected(CopernicusFailure{});
  }
  const std::string_view text(value, count - 1);
  float missing = 0;
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), missing);
  if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() || std::isinf(missing)) {
    return std::unexpected(CopernicusFailure{});
  }
  return missing;
}

}

std::optional<float> CopernicusBlock::HeightM(uint32_t row, uint32_t column) const noexcept {
  if (row >= Rows || column >= Columns) { return {}; }
  const float height = HeightsM[static_cast<size_t>(row) * Stride + column];
  return std::isfinite(height) && (!NoData || height != *NoData) ? std::optional<float>(height)
                                                                 : std::nullopt;
}

std::expected<CopernicusRaster, CopernicusFailure> ReadCopernicusRaster(CopernicusObject original) {
  if (auto valid = Validate(original); !valid) { return std::unexpected(valid.error()); }
  Reader reader{.Original = original};
  const auto raster = Open(reader);
  if (!raster) { return std::unexpected(reader.Failure()); }
  if (reader.Needed) { return std::unexpected(reader.Failure()); }
  auto root = Level(raster.get(), original.TotalBytes);
  if (!root) { return std::unexpected(reader.Needed ? reader.Failure() : root.error()); }
  if (auto located = Locate(raster.get(), *root); !located) {
    return std::unexpected(located.error());
  }
  auto noData = NoData(raster.get());
  if (!noData) { return std::unexpected(noData.error()); }
  CopernicusRaster result{.ObjectKey = std::string(original.ObjectKey),
                          .TotalBytes = original.TotalBytes,
                          .EntityTag = std::string(original.EntityTag),
                          .Levels = {},
                          .NoData = *noData};
  result.Levels.push_back(std::move(*root));
  while (TIFFLastDirectory(raster.get()) == 0) {
    if (result.Levels.size() >= kMostLevels) {
      return std::unexpected(CopernicusFailure{.Problem = CopernicusProblem::CapacityRefused});
    }
    if (TIFFReadDirectory(raster.get()) != 1) { return std::unexpected(reader.Failure()); }
    if (reader.Needed) { return std::unexpected(reader.Failure()); }
    auto level = Level(raster.get(), original.TotalBytes);
    if (!level) { return std::unexpected(reader.Needed ? reader.Failure() : level.error()); }
    uint32_t subfile = 0;
    const auto &previous = result.Levels.back();
    const auto &base = result.Levels.front();
    if (TIFFGetField(raster.get(), TIFFTAG_SUBFILETYPE, &subfile) != 1 ||
        (subfile & FILETYPE_REDUCEDIMAGE) == 0 || level->Rows >= previous.Rows ||
        level->Columns >= previous.Columns) {
      return std::unexpected(CopernicusFailure{});
    }
    const double columns = static_cast<double>(base.Columns) / level->Columns;
    const double rows = static_cast<double>(base.Rows) / level->Rows;
    level->WestDeg = base.WestDeg + (columns - 1) * base.ColumnStepDeg * 0.5;
    level->NorthDeg = base.NorthDeg - (rows - 1) * base.RowStepDeg * 0.5;
    level->ColumnStepDeg = base.ColumnStepDeg * columns;
    level->RowStepDeg = base.RowStepDeg * rows;
    result.Levels.push_back(std::move(*level));
  }
  return result;
}

std::expected<CopernicusBlock, CopernicusFailure>
ReadCopernicusBlock(CopernicusObject original, size_t level, uint32_t block) {
  const auto metadata = ReadCopernicusRaster(original);
  if (!metadata) { return std::unexpected(metadata.error()); }
  return ReadCopernicusBlock(original, *metadata, level, block);
}

std::expected<CopernicusBlock, CopernicusFailure> ReadCopernicusBlock(
    CopernicusObject original, const CopernicusRaster &metadata, size_t level, uint32_t block) {
  if (auto valid = Validate(original); !valid) { return std::unexpected(valid.error()); }
  if (metadata.ObjectKey != original.ObjectKey || metadata.EntityTag != original.EntityTag ||
      metadata.TotalBytes != original.TotalBytes || level >= metadata.Levels.size() ||
      block >= metadata.Levels[level].Blocks.size()) {
    return std::unexpected(CopernicusFailure{});
  }
  const auto &layout = metadata.Levels[level];
  Reader reader{.Original = original};
  const auto raster = Open(reader);
  if (!raster || TIFFSetDirectory(raster.get(), static_cast<tdir_t>(level)) != 1) {
    return std::unexpected(reader.Failure());
  }
  const uint32_t across = (layout.Columns + layout.BlockColumns - 1) / layout.BlockColumns;
  const uint32_t firstColumn = (block % across) * layout.BlockColumns;
  const uint32_t firstRow = (block / across) * layout.BlockRows;
  CopernicusBlock result{
      .FirstColumn = firstColumn,
      .FirstRow = firstRow,
      .Columns = std::min(layout.BlockColumns, layout.Columns - firstColumn),
      .Rows = std::min(layout.BlockRows, layout.Rows - firstRow),
      .Stride = layout.BlockColumns,
      .HeightsM = std::vector<float>(static_cast<size_t>(layout.BlockColumns) * layout.BlockRows),
      .NoData = metadata.NoData};
  const auto bytes =
      static_cast<tmsize_t>(result.HeightsM.size()) * static_cast<tmsize_t>(sizeof(float));
  if (TIFFTileSize64(raster.get()) != static_cast<uint64_t>(bytes) ||
      TIFFReadEncodedTile(raster.get(), block, result.HeightsM.data(), bytes) != bytes) {
    return std::unexpected(reader.Failure());
  }
  return result;
}

}
