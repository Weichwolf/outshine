#ifndef OUTSHINE_BASE_FORMAT_DIGEST_H
#define OUTSHINE_BASE_FORMAT_DIGEST_H

#include <cstdint>

namespace outshine {

constexpr unsigned kByteBits = 8u;
constexpr uint64_t kPrimeTail = 0xb3ull;

constexpr unsigned kPrimeShift = 40u;

constexpr uint64_t kFnv64Basis = 14695981039346656037ull;

constexpr uint64_t kDigestBasis = kFnv64Basis;
constexpr uint64_t kDigestPrime = 1099511628211ull;

static_assert(kDigestPrime == (1ull << kPrimeShift) + (1ull << kByteBits) + kPrimeTail,
              "the multiplier is FNV-1a's 64-bit prime, and it is DERIVED here so a transcription "
              "cannot quietly differ from the reference the way the basis beside it does");

static_assert(kDigestBasis == kFnv64Basis, "the digest is FNV-1a's, basis and all");

[[nodiscard]] constexpr uint64_t DigestFolded(uint64_t digest, uint8_t byte) {
  return (digest ^ byte) * kDigestPrime;
}

} // namespace outshine
#endif
