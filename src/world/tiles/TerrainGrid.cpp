#include "TerrainGrid.h"

#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_surface.h>
#include <SDL3_image/SDL_image.h>

#include <memory>

#include "Log.h"

namespace outshine::World {

namespace {

struct SurfaceDeleter {
  void operator()(SDL_Surface *s) const { SDL_DestroySurface(s); }
};
using OwnedSurface = std::unique_ptr<SDL_Surface, SurfaceDeleter>;

}

TerrainGrid TerrainGrid::FromTerrariumPng(const uint8_t *png, size_t len) {
  if (!png || len == 0) return NotHere();

  SDL_IOStream *io = SDL_IOFromConstMem(png, len);
  if (!io) return Undecodable();
  const OwnedSurface decoded(IMG_Load_IO(io, true));
  if (!decoded) {
    Log::Error("world", "dem_undecodable", {{"bytes", (int)len}, {"why", std::string(SDL_GetError())}});
    return Undecodable();
  }

  const OwnedSurface rgb(SDL_ConvertSurface(decoded.get(), SDL_PIXELFORMAT_RGB24));
  if (!rgb || rgb->w <= 0 || rgb->h <= 0) return Undecodable();

  TerrainField field((uint32_t)rgb->h, (uint32_t)rgb->w);
  const uint8_t *rows = (const uint8_t *)rgb->pixels;
  for (int r = 0; r < rgb->h; r++) {
    const uint8_t *p = rows + (size_t)r * (size_t)rgb->pitch;
    for (int c = 0; c < rgb->w; c++, p += 3) {

      field.SetM((uint32_t)r, (uint32_t)c,
                 (float)p[0] * 256.0f + (float)p[1] + (float)p[2] * (1.0f / 256.0f) - 32768.0f);
    }
  }
  return Holding(std::move(field));
}

TerrainMesh TerrainMesh::Over(const TerrainField &field, const TileEnuMap &map, uint32_t stride) {
  if (!field.Meshable()) return TerrainMesh(State::FieldTooSmall);
  if (map.Extent() == 0) return TerrainMesh(State::FrameUnusable);
  if (stride == 0) return TerrainMesh(State::StrideDoesNotDivide);

  if (((field.Rows() - 1) % stride) != 0 || ((field.Cols() - 1) % stride) != 0)
    return TerrainMesh(State::StrideDoesNotDivide);

  const uint32_t rowsOut = PostingsPerEdge(field.Rows(), stride);
  const uint32_t colsOut = PostingsPerEdge(field.Cols(), stride);

  TerrainMesh mesh(State::Built);
  mesh.PositionsEnuM_.resize((size_t)rowsOut * (size_t)colsOut * 3);

  const double tileWidthE = map.ScaleE() * (double)map.Extent();
  const double tileHeightN = map.ScaleN() * (double)map.Extent();

  for (uint32_t r = 0; r < rowsOut; r++) {
    const double fr = PostingFrac(r, rowsOut);
    for (uint32_t c = 0; c < colsOut; c++) {
      const double fc = PostingFrac(c, colsOut);
      const size_t vi = (size_t)r * colsOut + c;

      mesh.PositionsEnuM_[vi * 3 + 0] = (float)(map.OriginE() + fc * tileWidthE);
      mesh.PositionsEnuM_[vi * 3 + 1] = (float)(map.OriginN() + fr * tileHeightN);
      mesh.PositionsEnuM_[vi * 3 + 2] = field.InterpolatedM(fc, fr);
    }
  }
  return mesh;
}

}
