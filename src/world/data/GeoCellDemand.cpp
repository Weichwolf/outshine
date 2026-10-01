#include <world/data/GeoCellId.h>
#include "math/Units.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <numbers>
#include <string>
#include <vector>

namespace outshine::Data {
namespace {
struct Columns {
  uint32_t First = 0;
  uint32_t Last = 0;
};

uint32_t CellAt(double coordinate, double halfRange, uint32_t count) {
  const double index =
      std::floor((coordinate + halfRange) * static_cast<double>(count) / (2.0 * halfRange));
  return static_cast<uint32_t>(std::clamp(index, 0.0, static_cast<double>(count - 1)));
}
}

std::expected<std::vector<GeoCellId>, std::string>
CellsAround(double latitudeDeg, double longitudeDeg, double radiusM, int level, size_t cellsMost) {
  if (!std::isfinite(latitudeDeg) || !std::isfinite(longitudeDeg) || !std::isfinite(radiusM) ||
      latitudeDeg < -90.0 || latitudeDeg > 90.0 || longitudeDeg < -180.0 || longitudeDeg > 180.0 ||
      radiusM < 0.0 || level < 0 || level > GeoCellId::MaximumLevel || cellsMost == 0) {
    return std::unexpected("invalid geographic source demand");
  }
  const uint32_t count = uint32_t{1} << static_cast<unsigned>(level);
  const double minimumRadiusM = kWgs84A * (1.0 - kWgs84E2);
  const double angular = std::min(std::numbers::pi, radiusM / minimumRadiusM);
  const double latitude = latitudeDeg * kDeg2Rad;
  const double latitudeSpan = angular * kRad2Deg;
  const double south = std::max(-90.0, std::nextafter(latitudeDeg - latitudeSpan, -INFINITY));
  const double north = std::min(90.0, std::nextafter(latitudeDeg + latitudeSpan, INFINITY));
  const uint32_t firstRow = CellAt(south, 90.0, count);
  const uint32_t lastRow = CellAt(north, 90.0, count);
  std::array<Columns, 2> columns{};
  size_t ranges = 1;
  if (south == -90.0 || north == 90.0) {
    columns[0] = {0, count - 1};
  } else {
    const double longitudeSpan =
        std::asin(std::clamp(std::sin(angular) / std::cos(latitude), 0.0, 1.0)) * kRad2Deg;
    const double west = std::nextafter(longitudeDeg - longitudeSpan, -INFINITY);
    const double east = std::nextafter(longitudeDeg + longitudeSpan, INFINITY);
    if (west < -180.0) {
      columns[0] = {0, CellAt(east, 180.0, count)};
      columns[1] = {CellAt(west + 360.0, 180.0, count), count - 1};
      ranges = 2;
    } else if (east > 180.0) {
      columns[0] = {0, CellAt(east - 360.0, 180.0, count)};
      columns[1] = {CellAt(west, 180.0, count), count - 1};
      ranges = 2;
    } else {
      columns[0] = {CellAt(west, 180.0, count), CellAt(east, 180.0, count)};
    }
  }
  if (ranges == 2 && columns[0].Last >= columns[1].First) {
    columns[0] = {0, count - 1};
    ranges = 1;
  }
  const size_t rows = static_cast<size_t>(lastRow - firstRow) + 1;
  size_t total = 0;
  for (size_t at = 0; at < ranges; ++at) {
    const auto range = columns[at];
    const size_t width = static_cast<size_t>(range.Last - range.First) + 1;
    if (width > (cellsMost - total) / rows) {
      return std::unexpected("geographic source demand exceeds the cell admission limit");
    }
    total += width * rows;
  }
  std::vector<GeoCellId> cells;
  cells.reserve(total);
  for (size_t at = 0; at < ranges; ++at) {
    const auto range = columns[at];
    for (uint32_t x = range.First; x <= range.Last; ++x) {
      for (uint32_t y = firstRow; y <= lastRow; ++y) {
        cells.push_back({.Level = level, .X = x, .Y = y});
      }
    }
  }
  return cells;
}
}
