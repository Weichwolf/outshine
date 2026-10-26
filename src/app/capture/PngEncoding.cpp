#include "PngEncoding.h"

#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_surface.h>
#include <SDL3_image/SDL_image.h>
#include <cstdint>
#include <cstddef>
#include <vector>

namespace outshine::App {

namespace {

struct DynamicIo {
  SDL_IOStream *Stream = nullptr;

  explicit DynamicIo() : Stream(SDL_IOFromDynamicMem()) {}

  ~DynamicIo() {
    if (Stream != nullptr) { SDL_CloseIO(Stream); }
  }

  DynamicIo(const DynamicIo &) = delete;
  DynamicIo &operator=(const DynamicIo &) = delete;
};

struct OwnedSurface {
  SDL_Surface *Surface = nullptr;

  explicit OwnedSurface(SDL_Surface *surface) : Surface(surface) {}

  ~OwnedSurface() {
    if (Surface != nullptr) { SDL_DestroySurface(Surface); }
  }

  OwnedSurface(const OwnedSurface &) = delete;
  OwnedSurface &operator=(const OwnedSurface &) = delete;
};

}

bool EncodePng(const uint8_t *rgba, int width, int height, std::vector<uint8_t> &out) {
  out.clear();
  if ((rgba == nullptr) || width <= 0 || height <= 0) { return false; }

  const DynamicIo io;
  if (io.Stream == nullptr) { return false; }
  const OwnedSurface surface(
      SDL_CreateSurfaceFrom(width,
                            height,
                            SDL_PIXELFORMAT_RGBA32,
                            const_cast<void *>(static_cast<const void *>(rgba)),
                            width * 4));
  if (surface.Surface == nullptr) { return false; }
  if (!IMG_SavePNG_IO(surface.Surface, io.Stream, false)) { return false; }

  const Sint64 size = SDL_GetIOSize(io.Stream);
  if (size <= 0) { return false; }
  out.resize(static_cast<size_t>(size));
  if (SDL_SeekIO(io.Stream, 0, SDL_IO_SEEK_SET) < 0) { return false; }
  return SDL_ReadIO(io.Stream, out.data(), out.size()) == out.size();
}

}
