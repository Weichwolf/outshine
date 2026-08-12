#ifndef FETCHED_H
#define FETCHED_H

#include <cstdint>
#include <utility>
#include <vector>

namespace outshine::Data {

/* WHAT ONE STATUS MEANT TO ONE UPSTREAM. Per source and never one global function: an
 * out-of-coverage elevation tile, an empty vector tile and a rate-limited one are three different
 * conversations, and the single global mapping this replaces read a 204 that only our own server
 * ever minted. */
enum class Meaning : uint8_t { Bytes, Absent, Refused, Retry };

/* ONE SOURCE'S ANSWER, once. Working is a promise; everything else is settled and hands its meaning
 * and its bytes over together, so a caller cannot read one without the other. */
class Fetched {
public:
  enum class State { Working, Settled };

  static Fetched Working() { return Fetched(State::Working, Meaning::Retry, {}); }
  static Fetched Meant(Meaning what) { return Fetched(State::Settled, what, {}); }
  static Fetched Delivered(std::vector<uint8_t> bytes) {
    return Fetched(State::Settled, Meaning::Bytes, std::move(bytes));
  }

  [[nodiscard]] State Where() const noexcept { return Where_; }

  /* `bytes` is left empty for every meaning but Bytes. */
  [[nodiscard]] bool TryTake(Meaning *what, std::vector<uint8_t> *bytes) {
    if (Where_ != State::Settled) return false;
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
};

} // namespace outshine::Data
#endif
