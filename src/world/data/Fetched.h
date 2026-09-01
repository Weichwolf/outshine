#ifndef OUTSHINE_WORLD_DATA_FETCHED_H
#define OUTSHINE_WORLD_DATA_FETCHED_H

#include <cstdint>
#include <utility>
#include <vector>

namespace outshine::Data {

enum class Meaning : uint8_t { Bytes, Absent, Refused, Retry };

class Fetched {
public:
  enum class State { Working, Settled };

  static Fetched Working() { return {State::Working, Meaning::Retry, {}}; }

  static Fetched Meant(Meaning what) { return {State::Settled, what, {}}; }

  static Fetched MeantAfter(Meaning what, double retryAfterS) {
    Fetched made(State::Settled, what, {});
    made.RetryAfterS_ = retryAfterS;
    return made;
  }

  static Fetched Delivered(std::vector<uint8_t> bytes) {
    return {State::Settled, Meaning::Bytes, std::move(bytes)};
  }

  [[nodiscard]] State Where() const noexcept { return Where_; }

  [[nodiscard]] double RetryAfterS() const noexcept { return RetryAfterS_; }

  [[nodiscard]] bool TryTake(Meaning *what, std::vector<uint8_t> *bytes) {
    if (Where_ != State::Settled) { return false; }
    *what = What_;
    *bytes = std::move(Bytes_);
    return true;
  }

private:
  Fetched(State where, Meaning what, std::vector<uint8_t> bytes)
      : Where_(where), What_(what), Bytes_(std::move(bytes)) {}

  State Where_;
  Meaning What_;
  std::vector<uint8_t> Bytes_;
  double RetryAfterS_ = 0.0;
};

} // namespace outshine::Data
#endif
