#include "Coverage.h"
#include <nlohmann/json.hpp>
#include <array>
#include <bit>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <ios>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>
#include <utility>

namespace R = ReceiverCoverage;
using Json = nlohmann::json;

namespace {
using Clock = std::chrono::steady_clock;
constexpr size_t MaximumRectangles = size_t{16} * 1024u * 1024u;
static_assert(std::endian::native == std::endian::little);

std::vector<R::Rectangle> Read(const std::filesystem::path &path, uint32_t side) {
  std::ifstream input(path, std::ios::binary | std::ios::ate);
  constexpr auto ByteLimit = static_cast<std::streamoff>(MaximumRectangles) *
                             static_cast<std::streamoff>(sizeof(R::Rectangle));
  if (!input || input.tellg() <= 0 || input.tellg() > ByteLimit) { return {}; }
  const auto bytes = static_cast<size_t>(input.tellg());
  if (bytes % sizeof(R::Rectangle) != 0) { return {}; }
  std::vector<R::Rectangle> result(bytes / sizeof(R::Rectangle));
  input.seekg(0);
  input.read(reinterpret_cast<char *>(result.data()), static_cast<std::streamsize>(bytes));
  if (!input) { return {}; }
  for (const auto &r : result) {
    if (r.Left > r.Right || r.Right >= side || r.Top > r.Bottom || r.Bottom >= side) { return {}; }
  }
  return result;
}

struct Sample {
  Json Result;
  std::vector<uint8_t> Mask;
};

template <class Grid> Sample Measure(std::span<const R::Rectangle> rectangles, uint32_t side) {
  const auto begin = Clock::now();
  Grid grid(side);
  const auto allocated = Clock::now();
  for (const auto &r : rectangles) { grid.Mark(r); }
  const auto marked = Clock::now();
  grid.Finish();
  const auto finished = Clock::now();
  std::vector<uint8_t> mask(size_t{side} * side);
  for (size_t index = 0; index < mask.size(); ++index) { mask[index] = grid.Occupied(index); }
  const auto end = Clock::now();
  const auto ns = [](auto duration) {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
  };
  return {.Result = {{"allocate_ns", ns(allocated - begin)},
                     {"mark_ns", ns(marked - allocated)},
                     {"finish_ns", ns(finished - marked)},
                     {"snapshot_ns", ns(end - finished)},
                     {"total_ns", ns(end - begin)},
                     {"logical_state_bytes", grid.Bytes()},
                     {"logical_snapshot_bytes", mask.size()}},
          .Mask = std::move(mask)};
}

Sample Method(uint32_t index, std::span<const R::Rectangle> rectangles, uint32_t side) {
  switch (index) {
    case 0: return Measure<R::FloatMin>(rectangles, side);
    case 1: return Measure<R::ByteFill>(rectangles, side);
    case 2: return Measure<R::RowMask>(rectangles, side);
    default: return Measure<R::Difference>(rectangles, side);
  }
}

Json Work(std::span<const R::Rectangle> rectangles, uint32_t side) {
  uint64_t cells = 0;
  uint64_t words = 0;
  for (const auto &r : rectangles) {
    const uint64_t rows = r.Bottom - r.Top + 1;
    cells += rows * (r.Right - r.Left + 1);
    words += rows * (r.Right / 64 - r.Left / 64 + 1);
  }
  return {{"rectangles", rectangles.size()},
          {"scalar_cell_updates", cells},
          {"row_word_updates", words},
          {"difference_corner_updates", rectangles.size() * 4},
          {"difference_scan_cells", uint64_t{side} * side}};
}

bool Run(std::span<const R::Rectangle> rectangles,
         uint32_t side,
         const std::filesystem::path &out) {
  constexpr std::array Names{"float-min", "byte-fill", "row-mask", "difference"};
  std::vector<uint8_t> reference;
  Json samples = Json::array();
  bool equal = true;
  constexpr uint32_t Rounds = 7;
  for (uint32_t round = 0; round < Rounds; ++round) {
    for (uint32_t at = 0; at < Names.size(); ++at) {
      const uint32_t mode = (round + at) % Names.size();
      auto sample = Method(mode, rectangles, side);
      if (reference.empty()) { reference = sample.Mask; }
      equal &= reference == sample.Mask;
      sample.Result["method"] = Names[mode];
      sample.Result["round"] = round;
      samples.push_back(std::move(sample.Result));
    }
  }
  const Json result{{"side", side},
                    {"work", Work(rectangles, side)},
                    {"samples", samples},
                    {"identical_masks", equal},
                    {"qualified_for_page_suppression", false}};
  std::ofstream report(out / "result.json");
  report << result.dump(2) << '\n';
  std::ofstream mask(out / "mask.bin", std::ios::binary);
  mask.write(reinterpret_cast<const char *>(reference.data()),
             static_cast<std::streamsize>(reference.size()));
  return report && mask && equal;
}
}

int main(int argc, char **argv) {
  if (argc != 4) { return 2; }
  uint32_t side = 0;
  const std::string_view text(argv[2]);
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), side);
  if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() || side == 0 ||
      side > 1024) {
    return 2;
  }
  const auto rectangles = Read(argv[1], side);
  if (rectangles.empty()) { return 2; }
  return Run(rectangles, side, argv[3]) ? 0 : 1;
}
