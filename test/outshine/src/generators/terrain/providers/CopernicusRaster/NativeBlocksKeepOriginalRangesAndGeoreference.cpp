#include "Check.h"
#include "CopernicusRaster.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <vector>
#include <unistd.h>
#include <tiffio.h>

namespace {
using namespace outshine;
using namespace outshine::Data;
using namespace outshine::Test;
constexpr std::string_view kRevision = "\"original-fixture\"";
constexpr std::string_view kObjectKey = "synthetic:original";

struct Fixture {
  std::string Path;
  std::vector<uint8_t> Bytes;
  bool Created = false;

  explicit Fixture(bool noData) {
    const char *temp = std::getenv("TMPDIR");
    Path = std::string(temp ? temp : "/tmp") + "/outshine-copernicus-XXXXXX";
    const int descriptor = mkstemp(Path.data());
    CHECK(descriptor >= 0, "fixture uses system temporary storage");
    if (descriptor < 0) { return; }
    Created = true;
    {
      const std::unique_ptr<TIFF, decltype(&TIFFClose)> raster(
          TIFFFdOpen(descriptor, Path.c_str(), "w"), &TIFFClose);
      CHECK(raster != nullptr, "independent libtiff fixture writer opens");
      if (!raster) {
        (void)close(descriptor);
        return;
      }
      const std::array<TIFFFieldInfo, 4> fields = {
          {{33550, TIFF_VARIABLE2, TIFF_VARIABLE2, TIFF_DOUBLE, FIELD_CUSTOM, 1, 1, "PixelScale"},
           {33922, TIFF_VARIABLE2, TIFF_VARIABLE2, TIFF_DOUBLE, FIELD_CUSTOM, 1, 1, "TiePoint"},
           {34735, TIFF_VARIABLE2, TIFF_VARIABLE2, TIFF_SHORT, FIELD_CUSTOM, 1, 1, "GeoKeys"},
           {TIFFTAG_GDAL_NODATA,
            TIFF_VARIABLE2,
            TIFF_VARIABLE2,
            TIFF_ASCII,
            FIELD_CUSTOM,
            1,
            1,
            "NoData"}}};
      CHECK(TIFFMergeFieldInfo(raster.get(), fields.data(), fields.size()) == 0,
            "test writer registers metadata on this file only");
      for (uint32_t level = 0; level < 2; ++level) {
        const uint32_t columns = level == 0 ? 32 : 16, rows = level == 0 ? 48 : 24;
        CHECK(TIFFSetField(raster.get(), TIFFTAG_IMAGEWIDTH, columns) == 1 &&
                  TIFFSetField(raster.get(), TIFFTAG_IMAGELENGTH, rows) == 1 &&
                  TIFFSetField(raster.get(), TIFFTAG_TILEWIDTH, 16u) == 1 &&
                  TIFFSetField(raster.get(), TIFFTAG_TILELENGTH, 16u) == 1 &&
                  TIFFSetField(raster.get(), TIFFTAG_BITSPERSAMPLE, 32) == 1 &&
                  TIFFSetField(raster.get(), TIFFTAG_SAMPLESPERPIXEL, 1) == 1 &&
                  TIFFSetField(raster.get(), TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_IEEEFP) == 1 &&
                  TIFFSetField(raster.get(), TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG) == 1 &&
                  TIFFSetField(raster.get(), TIFFTAG_COMPRESSION, COMPRESSION_ADOBE_DEFLATE) == 1 &&
                  TIFFSetField(raster.get(), TIFFTAG_PREDICTOR, PREDICTOR_FLOATINGPOINT) == 1 &&
                  TIFFSetField(raster.get(), TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK) == 1 &&
                  TIFFSetField(raster.get(),
                               TIFFTAG_SUBFILETYPE,
                               level == 0 ? 0u : FILETYPE_REDUCEDIMAGE) == 1,
              "fixture has Float32, Deflate and floating-point predictor");
        if (level == 0) {
          const std::array<double, 3> scale{1.0 / columns, 1.0 / rows, 0};
          const std::array<double, 6> tie{0, 0, 0, 9, 55, 0};
          const std::array<uint16_t, 20> keys{1, 1, 0,    4, 1024, 0,    1,    2, 1025, 0,
                                              1, 2, 2048, 0, 1,    4326, 2054, 0, 1,    9102};
          CHECK(TIFFSetField(raster.get(), 33550, scale.size(), scale.data()) == 1 &&
                    TIFFSetField(raster.get(), 33922, tie.size(), tie.data()) == 1 &&
                    TIFFSetField(raster.get(), 34735, keys.size(), keys.data()) == 1,
                "native sample georeference is explicit and non-square");
          if (noData) {
            CHECK(TIFFSetField(raster.get(), TIFFTAG_GDAL_NODATA, 6u, "-9999") == 1,
                  "declared NoData is distinct from physical zero");
          }
        }
        for (uint32_t row = 0; row < rows; row += 16) {
          for (uint32_t column = 0; column < columns; column += 16) {
            std::array<float, 16 * 16> heights{};
            for (uint32_t y = 0; y < 16; ++y) {
              for (uint32_t x = 0; x < 16; ++x) {
                heights[y * 16 + x] = level == 0 ? 2.0f * (row + y) + 3.0f * (column + x) - 50.0f
                                                 : 4.0f * (row + y) + 6.0f * (column + x) - 47.5f;
              }
            }
            if (level == 0 && row == 0 && column == 0) {
              heights[0] = noData ? -9999.0f : -32767.0f;
              heights[17] = std::numeric_limits<float>::quiet_NaN();
              heights[18] = std::numeric_limits<float>::infinity();
            }
            CHECK(TIFFWriteEncodedTile(raster.get(),
                                       TIFFComputeTile(raster.get(), column, row, 0, 0),
                                       heights.data(),
                                       sizeof(heights)) == sizeof(heights),
                  "independent analytic samples are encoded");
          }
        }
        CHECK(TIFFWriteDirectory(raster.get()) == 1, "directory is complete");
      }
    }
    const auto file = std::unique_ptr<std::FILE, decltype(&std::fclose)>(
        std::fopen(Path.c_str(), "rb"), &std::fclose);
    CHECK(file != nullptr, "encoded fixture reopens as immutable bytes");
    if (!file) { return; }
    CHECK(std::fseek(file.get(), 0, SEEK_END) == 0, "fixture size is available");
    const long size = std::ftell(file.get());
    CHECK(size > 0, "fixture is not empty");
    if (size <= 0) { return; }
    Bytes.resize(static_cast<size_t>(size));
    std::rewind(file.get());
    CHECK(std::fread(Bytes.data(), 1, Bytes.size(), file.get()) == Bytes.size(),
          "original fixture bytes are retained without raster repacking");
  }

  ~Fixture() {
    if (Created) { (void)std::remove(Path.c_str()); }
  }
};

CopernicusPart
Part(std::span<const uint8_t> all, ByteRange range, std::string_view revision = kRevision) {
  return {.ObjectKey = kObjectKey,
          .Origin = {.Bytes = range, .TotalBytes = all.size(), .EntityTag = std::string(revision)},
          .Bytes = all.subspan(range.First, range.Length)};
}

}

int main() {
  Fixture fixture(false);
  if (fixture.Bytes.empty()) { return Report(); }
  std::array<CopernicusPart, 1> complete{
      Part(fixture.Bytes, {.First = 0, .Length = fixture.Bytes.size()})};
  const CopernicusObject original{.ObjectKey = kObjectKey,
                                  .TotalBytes = fixture.Bytes.size(),
                                  .EntityTag = kRevision,
                                  .Parts = complete};
  const auto metadata = ReadCopernicusRaster(original);
  CHECK(metadata && metadata->Levels.size() == 2 && !metadata->NoData,
        "native TIFF metadata needs no assumed NoData sentinel");
  if (!metadata) { return Report(); }
  const auto &native = metadata->Levels[0], &overview = metadata->Levels[1];
  CHECK(native.Columns == 32 && native.Rows == 48 && native.Blocks.size() == 6,
        "non-square native dimensions and all original block ranges are retained");
  CHECK(native.WestDeg == 9 && native.NorthDeg == 55 && native.ColumnStepDeg == 1.0 / 32 &&
            native.RowStepDeg == 1.0 / 48,
        "PixelIsPoint ties describe sample centers");
  CHECK(overview.Columns == 16 && overview.Rows == 24 &&
            std::abs(overview.WestDeg - (9 + 0.5 / 32)) < 1e-12 &&
            std::abs(overview.NorthDeg - (55 - 0.5 / 48)) < 1e-12,
        "averaged overview samples move to the centers of their source footprints");
  const auto block = ReadCopernicusBlock(original, *metadata, 0, 3);
  CHECK(block && block->FirstRow == 16 && block->FirstColumn == 16 && block->HeightM(1, 2) == 38.0f,
        "Deflate and float predictor preserve independently specified analytic heights");
  const auto first = ReadCopernicusBlock(original, 0, 0);
  CHECK(first && first->HeightM(0, 0) == -32767.0f && !first->HeightM(1, 1) &&
            !first->HeightM(1, 2),
        "undeclared sentinels remain data while nonfinite samples remain missing");
  const auto edge = ReadCopernicusBlock(original, 1, 1);
  CHECK(edge && edge->Rows == 8 && edge->Columns == 16 && !edge->HeightM(8, 0) &&
            edge->HeightM(0, 0) == 16 * 4.0f - 47.5f,
        "overview edge padding is never exposed as native terrain");

  std::vector<ByteRange> encoded;
  for (const auto &level : metadata->Levels) {
    encoded.insert(encoded.end(), level.Blocks.begin(), level.Blocks.end());
  }
  std::ranges::sort(encoded, {}, &ByteRange::First);
  std::vector<CopernicusPart> sparse;
  uint64_t at = 0;
  for (const auto &range : encoded) {
    if (at < range.First) {
      sparse.push_back(Part(fixture.Bytes, {.First = at, .Length = range.First - at}));
    }
    at = range.First + range.Length;
  }
  if (at < fixture.Bytes.size()) {
    sparse.push_back(Part(fixture.Bytes, {.First = at, .Length = fixture.Bytes.size() - at}));
  }
  auto source = original;
  source.Parts = sparse;
  CHECK(ReadCopernicusRaster(source), "metadata does not require any compressed height block");
  const auto waiting = ReadCopernicusBlock(source, 0, 3);
  CHECK(!waiting && waiting.error().Problem == CopernicusProblem::MissingBytes &&
            waiting.error().Needed && *waiting.error().Needed == native.Blocks[3],
        "absent original bytes return exact bounded demand instead of zero heights or IO");
  sparse.push_back(Part(fixture.Bytes, native.Blocks[3]));
  std::ranges::sort(sparse, [](const auto &a, const auto &b) {
    return a.Origin.Bytes.First < b.Origin.Bytes.First;
  });
  source.Parts = sparse;
  CHECK(ReadCopernicusBlock(source, 0, 3),
        "landing only the requested original range resumes decode");
  sparse[0].ObjectKey = "another-object";
  CHECK(!ReadCopernicusRaster(source),
        "matching ETags cannot substitute a different source object");
  sparse[0].ObjectKey = kObjectKey;
  auto foreignMetadata = *metadata;
  foreignMetadata.ObjectKey = "another-object";
  CHECK(!ReadCopernicusBlock(source, foreignMetadata, 0, 3),
        "cached metadata belongs to the exact source object");
  sparse[0].Origin.EntityTag = "\"another-version\"";
  CHECK(!ReadCopernicusRaster(source), "mixed object revisions are rejected before interpretation");
  sparse[0].Origin.EntityTag = kRevision;
  --sparse[0].Origin.TotalBytes;
  CHECK(!ReadCopernicusRaster(source), "mixed object lengths are rejected");
  auto oversized = original;
  oversized.TotalBytes = uint64_t{129} * 1024 * 1024;
  CHECK(!ReadCopernicusRaster(oversized) &&
            ReadCopernicusRaster(oversized).error().Problem == CopernicusProblem::CapacityRefused,
        "object and decoder budgets are enforced independently of successful decompression");
  CHECK(!ReadCopernicusBlock(original, 8, 0) && !ReadCopernicusBlock(original, 0, 64),
        "unavailable levels and blocks do not clamp to invented samples");

  Fixture missing(true);
  if (!missing.Bytes.empty()) {
    const std::array<CopernicusPart, 1> parts{
        Part(missing.Bytes, {.First = 0, .Length = missing.Bytes.size()})};
    const CopernicusObject declared{.ObjectKey = kObjectKey,
                                    .TotalBytes = missing.Bytes.size(),
                                    .EntityTag = kRevision,
                                    .Parts = parts};
    const auto nodata = ReadCopernicusBlock(declared, 0, 0);
    CHECK(nodata && nodata->NoData == -9999.0f && !nodata->HeightM(0, 0) &&
              nodata->HeightM(0, 1) == -47.0f,
          "explicit NoData removes only missing samples, not surrounding terrain");
  }
  return Report();
}
