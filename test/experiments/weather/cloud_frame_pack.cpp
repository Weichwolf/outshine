#include "CloudFrame.h"
#include "Check.h"

#include <array>
#include <bit>
#include <cstdint>
#include <fstream>
#include <ios>
#include <print>

namespace {
constexpr size_t kInputValues = 13;
static_assert(std::endian::native == std::endian::little);
static_assert(sizeof(double) == 8);
}

int main(int argc, char **argv) {
  using namespace outshine;
  using namespace outshine::Generators::Weather;
  using namespace outshine::Test;
  if (argc != 3) {
    std::println(stderr, "usage: cloud-frame-pack input-13-f64.bin output-112B.bin");
    return 2;
  }
  std::ifstream input(argv[1], std::ios::binary | std::ios::ate);
  const auto bytes = input ? static_cast<std::streamoff>(input.tellg()) : -1;
  std::array<double, kInputValues> values;
  CHECK(bytes == sizeof(values), "the bounded original input contains thirteen FP64 values");
  if (bytes != sizeof(values)) { return Report(); }
  input.seekg(0);
  CHECK(input.read(reinterpret_cast<char *>(values.data()), sizeof(values)),
        "all original physical parameters are read");
  if (!input) { return Report(); }
  CHECK(values[5] == 0.0 || values[5] == 1.0, "the original profile is explicitly supported");
  if (values[5] != 0.0 && values[5] != 1.0) { return Report(); }
  const CloudLayer layer{.RadiusM = values[0],
                         .BaseM = values[1],
                         .ThicknessM = values[2],
                         .Cover = values[4],
                         .ExtinctionPerM = values[3],
                         .WindMs = {{values[6], values[7], values[8]}},
                         .Profile =
                             values[5] == 1.0 ? CloudProfile::CumulusV1 : CloudProfile::Stratus};
  const auto frame = MakeCloudFrame(layer, values[9], {{values[10], values[11], values[12]}});
  CHECK(frame.has_value(), "the actual production C++ generator accepts the requested frame");
  if (!frame) { return Report(); }
  std::ofstream output(argv[2], std::ios::binary);
  CHECK(output.write(reinterpret_cast<const char *>(&*frame), sizeof(*frame)),
        "the complete agreed112B payload is saved without private repacking");
  return Report();
}
