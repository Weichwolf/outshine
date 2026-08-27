#ifndef RENDER_PICTURES_H
#define RENDER_PICTURES_H

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "Image.h"

namespace outshine::Render::Parity {

class Pictures {
public:
  explicit Pictures(std::string directory) : Directory_(std::move(directory)) {
    if (!Directory_.empty() && Directory_.back() != '/') { Directory_ += '/'; }
  }

  [[nodiscard]] bool Png(const std::string &name, const std::vector<uint8_t> &rgba, int width,
                         int height, std::string &error) const {
    if (!Names(name, error)) { return false; }
    std::vector<uint8_t> encoded;
    if (!outshine::Core::EncodePng(rgba.data(), width, height, encoded)) {
      error = name + " did not encode as a PNG at " + std::to_string(width) + "x" +
              std::to_string(height);
      return false;
    }
    std::FILE *file = std::fopen((Directory_ + name).c_str(), "wb");
    if (!file) {
      error = Directory_ + name + " could not be opened for writing";
      return false;
    }
    const bool whole = std::fwrite(encoded.data(), 1, encoded.size(), file) == encoded.size();
    std::fclose(file);
    if (!whole) {
      error = Directory_ + name + " was opened and not written whole";
      return false;
    }
    return true;
  }

private:
  [[nodiscard]] bool Names(const std::string &name, std::string &error) const {
    if (name.empty() || name.front() == '/' || name.find("..") != std::string::npos) {
      error = "'" + name + "' is not a name this sink will store under " + Directory_;
      return false;
    }
    return true;
  }

  std::string Directory_;
};

}
#endif
