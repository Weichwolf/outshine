#include "TextTarget.h"

#include <cerrno>
#include <cstdio>
#include <cstring>

namespace outshine {

TextTarget::TextTarget(TextStream stream) noexcept
    : Name_(stream == TextStream::Stdout ? "stdout" : "stderr"),
      File_(stream == TextStream::Stdout ? stdout : stderr) {}

TextTarget::TextTarget(const std::string &path) : Name_(path) {
  Owned_.reset(std::fopen(path.c_str(), "w"));
  if (!Owned_) {
    Refusal_ = "cannot write " + path + ": " + std::strerror(errno);
    return;
  }
  File_ = Owned_.get();
}

} // namespace outshine
