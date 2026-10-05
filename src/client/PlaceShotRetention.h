#ifndef OUTSHINE_CLIENT_PLACESHOTRETENTION_H
#define OUTSHINE_CLIENT_PLACESHOTRETENTION_H

#include <algorithm>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>

namespace outshine::Shots {
[[nodiscard]] inline std::expected<void, std::string>
KeepLatestPlaceShot(const std::filesystem::path &latest, std::string_view name) {
  std::error_code error;
  if (!std::filesystem::is_regular_file(std::filesystem::symlink_status(latest, error))) {
    return std::unexpected(latest.string() + ": successful shot is missing");
  }
  const std::string prefix = std::string(name) + "-";
  const auto matches = [&prefix](std::string_view file) {
    constexpr std::size_t kDigestCharacters = 8;
    if (!file.starts_with(prefix) || !file.ends_with(".png") ||
        file.size() != prefix.size() + kDigestCharacters + 4) {
      return false;
    }
    const auto digest = file.substr(prefix.size(), kDigestCharacters);
    return std::ranges::all_of(
        digest, [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); });
  };
  if (!matches(latest.filename().string())) {
    return std::unexpected(latest.string() + ": invalid place shot name");
  }
  std::filesystem::directory_iterator entry(latest.parent_path(), error);
  const std::filesystem::directory_iterator end;
  while (!error && entry != end) {
    const auto path = entry->path();
    if (path != latest && matches(path.filename().string()) &&
        std::filesystem::is_regular_file(entry->symlink_status(error)) && !error) {
      std::filesystem::remove(path, error);
    }
    if (!error) { entry.increment(error); }
  }
  if (error) { return std::unexpected(latest.string() + ": " + error.message()); }
  return {};
}
}
#endif
