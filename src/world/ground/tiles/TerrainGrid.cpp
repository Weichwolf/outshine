#include "TerrainGrid.h"

#include "Log.h"
#include "Png.h"

namespace outshine::Ground {

TerrainGrid TerrainGrid::FromTerrariumPng(const uint8_t *png, size_t len) {
  if (!png || len == 0) { return NotHere(); }

  const Io::Png read = Io::ReadPng(png, len);
  if (!read.Read) {
    Log::Error("world", "dem_undecodable", {{"bytes", (int)len}, {"why", read.Error}});
    return Undecodable();
  }

  TerrainField field(read.High, read.Wide);
  const size_t stride = (size_t)read.Wide * read.Channels;
  for (uint32_t r = 0; r < read.High; r++) {
    const uint8_t *p = read.Bytes.data() + (size_t)r * stride;
    for (uint32_t c = 0; c < read.Wide; c++, p += read.Channels) {
      field.SetM(
          r, c, (float)p[0] * 256.0f + (float)p[1] + (float)p[2] * (1.0f / 256.0f) - 32768.0f);
    }
  }
  return Holding(std::move(field));
}

TerrainMesh TerrainMesh::Over(const TerrainField &field, const TileEnuMap &map, uint32_t stride) {
  if (!field.Meshable()) { return TerrainMesh(State::FieldTooSmall); }
  if (map.Extent() == 0) { return TerrainMesh(State::FrameUnusable); }
  if (stride == 0) { return TerrainMesh(State::StrideDoesNotDivide); }

  if (((field.Rows() - 1) % stride) != 0 || ((field.Cols() - 1) % stride) != 0) {
    return TerrainMesh(State::StrideDoesNotDivide);
  }

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
      mesh.PositionsEnuM_[vi * 3 + 2] = field.PostingM(fc, fr);
    }
  }
  return mesh;
}

}
