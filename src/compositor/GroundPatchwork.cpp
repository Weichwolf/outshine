#include "GroundPatchwork.h"

#include <cmath>

#include "TileGeodesy.h"

namespace outshine {

void NormalsFrom(const std::vector<float> &positionM,
                 const std::vector<uint32_t> &index,
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

void SphereTile(int zoom, uint32_t x, uint32_t y, int grid, TileBuild *out) {
  const int side = grid < 2 ? 2 : grid;
  const Ground::GeoBounds bounds = Ground::TileBounds(zoom, x, y);
  const Ground::Geo middle{.LonDeg = 0.5 * (bounds.MinLonDeg + bounds.MaxLonDeg),
                           .LatDeg = 0.5 * (bounds.MinLatDeg + bounds.MaxLatDeg),
                           .AltM = 0.0};
  const Ground::Ecef anchor = Ground::GeoToEcefWgs84(middle);
  out->OriginEcef[0] = anchor.X;
  out->OriginEcef[1] = anchor.Y;
  out->OriginEcef[2] = anchor.Z;
  out->ErrM = 0.0f;
  out->Clusters.clear();
  out->Verts.clear();
  out->Idx.clear();
  out->Verts.reserve((size_t)side * (size_t)side * kTileVertexFloats);
  for (int row = 0; row < side; ++row) {
    const double v = (double)row / (double)(side - 1);
    for (int column = 0; column < side; ++column) {
      const double u = (double)column / (double)(side - 1);
      const Ground::Geo where{
          .LonDeg = bounds.MinLonDeg + u * (bounds.MaxLonDeg - bounds.MinLonDeg),
          .LatDeg = bounds.MaxLatDeg + v * (bounds.MinLatDeg - bounds.MaxLatDeg),
          .AltM = 0.0};
      const Ground::Ecef at = Ground::GeoToEcefWgs84(where);
      const double away = std::sqrt(at.X * at.X + at.Y * at.Y + at.Z * at.Z);
      out->Verts.push_back((float)(at.X - anchor.X));
      out->Verts.push_back((float)(at.Y - anchor.Y));
      out->Verts.push_back((float)(at.Z - anchor.Z));
      out->Verts.push_back((float)u);
      out->Verts.push_back((float)v);
      out->Verts.push_back((float)(at.X / away));
      out->Verts.push_back((float)(at.Y / away));
      out->Verts.push_back((float)(at.Z / away));
    }
  }
  out->Idx.reserve((size_t)(side - 1) * (size_t)(side - 1) * 6u);
  for (int row = 0; row + 1 < side; ++row) {
    for (int column = 0; column + 1 < side; ++column) {
      const uint32_t a = (uint32_t)(row * side + column);
      const uint32_t b = a + 1;
      const uint32_t c = a + (uint32_t)side;
      const uint32_t d = c + 1;
      out->Idx.push_back(a);
      out->Idx.push_back(c);
      out->Idx.push_back(b);
      out->Idx.push_back(b);
      out->Idx.push_back(c);
      out->Idx.push_back(d);
    }
  }
}

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
  for (int axis = 0; axis < 3; ++axis) { out.ClusterEyeM[axis] = over.EyeM[axis]; }
  bool anchored = false;

  const int levels = over.Levels < 1 ? 1 : over.Levels;
  const long widest = kBlockTiles * (1L << (levels - 1));
  long maskX0 = 0, maskY0 = 0;
  {
    const int coarsest = over.Zoom - (levels - 1) < 1 ? 1 : over.Zoom - (levels - 1);
    const long span = 1L << (over.Zoom - coarsest);
    const Ground::TileFrac at = Ground::ToTileFracClamped(
        Ground::Geo{.LonDeg = over.LonDeg, .LatDeg = over.LatDeg}, coarsest);
    maskX0 = 2 * (long)std::floor(((double)(long)std::floor(at.X) - 1.0) / 2.0) * span;
    maskY0 = 2 * (long)std::floor(((double)(long)std::floor(at.Y) - 1.0) / 2.0) * span;
  }
  std::vector<uint8_t> covering((size_t)widest * (size_t)widest, 0u);
  const auto marked = [&covering, widest, maskX0, maskY0](long fx, long fy) -> uint8_t * {
    const long ix = fx - maskX0, iy = fy - maskY0;
    if (ix < 0 || iy < 0 || ix >= widest || iy >= widest) { return nullptr; }
    return &covering[(size_t)iy * (size_t)widest + (size_t)ix];
  };
  for (int level = 0; level < levels; ++level) {
    const int zoom = over.Zoom - level;
    if (zoom < 1) { break; }
    const long span = 1L << level;
    const Ground::TileFrac at =
        Ground::ToTileFracClamped(Ground::Geo{.LonDeg = over.LonDeg, .LatDeg = over.LatDeg}, zoom);
    const long originX = 2 * (long)std::floor(((double)(long)std::floor(at.X) - 1.0) / 2.0);
    const long originY = 2 * (long)std::floor(((double)(long)std::floor(at.Y) - 1.0) / 2.0);
    std::vector<std::pair<long, long>> standing;
    for (long row = 0; row < kBlockTiles; ++row) {
      for (long column = 0; column < kBlockTiles; ++column) {
        long x = originX + column, y = originY + row;
        const long heldX0 = x * span, heldX1 = heldX0 + span - 1;
        const long heldY0 = y * span, heldY1 = heldY0 + span - 1;
        bool covered = true, touches = false;
        for (long fy = heldY0; fy <= heldY1; ++fy) {
          for (long fx = heldX0; fx <= heldX1; ++fx) {
            const uint8_t *const cell = marked(fx, fy);
            if (cell != nullptr && *cell != 0u) {
              touches = true;
            } else {
              covered = false;
            }
          }
        }
        if (covered) {
          ++out.Skipped;
          continue;
        }
        if (touches) { ++out.Overlapped; }
        if (!Ground::WrapTile(zoom, &x, &y)) { continue; }
        TileBuild built;
        const TileMeshes::Reply said =
            over.Asking ? tiles.Wants(zoom, (uint32_t)x, (uint32_t)y, over.Grid)
                        : tiles.Mesh(zoom, (uint32_t)x, (uint32_t)y, over.Grid, &built);
        bool ofTheGround = true;
        if (zoom >= 0 && zoom < 24) { ++out.WantedAtZoom[zoom]; }
        if (said == TileMeshes::Reply::Pending) {
          ++out.Pending;
          if (zoom >= 0 && zoom < 24) { ++out.PendingAtZoom[zoom]; }
          ofTheGround = false;
        } else if (said == TileMeshes::Reply::Absent || said == TileMeshes::Reply::Undeclared) {
          ++out.Absent;
          ofTheGround = false;
        } else if (said == TileMeshes::Reply::Refused) {
          ++out.Refused;
          ofTheGround = false;
        }
        if (ofTheGround && (built.Verts.empty() || built.Idx.empty())) { ofTheGround = false; }
        if (!ofTheGround && !over.Asking) { ++out.Bare; }
        if (over.Asking) {
          ++out.Tiles;
          if (ofTheGround) { standing.push_back({heldX0, heldY0}); }
          continue;
        }
        if (!ofTheGround) { SphereTile(zoom, (uint32_t)x, (uint32_t)y, over.Grid, &built); }
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
        const uint32_t rebase = (uint32_t)out.AllIndex.size();
        for (const uint32_t one : built.Idx) { out.AllIndex.push_back(first + one); }
        for (const DagCluster &cluster : built.Clusters) {
          DagCluster carried = cluster;
          carried.First = rebase + cluster.First;
          for (int axis = 0; axis < 3; ++axis) {
            carried.SelfCenter[axis] = (float)((double)cluster.SelfCenter[axis] +
                                               built.OriginEcef[axis] - out.ClusterEyeM[axis]);
            carried.ParentCenter[axis] = (float)((double)cluster.ParentCenter[axis] +
                                                 built.OriginEcef[axis] - out.ClusterEyeM[axis]);
          }
          out.Clusters.push_back(carried);
        }
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
        if (ofTheGround) { standing.push_back({heldX0, heldY0}); }
      }
    }
    for (const auto &one : standing) {
      for (long fy = one.second; fy < one.second + span; ++fy) {
        for (long fx = one.first; fx < one.first + span; ++fx) {
          uint8_t *const cell = marked(fx, fy);
          if (cell != nullptr) { *cell = 1u; }
        }
      }
    }
    standing.clear();
    out.ReachTiles = kBlockTiles * span;
    out.CoarsestZoom = zoom;
  }

  if (out.Tiles == 0) {
    return std::unexpected("no tile of the " + std::to_string(levels) + "-level cascade around " +
                           std::to_string(over.LatDeg) + ", " + std::to_string(over.LonDeg) +
                           " meshed -- " + std::to_string(out.Pending) + " pending, " +
                           std::to_string(out.Absent) + " absent, " + std::to_string(out.Refused) +
                           " refused");
  }
  return out;
}

}
