#ifndef OUTSHINE_TEST_NATIVECOG_H
#define OUTSHINE_TEST_NATIVECOG_H

#include "Check.h"
#include <Address.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <span>
#include <system_error>
#include <limits>
#include <memory>
#include <string>
#include <vector>
#include <unistd.h>
#include <tiffio.h>

namespace outshine::Test {

enum class NativeCogProfile { Analytic, LargeBlock, LargeBlockOverlapsHeader };

inline float NativeHeight(double latitude, double longitude) {
  return static_cast<float>(10 * latitude + 2 * longitude - 600);
}

struct NativeCogTile {
  Data::CellId Cell;
  uint32_t Columns;
  uint32_t Rows;
  uint32_t Side;
  uint32_t Row;
  uint32_t Column;
  double OffsetX;
  double OffsetY;
  bool Missing;
  bool Large;
};

inline void
DeclareNativeCogDirectory(TIFF *file, Data::CellId at, const NativeCogTile &tile, uint32_t level) {
  CHECK(TIFFSetField(file, TIFFTAG_IMAGEWIDTH, tile.Columns) == 1 &&
            TIFFSetField(file, TIFFTAG_IMAGELENGTH, tile.Rows) == 1 &&
            TIFFSetField(file, TIFFTAG_TILEWIDTH, tile.Side) == 1 &&
            TIFFSetField(file, TIFFTAG_TILELENGTH, tile.Side) == 1 &&
            TIFFSetField(file, TIFFTAG_BITSPERSAMPLE, 32) == 1 &&
            TIFFSetField(file, TIFFTAG_SAMPLESPERPIXEL, 1) == 1 &&
            TIFFSetField(file, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_IEEEFP) == 1 &&
            TIFFSetField(file, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG) == 1 &&
            TIFFSetField(file, TIFFTAG_COMPRESSION, COMPRESSION_ADOBE_DEFLATE) == 1 &&
            TIFFSetField(file, TIFFTAG_PREDICTOR, PREDICTOR_FLOATINGPOINT) == 1 &&
            TIFFSetField(file, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK) == 1 &&
            TIFFSetField(file, TIFFTAG_SUBFILETYPE, level == 0 ? 0u : FILETYPE_REDUCEDIMAGE) == 1,
        "analytic fixture has native float samples and an averaged overview");
  if (level == 0) {
    const std::array<double, 3> scale{1.0 / tile.Columns, 1.0 / tile.Rows, 0};
    const std::array<double, 6> tie{
        0, 0, 0, static_cast<double>(at.WestDeg), static_cast<double>(at.SouthDeg + 1), 0};
    const std::array<uint16_t, 20> keys{1, 1, 0,    4, 1024, 0,    1,    2, 1025, 0,
                                        1, 2, 2048, 0, 1,    4326, 2054, 0, 1,    9102};
    CHECK(TIFFSetField(file, 33550, scale.size(), scale.data()) == 1 &&
              TIFFSetField(file, 33922, tie.size(), tie.data()) == 1 &&
              TIFFSetField(file, 34735, keys.size(), keys.data()) == 1,
          "fixture names its geographic source cell independently of Mercator");
  }
}

inline void
FillNativeCogTile(std::span<float> heights, const NativeCogTile &tile, uint32_t &noise) {
  for (uint32_t y = 0; y < tile.Side; ++y) {
    for (uint32_t x = 0; x < tile.Side; ++x) {
      const size_t index = static_cast<size_t>(y) * tile.Side + x;
      heights[index] = tile.Missing
                           ? std::numeric_limits<float>::quiet_NaN()
                           : NativeHeight(tile.Cell.SouthDeg + 1 - tile.OffsetY -
                                              static_cast<double>(tile.Row + y) / tile.Rows,
                                          tile.Cell.WestDeg + tile.OffsetX +
                                              static_cast<double>(tile.Column + x) / tile.Columns);
      if (tile.Large && !tile.Missing &&
          (tile.Row + y < tile.Rows / 2 || tile.Column + x >= tile.Columns / 2)) {
        noise ^= noise << 13;
        noise ^= noise >> 17;
        noise ^= noise << 5;
        heights[index] += static_cast<float>(noise & 0x00ffffffu) / 65536.0f;
      }
    }
  }
}

inline std::vector<uint8_t> NativeCog(Data::CellId at,
                                      bool missing = false,
                                      NativeCogProfile profile = NativeCogProfile::Analytic) {
  std::error_code error;
  const auto temporary = std::filesystem::temp_directory_path(error);
  CHECK(!error, "native fixture has a temporary directory");
  if (error) { return {}; }
  std::string path = (temporary / "outshine-native-cog-XXXXXX").string();
  const int descriptor = mkstemp(path.data());
  CHECK(descriptor >= 0, "analytic COG fixture has a temporary file");
  if (descriptor < 0) { return {}; }
  {
    const std::unique_ptr<TIFF, decltype(&TIFFClose)> file(
        TIFFFdOpen(descriptor, path.c_str(), "w"), &TIFFClose);
    CHECK(file != nullptr, "independent native fixture writer opens");
    if (!file) {
      (void)close(descriptor);
      (void)std::remove(path.c_str());
      return {};
    }
    // Original blocks stay outside the initial header so the runtime must fetch pinned ranges.
    CHECK(ftruncate(descriptor,
                    profile == NativeCogProfile::LargeBlockOverlapsHeader ? 8 : 32768) == 0,
          "native fixture separates metadata and original blocks");
    const std::array<TIFFFieldInfo, 3> fields = {{{.field_tag = 33550,
                                                   .field_readcount = TIFF_VARIABLE2,
                                                   .field_writecount = TIFF_VARIABLE2,
                                                   .field_type = TIFF_DOUBLE,
                                                   .field_bit = FIELD_CUSTOM,
                                                   .field_oktochange = 1,
                                                   .field_passcount = 1,
                                                   .field_name = "PixelScale"},
                                                  {.field_tag = 33922,
                                                   .field_readcount = TIFF_VARIABLE2,
                                                   .field_writecount = TIFF_VARIABLE2,
                                                   .field_type = TIFF_DOUBLE,
                                                   .field_bit = FIELD_CUSTOM,
                                                   .field_oktochange = 1,
                                                   .field_passcount = 1,
                                                   .field_name = "TiePoint"},
                                                  {.field_tag = 34735,
                                                   .field_readcount = TIFF_VARIABLE2,
                                                   .field_writecount = TIFF_VARIABLE2,
                                                   .field_type = TIFF_SHORT,
                                                   .field_bit = FIELD_CUSTOM,
                                                   .field_oktochange = 1,
                                                   .field_passcount = 1,
                                                   .field_name = "GeoKeys"}}};
    CHECK(TIFFMergeFieldInfo(file.get(), fields.data(), fields.size()) == 0,
          "fixture registers georeference");
    const bool large = profile != NativeCogProfile::Analytic;
    const uint32_t baseColumns = large ? 1024 : (at.SouthDeg % 2 == 0 ? 128 : 64);
    const uint32_t baseRows = large ? 1024 : 128;
    const uint32_t blockSide = large ? 1024 : 16;
    for (uint32_t level = 0; level < (large ? 1u : 2u); ++level) {
      const uint32_t columns = baseColumns >> level;
      const uint32_t rows = baseRows >> level;
      const NativeCogTile layout{.Cell = at,
                                 .Columns = columns,
                                 .Rows = rows,
                                 .Side = blockSide,
                                 .Row = 0,
                                 .Column = 0,
                                 .OffsetX = 0,
                                 .OffsetY = 0,
                                 .Missing = missing,
                                 .Large = large};
      DeclareNativeCogDirectory(file.get(), at, layout, level);
      const double offsetX = level == 0 ? 0 : 0.5 / baseColumns;
      const double offsetY = level == 0 ? 0 : 0.5 / baseRows;
      std::vector<float> heights(static_cast<size_t>(blockSide) * blockSide);
      uint32_t noise = 1;
      for (uint32_t row = 0; row < rows; row += blockSide) {
        for (uint32_t col = 0; col < columns; col += blockSide) {
          const NativeCogTile tile{.Cell = at,
                                   .Columns = columns,
                                   .Rows = rows,
                                   .Side = blockSide,
                                   .Row = row,
                                   .Column = col,
                                   .OffsetX = offsetX,
                                   .OffsetY = offsetY,
                                   .Missing = missing,
                                   .Large = large};
          FillNativeCogTile(heights, tile, noise);
          CHECK(TIFFWriteEncodedTile(file.get(),
                                     TIFFComputeTile(file.get(), col, row, 0, 0),
                                     heights.data(),
                                     static_cast<tmsize_t>(heights.size() * sizeof(float))) ==
                    static_cast<tmsize_t>(heights.size() * sizeof(float)),
                "native analytic samples are encoded by an independent writer");
        }
      }
      CHECK(TIFFWriteDirectory(file.get()) == 1, "native fixture directory completes");
    }
  }
  std::vector<uint8_t> result;
  const std::unique_ptr<std::FILE, decltype(&std::fclose)> file(std::fopen(path.c_str(), "rb"),
                                                                &std::fclose);
  if (file && std::fseek(file.get(), 0, SEEK_END) == 0) {
    const long bytes = std::ftell(file.get());
    if (bytes > 0) {
      result.resize(static_cast<size_t>(bytes));
      std::rewind(file.get());
      CHECK(std::fread(result.data(), 1, result.size(), file.get()) == result.size(),
            "original fixture bytes reopen");
      result.resize(std::max<size_t>(result.size(), 65536), 0);
    }
  }
  (void)std::remove(path.c_str());
  return result;
}
}
#endif
