#include "Sha256.h"
#include "Sha256Stream.h"

#include <array>
#include <expected>
#include <algorithm>
#include <limits>
#ifdef __APPLE__
#include <CommonCrypto/CommonDigest.h>
#endif
#ifdef __linux__
#include <openssl/evp.h>
#include <memory>
#endif
#include <span>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <string>
#include <string_view>

namespace outshine {
namespace {
#ifdef __linux__
EVP_MD *Sha256Method() noexcept {
  static const std::unique_ptr<EVP_MD, decltype(&EVP_MD_free)> method(
      EVP_MD_fetch(nullptr, "SHA256", nullptr), &EVP_MD_free);
  return method.get();
}
#endif

std::string DigestHex(std::span<const uint8_t, 32> digest) {
  static constexpr std::string_view kHex = "0123456789abcdef";
  std::string out(digest.size() * 2, '0');
  for (size_t at = 0; at < digest.size(); ++at) {
    out[at * 2] = kHex[digest[at] >> 4u];
    out[at * 2 + 1] = kHex[digest[at] & 15u];
  }
  return out;
}
}
#ifndef __APPLE__
namespace {

constexpr std::array<uint32_t, 64> kRoundConstants = {
    {0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u,
     0xab1c5ed5u, 0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu,
     0x9bdc06a7u, 0xc19bf174u, 0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu,
     0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau, 0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
     0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu,
     0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u, 0xa2bfe8a1u, 0xa81a664bu,
     0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u, 0x19a4c116u,
     0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
     0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u,
     0xc67178f2u}};

[[nodiscard]] uint32_t Rotr(uint32_t v, uint32_t n) {
  return (v >> n) | (v << (32u - n));
}

constexpr std::array<uint32_t, 8> kInitialState = {{0x6a09e667u,
                                                    0xbb67ae85u,
                                                    0x3c6ef372u,
                                                    0xa54ff53au,
                                                    0x510e527fu,
                                                    0x9b05688cu,
                                                    0x1f83d9abu,
                                                    0x5be0cd19u}};

constexpr unsigned kByteShift = 8u;
constexpr size_t kWordBytes = 4;

void Compress(std::span<uint32_t, 8> state, std::span<const uint8_t, 64> block) {
  std::array<uint32_t, 64> w{};
  for (int i = 0; i < 16; i++) {
    w[i] = (static_cast<uint32_t>(block[i * kWordBytes]) << (3u * kByteShift)) |
           (static_cast<uint32_t>(block[i * kWordBytes + 1]) << (2u * kByteShift)) |
           (static_cast<uint32_t>(block[i * kWordBytes + 2]) << kByteShift) |
           static_cast<uint32_t>(block[i * kWordBytes + 3]);
  }
  for (int i = 16; i < 64; i++) {
    const uint32_t s0 = Rotr(w[i - 15], 7) ^ Rotr(w[i - 15], 18) ^ (w[i - 15] >> 3u);
    const uint32_t s1 = Rotr(w[i - 2], 17) ^ Rotr(w[i - 2], 19) ^ (w[i - 2] >> 10u);
    w[i] = w[i - 16] + s0 + w[i - 7] + s1;
  }
  uint32_t a = state[0];
  uint32_t b = state[1];
  uint32_t c = state[2];
  uint32_t d = state[3];
  uint32_t e = state[4];
  uint32_t f = state[5];
  uint32_t g = state[6];
  uint32_t h = state[7];
  for (int i = 0; i < 64; i++) {
    const uint32_t s1 = Rotr(e, 6) ^ Rotr(e, 11) ^ Rotr(e, 25);
    const uint32_t ch = (e & f) ^ (~e & g);
    const uint32_t t1 = h + s1 + ch + kRoundConstants[i] + w[i];
    const uint32_t s0 = Rotr(a, 2) ^ Rotr(a, 13) ^ Rotr(a, 22);
    const uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
    const uint32_t t2 = s0 + maj;
    h = g;
    g = f;
    f = e;
    e = d + t1;
    d = c;
    c = b;
    b = a;
    a = t1 + t2;
  }
  state[0] += a;
  state[1] += b;
  state[2] += c;
  state[3] += d;
  state[4] += e;
  state[5] += f;
  state[6] += g;
  state[7] += h;
}

}
#endif

Sha256Prefix::Sha256Prefix(const void *data, size_t bytes) noexcept {
#ifdef __APPLE__
  (void)CC_SHA256_Init(&State_);
#else
  State_ = kInitialState;
#endif
  Append(data, bytes);
}

void Sha256Prefix::Append(const void *data, size_t bytes) noexcept {
  if (bytes == 0) { return; }
  const auto *cursor = static_cast<const uint8_t *>(data);
#ifdef __APPLE__
  for (size_t left = bytes; left != 0;) {
    const auto count = static_cast<CC_LONG>(
        std::min(left, static_cast<size_t>(std::numeric_limits<CC_LONG>::max())));
    (void)CC_SHA256_Update(&State_, cursor, count);
    cursor += count;
    left -= count;
  }
#else
  Bytes_ += static_cast<uint64_t>(bytes);
  if (Buffered_ != 0) {
    const auto count = std::min(BufferedBytes_.size() - Buffered_, bytes);
    std::memcpy(BufferedBytes_.data() + Buffered_, cursor, count);
    Buffered_ += count;
    cursor += count;
    bytes -= count;
    if (Buffered_ != BufferedBytes_.size()) { return; }
    Compress(State_, BufferedBytes_);
    Buffered_ = 0;
  }
  while (bytes >= BufferedBytes_.size()) {
    Compress(State_, std::span<const uint8_t, 64>(cursor, BufferedBytes_.size()));
    cursor += BufferedBytes_.size();
    bytes -= BufferedBytes_.size();
  }
  if (bytes != 0) { std::memcpy(BufferedBytes_.data(), cursor, bytes); }
  Buffered_ = bytes;
#endif
}

std::array<uint8_t, 32> Sha256Prefix::Digest(const void *data, size_t bytes) const noexcept {
  auto copy = *this;
  copy.Append(data, bytes);
#ifdef __APPLE__
  std::array<unsigned char, CC_SHA256_DIGEST_LENGTH> digest{};
  (void)CC_SHA256_Final(digest.data(), &copy.State_);
  return digest;
#else
  std::array<uint8_t, 128> tail{};
  if (copy.Buffered_ != 0) { std::memcpy(tail.data(), copy.BufferedBytes_.data(), copy.Buffered_); }
  tail[copy.Buffered_] = 0x80;
  const size_t tailBlocks = (copy.Buffered_ + 9 > 64) ? 2u : 1u;
  const uint64_t bits = copy.Bytes_ * 8u;
  for (uint32_t i = 0; i < 8u; i++) {
    tail[tailBlocks * 64 - 1 - static_cast<size_t>(i)] = static_cast<uint8_t>(bits >> (8u * i));
  }
  for (size_t block = 0; block < tailBlocks; block++) {
    Compress(copy.State_, std::span<const uint8_t, 64>(tail.data() + block * 64, 64));
  }
  std::array<uint8_t, 32> digest{};
  for (size_t i = 0; i < copy.State_.size(); ++i) {
    for (size_t n = 0; n < kWordBytes; ++n) {
      digest[i * kWordBytes + n] =
          static_cast<uint8_t>(copy.State_[i] >> ((kWordBytes - 1u - n) * kByteShift));
    }
  }
  return digest;
#endif
}

std::string Sha256Prefix::Hex(const void *data, size_t bytes) const {
  const auto digest = Digest(data, bytes);
  return DigestHex(digest);
}

std::array<uint8_t, 32> Sha256Digest(const void *data, size_t bytes) {
#ifdef __linux__
  const auto *method = Sha256Method();
  std::array<uint8_t, 32> digest{};
  unsigned written = 0;
  if (method != nullptr && EVP_Digest(data, bytes, digest.data(), &written, method, nullptr) == 1 &&
      written == digest.size()) {
    return digest;
  }
#endif
  return Sha256Prefix(nullptr, 0).Digest(data, bytes);
}

Sha256Stream::Sha256Stream() noexcept {
#ifdef __linux__
  const auto *method = Sha256Method();
  if (method == nullptr) { return; }
  auto *context = EVP_MD_CTX_new();
  Context_ = context;
  if (context != nullptr && EVP_DigestInit_ex(context, method, nullptr) == 1) {
    Status_ = Status::Ready;
  }
#else
  Status_ = Status::Ready;
#endif
}

Sha256Stream::~Sha256Stream() {
#ifdef __linux__
  EVP_MD_CTX_free(static_cast<EVP_MD_CTX *>(Context_));
#endif
}

void Sha256Stream::Update(const void *data, size_t bytes) noexcept {
  if (Status_ != Status::Ready || bytes == 0) { return; }
#ifdef __linux__
  if (EVP_DigestUpdate(static_cast<EVP_MD_CTX *>(Context_), data, bytes) != 1) {
    Status_ = Status::Failed;
  }
#else
  Prefix_.Append(data, bytes);
#endif
}

std::expected<std::array<uint8_t, 32>, Sha256StreamError> Sha256Stream::Finish() noexcept {
  if (Status_ == Status::Finished) { return std::unexpected(Sha256StreamError::Finished); }
  Update(Buffer_.data(), Buffered_);
  Buffered_ = 0;
  if (Status_ == Status::Failed) { return std::unexpected(Sha256StreamError::Provider); }
  Status_ = Status::Finished;
#ifdef __linux__
  std::array<uint8_t, 32> digest{};
  unsigned written = 0;
  if (EVP_DigestFinal_ex(static_cast<EVP_MD_CTX *>(Context_), digest.data(), &written) != 1 ||
      written != digest.size()) {
    Status_ = Status::Failed;
    return std::unexpected(Sha256StreamError::Provider);
  }
  return digest;
#else
  return Prefix_.Digest(nullptr, 0);
#endif
}

std::string Sha256Hex(const void *data, size_t bytes) {
  return DigestHex(Sha256Digest(data, bytes));
}

std::string Sha256Hex(std::string_view text) {
  return Sha256Hex(text.data(), text.size());
}

}
