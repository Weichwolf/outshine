#ifndef OUTSHINE_WORLD_DATA_CONTENTSTORE_H
#define OUTSHINE_WORLD_DATA_CONTENTSTORE_H

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "Address.h"
#include "SourceDecl.h"

namespace outshine::Data {

[[nodiscard]] std::string ContentKey(const SourceDecl &decl, const Address &at);
[[nodiscard]] std::string SourceKey(const SourceDecl &decl);

class ContentStore {
public:
  enum class Use { Off, On };
  enum class Presence { Unknown, Bytes, Absent };

  struct Entry {
    Presence Where = Presence::Unknown;
    std::vector<uint8_t> Bytes;
  };

  static constexpr int64_t UnpinnedAbsenceLifetimeS = 24 * 60 * 60;
  static constexpr int64_t PinnedAbsenceLifetimeS = 7 * UnpinnedAbsenceLifetimeS;
  static constexpr size_t MaximumAbsenceEntries = 65536;
  static constexpr size_t DefaultAbsenceEntries = 4096;

  struct Config {
    std::string Directory;
    Use Using = Use::On;

    size_t CapBytes = 0;
    size_t AbsenceEntries = DefaultAbsenceEntries;
    std::function<int64_t()> UtcSeconds;
  };

  explicit ContentStore(const Config &config);

  ContentStore(const ContentStore &) = delete;
  ContentStore &operator=(const ContentStore &) = delete;

  [[nodiscard]] std::optional<std::vector<uint8_t>> Read(std::string_view key,
                                                         size_t mostBytes = 0) const;
  [[nodiscard]] bool Keep(std::string_view key, const uint8_t *data, size_t bytes);
  [[nodiscard]] Entry Lookup(std::string_view key, size_t mostBytes = 0) const;
  [[nodiscard]] bool KeepAbsent(std::string_view key, int64_t lifetimeS);

  [[nodiscard]] const std::string &Directory() const noexcept { return Directory_; }

  [[nodiscard]] bool Enabled() const noexcept { return Using_ == Use::On; }

  struct Ledger {
    long long Hits = 0, Misses = 0, Writes = 0, WriteFailures = 0, Swept = 0;
    long long SweptBytes = 0;
  };

  [[nodiscard]] Ledger Counters() const;

private:
  [[nodiscard]] static bool ValidKey(std::string_view key);
  [[nodiscard]] static std::optional<std::vector<uint8_t>> ReadEntry(const std::string &path,
                                                                     size_t limit);
  [[nodiscard]] std::optional<std::vector<uint8_t>> ReadBytes(std::string_view key,
                                                              size_t mostBytes) const;
  [[nodiscard]] int64_t UtcSeconds() const;
  [[nodiscard]] bool HasAbsence(std::string_view key) const;
  [[nodiscard]] bool AbsenceDirectory(bool create) const;
  [[nodiscard]] bool RemoveAbsence(std::string_view key) const;
  void LoadAbsences();
  void ForgetAbsence(std::string_view key);
  [[nodiscard]] bool AdmitAbsence(const std::string &key, int64_t expiry);

  std::string Directory_;
  Use Using_;
  size_t CapBytes_;
  size_t AbsenceEntries_;
  std::function<int64_t()> UtcSeconds_;
  mutable std::mutex AbsenceMutex_;
  mutable std::map<std::string, int64_t, std::less<>> Absences_;

  mutable std::atomic<long long> Hits_{0}, Misses_{0};
  std::atomic<long long> Writes_{0}, WriteFailures_{0};
  long long Swept_ = 0, SweptBytes_ = 0;
};

}
#endif
