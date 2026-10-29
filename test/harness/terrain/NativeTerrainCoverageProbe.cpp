#include "GroundPatchwork.h"
#include "TileMeshes.h"
#include <array>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <print>
#include <string>
#include <string_view>
#include <system_error>

namespace {
class CoverageSource final : public outshine::TileMeshes {
public:
  explicit CoverageSource(unsigned mode) : Mode_(mode) {}

  Reply Mesh([[maybe_unused]] outshine::Data::TileId tile,
             [[maybe_unused]] int grid,
             [[maybe_unused]] outshine::TileBuild *output) override {
    return Reply::Refused;
  }

  Reply MeshAwaited([[maybe_unused]] outshine::Data::TileId tile,
                    [[maybe_unused]] int grid,
                    [[maybe_unused]] outshine::TileBuild *output) override {
    return Reply::Refused;
  }

  Reply Wants([[maybe_unused]] outshine::Data::TileId tile, [[maybe_unused]] int grid) override {
    return Reply::Refused;
  }

  Reply Inspect(outshine::Data::TileId tile, [[maybe_unused]] int grid) const override {
    ++Queries_;
    if (Mode_ == 0) { return Reply::Ready; }
    if (Mode_ == 1) { return Reply::Pending; }
    constexpr std::array replies{Reply::Ready,
                                 Reply::Pending,
                                 Reply::Deferred,
                                 Reply::Absent,
                                 Reply::Refused,
                                 Reply::Undeclared};
    return replies[(tile.X + 3 * tile.Y + static_cast<uint32_t>(tile.Zoom)) % replies.size()];
  }

  [[nodiscard]] uint64_t Queries() const { return Queries_; }

private:
  unsigned Mode_;
  mutable uint64_t Queries_ = 0;
};
}

int main(int argc, char *const *argv) {
  size_t iterations = 20000;
  if (argc > 2) { return 2; }
  if (argc == 2) {
    const std::string_view text(argv[1]);
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), iterations);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() ||
        (iterations == 0 || iterations > 1'000'000)) {
      return 2;
    }
  }
  for (unsigned mode = 0; mode < 3; ++mode) {
    CoverageSource source(mode);
    uint64_t checksum = 0;
    const auto began = std::chrono::steady_clock::now();
    const auto cpu = std::clock();
    for (size_t index = 0; index < iterations; ++index) {
      const auto result = outshine::LayPatchwork(
          source,
          {.Zoom = 20, .Levels = 12, .Grid = 2, .Access = outshine::TerrainTileAccess::Inspect});
      if (!result) {
        std::println(stderr, "{}", result.error());
        return 1;
      }
      checksum += result->Tiles + result->Skipped + result->Overlapped + result->Pending +
                  result->Absent + result->Refused + result->ContactPending;
    }
    const double cpuSeconds = static_cast<double>(std::clock() - cpu) / CLOCKS_PER_SEC;
    const double seconds =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count();
    std::println(
        "{{\"mode\":{},\"iterations\":{},\"seconds\":{:.9f},\"cpu_seconds\":{:.9f},\"queries\":"
        "{},\"checksum\":{}}}",
        mode,
        iterations,
        seconds,
        cpuSeconds,
        source.Queries(),
        checksum);
  }
}
