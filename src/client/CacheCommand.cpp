#include "CacheCommand.h"
#include "CacheTrim.h"
#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <expected>
#include <filesystem>
#include <limits>
#include <memory>
#include <optional>
#include <print>
#include <ratio>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <SDL3/SDL.h>

namespace outshine::Client {
namespace {
constexpr uint64_t kMiB = uint64_t{1024} * 1024;

struct Options {
  std::optional<uint64_t> Assets;
  std::optional<uint64_t> Sources;
  std::optional<std::string> Root;
  bool Apply = false;
  bool Compact = false;
};

struct OptionValue {
  std::string_view Name;
  std::string_view Value;
};

void Usage() {
  std::println("Usage: outshine cache trim [options]\n"
               "  --assets-mib N   total compressed package budget across asset databases\n"
               "  --sources-mib N  source-file budget, including cached absence records\n"
               "  --root PATH      cache parent containing assets/ and sources/\n"
               "                   default: application preferences directory\n"
               "  --apply          execute; default is a preview\n"
               "  --compact        compact selected SQLite files after applying\n\n"
               "Specify at least one budget in whole MiB. Zero clears that cache.\n"
               "Oldest usage goes first; legacy entries have unknown usage.\n"
               "Budgets count payload bytes; file sizes include SQLite overhead/WAL.\n"
               "No network, rendering, scenario or game-state access.");
}

std::expected<uint64_t, std::string> ReadBudget(std::string_view value) {
  uint64_t mib = 0;
  const auto parsed = std::from_chars(value.data(), value.data() + value.size(), mib);
  if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() ||
      mib > std::numeric_limits<uint64_t>::max() / kMiB) {
    return std::unexpected("requires a representable whole MiB budget");
  }
  return mib * kMiB;
}

std::expected<void, std::string> AssignOption(Options &options, OptionValue option) {
  if (option.Name == "--root") {
    if (options.Root) { return std::unexpected("duplicate --root"); }
    options.Root = option.Value;
    return {};
  }
  auto bytes = ReadBudget(option.Value);
  if (!bytes) { return std::unexpected(std::string(option.Name) + " " + bytes.error()); }
  auto &budget = option.Name == "--assets-mib" ? options.Assets : options.Sources;
  if (budget) { return std::unexpected("duplicate " + std::string(option.Name)); }
  budget = *bytes;
  return {};
}

std::expected<Options, std::string> Parse(std::span<const char *const> arguments) {
  Options options;
  if (arguments.empty() || std::string_view(arguments.front()) != "trim") {
    return std::unexpected("cache requires the trim subcommand");
  }
  arguments = arguments.subspan(1);
  while (!arguments.empty()) {
    const std::string_view flag = arguments.front();
    arguments = arguments.subspan(1);
    if (flag == "--apply") {
      options.Apply = true;
      continue;
    }
    if (flag == "--compact") {
      options.Compact = true;
      continue;
    }
    if (flag != "--assets-mib" && flag != "--sources-mib" && flag != "--root") {
      return std::unexpected("unknown cache option: " + std::string(flag));
    }
    if (arguments.empty() || std::string_view(arguments.front()).empty() ||
        std::string_view(arguments.front()).starts_with("--")) {
      return std::unexpected(std::string(flag) + " requires a value");
    }
    const std::string_view value = arguments.front();
    arguments = arguments.subspan(1);
    if (auto set = AssignOption(options, {.Name = flag, .Value = value}); !set) {
      return std::unexpected(set.error());
    }
  }
  if (!options.Assets && !options.Sources) {
    return std::unexpected("cache trim requires --assets-mib or --sources-mib");
  }
  return options;
}

void Report(std::string_view name, const CacheTrimResult &result, bool apply, double elapsedMs) {
  std::println("CACHE\t{}\tmode={}\tstored_bytes={}\tretained_bytes={}\tentries_selected={}\t"
               "packages_selected={}\tunknown_use={}\tfile_bytes_before={}\tfile_bytes_after={}\t"
               "elapsed_ms={:.3f}",
               name,
               apply ? "apply" : "preview",
               result.StoredBytes,
               result.RetainedBytes,
               result.EntriesRemoved,
               result.PackagesRemoved,
               result.UnknownUse,
               result.FileBytesBefore,
               result.FileBytesAfter,
               elapsedMs);
}
}

int RunCacheCommand(std::span<const char *const> arguments) {
  if (arguments.empty() || std::ranges::any_of(arguments, [](const char *arg) {
        return std::string_view(arg) == "--help";
      })) {
    Usage();
    return 0;
  }
  auto options = Parse(arguments);
  if (!options) {
    std::println(stderr, "outshine: {}", options.error());
    return 2;
  }
  if (!options->Root) {
    const std::unique_ptr<char, decltype(&SDL_free)> root(SDL_GetPrefPath("outshine", "outshine"),
                                                          SDL_free);
    if (!root) {
      std::println(stderr, "outshine: {}", SDL_GetError());
      return 1;
    }
    options->Root = root.get();
  }
  const std::filesystem::path root = *options->Root;
  for (const bool assets : {true, false}) {
    const auto &budget = assets ? options->Assets : options->Sources;
    if (!budget) { continue; }
    const CacheTrimOptions trim{
        .BudgetBytes = *budget, .Apply = options->Apply, .Compact = options->Compact};
    const std::array paths{root / "assets/assets.sqlite", root / "assets/prototypes.sqlite"};
    const auto began = std::chrono::steady_clock::now();
    auto result = assets ? TrimAssetCaches(paths, trim) : TrimSourceCache(root / "sources", trim);
    if (!result) {
      std::println(stderr, "outshine: {}", result.error());
      return 1;
    }
    const double elapsedMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
    Report(assets ? "assets" : "sources", *result, options->Apply, elapsedMs);
  }
  return 0;
}

}
