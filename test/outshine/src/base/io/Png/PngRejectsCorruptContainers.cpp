#include "Png.h"
#include "Check.h"
#include <cstdint>
#include <vector>
#include <zlib.h>

namespace {
void HeaderCrc(std::vector<uint8_t> &png) {
  const auto crc = crc32(0, png.data() + 12, 17);
  for (unsigned at = 0; at < 4; ++at) {
    png[29 + at] = static_cast<uint8_t>(crc >> ((3 - at) * 8));
  }
}
}

int main() {
  using namespace outshine::Test;
  constexpr uint8_t png[] = {
      0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44,
      0x52, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02, 0x08, 0x02, 0x00, 0x00, 0x00, 0xfd,
      0xd4, 0x9a, 0x73, 0x00, 0x00, 0x00, 0x12, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9c, 0x63, 0xe0,
      0x12, 0x91, 0xd3, 0x30, 0xb2, 0x61, 0x61, 0x05, 0x03, 0x00, 0x08, 0x81, 0x00, 0xf5, 0x9b,
      0x68, 0x36, 0x1d, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82};

  const std::vector<uint8_t> original(std::begin(png), std::end(png));
  const auto rejected = [](const std::vector<uint8_t> &bytes) {
    const auto image = outshine::Io::ReadPng(bytes.data(), bytes.size());
    return !image.Read && !image.Error.empty() && image.Bytes.empty() && image.Wide == 0;
  };
  CHECK(outshine::Io::ReadPng(original.data(), original.size()).Read,
        "independent fixture decodes");
  auto corrupt = original;
  corrupt[29] ^= 1;
  CHECK(rejected(corrupt), "IHDR checksum corruption is rejected before publication");
  corrupt = original;
  corrupt[original.size() - 13] ^= 1;
  CHECK(rejected(corrupt), "IDAT checksum corruption is rejected");
  corrupt = original;
  corrupt.resize(corrupt.size() - 12);
  CHECK(rejected(corrupt), "missing IEND cannot produce an accepted height tile");
  for (const size_t method : {size_t{26}, size_t{27}}) {
    corrupt = original;
    corrupt[method] = 1;
    HeaderCrc(corrupt);
    CHECK(rejected(corrupt), "unknown compression or filter method is rejected despite valid CRC");
  }
  corrupt = original;
  corrupt.insert(corrupt.begin() + 33, original.begin() + 8, original.begin() + 33);
  CHECK(rejected(corrupt), "duplicate IHDR cannot replace the declared image layout");
  corrupt.assign(original.begin(), original.begin() + 8);
  corrupt.insert(corrupt.end(), original.begin() + 33, original.end() - 12);
  corrupt.insert(corrupt.end(), original.begin() + 8, original.begin() + 33);
  corrupt.insert(corrupt.end(), original.end() - 12, original.end());
  CHECK(rejected(corrupt), "IDAT before IHDR is not an alternative chunk order");
  corrupt = original;
  corrupt[33] = 0xff;
  CHECK(rejected(corrupt),
        "oversized chunk length is rejected without wrapping or reading past input");
  return Report();
}
