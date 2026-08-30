#ifndef OUTSHINE_BASE_IO_HEAP_H
#define OUTSHINE_BASE_IO_HEAP_H

#include <cstddef>

namespace outshine {

class Heap {
public:
  static void *Take(const char *item, size_t bytes);

  static size_t LiveBytes();

  class Tagged {
  public:
    explicit Tagged(const char *tag) noexcept;
    ~Tagged() noexcept;
    Tagged(const Tagged &) = delete;
    Tagged &operator=(const Tagged &) = delete;

  private:
    const char *Held_;
  };

  [[nodiscard]] static size_t TakenUnder(const char *tag);

  [[nodiscard]] static size_t TagCount();
  [[nodiscard]] static const char *TagAt(size_t at);
  [[nodiscard]] static size_t TakenAt(size_t at);

  [[noreturn]] static void Exhausted(const char *item);
};

} // namespace outshine
#endif
