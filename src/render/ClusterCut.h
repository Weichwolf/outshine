/* THE ONE CUT the geometry stage makes, opened once a frame and read by every unit in it. What was
 * written per unit before was the same three steps each time — the pixel focal length, the frustum,
 * and `DagSelect` over the clusters with neighbouring runs merged into one draw — and three copies
 * of a ladder are three ladders the moment one of them is touched.
 *
 * Capacity is kept across frames, which is why the ranges are read out of the object rather than
 * returned (F.20's hot-loop exception, stated in the rule itself). */
#ifndef CLUSTERCUT_H
#define CLUSTERCUT_H

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <vector>

#include "ClusterDag.h"
#include "FrameContext.h"
#include "Frustum.h"
#include "PixelFocalLength.h"

namespace outshine::Render {

class ClusterCut {
public:
  /* `Item` is the caller's own index for whatever buffer the range indexes into; a unit with one
   * mesh passes 0 and ignores it. */
  struct Range {
    uint32_t Item = 0, First = 0, Count = 0;
  };

  static constexpr int kLevelBins = 8;

  void Open(const FrameContext &ctx) {
    FPx_ = (float)PixelFocalLength(ctx.Height, (double)ctx.FovDeg);
    View_.Set(ctx.Mvp20);
  }

  float PixelFocal() const { return FPx_; }
  const Frustum &View() const { return View_; }

  /* One whole item against the frustum, before its clusters are looked at: a tile that is not in
   * the picture leaves on one test instead of on one per cluster. */
  [[nodiscard]] bool Sees(const double rel[3], const float ctr[3], float rad) const {
    return View_.Visible(rel, ctr, rad);
  }

  void Begin() {
    Ranges_.clear();
    Indices_ = 0;
    Closed_ = false;
    for (int i = 0; i < kLevelBins; i++) ByLevel_[i] = 0;
  }

  /* `eyeLocal` is the eye in the item's own vertex frame, `rel` the item's origin relative to the
   * eye — the two are negatives of each other wherever the vertex frame is the anchor. `up` zero
   * selects the isotropic error measure, which is what a body without a vertical has. */
  void Take(uint32_t item, const DagCluster *clusters, int n, const double eyeLocal[3],
            const double rel[3], const float up[3]) {
    const float tau = SseTauPx();
    for (int i = 0; i < n; i++) {
      const DagCluster &c = clusters[i];
      if (!DagSelect(c, eyeLocal, FPx_, tau, up)) continue;
      if (!View_.Visible(rel, c.SelfCenter, c.SelfRadius)) continue;
      Indices_ += (long)c.Count;
      ByLevel_[c.Level < kLevelBins ? c.Level : kLevelBins - 1] += (long)c.Count / 3;
      Ranges_.push_back(Range{item, c.First, c.Count});
    }
  }

  /* THE MERGE HAPPENS HERE AND NOT IN `Take`, because a merge that only looks at the tail is a rule
   * about the ORDER THE CALLER HAPPENED TO USE. Regions arrive nearest-first and a region is not one
   * item, so the same clusters interleaved would have become as many draws as there are
   * interleavings — a draw count that moves for a reason that is not the picture. Sorted by item
   * first, which is also the order the caller rebinds in. */
  void Close() {
    std::sort(Ranges_.begin(), Ranges_.end(), [](const Range &a, const Range &b) {
      return a.Item != b.Item ? a.Item < b.Item : a.First < b.First;
    });
    size_t kept = 0;
    for (size_t i = 0; i < Ranges_.size(); i++) {
      if (kept > 0 && Ranges_[kept - 1].Item == Ranges_[i].Item &&
          Ranges_[kept - 1].First + Ranges_[kept - 1].Count == Ranges_[i].First)
        Ranges_[kept - 1].Count += Ranges_[i].Count;
      else
        Ranges_[kept++] = Ranges_[i];
    }
    Ranges_.resize(kept);
    Closed_ = true;
  }

  const std::vector<Range> &Ranges() const {
    assert(Closed_);
    return Ranges_;
  }
  long Indices() const { return Indices_; }
  const long *IndicesByLevel() const { return ByLevel_; }

private:
  Frustum View_;
  std::vector<Range> Ranges_;
  long Indices_ = 0;
  long ByLevel_[kLevelBins] = {};
  float FPx_ = 1.0f;
  bool Closed_ = false;
};

} // namespace outshine::Render
#endif
