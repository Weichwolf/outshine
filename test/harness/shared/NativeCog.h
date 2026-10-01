#ifndef OUTSHINE_TEST_NATIVECOG_H
#define OUTSHINE_TEST_NATIVECOG_H

#include "Check.h"
#include <world/data/Address.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
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

inline std::vector<uint8_t> NativeCog(Data::CellId at,
                                      bool missing = false,
                                      NativeCogProfile profile = NativeCogProfile::Analytic) {
  const char *temp = std::getenv("TMPDIR");
  std::string path = std::string(temp ? temp : "/tmp") + "/outshine-native-cog-XXXXXX";
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
    const std::array<TIFFFieldInfo, 3> fields = {
        {{33550, TIFF_VARIABLE2, TIFF_VARIABLE2, TIFF_DOUBLE, FIELD_CUSTOM, 1, 1, "PixelScale"},
         {33922, TIFF_VARIABLE2, TIFF_VARIABLE2, TIFF_DOUBLE, FIELD_CUSTOM, 1, 1, "TiePoint"},
         {34735, TIFF_VARIABLE2, TIFF_VARIABLE2, TIFF_SHORT, FIELD_CUSTOM, 1, 1, "GeoKeys"}}};
    CHECK(TIFFMergeFieldInfo(file.get(), fields.data(), fields.size()) == 0,
          "fixture registers georeference");
    const bool large = profile != NativeCogProfile::Analytic;
    const uint32_t baseColumns = large ? 1024 : (at.SouthDeg % 2 == 0 ? 128 : 64);
    const uint32_t baseRows = large ? 1024 : 128;
    const uint32_t blockSide = large ? 1024 : 16;
    for (uint32_t level = 0; level < (large ? 1u : 2u); ++level) {
      const uint32_t columns = baseColumns >> level, rows = baseRows >> level;
      CHECK(TIFFSetField(file.get(), TIFFTAG_IMAGEWIDTH, columns) == 1 &&
                TIFFSetField(file.get(), TIFFTAG_IMAGELENGTH, rows) == 1 &&
                TIFFSetField(file.get(), TIFFTAG_TILEWIDTH, blockSide) == 1 &&
                TIFFSetField(file.get(), TIFFTAG_TILELENGTH, blockSide) == 1 &&
                TIFFSetField(file.get(), TIFFTAG_BITSPERSAMPLE, 32) == 1 &&
                TIFFSetField(file.get(), TIFFTAG_SAMPLESPERPIXEL, 1) == 1 &&
                TIFFSetField(file.get(), TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_IEEEFP) == 1 &&
                TIFFSetField(file.get(), TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG) == 1 &&
                TIFFSetField(file.get(), TIFFTAG_COMPRESSION, COMPRESSION_ADOBE_DEFLATE) == 1 &&
                TIFFSetField(file.get(), TIFFTAG_PREDICTOR, PREDICTOR_FLOATINGPOINT) == 1 &&
                TIFFSetField(file.get(), TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK) == 1 &&
                TIFFSetField(
                    file.get(), TIFFTAG_SUBFILETYPE, level == 0 ? 0u : FILETYPE_REDUCEDIMAGE) == 1,
            "analytic fixture has native float samples and an averaged overview");
      if (level == 0) {
        const std::array<double, 3> scale{1.0 / columns, 1.0 / rows, 0};
        const std::array<double, 6> tie{
            0, 0, 0, static_cast<double>(at.WestDeg), static_cast<double>(at.SouthDeg + 1), 0};
        const std::array<uint16_t, 20> keys{1, 1, 0,    4, 1024, 0,    1,    2, 1025, 0,
                                            1, 2, 2048, 0, 1,    4326, 2054, 0, 1,    9102};
        CHECK(TIFFSetField(file.get(), 33550, scale.size(), scale.data()) == 1 &&
                  TIFFSetField(file.get(), 33922, tie.size(), tie.data()) == 1 &&
                  TIFFSetField(file.get(), 34735, keys.size(), keys.data()) == 1,
              "fixture names its geographic source cell independently of Mercator");
      }
      const double offsetX = level == 0 ? 0 : 0.5 / baseColumns;
      const double offsetY = level == 0 ? 0 : 0.5 / baseRows;
      std::vector<float> heights(static_cast<size_t>(blockSide) * blockSide);
      uint32_t noise = 1;
      for (uint32_t row = 0; row < rows; row += blockSide) {
        for (uint32_t col = 0; col < columns; col += blockSide) {
          for (uint32_t y = 0; y < blockSide; ++y) {
            for (uint32_t x = 0; x < blockSide; ++x) {
              const size_t index = static_cast<size_t>(y) * blockSide + x;
              heights[index] =
                  missing ? std::numeric_limits<float>::quiet_NaN()
                          : NativeHeight(
                                at.SouthDeg + 1 - offsetY - static_cast<double>(row + y) / rows,
                                at.WestDeg + offsetX + static_cast<double>(col + x) / columns);
              if (large && !missing && (row + y < rows / 2 || col + x >= columns / 2)) {
                noise ^= noise << 13;
                noise ^= noise >> 17;
                noise ^= noise << 5;
                heights[index] += static_cast<float>(noise & 0x00ffffffu) / 65536.0f;
              }
            }
          }
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
