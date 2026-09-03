#include "TerrainGrid.h"

#include "GroundSample.h"

#include "Log.h"
#include "Png.h"
#include <cstdint>
#include <cstddef>
#include <utility>

namespace outshine::Ground {

constexpr float kTerrariumOffsetM = 32768.0f;

constexpr float kByteSteps = 256.0f;

TerrainGrid TerrainGrid::FromTerrariumPng(const uint8_t *png, size_t len) {
  if ((png == nullptr) || len == 0) { return NotHere(); }

  const Io::Png read = Io::ReadPng(png, len);
  if (!read.Read) {
    Log::Error("world", "dem_undecodable", {{"bytes", static_cast<int>(len)}, {"why", read.Error}});
    return Undecodable();
  }

  TerrainField field(read.High, read.Wide);
  const size_t stride = static_cast<size_t>(read.Wide) * read.Channels;
  size_t offEarth = 0;
  float deepest = 0.0f;
  for (uint32_t r = 0; r < read.High; r++) {
    const uint8_t *p = read.Bytes.data() + static_cast<size_t>(r) * stride;
    for (uint32_t c = 0; c < read.Wide; c++, p += read.Channels) {
      const float aslM = static_cast<float>(p[0]) * kByteSteps + static_cast<float>(p[1]) +
                         static_cast<float>(p[2]) * (1.0f / kByteSteps) - kTerrariumOffsetM;
      if (!GroundSample::HeightIsOnEarth(aslM)) {
        ++offEarth;
        deepest = aslM < deepest ? aslM : deepest;
      }
      field.SetM(r, c, aslM);
    }
  }
  if (offEarth > 0) {
    Log::Error("world",
               "dem_off_earth",
               {{"pixels", static_cast<int>(offEarth)},
                {"deepestM", static_cast<int>(deepest)},
                {"ofPixels", static_cast<int>(read.High * read.Wide)}});
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
  mesh.PositionsEnuM_.resize(static_cast<size_t>(rowsOut) * static_cast<size_t>(colsOut) * 3);

  const double tileWidthE = map.ScaleE() * static_cast<double>(map.Extent());
  const double tileHeightN = map.ScaleN() * static_cast<double>(map.Extent());

  for (uint32_t r = 0; r < rowsOut; r++) {
    const double fr = PostingFrac(r, rowsOut);
    for (uint32_t c = 0; c < colsOut; c++) {
      const double fc = PostingFrac(c, colsOut);
      const size_t vi = static_cast<size_t>(r) * colsOut + c;

      mesh.PositionsEnuM_[vi * 3 + 0] = static_cast<float>(map.OriginE() + fc * tileWidthE);
      mesh.PositionsEnuM_[vi * 3 + 1] = static_cast<float>(map.OriginN() + fr * tileHeightN);
      mesh.PositionsEnuM_[vi * 3 + 2] = field.PostingM({.Col = fc, .Row = fr});
    }
  }
  return mesh;
}

} // namespace outshine::Ground
