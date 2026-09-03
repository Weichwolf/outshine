#include "TerrainGrid.h"

#include "GroundSample.h"

#include "Log.h"
#include "Png.h"
#include <algorithm>
#include <vector>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <utility>

namespace outshine::Ground {

constexpr float kTerrariumOffsetM = 32768.0f;

constexpr float kByteSteps = 256.0f;

namespace {

struct Texel {
  uint32_t Row = 0;
  uint32_t Col = 0;
};

float SoundNeighbourMean(const TerrainField &field, Texel at) {
  double summed = 0.0;
  size_t took = 0;
  for (int dr = -1; dr <= 1; ++dr) {
    for (int dc = -1; dc <= 1; ++dc) {
      const int64_t nr = static_cast<int64_t>(at.Row) + dr;
      const int64_t nc = static_cast<int64_t>(at.Col) + dc;
      if (nr < 0 || nc < 0 || std::cmp_greater_equal(nr, field.Rows()) ||
          std::cmp_greater_equal(nc, field.Cols())) {
        continue;
      }
      const float held = field.AtM(static_cast<uint32_t>(nr), static_cast<uint32_t>(nc));
      if (!GroundSample::HeightIsOnEarth(held)) { continue; }
      summed += held;
      ++took;
    }
  }
  return took > 0 ? static_cast<float>(summed / static_cast<double>(took)) : 0.0f;
}

bool AnySoundSample(const TerrainField &field) {
  for (uint32_t r = 0; r < field.Rows(); ++r) {
    for (uint32_t c = 0; c < field.Cols(); ++c) {
      if (GroundSample::HeightIsOnEarth(field.AtM(r, c))) { return true; }
    }
  }
  return false;
}

constexpr float kOutlierDeviations = 20.0f;
constexpr float kLeastDeviationM = 1.0f;

float MedianOf(std::vector<float> &held) {
  if (held.empty()) { return 0.0f; }
  const size_t middle = held.size() / 2;
  std::ranges::nth_element(held, held.begin() + static_cast<ptrdiff_t>(middle));
  return held[middle];
}

size_t FlattenOutliers(TerrainField &field) {
  std::vector<float> sound;
  sound.reserve(static_cast<size_t>(field.Rows()) * field.Cols());
  for (uint32_t r = 0; r < field.Rows(); ++r) {
    for (uint32_t c = 0; c < field.Cols(); ++c) {
      const float held = field.AtM(r, c);
      if (GroundSample::HeightIsOnEarth(held)) { sound.push_back(held); }
    }
  }
  if (sound.size() * 2 < static_cast<size_t>(field.Rows()) * field.Cols()) { return 0; }

  const float middle = MedianOf(sound);
  for (float &one : sound) { one = std::fabs(one - middle); }
  const float spread = std::max(MedianOf(sound), kLeastDeviationM);
  const float reach = kOutlierDeviations * spread;

  size_t caught = 0;
  for (uint32_t r = 0; r < field.Rows(); ++r) {
    for (uint32_t c = 0; c < field.Cols(); ++c) {
      if (std::fabs(field.AtM(r, c) - middle) <= reach) { continue; }
      field.SetM(r, c, middle);
      ++caught;
    }
  }
  return caught;
}

bool FillOffEarth(TerrainField &field) {
  if (!AnySoundSample(field)) { return false; }
  for (uint32_t r = 0; r < field.Rows(); ++r) {
    for (uint32_t c = 0; c < field.Cols(); ++c) {
      if (GroundSample::HeightIsOnEarth(field.AtM(r, c))) { continue; }
      field.SetM(r, c, SoundNeighbourMean(field, {.Row = r, .Col = c}));
    }
  }
  return true;
}

} // namespace

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
  uint32_t firstRow = read.High;
  uint32_t lastRow = 0;
  uint32_t firstCol = read.Wide;
  uint32_t lastCol = 0;
  for (uint32_t r = 0; r < read.High; r++) {
    const uint8_t *p = read.Bytes.data() + static_cast<size_t>(r) * stride;
    for (uint32_t c = 0; c < read.Wide; c++, p += read.Channels) {
      const float aslM = static_cast<float>(p[0]) * kByteSteps + static_cast<float>(p[1]) +
                         static_cast<float>(p[2]) * (1.0f / kByteSteps) - kTerrariumOffsetM;
      if (!GroundSample::HeightIsOnEarth(aslM)) {
        ++offEarth;
        deepest = aslM < deepest ? aslM : deepest;
        firstRow = std::min(firstRow, r);
        lastRow = std::max(lastRow, r);
        firstCol = std::min(firstCol, c);
        lastCol = std::max(lastCol, c);
      }
      field.SetM(r, c, aslM);
    }
  }
  if (offEarth > 0 && !FillOffEarth(field)) {
    Log::Error("world",
               "dem_all_off_earth",
               {{"pixels", static_cast<int>(offEarth)},
                {"ofPixels", static_cast<int>(read.High * read.Wide)}});
    return Undecodable();
  }
  if (const size_t caught = FlattenOutliers(field); caught > 0) {
    Log::Error("world",
               "dem_outliers",
               {{"pixels", static_cast<int>(caught)},
                {"ofPixels", static_cast<int>(read.High * read.Wide)}});
  }
  if (offEarth > 0) {
    Log::Error("world",
               "dem_off_earth",
               {{"pixels", static_cast<int>(offEarth)},
                {"deepestM", static_cast<int>(deepest)},
                {"ofPixels", static_cast<int>(read.High * read.Wide)},
                {"rowsSpanned", static_cast<int>(lastRow - firstRow + 1)},
                {"colsSpanned", static_cast<int>(lastCol - firstCol + 1)}});
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
