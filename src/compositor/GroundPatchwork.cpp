#include "GroundPatchwork.h"

#include <cmath>

#include "TileGeodesy.h"

namespace outshine {

void NormalsFrom(const std::vector<float> &positionM, const std::vector<uint32_t> &index,
                 std::vector<float> &into) {
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

constexpr long kBlockTiles = 4;

std::expected<Patchwork, std::string> LayPatchwork(TileMeshes &tiles, const Around &over) {
  if (over.Zoom <= 0) {
    return std::unexpected("a patchwork is laid at a declared zoom, and this one asks for " +
                           std::to_string(over.Zoom));
  }
  if (over.Levels < 1) {
    return std::unexpected("a cascade lays at least its finest level, and this one lays " +
                           std::to_string(over.Levels));
  }

  Patchwork out;
  bool anchored = false;

  long fineX0 = 0, fineX1 = -1, fineY0 = 0, fineY1 = -1;
  const int levels = over.Levels < 1 ? 1 : over.Levels;
  for (int level = 0; level < levels; ++level) {
    const int zoom = over.Zoom - level;
    if (zoom < 1) { break; }
    const long span = 1L << level;
    const Ground::TileFrac at =
        Ground::ToTileFracClamped(Ground::Geo{.LonDeg = over.LonDeg, .LatDeg = over.LatDeg}, zoom);
    const long originX = 2 * (long)std::floor(((double)(long)std::floor(at.X) - 1.0) / 2.0);
    const long originY = 2 * (long)std::floor(((double)(long)std::floor(at.Y) - 1.0) / 2.0);
    for (long row = 0; row < kBlockTiles; ++row) {
      for (long column = 0; column < kBlockTiles; ++column) {
      long x = originX + column, y = originY + row;
      const long heldX0 = x * span, heldX1 = heldX0 + span - 1;
      const long heldY0 = y * span, heldY1 = heldY0 + span - 1;
      if (fineX1 >= fineX0 && heldX0 >= fineX0 && heldX1 <= fineX1 && heldY0 >= fineY0 &&
          heldY1 <= fineY1) {
        ++out.Skipped;
        continue;
      }
      if (fineX1 >= fineX0 && heldX1 >= fineX0 && heldX0 <= fineX1 && heldY1 >= fineY0 &&
          heldY0 <= fineY1) {
        ++out.Overlapped;
      }
      if (!Ground::WrapTile(zoom, &x, &y)) { continue; }
      TileBuild built;
      const TileMeshes::Reply said =
          over.Awaited ? tiles.MeshAwaited(zoom, (uint32_t)x, (uint32_t)y, over.Grid, &built)
                       : tiles.Mesh(zoom, (uint32_t)x, (uint32_t)y, over.Grid, &built);
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
      for (size_t vertex = 0; vertex + kTileVertexFloats <= built.Verts.size();
           vertex += kTileVertexFloats) {
        out.PositionM.push_back((float)((double)built.Verts[vertex] + shift[0]));
        out.PositionM.push_back((float)((double)built.Verts[vertex + 1] + shift[1]));
        out.PositionM.push_back((float)((double)built.Verts[vertex + 2] + shift[2]));
        out.Uv.push_back(built.Verts[vertex + 3]);
        out.Uv.push_back(built.Verts[vertex + 4]);
        out.NormalM.push_back(built.Verts[vertex + 5]);
        out.NormalM.push_back(built.Verts[vertex + 6]);
        out.NormalM.push_back(built.Verts[vertex + 7]);
      }
      out.ClustersHeld += built.Clusters.size();
      if (over.FocalPx > 0.0f && !built.Clusters.empty()) {
        const double eyeInTile[3] = {over.EyeM[0] - built.OriginEcef[0],
                                     over.EyeM[1] - built.OriginEcef[1],
                                     over.EyeM[2] - built.OriginEcef[2]};
        for (const DagCluster &cluster : built.Clusters) {
          if (!DagSelect(cluster, eyeInTile, over.FocalPx, over.Tau, over.Up)) { continue; }
          ++out.ClustersDrawn;
          for (uint32_t step = 0; step < cluster.Count; ++step) {
            out.Index.push_back(first + built.Idx[cluster.First + step]);
          }
        }
      } else {
        out.ClustersDrawn += built.Clusters.size();
        for (const uint32_t one : built.Idx) { out.Index.push_back(first + one); }
      }
      out.WorstErrM = (double)built.ErrM > out.WorstErrM ? (double)built.ErrM : out.WorstErrM;
      ++out.Tiles;
      }
    }
    fineX0 = originX * span;
    fineX1 = (originX + kBlockTiles) * span - 1;
    fineY0 = originY * span;
    fineY1 = (originY + kBlockTiles) * span - 1;
    out.ReachTiles = kBlockTiles * span;
    out.CoarsestZoom = zoom;
  }

  if (out.Tiles == 0) {
    return std::unexpected(
        "no tile of the " + std::to_string(levels) + "-level cascade around " +
        std::to_string(over.LatDeg) + ", " + std::to_string(over.LonDeg) + " meshed -- " +
        std::to_string(out.Pending) + " pending, " + std::to_string(out.Absent) + " absent, " +
        std::to_string(out.Refused) + " refused");
  }
  return out;
}

}
