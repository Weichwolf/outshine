#include "ContentStore.h"

#include "WriteFileAtomically.h"

#include <algorithm>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <cstddef>
#include <filesystem>
#include <limits>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>

namespace outshine::Data {
namespace {
constexpr std::string_view kDirectory = ".outshine-absence-v1";
constexpr std::string_view kSignature = "outshine-absence-v1\n";
constexpr size_t kRecordBytes = 64;

std::string MarkerPath(const std::string &directory, std::string_view key) {
  return directory + "/" + std::string(kDirectory) + "/" + std::string(key);
}

std::optional<int64_t> Expiry(std::string_view record, int64_t now) {
  if (record.size() <= kSignature.size() || !record.starts_with(kSignature) ||
      record.back() != '\n') {
    return std::nullopt;
  }
  record.remove_prefix(kSignature.size());
  record.remove_suffix(1);
  int64_t expiry = 0;
  const auto parsed = std::from_chars(record.data(), record.data() + record.size(), expiry);
  if (parsed.ec != std::errc{} || parsed.ptr != record.data() + record.size() || expiry <= now ||
      expiry - now > ContentStore::PinnedAbsenceLifetimeS) {
    return std::nullopt;
  }
  return expiry;
}
}

int64_t ContentStore::UtcSeconds() const {
  if (UtcSeconds_) { return UtcSeconds_(); }
  return std::chrono::duration_cast<std::chrono::seconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

bool ContentStore::AbsenceDirectory(bool create) const {
  const std::filesystem::path path = Directory_ + "/" + std::string(kDirectory);
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

bool ContentStore::RemoveAbsence(std::string_view key) const {
  if (!AbsenceDirectory(false)) { return false; }
  const auto path = MarkerPath(Directory_, key);
  std::error_code error;
  const auto status = std::filesystem::symlink_status(path, error);
  if (!std::filesystem::exists(status)) { return true; }
  if (error || !std::filesystem::is_regular_file(status)) { return false; }
  return std::filesystem::remove(path, error) && !error;
}

bool ContentStore::AdmitAbsence(const std::string &key, int64_t expiry) {
  const auto [stored, inserted] = Absences_.insert_or_assign(key, expiry);
  if (!inserted || Absences_.size() <= AbsenceEntries_) { return true; }
  const auto oldest =
      std::ranges::min_element(Absences_, {}, [](const auto &entry) { return entry.second; });
  if (!RemoveAbsence(oldest->first)) {
    Absences_.erase(stored);
    return false;
  }
  const bool retained = oldest != stored;
  Absences_.erase(oldest);
  return retained;
}

void ContentStore::LoadAbsences() {
  if (!AbsenceDirectory(false)) { return; }
  const auto now = UtcSeconds();
  if (now < 0) { return; }
  std::error_code error;
  const auto directory = Directory_ + "/" + std::string(kDirectory);
  for (std::filesystem::directory_iterator it(directory, error), end; !error && it != end;
       it.increment(error)) {
    const std::string key = it->path().filename().string();
    if (!ValidKey(key)) { continue; }
    const auto bytes = ReadEntry(it->path().string(), kRecordBytes);
    if (!bytes) {
      (void)RemoveAbsence(key);
      continue;
    }
    const std::string record(bytes->begin(), bytes->end());
    const auto expiry = Expiry(record, now);
    if (!expiry || !AdmitAbsence(key, *expiry)) { (void)RemoveAbsence(key); }
  }
}

bool ContentStore::HasAbsence(std::string_view key) const {
  const std::scoped_lock lock(AbsenceMutex_);
  const auto found = Absences_.find(key);
  if (found == Absences_.end()) { return false; }
  const auto now = UtcSeconds();
  if (now >= 0 && found->second > now && found->second - now <= PinnedAbsenceLifetimeS) {
    return true;
  }
  (void)RemoveAbsence(key);
  Absences_.erase(found);
  return false;
}

void ContentStore::ForgetAbsence(std::string_view key) {
  const std::scoped_lock lock(AbsenceMutex_);
  const auto found = Absences_.find(key);
  if (found != Absences_.end()) { Absences_.erase(found); }
  if (AbsenceDirectory(false)) { (void)RemoveAbsence(key); }
}

bool ContentStore::KeepAbsent(std::string_view key, int64_t lifetimeS) {
  if (Using_ != Use::On) { return false; }
  const auto now = UtcSeconds();
  if (!ValidKey(key) || now < 0 || lifetimeS <= 0 || lifetimeS > PinnedAbsenceLifetimeS ||
      now > std::numeric_limits<int64_t>::max() - lifetimeS) {
    return false;
  }
  const std::scoped_lock lock(AbsenceMutex_);
  if (!AbsenceDirectory(true)) { return false; }
  const auto path = MarkerPath(Directory_, key);
  std::error_code error;
  const auto status = std::filesystem::symlink_status(path, error);
  if (std::filesystem::exists(status) && (!std::filesystem::is_regular_file(status) || error)) {
    return false;
  }
  const int64_t expiry = now + lifetimeS;
  const std::string record = std::string(kSignature) + std::to_string(expiry) + "\n";
  if (!WriteFileAtomically(path, std::as_bytes(std::span(record.data(), record.size())))) {
    return false;
  }
  if (!AdmitAbsence(std::string(key), expiry)) {
    (void)RemoveAbsence(key);
    return false;
  }
  Writes_.fetch_add(1, std::memory_order_relaxed);
  return true;
}

}
