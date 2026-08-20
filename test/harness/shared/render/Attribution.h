#ifndef RENDER_ATTRIBUTION_H
#define RENDER_ATTRIBUTION_H

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

#include "Mask.h"

#include "Subject.h"
#include "Transform.h"

namespace outshine::Render::Parity {

struct NodeDisagreement {
  std::string Node;
  size_t Triangles = 0;
  size_t OursOnly = 0;
  size_t TheirsOnly = 0;
};

struct Attribution {
  std::vector<NodeDisagreement> Nodes;
  size_t Unattributed = 0;
  size_t Unprojectable = 0;
};

namespace Detail {

inline bool Inside(const double corner[3][2], double x, double y) {
  double sign = 0;
  for (int edge = 0; edge < 3; ++edge) {
    const double *from = corner[edge];
    const double *to = corner[(edge + 1) % 3];
    const double side =
        (to[0] - from[0]) * (y - from[1]) - (to[1] - from[1]) * (x - from[0]);
    if (side == 0.0) { continue; }
    if (sign != 0.0 && (side > 0) != (sign > 0)) { return false; }
    sign = side;
  }
  return true;
}

class Occupancy {
public:
  Occupancy(const Mask &ours, const Mask &theirs)
      : Across_((ours.Width + kTile - 1) / kTile), Down_((ours.Height + kTile - 1) / kTile),
        Summed_((size_t)(Across_ + 1) * (size_t)(Down_ + 1), 0) {
    for (int y = 0; y < ours.Height; ++y) {
      for (int x = 0; x < ours.Width; ++x) {
        if (ours.At(x, y) == theirs.At(x, y)) { continue; }
        Summed_[At(x / kTile + 1, y / kTile + 1)] += 1;
      }
    }
    for (int down = 1; down <= Down_; ++down) {
      for (int across = 1; across <= Across_; ++across) {
        Summed_[At(across, down)] += Summed_[At(across - 1, down)] +
                                     Summed_[At(across, down - 1)] -
                                     Summed_[At(across - 1, down - 1)];
      }
    }
  }

  bool Any(int fromX, int toX, int fromY, int toY) const {
    const int left = std::min(fromX / kTile, Across_), right = std::min(toX / kTile + 1, Across_);
    const int top = std::min(fromY / kTile, Down_), bottom = std::min(toY / kTile + 1, Down_);
    if (left >= right || top >= bottom) { return false; }
    return Summed_[At(right, bottom)] - Summed_[At(left, bottom)] - Summed_[At(right, top)] +
               Summed_[At(left, top)] >
           0;
  }

private:
  static constexpr int kTile = 16;
  size_t At(int across, int down) const {
    return (size_t)down * (size_t)(Across_ + 1) + (size_t)across;
  }
  int Across_ = 0, Down_ = 0;
  std::vector<size_t> Summed_;
};

inline void Span(const double corner[3][2], int limit, int axis, int &low, int &high) {
  double least = corner[0][axis], most = corner[0][axis];
  for (int which = 1; which < 3; ++which) {
    least = std::min(least, corner[which][axis]);
    most = std::max(most, corner[which][axis]);
  }
  low = std::max(0, (int)std::floor(least));
  high = std::min(limit - 1, (int)std::ceil(most));
}

}

[[nodiscard]] inline size_t TrianglesOutsideTheDepthRange(const outshine::Gltf::Subject &geometry,
                                                         const outshine::Gltf::Transform &clip) {
  size_t outside = 0;
  for (const outshine::Gltf::Part &part : geometry.Parts()) {
    for (size_t triangle = 0; triangle * 3u + 2u < part.IndexCount; ++triangle) {
      for (int which = 0; which < 3; ++which) {
        const size_t vertex = geometry.Indices()[part.FirstIndex + triangle * 3u + (size_t)which];
        const double point[3] = {geometry.PositionsM()[vertex * 3],
                                 geometry.PositionsM()[vertex * 3 + 1],
                                 geometry.PositionsM()[vertex * 3 + 2]};
        double ndc[3];
        clip.Point(point, ndc);
        if (!(ndc[2] >= -1.0 && ndc[2] <= 1.0)) { ++outside; break; }
      }
    }
  }
  return outside;
}

inline Attribution AttributeDisagreement(const outshine::Gltf::Subject &geometry,
                                         const outshine::Gltf::Transform &clip,
                                         const outshine::Gltf::Viewport &viewport, const Mask &ours,
                                         const Mask &theirs) {
  Attribution table;
  Mask touched;
  touched.Width = ours.Width;
  touched.Height = ours.Height;
  touched.In.assign((size_t)ours.Width * (size_t)ours.Height, 0u);
  const Detail::Occupancy disagreeing(ours, theirs);

  for (const outshine::Gltf::Part &part : geometry.Parts()) {
    NodeDisagreement row;
    row.Node = part.NodeName;
    row.Triangles = part.IndexCount / 3u;
    std::vector<Pixel> covered;
    for (size_t triangle = 0; triangle * 3u + 2u < part.IndexCount; ++triangle) {
      double corner[3][2];
      bool projects = true;
      for (int which = 0; which < 3; ++which) {
        const size_t vertex = geometry.Indices()[part.FirstIndex + triangle * 3u + (size_t)which];
        const double point[3] = {geometry.PositionsM()[vertex * 3], geometry.PositionsM()[vertex * 3 + 1],
                                 geometry.PositionsM()[vertex * 3 + 2]};
        double ndc[3];
        clip.Point(point, ndc);
        if (!(ndc[2] >= -1.0 && ndc[2] <= 1.0)) {
          projects = false;
          break;
        }
        viewport.Raster(ndc, corner[which]);
      }
      if (!projects) {
        ++table.Unprojectable;
        continue;
      }
      int fromX = 0, toX = 0, fromY = 0, toY = 0;
      Detail::Span(corner, ours.Width, 0, fromX, toX);
      Detail::Span(corner, ours.Height, 1, fromY, toY);
      if (!disagreeing.Any(fromX, toX, fromY, toY)) { continue; }
      for (int y = fromY; y <= toY; ++y) {
        for (int x = fromX; x <= toX; ++x) {
          if (ours.At(x, y) == theirs.At(x, y)) { continue; }
          if (!Detail::Inside(corner, (double)x, (double)y)) { continue; }
          covered.push_back({x, y});
        }
      }
    }
    for (const Pixel &pixel : covered) {
      const size_t at = (size_t)pixel.Y * (size_t)ours.Width + (size_t)pixel.X;
      if (touched.In[at] == 2u) { continue; }
      touched.In[at] = 2u;
      (ours.At(pixel.X, pixel.Y) ? row.OursOnly : row.TheirsOnly) += 1u;
    }

    for (const Pixel &pixel : covered) {
      touched.In[(size_t)pixel.Y * (size_t)ours.Width + (size_t)pixel.X] = 1u;
    }
    table.Nodes.push_back(std::move(row));
  }

  for (int y = 0; y < ours.Height; ++y) {
    for (int x = 0; x < ours.Width; ++x) {
      if (ours.At(x, y) == theirs.At(x, y)) { continue; }
      table.Unattributed += touched.At(x, y) ? 0u : 1u;
    }
  }
  return table;
}

}

#endif
