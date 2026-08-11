#ifndef GROUNDPATCH_H
#define GROUNDPATCH_H

#include <cstdint>
#include <memory>
#include <vector>

#include "GroundSample.h"
#include "Region.h"
#include "Span.h"

namespace outshine::Generators {

class GroundPatch {
public:
  struct Posting {
    GroundSample Height = GroundSample::Waiting();
  };

  /* Null when ANY posting is unresolved: a region with a hole in its ground is never offered, so
   * no generator ever has to branch on a datum that is not there. `side` postings per axis, row
   * major from the region anchor, the last one on the far edge. */
  static std::shared_ptr<const GroundPatch> Complete(const Region &region, int side,
                                                     Span<const Posting> postings);

  int Side() const { return Side_; }
  double SpacingEm() const { return SpacingEm_; }
  double SpacingNm() const { return SpacingNm_; }

  double HeightAslM(double eastM, double northM) const noexcept;
  /* The drawn surface's gradient, metres per metre, in the region frame. */
  void GradientAt(double eastM, double northM, double *dhde, double *dhdn) const noexcept;
  double SlopeDeg(double eastM, double northM) const noexcept;

  size_t HeapBytes() const;

private:
  GroundPatch(int side, double spacingEm, double spacingNm, Span<const Posting> postings);

  int Side_;
  double SpacingEm_, SpacingNm_;
  std::vector<double> AslM_;
};

} // namespace outshine::Generators
#endif
