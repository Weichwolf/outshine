#ifndef OUTSHINE_BASE_IO_TEXTTARGET_H
#define OUTSHINE_BASE_IO_TEXTTARGET_H

#include <cstdio>
#include <memory>
#include <string>

namespace outshine {

enum class TextStream { Stdout, Stderr };

class TextTarget {
public:
  explicit TextTarget(TextStream stream) noexcept;
  explicit TextTarget(const std::string &path);

  [[nodiscard]] const std::string &Refusal() const noexcept { return Refusal_; }

  [[nodiscard]] const std::string &Name() const noexcept { return Name_; }

  [[nodiscard]] std::FILE *File() const noexcept { return File_; }

private:
  using Owned = std::unique_ptr<std::FILE, int (*)(std::FILE *)>;

  std::string Name_, Refusal_;

  Owned Owned_{nullptr, &std::fclose};
  std::FILE *File_ = nullptr;
};

}
#endif
