#include "Png.h"

#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_surface.h>
#include <SDL3_image/SDL_image.h>

namespace outshine::Clients {

namespace {

/* SDL grows the stream and hands the block back at close; the copy out of it is what the caller
 * owns, so nothing SDL allocated leaves this file. */
struct DynamicIo {
  SDL_IOStream *Stream = nullptr;

  explicit DynamicIo() : Stream(SDL_IOFromDynamicMem()) {}
  ~DynamicIo() {
    if (Stream) SDL_CloseIO(Stream);
  }
  DynamicIo(const DynamicIo &) = delete;
  DynamicIo &operator=(const DynamicIo &) = delete;
};

}  // namespace

bool EncodePng(const uint8_t *rgba, int width, int height, std::vector<uint8_t> &out) {
  out.clear();
  if (!rgba || width <= 0 || height <= 0) return false;

  DynamicIo io;
  if (!io.Stream) return false;
  SDL_Surface *surface =
      SDL_CreateSurfaceFrom(width, height, SDL_PIXELFORMAT_RGBA32, (void *)rgba, width * 4);
  if (!surface) return false;
  const bool wrote = IMG_SavePNG_IO(surface, io.Stream, false);
  SDL_DestroySurface(surface);
  if (!wrote) return false;

  const Sint64 size = SDL_GetIOSize(io.Stream);
  if (size <= 0) return false;
  out.resize((size_t)size);
  if (SDL_SeekIO(io.Stream, 0, SDL_IO_SEEK_SET) < 0) return false;
  return SDL_ReadIO(io.Stream, out.data(), out.size()) == out.size();
}

}  // namespace outshine::Clients
