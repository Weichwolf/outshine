#ifndef OUTSHINE_WORLD_DATA_CONTENTSTORE_H
#define OUTSHINE_WORLD_DATA_CONTENTSTORE_H

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include <world/data/Address.h>
#include <world/data/GeoCellId.h>
#include <world/data/SourceDecl.h>
#include <world/data/Transport.h>

namespace outshine::Data {

[[nodiscard]] std::string ContentKey(const SourceDecl &decl, const Address &at);
[[nodiscard]] std::string ContentKey(const SourceDecl &decl,
                                     const Address &at,
                                     const std::optional<ByteRange> &range,
                                     std::string_view entityTag);
[[nodiscard]] std::string SourceKey(const SourceDecl &decl);

class ContentStore {
public:
  enum class Use { Off, On };
  enum class Presence { Unknown, Bytes, Absent };

  struct Entry {
    Presence Where = Presence::Unknown;
    std::vector<uint8_t> Bytes;
  };

  static constexpr size_t MaximumCoverageProbes = 65536;

  struct Config {
    std::string Directory;
    Use Using = Use::On;

    size_t CapBytes = 0;
  };

  explicit ContentStore(const Config &config);

  [[nodiscard]] bool Trim();

  ContentStore(const ContentStore &) = delete;
  ContentStore &operator=(const ContentStore &) = delete;

  [[nodiscard]] std::optional<std::vector<uint8_t>> Read(std::string_view key,
                                                         size_t mostBytes = 0) const;
  [[nodiscard]] bool Keep(std::string_view key, const uint8_t *data, size_t bytes);
  [[nodiscard]] Entry Lookup(std::string_view key, size_t mostBytes = 0) const;
  [[nodiscard]] bool KeepAbsent(std::string_view key);
  [[nodiscard]] bool
  KeepCell(const SourceDecl &decl, GeoCellId cell, std::span<const uint8_t> bytes);
  [[nodiscard]] Entry LookupCell(const SourceDecl &decl, GeoCellId cell);
  [[nodiscard]] bool HasCompleteChildCoverage(const SourceDecl &decl,
                                              GeoCellId cell,
                                              size_t probesMost = MaximumCoverageProbes) const;
  [[nodiscard]] bool CanResumeFromChildren(const SourceDecl &decl,
                                           GeoCellId cell,
                                           size_t probesMost = MaximumCoverageProbes) const;

  [[nodiscard]] const std::string &Directory() const noexcept { return Directory_; }

  [[nodiscard]] bool Enabled() const noexcept { return Using_ == Use::On; }

  struct Ledger {
    long long Hits = 0, Misses = 0, Writes = 0, WriteFailures = 0, Swept = 0;
    long long SweptBytes = 0;
    uint64_t ReadCalls = 0, ReadBytes = 0, WriteBytes = 0;
    double ReadMs = 0.0, WriteMs = 0.0;
  };

  [[nodiscard]] Ledger Counters() const;

private:
  enum class ChildCoverage { Any, Complete };
  [[nodiscard]] bool HasChildCoverage(const SourceDecl &decl,
                                      GeoCellId cell,
                                      size_t probesMost,
                                      ChildCoverage required) const;
  [[nodiscard]] bool
  WriteCellReceipt(const SourceDecl &decl, GeoCellId cell, std::span<const uint8_t> bytes);
  [[nodiscard]] std::optional<std::vector<uint8_t>> ReadVerifiedCellBytes(const SourceDecl &decl,
                                                                          GeoCellId cell) const;
  [[nodiscard]] static bool ValidKey(std::string_view key);
  [[nodiscard]] std::optional<std::vector<uint8_t>> ReadEntry(const std::string &path,
                                                              size_t limit) const;
  [[nodiscard]] std::optional<std::vector<uint8_t>> ReadBytes(std::string_view key,
                                                              size_t mostBytes) const;
  [[nodiscard]] bool WriteEntry(const std::string &path, std::span<const std::byte> bytes);
  [[nodiscard]] bool HasAbsence(std::string_view key) const;
  [[nodiscard]] bool AbsenceDirectory(bool create) const;
  [[nodiscard]] bool RemoveAbsence(std::string_view key) const;
  void ForgetAbsence(std::string_view key);

  std::string Directory_;
  Use Using_;
  size_t CapBytes_;
  mutable std::mutex AbsenceMutex_;

  mutable std::atomic<long long> Hits_{0}, Misses_{0};
  mutable std::atomic<uint64_t> ReadCalls_{0}, ReadBytes_{0}, ReadNs_{0};
  std::atomic<uint64_t> WriteBytes_{0}, WriteNs_{0};
  std::atomic<long long> Writes_{0}, WriteFailures_{0};
  long long Swept_ = 0, SweptBytes_ = 0;
};

}
#endif
