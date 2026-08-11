/* THE ONE CUT the geometry stage makes, opened once a frame and read by every unit in it. What was
 * written per unit before was the same three steps each time — the pixel focal length, the frustum,
 * and `DagSelect` over the clusters with neighbouring runs merged into one draw — and three copies
 * of a ladder are three ladders the moment one of them is touched.
 *
 * Capacity is kept across frames, which is why the ranges are read out of the object rather than
 * returned (F.20's hot-loop exception, stated in the rule itself). */
#ifndef CLUSTERCUT_H
#define CLUSTERCUT_H

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
  bool Sees(const double rel[3], const float ctr[3], float rad) const {
    return View_.Visible(rel, ctr, rad);
  }

  void Begin() {
    Ranges_.clear();
    Indices_ = 0;
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
      /* Clusters are laid down level by level in Morton order, so a run of neighbours at one level
       * is contiguous and collapses into ONE draw. */
      if (!Ranges_.empty() && Ranges_.back().Item == item &&
          Ranges_.back().First + Ranges_.back().Count == c.First)
        Ranges_.back().Count += c.Count;
      else
        Ranges_.push_back(Range{item, c.First, c.Count});
    }
  }

  const std::vector<Range> &Ranges() const { return Ranges_; }
  long Indices() const { return Indices_; }
  const long *IndicesByLevel() const { return ByLevel_; }

private:
  Frustum View_;
  std::vector<Range> Ranges_;
  long Indices_ = 0;
  long ByLevel_[kLevelBins] = {};
  float FPx_ = 1.0f;
};

} // namespace outshine::Render
#endif
