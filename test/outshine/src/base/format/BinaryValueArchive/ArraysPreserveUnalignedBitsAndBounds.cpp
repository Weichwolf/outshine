#include "BinaryValueArchive.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;

template <typename T> bool SameBits(std::span<const T> a, std::span<const T> b) {
  return std::ranges::equal(std::as_bytes(a), std::as_bytes(b));
}

template <typename T, size_t N> void RoundTrip(const std::array<T, N> &values) {
  constexpr size_t kLimit = 4096;
  constexpr std::array<uint8_t, 3> kPrefix{1, 2, 3};
  constexpr uint8_t kMarker = 0x5a;
  BinaryValueWriter writer(kLimit);
  CHECK(writer.Out.Put(kPrefix) && writer.Array(std::span<const T>(values)) && writer(kMarker),
        "the unaligned array fixture encodes");
  for (const bool reused : {false, true}) {
    std::vector<T> decoded(reused ? values.size() * 2 + 1 : 0);
    BinaryValueReader reader(writer.Out.Bytes());
    CHECK(reader.In.Take(kPrefix.size()).has_value() && reader.Array(decoded),
          "fresh and already allocated arrays decode");
    CHECK(SameBits<T>(decoded, values), "every element retains its complete binary representation");
    uint8_t marker = 0;
    CHECK(reader(marker) && marker == kMarker && reader.In.Remaining() == 0,
          "decoding consumes exactly the declared array bytes");
  }
  std::vector<uint8_t> truncated(writer.Out.Bytes().begin(), writer.Out.Bytes().end() - 2);
  BinaryValueReader reader(truncated);
  CHECK(reader.In.Take(kPrefix.size()).has_value(), "truncated input retains its prefix");
  std::vector<T> preserved(values.begin(), values.end());
  CHECK(!reader.Array(preserved) && SameBits<T>(preserved, values),
        "truncation fails before changing the destination");
  BinaryValueWriter empty(kLimit);
  CHECK(empty.Array(std::span<const T>{}), "an empty array encodes");
  BinaryValueReader clear(empty.Out.Bytes());
  CHECK(clear.Array(preserved) && preserved.empty(), "an empty array clears existing storage");
}
}

int main() {
  constexpr auto kByteMax = std::numeric_limits<uint8_t>::max();
  constexpr uint32_t kNanPayload = 0x7fc12345;
  constexpr uint64_t kDoubleNanPayload = 0x7ff8123456789abc;
  constexpr float kFraction = 0.25f;
  constexpr int kPaddingPattern = 0xa5;
  RoundTrip(std::array<uint8_t, 4>{0, 1, kByteMax - 1, kByteMax});
  RoundTrip(std::array<uint16_t, 3>{0, 1, std::numeric_limits<uint16_t>::max()});
  RoundTrip(std::array<uint32_t, 4>{0, 1, kNanPayload, std::numeric_limits<uint32_t>::max()});
  RoundTrip(std::array<int32_t, 4>{
      0, -1, std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max()});
  RoundTrip(std::array<uint64_t, 3>{0, 1, std::numeric_limits<uint64_t>::max()});
  constexpr std::array<uint32_t, 8> kFloatBits{
      0, 0x80000000, 1, 0x7fc12345, 0x7f812345, 0xff812345, 0x7f800000, 0xff800000};
  std::array<float, kFloatBits.size()> floats;
  std::memcpy(floats.data(), kFloatBits.data(), sizeof(floats));
  RoundTrip(floats);
  RoundTrip(std::array<double, 4>{0,
                                  -0.0,
                                  std::numeric_limits<double>::denorm_min(),
                                  std::bit_cast<double>(kDoubleNanPayload)});
  RoundTrip(std::array<std::array<float, 2>, 2>{{{kFraction, -0.0f}, {1.0f, -2.0f}}});

  struct Padded {
    uint8_t Tag;
    uint32_t Value;
  };

  std::array<Padded, 2> records;
  std::memset(records.data(), kPaddingPattern, sizeof(records));
  RoundTrip(records);
  return Report();
}
