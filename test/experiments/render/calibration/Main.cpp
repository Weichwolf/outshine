#include "Calibration.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <cstdint>
#include <vector>
#include <expected>
#include <string_view>
#include <ios>
#include <print>
#include <utility>

namespace {
using namespace outshine::Diagnostics::Calibration;

Native::Result<std::vector<uint32_t>> ReadShader(const char *path) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  const auto size = file.tellg();
  if (!file.good() || size <= 0 || size > 1024ll * 1024 || size % sizeof(uint32_t) != 0) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "calibration shader is absent or invalid"});
  }
  std::vector<uint32_t> words(static_cast<size_t>(size) / sizeof(uint32_t));
  file.seekg(0);
  file.read(reinterpret_cast<char *>(words.data()), static_cast<std::streamsize>(size));
  if (!file.good()) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "calibration shader read failed"});
  }
  return words;
}
}

int main(int argc, char **argv) {
  if (argc != 9) {
    std::println(stderr,
                 "expected four shaders, source commit, compiler flags host-load description and "
                 "validation flag");
    return EXIT_FAILURE;
  }
  auto fma = ReadShader(argv[1]);
  auto triad = ReadShader(argv[2]);
  auto vertex = ReadShader(argv[3]);
  auto fragment = ReadShader(argv[4]);
  if (!fma || !triad || !vertex || !fragment) {
    std::println(stderr, "calibration shader input failed");
    return EXIT_FAILURE;
  }
  auto device =
      Native::CreateDevice({.Validation = std::string_view(argv[8]) == "1", .GpuTiming = true});
  if (!device) {
    std::println(stderr, "{}", device.error().Message);
    return EXIT_FAILURE;
  }
  auto report = Run(**device,
                    {.Fma = *fma, .Triad = *triad, .Vertex = *vertex, .Fragment = *fragment},
                    {.Commit = argv[5], .Flags = argv[6], .HostLoad = argv[7]});
  if (!report) {
    std::println(stderr, "{}", report.error().Message);
    return EXIT_FAILURE;
  }
  std::print("{}", Json(*report));
  return EXIT_SUCCESS;
}
