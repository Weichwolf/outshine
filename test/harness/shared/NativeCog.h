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

inline float NativeHeight(double latitude, double longitude) {
  return static_cast<float>(10 * latitude + 2 * longitude - 600);
}

inline std::vector<uint8_t> NativeCog(Data::CellId at, bool missing = false) {
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
    CHECK(ftruncate(descriptor, 32768) == 0,
          "native fixture separates metadata and original blocks");
    const std::array<TIFFFieldInfo, 3> fields = {
        {{33550, TIFF_VARIABLE2, TIFF_VARIABLE2, TIFF_DOUBLE, FIELD_CUSTOM, 1, 1, "PixelScale"},
         {33922, TIFF_VARIABLE2, TIFF_VARIABLE2, TIFF_DOUBLE, FIELD_CUSTOM, 1, 1, "TiePoint"},
         {34735, TIFF_VARIABLE2, TIFF_VARIABLE2, TIFF_SHORT, FIELD_CUSTOM, 1, 1, "GeoKeys"}}};
    CHECK(TIFFMergeFieldInfo(file.get(), fields.data(), fields.size()) == 0,
          "fixture registers georeference");
    const uint32_t baseColumns = at.SouthDeg % 2 == 0 ? 128 : 64;
    for (uint32_t level = 0; level < 2; ++level) {
      const uint32_t columns = baseColumns >> level, rows = 128u >> level;
      CHECK(TIFFSetField(file.get(), TIFFTAG_IMAGEWIDTH, columns) == 1 &&
                TIFFSetField(file.get(), TIFFTAG_IMAGELENGTH, rows) == 1 &&
                TIFFSetField(file.get(), TIFFTAG_TILEWIDTH, 16u) == 1 &&
                TIFFSetField(file.get(), TIFFTAG_TILELENGTH, 16u) == 1 &&
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
      const double offsetY = level == 0 ? 0 : 0.5 / 128;
      for (uint32_t row = 0; row < rows; row += 16) {
        for (uint32_t col = 0; col < columns; col += 16) {
          std::array<float, 256> heights;
          for (uint32_t y = 0; y < 16; ++y) {
            for (uint32_t x = 0; x < 16; ++x) {
              heights[y * 16 + x] =
                  missing ? std::numeric_limits<float>::quiet_NaN()
                          : NativeHeight(
                                at.SouthDeg + 1 - offsetY - static_cast<double>(row + y) / rows,
                                at.WestDeg + offsetX + static_cast<double>(col + x) / columns);
            }
          }
          CHECK(TIFFWriteEncodedTile(file.get(),
                                     TIFFComputeTile(file.get(), col, row, 0, 0),
                                     heights.data(),
                                     sizeof(heights)) == sizeof(heights),
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
