#include "TextTarget.h"

#include <system_error>

#include <cerrno>
#include <cstdio>
#include <cstring>

namespace outshine {

TextTarget::TextTarget(TextStream stream)
    : Name_(stream == TextStream::Stdout ? "stdout" : "stderr"),
      File_(stream == TextStream::Stdout ? stdout : stderr) {}

TextTarget::TextTarget(const std::string &path) : Name_(path) {
  Owned_.reset(std::fopen(path.c_str(), "w"));
  if (!Owned_) {
    Refusal_ = "cannot write " + path + ": " + std::system_category().message(errno);
    return;
  }
  File_ = Owned_.get();
}

} // namespace outshine
