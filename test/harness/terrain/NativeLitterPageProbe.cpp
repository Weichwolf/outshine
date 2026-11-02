#include "PreparedLitterPage.h"
#include "AssetCache.h"
#include "AssetGeneration.h"
#include "Check.h"
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <expected>
#include <filesystem>
#include <fstream>
#include <ios>
#include <limits>
#include <print>
#include <ratio>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;
enum class Mode { Build, Hit };

bool Number(std::string_view text, auto &value) {
  const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
  return result.ec == std::errc{} && result.ptr == text.data() + text.size();
}

bool Save(const std::filesystem::path &path, std::span<const std::byte> bytes) {
  std::ofstream file(path, std::ios::binary);
  file.write(reinterpret_cast<const char *>(bytes.data()),
             static_cast<std::streamsize>(bytes.size()));
  file.close();
  return static_cast<bool>(file);
}

bool CaptureLevel(AssetCache &cache,
                  const std::filesystem::path &output,
                  LitterPageAddress address,
                  Mode mode,
                  uint32_t count,
                  uint64_t &generated) {
  const auto plan = PlanLitterPage(address);
  CHECK(plan, "the requested mip has a bounded page layout");
  if (!plan) { return false; }
  const size_t limit = kLitterPageHeaderBytes + size_t{plan->Side} * plan->Side;
  std::vector<double> times;
  times.reserve(count);
  double preparationUs = 0;
  double resolutionUs = 0;
  std::string finalKey;
  for (uint32_t page = 0; page < count; ++page) {
    auto at = address;
    at.Tile[0] += static_cast<int32_t>(page);
    auto key = LitterPageKey(at);
    CHECK(key, "the selected page has a versioned product identity");
    if (!key) { return false; }
    AssetFactory factory;
    if (mode == Mode::Build) {
      factory = [&](size_t bound) {
        ++generated;
        const auto began = std::chrono::steady_clock::now();
        auto package = PrepareLitterPage(at, bound);
        preparationUs +=
            std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - began)
                .count();
        return package;
      };
    }
    const auto began = std::chrono::steady_clock::now();
    auto asset = ResolveAsset(cache, *key, limit, factory);
    const double us =
        std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - began).count();
    CHECK(asset, "native page resolution respects generator availability");
    if (!asset) {
      std::println(stderr, "{}", asset.error());
      return false;
    }
    auto view = DecodeLitterPage(at, asset->Bytes());
    CHECK(view, "the stored native body decodes directly to ready page pixels");
    if (!view) { return false; }
    if (count == 1) {
      CHECK(Save(output / ("mip-" + std::to_string(at.Level) + ".r8"), std::as_bytes(view->Pixels)),
            "the selected page pixels are completely exported");
    }
    times.push_back(us);
    resolutionUs += us;
    finalKey = std::move(*key);
  }
  CHECK(Save(output / ("mip-" + std::to_string(address.Level) + ".f64"),
             std::as_bytes(std::span(times))),
        "all individual resolution times are exported outside their timed scopes");
  std::println("litter level={} core={} side={} count={} bytes={} generated={} resolve_us={:.3f} "
               "prepare_us={:.3f} key={}",
               address.Level,
               address.CoreTexels,
               plan->Side,
               count,
               limit,
               generated,
               resolutionUs,
               preparationUs,
               finalKey);
  return true;
}
}

int main(int argc, char *const *argv) {
  if (argc != 8 && argc != 9) { return 2; }
  LitterPageAddress address;
  uint32_t first = 0;
  uint32_t count = 1;
  const std::string_view mode(argv[7]);
  if (!Number(argv[3], address.Tile[0]) || !Number(argv[4], address.Tile[1]) ||
      !Number(argv[5], first) || !Number(argv[6], address.CoreTexels) ||
      (argc == 9 && !Number(argv[8], count)) || first > kLitterPageMipMax || count == 0 ||
      count > 4096 || (mode != "build" && mode != "hit")) {
    return 2;
  }
  const auto modeValue = mode == "build" ? Mode::Build : Mode::Hit;
  address.Level = kLitterPageMipMax;
  auto last = address;
  const int64_t finalX = int64_t{address.Tile[0]} + count - 1;
  if (finalX > std::numeric_limits<int32_t>::max()) { return 2; }
  last.Tile[0] = static_cast<int32_t>(finalX);
  if (!PlanLitterPage(address) || !PlanLitterPage(last)) { return 2; }
  std::error_code error;
  std::filesystem::create_directories(argv[2], error);
  CHECK(!error, "private page output directory is available");
  auto cache = AssetCache::Open(argv[1]);
  CHECK(cache, "the native asset database opens");
  if (!cache || error) { return Report(); }
  uint64_t generated = 0;
  for (int level = static_cast<int>(kLitterPageMipMax); std::cmp_greater_equal(level, first);
       --level) {
    address.Level = static_cast<uint32_t>(level);
    if (!CaptureLevel(**cache, argv[2], address, modeValue, count, generated)) { return Report(); }
  }
  return Report();
}
