#include "RawSourceCache.h"

#include <atomic>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <system_error>

namespace outshine::Data {
namespace {
constexpr std::string_view kLegacyMarkerDirectory = ".outshine-absence-v1";
constexpr std::string_view kSignature = "outshine-absence-v2\n";
constexpr std::string_view kEmptySignature = "outshine-empty-bytes-v1\n";
constexpr std::string_view kLegacySignature = "outshine-absence-v1\n";
constexpr size_t kRecordBytes = 64;

std::string MarkerPath(const std::string &directory, std::string_view key) {
  return directory + "/" + std::string(kLegacyMarkerDirectory) + "/" + std::string(key);
}

bool ValidAbsenceRecord(std::string_view record) {
  if (record == kSignature) { return true; }
  if (record.size() <= kLegacySignature.size() || !record.starts_with(kLegacySignature) ||
      record.back() != '\n') {
    return false;
  }
  record.remove_prefix(kLegacySignature.size());
  record.remove_suffix(1);
  int64_t expiry = 0;
  const auto parsed = std::from_chars(record.data(), record.data() + record.size(), expiry);
  if (parsed.ec != std::errc{} || parsed.ptr != record.data() + record.size() || expiry <= 0) {
    return false;
  }
  const auto now = std::chrono::duration_cast<std::chrono::seconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count();
  constexpr int64_t kLegacyMaximumLifetimeS = int64_t{7} * 24 * 60 * 60;
  return now >= 0 && (expiry <= now || expiry - now <= kLegacyMaximumLifetimeS);
}
}

bool RawSourceCache::StatusDirectory(bool create) const {
  const std::filesystem::path path = Directory_ + "/" + std::string(kLegacyMarkerDirectory);
  std::error_code error;
  auto status = std::filesystem::symlink_status(path, error);
  if (!std::filesystem::exists(status) && create) {
    error.clear();
    (void)std::filesystem::create_directory(path, error);
    if (error) { return false; }
    status = std::filesystem::symlink_status(path, error);
  }
  return !error && std::filesystem::is_directory(status);
}

bool RawSourceCache::RemoveStatus(std::string_view key) const {
  if (!StatusDirectory(false)) { return false; }
  const auto path = MarkerPath(Directory_, key);
  std::error_code error;
  const auto status = std::filesystem::symlink_status(path, error);
  if (!std::filesystem::exists(status)) { return true; }
  if (error || !std::filesystem::is_regular_file(status)) { return false; }
  return std::filesystem::remove(path, error) && !error;
}

RawSourceCache::Presence RawSourceCache::ReadStatus(std::string_view key) const {
  const std::scoped_lock lock(StatusMutex_);
  std::error_code error;
  const auto data = std::filesystem::symlink_status(Directory_ + "/" + std::string(key), error);
  if ((error && error != std::errc::no_such_file_or_directory) || std::filesystem::exists(data) ||
      !StatusDirectory(false)) {
    return Presence::Unknown;
  }
  const auto bytes = ReadEntry(MarkerPath(Directory_, key), kRecordBytes);
  if (!bytes) { return Presence::Unknown; }
  const std::string_view record(reinterpret_cast<const char *>(bytes->data()), bytes->size());
  const auto status = record == kEmptySignature    ? Presence::Bytes
                      : ValidAbsenceRecord(record) ? Presence::Absent
                                                   : Presence::Unknown;
  if (status != Presence::Unknown) { ObserveUsage(key, bytes->size()); }
  return status;
}

void RawSourceCache::ForgetStatus(std::string_view key) {
  const std::scoped_lock lock(StatusMutex_);
  (void)RemoveStatus(key);
}

bool RawSourceCache::KeepAbsent(std::string_view key) {
  return KeepStatus(key, kSignature);
}

bool RawSourceCache::KeepEmpty(std::string_view key) {
  return KeepStatus(key, kEmptySignature);
}

bool RawSourceCache::KeepStatus(std::string_view key, std::string_view signature) {
  if (Using_ != Use::On || !ValidKey(key)) { return false; }
  const std::scoped_lock lock(StatusMutex_);
  if (!StatusDirectory(true)) { return false; }
  const auto path = MarkerPath(Directory_, key);
  std::error_code error;
  const auto status = std::filesystem::symlink_status(path, error);
  if (std::filesystem::exists(status) && (!std::filesystem::is_regular_file(status) || error)) {
    return false;
  }
  if (!WriteEntry(path, std::as_bytes(std::span(signature.data(), signature.size())))) {
    return false;
  }
  ObserveUsage(key, signature.size(), true);
  Writes_.fetch_add(1, std::memory_order_relaxed);
  return true;
}

}
