#ifndef OUTSHINE_WORLD_DATA_COPERNICUSRASTER_H
#define OUTSHINE_WORLD_DATA_COPERNICUSRASTER_H

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <world/data/Transport.h>

namespace outshine::Data {

struct CopernicusPart {
  std::string_view ObjectKey;
  RangeResponse Origin;
  std::span<const uint8_t> Bytes;
};

struct CopernicusObject {
  std::string_view ObjectKey;
  uint64_t TotalBytes = 0;
  std::string_view EntityTag;
  std::span<const CopernicusPart> Parts;
};

enum class CopernicusProblem { MissingBytes, InvalidRaster, CapacityRefused };

struct CopernicusFailure {
  CopernicusProblem Problem = CopernicusProblem::InvalidRaster;
  std::optional<ByteRange> Needed;
};

struct CopernicusLevel {
  uint32_t Columns = 0, Rows = 0;
  uint32_t BlockColumns = 0, BlockRows = 0;
  double WestDeg = 0, NorthDeg = 0;
  double ColumnStepDeg = 0, RowStepDeg = 0;
  std::vector<ByteRange> Blocks;
};

struct CopernicusRaster {
  std::string ObjectKey;
  uint64_t TotalBytes = 0;
  std::string EntityTag;
  std::vector<CopernicusLevel> Levels;
  std::optional<float> NoData;
};

struct CopernicusBlock {
  uint32_t FirstColumn = 0, FirstRow = 0;
  uint32_t Columns = 0, Rows = 0;
  uint32_t Stride = 0;
  std::vector<float> HeightsM;
  std::optional<float> NoData;

  [[nodiscard]] std::optional<float> HeightM(uint32_t row, uint32_t column) const noexcept;
};

[[nodiscard]] std::expected<CopernicusRaster, CopernicusFailure>
ReadCopernicusRaster(CopernicusObject original);

[[nodiscard]] std::expected<CopernicusBlock, CopernicusFailure>
ReadCopernicusBlock(CopernicusObject original, size_t level, uint32_t block);

[[nodiscard]] std::expected<CopernicusBlock, CopernicusFailure> ReadCopernicusBlock(
    CopernicusObject original, const CopernicusRaster &metadata, size_t level, uint32_t block);

}
#endif
