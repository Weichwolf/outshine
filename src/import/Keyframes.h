#ifndef OUTSHINE_IMPORT_KEYFRAMES_H
#define OUTSHINE_IMPORT_KEYFRAMES_H

#include <cstddef>

namespace outshine {

class Keyframes {
public:
  enum class Interpolation {
    Step,
    Linear,
    CubicSpline,
  };

  Keyframes() = default;

  Keyframes(Interpolation how,
            const double *frames,
            size_t count,
            const double *values,
            size_t components)
      : Frames_(frames), Values_(values), Count_(count), Components_(components), How_(how) {}

  [[nodiscard]] bool Valid() const {
    return (Frames_ != nullptr) && (Values_ != nullptr) && Count_ > 0 && Components_ > 0;
  }

  [[nodiscard]] size_t Count() const { return Count_; }

  [[nodiscard]] size_t Components() const { return Components_; }

  [[nodiscard]] Interpolation How() const { return How_; }

  void At(double abscissa, double *out) const;

  [[nodiscard]] bool Span(double abscissa, size_t &keyframe, double &weight) const;

  [[nodiscard]] const double *ValueAt(size_t keyframe) const { return Value(keyframe); }

  [[nodiscard]] double AtScalar(double abscissa) const {
    double v = 0.0;
    At(abscissa, &v);
    return v;
  }

private:
  [[nodiscard]] size_t Segment(double abscissa) const;

  [[nodiscard]] const double *Value(size_t k) const {
    return Values_ + k * Components_ * (How_ == Interpolation::CubicSpline ? 3u : 1u) +
           (How_ == Interpolation::CubicSpline ? Components_ : 0u);
  }

  [[nodiscard]] const double *OutTangent(size_t k) const {
    return Values_ + (k * 3u + 2u) * Components_;
  }

  [[nodiscard]] const double *InTangent(size_t k) const { return Values_ + k * 3u * Components_; }

  const double *Frames_ = nullptr;
  const double *Values_ = nullptr;
  size_t Count_ = 0, Components_ = 0;
  Interpolation How_ = Interpolation::Linear;
};

} // namespace outshine
#endif
