#include "PreparedLitterField.h"
#include "AssetCache.h"
#include "AssetGeneration.h"
#include "Check.h"
#include <charconv>
#include <chrono>
#include <cstdio>
#include <ratio>
#include <utility>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <fstream>
#include <ios>
#include <memory>
#include <print>
#include <string>
#include <string_view>
#include <system_error>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;

bool Number(std::string_view text, auto &value) {
  const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
  return result.ec == std::errc{} && result.ptr == text.data() + text.size();
}
}

int main(int argc, char *const *argv) {
  if (argc != 7) { return 2; }
  LitterFieldAddress address;
  uint32_t first = 0;
  const std::string_view mode(argv[6]);
  if (!Number(argv[3], address.Tile[0]) || !Number(argv[4], address.Tile[1]) ||
      !Number(argv[5], first) || first > kLitterMipMax || (mode != "build" && mode != "hit")) {
    return 2;
  }
  std::error_code error;
  std::filesystem::create_directories(argv[2], error);
  CHECK(!error, "private litter output directory is available");
  auto cache = AssetCache::Open(argv[1]);
  CHECK(cache, "native asset database opens");
  if (!cache || error) { return Report(); }
  uint64_t generated = 0;
  for (int level = static_cast<int>(kLitterMipMax); std::cmp_greater_equal(level, first); --level) {
    address.Level = static_cast<uint32_t>(level);
    auto key = LitterFieldKey(address);
    CHECK(key, "requested lattice mip has a product identity");
    if (!key) { return Report(); }
    const uint32_t side = kLitterFineSide >> address.Level;
    const size_t limit = kLitterFieldHeaderBytes + size_t{side} * side;
    AssetFactory factory;
    double preparationUs = 0;
    if (mode == "build") {
      factory = [&](size_t bound) {
        ++generated;
        const auto start = std::chrono::steady_clock::now();
        auto package = PrepareLitterField(address, bound);
        preparationUs =
            std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start)
                .count();
        return package;
      };
    }
    const auto began = std::chrono::steady_clock::now();
    auto asset = ResolveAsset(**cache, *key, limit, factory);
    const double microseconds =
        std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - began).count();
    CHECK(asset, "asset resolution succeeds with the selected generator availability");
    if (!asset) {
      std::println(stderr, "{}", asset.error());
      return Report();
    }
    auto view = DecodeLitterField(address, asset->Bytes());
    CHECK(view, "native database body decodes directly to ready pixels");
    if (!view) { return Report(); }
    std::ofstream file(std::filesystem::path(argv[2]) / ("mip-" + std::to_string(level) + ".r8"),
                       std::ios::binary);
    file.write(reinterpret_cast<const char *>(view->Pixels.data()),
               static_cast<std::streamsize>(view->Pixels.size()));
    file.close();
    CHECK(file, "ready asset pixels are completely saved");
    std::println(
        "litter level={} side={} bytes={} generated={} resolve_us={:.3f} prepare_us={:.3f} key={}",
        level,
        side,
        view->Pixels.size(),
        generated,
        microseconds,
        preparationUs,
        *key);
  }
  return Report();
}
