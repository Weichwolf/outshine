#ifndef OUTSHINE_BASE_IO_JOBADMISSION_H
#define OUTSHINE_BASE_IO_JOBADMISSION_H

#include <memory>
#include <optional>

namespace outshine {
class JobAdmission {
  struct State;

public:
  class Lease {
  public:
    Lease(Lease &&other) noexcept;
    Lease &operator=(Lease &&other) noexcept;
    ~Lease();
    Lease(const Lease &) = delete;
    Lease &operator=(const Lease &) = delete;

    [[nodiscard]] explicit operator bool() const noexcept { return State_ != nullptr; }

  private:
    friend class JobAdmission;
    explicit Lease(std::shared_ptr<State> state);
    void Release() noexcept;
    std::shared_ptr<State> State_;
  };

  explicit JobAdmission(int maximum);
  JobAdmission(const JobAdmission &) = delete;
  JobAdmission &operator=(const JobAdmission &) = delete;
  [[nodiscard]] std::optional<Lease> TryAcquire() const;
  [[nodiscard]] int Active() const noexcept;
  [[nodiscard]] int Maximum() const noexcept;

private:
  std::shared_ptr<State> State_;
};
}
#endif
