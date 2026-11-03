#ifndef OUTSHINE_BASE_MEMORY_MEMORYBUDGET_H
#define OUTSHINE_BASE_MEMORY_MEMORYBUDGET_H
#include <cstddef>
#include <memory>
#include <optional>

namespace outshine {
class MemoryBudget {
  struct State;

public:
  class Reservation {
  public:
    Reservation() = default;
    Reservation(const Reservation &) = delete;
    Reservation &operator=(const Reservation &) = delete;
    Reservation(Reservation &&other) noexcept;
    Reservation &operator=(Reservation &&other) noexcept;
    ~Reservation();

    [[nodiscard]] size_t Bytes() const noexcept { return Bytes_; }

    // A reservation is single-owner; only the shared budget is concurrent.
    [[nodiscard]] bool Resize(size_t bytes) noexcept;
    [[nodiscard]] bool TransferTo(Reservation &target, size_t bytes) noexcept;

  private:
    friend class MemoryBudget;
    Reservation(std::shared_ptr<State> state, size_t bytes) noexcept;
    std::shared_ptr<State> State_;
    size_t Bytes_ = 0;
  };

  explicit MemoryBudget(size_t maximum);
  [[nodiscard]] size_t Maximum() const noexcept;
  [[nodiscard]] size_t Used() const noexcept;
  // Contention may defer admission, even while capacity is available.
  [[nodiscard]] std::optional<Reservation> TryReserve(size_t bytes) const noexcept;

private:
  std::shared_ptr<State> State_;
};
}
#endif
