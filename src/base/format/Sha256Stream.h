#ifndef OUTSHINE_BASE_FORMAT_SHA256STREAM_H
#define OUTSHINE_BASE_FORMAT_SHA256STREAM_H

#ifndef __linux__
#include "Sha256.h"
#endif
#include <algorithm>
#include <array>
#include <cstring>
#include <cstddef>
#include <cstdint>
#include <expected>

namespace outshine {

enum class Sha256StreamError { Provider, Finished };

// A single-use stream. Failed providers never return a digest.
class Sha256Stream {
public:
  Sha256Stream() noexcept;
  ~Sha256Stream();
  Sha256Stream(const Sha256Stream &) = delete;
  Sha256Stream &operator=(const Sha256Stream &) = delete;

  void Append(const void *data, size_t bytes) noexcept {
    const auto *cursor = static_cast<const uint8_t *>(data);
    while (Status_ == Status::Ready && bytes != 0) {
      if (Buffered_ == 0 && bytes >= Buffer_.size()) {
        Update(cursor, bytes);
        return;
      }
      const size_t count = std::min(bytes, Buffer_.size() - Buffered_);
      std::memcpy(Buffer_.data() + Buffered_, cursor, count);
      Buffered_ += count;
      cursor += count;
      bytes -= count;
      if (Buffered_ == Buffer_.size()) {
        Update(Buffer_.data(), Buffered_);
        Buffered_ = 0;
      }
    }
  }

  [[nodiscard]] std::expected<std::array<uint8_t, 32>, Sha256StreamError> Finish() noexcept;

private:
  void Update(const void *data, size_t bytes) noexcept;
  enum class Status { Ready, Failed, Finished };

#ifdef __linux__
  void *Context_ = nullptr;
#else
  Sha256Prefix Prefix_{nullptr, 0};
#endif
  std::array<uint8_t, 1024> Buffer_;
  size_t Buffered_ = 0;
  Status Status_ = Status::Failed;
};

}
#endif
