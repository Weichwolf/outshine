#include "Image.h"

#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_surface.h>
#include <SDL3_image/SDL_image.h>

namespace outshine::Clients {

namespace {

struct DynamicIo {
  SDL_IOStream *Stream = nullptr;

  explicit DynamicIo() : Stream(SDL_IOFromDynamicMem()) {}
  ~DynamicIo() {
    if (Stream) SDL_CloseIO(Stream);
  }
  DynamicIo(const DynamicIo &) = delete;
  DynamicIo &operator=(const DynamicIo &) = delete;
};

struct OwnedSurface {
  SDL_Surface *Surface = nullptr;

  explicit OwnedSurface(SDL_Surface *surface) : Surface(surface) {}
  ~OwnedSurface() {
    if (Surface) SDL_DestroySurface(Surface);
  }
  OwnedSurface(const OwnedSurface &) = delete;
  OwnedSurface &operator=(const OwnedSurface &) = delete;
};

}

bool DecodeImage(const uint8_t *bytes, size_t count, Raster &out) {
  out = Raster();
  if (!bytes || count == 0) return false;

  SDL_IOStream *io = SDL_IOFromConstMem(bytes, count);
  if (!io) return false;

  const OwnedSurface decoded(IMG_Load_IO(io, true));
  if (!decoded.Surface) return false;

  const OwnedSurface rgba(SDL_ConvertSurface(decoded.Surface, SDL_PIXELFORMAT_RGBA32));
  if (!rgba.Surface) return false;
  if (rgba.Surface->w <= 0 || rgba.Surface->h <= 0) return false;

  out.Width = rgba.Surface->w;
  out.Height = rgba.Surface->h;
  out.Rgba.resize(static_cast<size_t>(out.Width) * static_cast<size_t>(out.Height) * 4u);
  const uint8_t *source = static_cast<const uint8_t *>(rgba.Surface->pixels);
  const size_t rowBytes = static_cast<size_t>(out.Width) * 4u;
  for (int row = 0; row < out.Height; ++row) {
    const uint8_t *from = source + static_cast<size_t>(row) * static_cast<size_t>(rgba.Surface->pitch);
    uint8_t *to = out.Rgba.data() + static_cast<size_t>(row) * rowBytes;
    for (size_t at = 0; at < rowBytes; ++at) { to[at] = from[at]; }
  }
  return true;
}

bool EncodePng(const uint8_t *rgba, int width, int height, std::vector<uint8_t> &out) {
  out.clear();
  if (!rgba || width <= 0 || height <= 0) return false;

  DynamicIo io;
  if (!io.Stream) return false;
  const OwnedSurface surface(
      SDL_CreateSurfaceFrom(width, height, SDL_PIXELFORMAT_RGBA32, (void *)rgba, width * 4));
  if (!surface.Surface) return false;
  if (!IMG_SavePNG_IO(surface.Surface, io.Stream, false)) return false;

  const Sint64 size = SDL_GetIOSize(io.Stream);
  if (size <= 0) return false;
  out.resize((size_t)size);
  if (SDL_SeekIO(io.Stream, 0, SDL_IO_SEEK_SET) < 0) return false;
  return SDL_ReadIO(io.Stream, out.data(), out.size()) == out.size();
}

}
