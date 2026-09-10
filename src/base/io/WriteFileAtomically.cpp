#include "WriteFileAtomically.h"
#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <expected>
#include <span>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>

namespace outshine {
namespace {
std::atomic<uint64_t> Serial{0};
constexpr size_t kCreationAttempts = 64;

struct TemporaryOutput {
  std::string Path;
  std::unique_ptr<std::FILE, decltype(&std::fclose)> File{nullptr, &std::fclose};
  bool Created = false;

  ~TemporaryOutput() {
    File.reset();
    if (Created) { std::remove(Path.c_str()); }
  }
};
}

namespace Says {
constexpr std::string_view InvalidOutputPath = ": invalid output file path";
constexpr std::string_view TemporaryOpenFailed = ": temporary output file could not be created";
constexpr std::string_view WriteFailed = ": file write failed";
constexpr std::string_view OutputCloseFailed = ": output file close failed";
constexpr std::string_view PublishFailed = ": file replacement failed";
}

std::expected<void, std::string> WriteFileAtomically(std::string_view path,
                                                     std::span<const std::byte> bytes) {
  const auto error = [path](std::string_view why) {
    return std::unexpected(std::string(path) + std::string(why));
  };
  if (path.empty() || path.contains('\0')) { return error(Says::InvalidOutputPath); }
  const std::filesystem::path destination(path);
  TemporaryOutput pending;
  for (size_t attempt = 0; attempt < kCreationAttempts; ++attempt) {
    pending.Path = std::string(path) + ".outshine-" +
                   std::to_string(Serial.fetch_add(1, std::memory_order_relaxed)) + ".tmp";
    pending.File.reset(std::fopen(pending.Path.c_str(), "wbx"));
    if (pending.File) {
      pending.Created = true;
      break;
    }
    if (errno != EEXIST) { break; }
  }
  if (!pending.File) { return error(Says::TemporaryOpenFailed); }
  const size_t wrote = std::fwrite(bytes.data(), 1, bytes.size(), pending.File.get());
  if (wrote != bytes.size() || std::ferror(pending.File.get()) != 0) {
    return error(Says::WriteFailed);
  }
  if (std::fclose(pending.File.release()) != 0) { return error(Says::OutputCloseFailed); }
  std::error_code failure;
  std::filesystem::rename(pending.Path, destination, failure);
  if (failure) { return error(Says::PublishFailed); }
  pending.Created = false;
  return {};
}
}
