#ifndef OUTSHINE_EXPERIMENT_FIXED_COVERAGE_EXACT_H
#define OUTSHINE_EXPERIMENT_FIXED_COVERAGE_EXACT_H

#include <gmp.h>
#include <bit>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <limits>

namespace FixedCoverage {
class Exact {
public:
  Exact() { mpq_init(Value_); }

  explicit Exact(double value) : Exact() {
    if (!std::isfinite(value)) { std::_Exit(2); }
    mpq_set_d(Value_, value);
    mpq_canonicalize(Value_);
  }

  Exact(const Exact &other) : Exact() { mpq_set(Value_, other.Value_); }

  Exact(Exact &&other) noexcept : Exact() { mpq_swap(Value_, other.Value_); }

  Exact &operator=(const Exact &other) {
    mpq_set(Value_, other.Value_);
    return *this;
  }

  Exact &operator=(Exact &&other) noexcept {
    mpq_swap(Value_, other.Value_);
    return *this;
  }

  ~Exact() { mpq_clear(Value_); }

  [[nodiscard]] static Exact Float32(float value) {
    constexpr uint32_t FractionBits = 23;
    constexpr uint32_t ExponentBits = 8;
    constexpr uint32_t ExponentMask = (1u << ExponentBits) - 1u;
    constexpr uint32_t FractionMask = (1u << FractionBits) - 1u;
    constexpr int ExponentBias = 127;
    constexpr uint32_t OneBits = 0x3f800000;
    static_assert(std::numeric_limits<float>::is_iec559);
    static_assert(std::numeric_limits<float>::digits == FractionBits + 1);
    static_assert(std::bit_cast<uint32_t>(1.0f) == OneBits);
    const auto bits = std::bit_cast<uint32_t>(value);
    const auto exponent = (bits >> FractionBits) & ExponentMask;
    if (exponent == ExponentMask) { std::_Exit(2); }
    const uint32_t mantissa = (bits & FractionMask) | (exponent == 0 ? 0u : 1u << FractionBits);
    const int power = (exponent == 0 ? 1 : static_cast<int>(exponent)) - ExponentBias -
                      static_cast<int>(FractionBits);
    Exact result;
    mpq_set_ui(result.Value_, mantissa, 1);
    if (power < 0) {
      mpq_div_2exp(result.Value_, result.Value_, static_cast<mp_bitcnt_t>(-power));
    } else {
      mpq_mul_2exp(result.Value_, result.Value_, static_cast<mp_bitcnt_t>(power));
    }
    if ((bits >> (FractionBits + ExponentBits)) != 0) { mpq_neg(result.Value_, result.Value_); }
    return result;
  }

  [[nodiscard]] int Sign() const { return mpq_sgn(Value_); }

  [[nodiscard]] double Approximate() const { return mpq_get_d(Value_); }

  [[nodiscard]] int32_t RoundSmallNonnegative() const {
    if (Sign() < 0 || mpq_cmp_ui(Value_, INT32_MAX - 1, 1) > 0) { std::_Exit(2); }
    Exact shifted = *this + Exact(.5);
    mpz_t integer;
    mpz_init(integer);
    mpz_fdiv_q(integer, mpq_numref(shifted.Value_), mpq_denref(shifted.Value_));
    const auto result = static_cast<int32_t>(mpz_get_si(integer));
    mpz_clear(integer);
    return result;
  }

  friend Exact operator+(const Exact &left, const Exact &right) {
    Exact result;
    mpq_add(result.Value_, left.Value_, right.Value_);
    return result;
  }

  friend Exact operator-(const Exact &left, const Exact &right) {
    Exact result;
    mpq_sub(result.Value_, left.Value_, right.Value_);
    return result;
  }

  friend Exact operator*(const Exact &left, const Exact &right) {
    Exact result;
    mpq_mul(result.Value_, left.Value_, right.Value_);
    return result;
  }

  friend Exact operator/(const Exact &left, const Exact &right) {
    if (right.Sign() == 0) { std::_Exit(2); }
    Exact result;
    mpq_div(result.Value_, left.Value_, right.Value_);
    return result;
  }

  friend bool operator==(const Exact &left, const Exact &right) {
    return mpq_cmp(left.Value_, right.Value_) == 0;
  }

  friend bool operator<(const Exact &left, const Exact &right) {
    return mpq_cmp(left.Value_, right.Value_) < 0;
  }

private:
  mpq_t Value_;
};
}
#endif
