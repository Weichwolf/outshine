#ifndef OUTSHINE_STORAGE_SQLITEFILEBUDGET_H
#define OUTSHINE_STORAGE_SQLITEFILEBUDGET_H

#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <string>

namespace outshine {
// Connections and growth checks run on storage workers, never the frame thread.
class SqliteFileBudget {
public:
  static constexpr uint64_t DefaultBytes = uint64_t{32} * 1024 * 1024 * 1024;

  struct Usage {
    uint64_t FileBytes = 0;
    uint64_t AllocatedBytes = 0;
    uint64_t ChargedBytes = 0;
    uint64_t WalBytes = 0;
    uint64_t TargetCharge = 0;
  };
  struct State;

  class Maintenance {
  public:
    explicit Maintenance(const SqliteFileBudget &budget);
    ~Maintenance();
    Maintenance(const Maintenance &) = delete;
    Maintenance &operator=(const Maintenance &) = delete;

  private:
    std::shared_ptr<State> State_;
  };

  [[nodiscard]] static std::expected<std::shared_ptr<SqliteFileBudget>, std::string>
  Open(const std::filesystem::path &directory);
  [[nodiscard]] static std::expected<void, std::string>
  Configure(const std::filesystem::path &directory, uint64_t bytes);
  ~SqliteFileBudget();
  SqliteFileBudget(const SqliteFileBudget &) = delete;
  SqliteFileBudget &operator=(const SqliteFileBudget &) = delete;
  [[nodiscard]] const char *VfsName() const noexcept;
  [[nodiscard]] uint64_t Limit() const noexcept;
  [[nodiscard]] uint64_t Reserve() const noexcept;
  [[nodiscard]] const std::filesystem::path &Directory() const noexcept;
  [[nodiscard]] std::expected<Usage, std::string> Inspect() const;
  [[nodiscard]] static bool TakeDenial() noexcept;
  static void ClearDenial() noexcept;
  [[nodiscard]] static bool WasDenied() noexcept;

private:
  explicit SqliteFileBudget(std::shared_ptr<State> state);
  std::shared_ptr<State> State_;
};
}
#endif
