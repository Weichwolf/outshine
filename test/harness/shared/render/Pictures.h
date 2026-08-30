#ifndef RENDER_PICTURES_H
#define RENDER_PICTURES_H

#include <cstdint>
#include <string>
#include <vector>

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

namespace outshine::Render::Parity {

class Pictures {
public:
  explicit Pictures(std::string directory) : Directory_(std::move(directory)) {
    if (!Directory_.empty() && Directory_.back() != '/') { Directory_ += '/'; }
  }

  // SDL3_IMAGE WRITES THE PNG, because SDL3 supplies the function and this tree does not keep a
  // second mechanism beside one it already carries. It also cost this runner its last reason to
  // reach into `src/content/shade` for the engine's own encoder -- a scorer writing ITS OWN
  // artefacts has no business inside the thing it is scoring.
  [[nodiscard]] bool Png(const std::string &name, const std::vector<uint8_t> &rgba, int width,
                         int height, std::string &error) const {
    if (!Names(name, error)) { return false; }
    if (width <= 0 || height <= 0 ||
        rgba.size() < (size_t)width * (size_t)height * 4u) {
      error = name + " was handed " + std::to_string(rgba.size()) + " bytes for a " +
              std::to_string(width) + "x" + std::to_string(height) + " picture";
      return false;
    }
    SDL_Surface *const holding = SDL_CreateSurfaceFrom(
        width, height, SDL_PIXELFORMAT_RGBA32, const_cast<uint8_t *>(rgba.data()), width * 4);
    if (holding == nullptr) {
      error = name + " found no surface to write from: " + SDL_GetError();
      return false;
    }
    const bool written = IMG_SavePNG(holding, (Directory_ + name).c_str());
    SDL_DestroySurface(holding);
    if (!written) {
      error = Directory_ + name + " was not written: " + SDL_GetError();
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
