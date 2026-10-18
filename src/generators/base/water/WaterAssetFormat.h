#ifndef OUTSHINE_GENERATORS_BASE_WATER_WATERASSETFORMAT_H
#define OUTSHINE_GENERATORS_BASE_WATER_WATERASSETFORMAT_H

#include "WaterAsset.h"
#include "BinaryValueArchive.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace outshine::Generators::WaterFormat {
template <size_t MinimumBytes, class T, class Fields>
bool Records(BinaryValueReader &archive, std::vector<T> &records, Fields fields) {
  uint64_t count = 0;
  if (!archive(count) || count > archive.In.Remaining() / MinimumBytes ||
      count > records.max_size()) {
    return false;
  }
  records.resize(static_cast<size_t>(count));
  return std::ranges::all_of(records, [&](auto &record) { return fields(archive, record); });
}

template <size_t MinimumBytes, class T, class Fields>
bool Records(BinaryValueWriter &archive, const std::vector<T> &records, Fields fields) {
  return archive(static_cast<uint64_t>(records.size())) &&
         std::ranges::all_of(records, [&](const auto &record) { return fields(archive, record); });
}

bool Metadata(BinaryValueWriter &out, const WaterAsset::Data &data);
bool Metadata(BinaryValueReader &in, WaterAsset::Data &data);
[[nodiscard]] bool ValidMetadata(const WaterAsset::Data &data) noexcept;
}
#endif
