#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include "Check.h"

namespace {
constexpr size_t kMaxLines = 120;
constexpr size_t kMaxBytes = 12 * 1024;

constexpr bool Fits(size_t lines, size_t bytes) {
  return lines <= kMaxLines && bytes <= kMaxBytes;
}

static_assert(Fits(kMaxLines, kMaxBytes));
static_assert(!Fits(kMaxLines + 1, 0));
static_assert(!Fits(1, kMaxBytes + 1));
}

int main() {
  using namespace outshine::Test;
  size_t items = 0;
  for (const auto &entry : std::filesystem::directory_iterator("board")) {
    if (!entry.is_regular_file() || entry.path().extension() != ".md") { continue; }
    ++items;
    std::ifstream input(entry.path(), std::ios::binary);
    CHECK(input.good(), "every work item can be read");
    const std::string body{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    const size_t lines = static_cast<size_t>(std::ranges::count(body, '\n')) +
                         (!body.empty() && body.back() != '\n' ? 1u : 0u);
    if (!Fits(lines, body.size())) {
      std::printf("  %s: %zu/%zu lines, %zu/%zu bytes\n",
                  entry.path().c_str(), lines, kMaxLines, body.size(), kMaxBytes);
    }
    CHECK(Fits(lines, body.size()), "a work item is bounded by 120 lines and 12 KiB");
  }
  CHECK(items > 0, "the claim inspected the actual board");
  Covers("work-item size only; it does not establish completeness or correct dependencies");
  return Report();
}
