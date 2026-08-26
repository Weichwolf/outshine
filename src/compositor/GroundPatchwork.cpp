#include "GroundPatchwork.h"

#include <cmath>

#include "TileGeodesy.h"

namespace outshine {

namespace {

void NormalsFrom(const std::vector<float> &positionM,
                               const std::vector<uint32_t> &index, std::vector<float> &into) {
  into.assign(positionM.size(), 0.0f);
  for (size_t at = 0; at + 2 < index.size(); at += 3) {
    const uint32_t a = index[at], b = index[at + 1], c = index[at + 2];
    if ((size_t)c * 3 + 2 >= positionM.size()) { continue; }
    const float abx = positionM[b * 3] - positionM[a * 3];
    const float aby = positionM[b * 3 + 1] - positionM[a * 3 + 1];
    const float abz = positionM[b * 3 + 2] - positionM[a * 3 + 2];
    const float acx = positionM[c * 3] - positionM[a * 3];
    const float acy = positionM[c * 3 + 1] - positionM[a * 3 + 1];
    const float acz = positionM[c * 3 + 2] - positionM[a * 3 + 2];
    const float nx = aby * acz - abz * acy;
    const float ny = abz * acx - abx * acz;
    const float nz = abx * acy - aby * acx;
    for (const uint32_t one : {a, b, c}) {
      into[one * 3] += nx;
      into[one * 3 + 1] += ny;
      into[one * 3 + 2] += nz;
    }
  }
  for (size_t at = 0; at + 2 < into.size(); at += 3) {
    const float length =
        std::sqrt(into[at] * into[at] + into[at + 1] * into[at + 1] + into[at + 2] * into[at + 2]);
    if (length <= 0.0f) {
      into[at] = 0.0f;
      into[at + 1] = 1.0f;
      into[at + 2] = 0.0f;
      continue;
    }
    into[at] /= length;
    into[at + 1] /= length;
    into[at + 2] /= length;
  }
}

}

std::expected<Patchwork, std::string> LayPatchwork(TileMeshes &tiles, const Around &over) {
  if (over.Zoom <= 0) {
    return std::unexpected("a patchwork is laid at a declared zoom, and this one asks for " +
                           std::to_string(over.Zoom));
  }
  if (over.Ring < 0) {
    return std::unexpected("a ring reaches out from its centre tile, and this one reaches " +
                           std::to_string(over.Ring));
  }

  const Ground::TileFrac at =
      Ground::ToTileFracClamped(Ground::Geo{.LonDeg = over.LonDeg, .LatDeg = over.LatDeg}, over.Zoom);
  Patchwork out;
  bool anchored = false;

  for (long row = -over.Ring; row <= over.Ring; ++row) {
    for (long column = -over.Ring; column <= over.Ring; ++column) {
      long x = (long)std::floor(at.X) + column, y = (long)std::floor(at.Y) + row;
      if (!Ground::WrapTile(over.Zoom, &x, &y)) { continue; }
      TileBuild built;
      const TileMeshes::Reply said =
          tiles.Mesh(over.Zoom, (uint32_t)x, (uint32_t)y, over.Grid, &built);
      if (said == TileMeshes::Reply::Pending) {
        ++out.Pending;
        continue;
      }
      if (said == TileMeshes::Reply::Absent ||
          said == TileMeshes::Reply::Undeclared) {
        ++out.Absent;
        continue;
      }
      if (said == TileMeshes::Reply::Refused) {
        ++out.Refused;
        continue;
      }
      if (built.Verts.empty() || built.Idx.empty()) { continue; }

      if (!anchored) {
        for (int axis = 0; axis < 3; ++axis) { out.OriginEcef[axis] = built.OriginEcef[axis]; }
        anchored = true;
      }
      const double shift[3] = {built.OriginEcef[0] - out.OriginEcef[0],
                               built.OriginEcef[1] - out.OriginEcef[1],
                               built.OriginEcef[2] - out.OriginEcef[2]};
      const uint32_t first = (uint32_t)(out.PositionM.size() / 3);
      for (size_t vertex = 0; vertex + 2 < built.Verts.size();
           vertex += kTileVertexFloats) {
        out.PositionM.push_back((float)((double)built.Verts[vertex] + shift[0]));
        out.PositionM.push_back((float)((double)built.Verts[vertex + 1] + shift[1]));
        out.PositionM.push_back((float)((double)built.Verts[vertex + 2] + shift[2]));
      }
      for (const uint32_t one : built.Idx) { out.Index.push_back(first + one); }
      out.WorstErrM = (double)built.ErrM > out.WorstErrM ? (double)built.ErrM : out.WorstErrM;
      ++out.Tiles;
    }
  }

  if (out.Tiles == 0) {
    return std::unexpected(
        "no tile of the " + std::to_string(2 * over.Ring + 1) + " by " +
        std::to_string(2 * over.Ring + 1) + " ring around " + std::to_string(over.LatDeg) + ", " +
        std::to_string(over.LonDeg) + " meshed -- " + std::to_string(out.Pending) +
        " pending, " + std::to_string(out.Absent) + " absent, " + std::to_string(out.Refused) +
        " refused");
  }
  NormalsFrom(out.PositionM, out.Index, out.NormalM);
  return out;
}

}
