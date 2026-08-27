#ifndef OUTSHINE_GENERATORS_BASE_GROUNDPATCH_H
#define OUTSHINE_GENERATORS_BASE_GROUNDPATCH_H

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

  static std::shared_ptr<const GroundPatch> Complete(const Region &region, int side,
                                                     Span<const Posting> postings);

  [[nodiscard]] int Side() const { return Side_; }
  [[nodiscard]] double SpacingEm() const { return SpacingEm_; }
  [[nodiscard]] double SpacingNm() const { return SpacingNm_; }

  [[nodiscard]] double HeightAslM(double eastM, double northM) const noexcept;

  void GradientAt(double eastM, double northM, double *dhde, double *dhdn) const noexcept;
  [[nodiscard]] double SlopeDeg(double eastM, double northM) const noexcept;

  [[nodiscard]] size_t HeapBytes() const;

private:
  GroundPatch(int side, double spacingEm, double spacingNm, Span<const Posting> postings);

  int Side_;
  double SpacingEm_, SpacingNm_;
  std::vector<double> AslM_;
};

}
#endif
