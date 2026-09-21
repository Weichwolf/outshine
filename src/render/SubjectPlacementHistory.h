#ifndef OUTSHINE_RENDER_SUBJECTPLACEMENTHISTORY_H
#define OUTSHINE_RENDER_SUBJECTPLACEMENTHISTORY_H

#include <cassert>
#include <cstddef>
#include <limits>
#include <vector>

#include "math/Mat4.h"

namespace outshine::Render {

class SubjectPlacementHistory {
public:
  void Reset() noexcept {
    for (Mat4 &body : Bodies_) { body = Unsent(); }
    Built_ = Unsent();
  }

  void Resize(size_t bodies) { Bodies_.resize(bodies, Unsent()); }

  void EnsureOne() {
    if (Bodies_.empty()) { Bodies_.push_back(Unsent()); }
  }

  [[nodiscard]] size_t Bodies() const noexcept { return Bodies_.size(); }

  [[nodiscard]] bool NeedsBodyUpload(size_t body, const Mat4 &candidate) const noexcept {
    assert(body < Bodies_.size());
    return !(Bodies_[body] == candidate);
  }

  [[nodiscard]] bool NeedsBuiltUpload(const Mat4 &candidate) const noexcept {
    return !(Built_ == candidate);
  }

  void RecordsUpload(size_t body, const Mat4 &bodyTransform, const Mat4 &builtTransform) noexcept {
    assert(body < Bodies_.size());
    Bodies_[body] = bodyTransform;
    Built_ = builtTransform;
  }

private:
  [[nodiscard]] static Mat4 Unsent() noexcept {
    Mat4 value;
    value.Column.fill(std::numeric_limits<double>::quiet_NaN());
    return value;
  }

  std::vector<Mat4> Bodies_;
  Mat4 Built_ = Unsent();
};

}
#endif
