#ifndef OUTSHINE_WORLD_DATA_SOURCERANGE_H
#define OUTSHINE_WORLD_DATA_SOURCERANGE_H

#include <cstddef>
#include <limits>
#include <optional>
#include <span>
#include <vector>
#include <world/data/Transport.h>

namespace outshine::Data {

inline constexpr size_t MaximumRangeRecordOverhead =
    MaximumEntityTagBytes + size_t{3} * (std::numeric_limits<uint64_t>::digits10 + 1) + 4 + 8 + 64;

[[nodiscard]] std::optional<std::vector<uint8_t>> PackSourceRange(std::span<const uint8_t> bytes,
                                                                  const RangeResponse &origin);

[[nodiscard]] std::optional<RangeResponse> UnpackSourceRange(std::vector<uint8_t> &record);

}
#endif
