#ifndef OUTSHINE_BASE_MATH_KEYFRAMES_H
#define OUTSHINE_BASE_MATH_KEYFRAMES_H

#include <cstddef>
#include <expected>
#include <span>

namespace outshine {

class Keyframes {
public:
  enum class Interpolation {
    Step,
    Linear,
    CubicSpline,
  };

  Keyframes() = default;

  enum class Error { InvalidInterpolation, InvalidDimensions, InvalidTimeGrid, NonFiniteValue };

  [[nodiscard]] static std::expected<Keyframes, Error> Build(Interpolation how,
                                                             std::span<const double> frames,
                                                             std::span<const double> values,
                                                             size_t components) noexcept;

  [[nodiscard]] bool Valid() const { return !Frames_.empty(); }

  [[nodiscard]] size_t Count() const { return Frames_.size(); }

  [[nodiscard]] size_t Components() const { return Components_; }

  [[nodiscard]] Interpolation How() const { return How_; }

  void At(double abscissa, std::span<double> out) const;

  [[nodiscard]] bool Span(double abscissa, size_t &keyframe, double &weight) const;

  [[nodiscard]] const double *ValueAt(size_t keyframe) const { return Value(keyframe); }

  [[nodiscard]] double AtScalar(double abscissa) const {
    double v = 0.0;
    At(abscissa, std::span(&v, 1));
    return v;
  }

private:
  [[nodiscard]] size_t Segment(double abscissa) const;

  [[nodiscard]] const double *Value(size_t k) const {
    return Values_.data() + k * Components_ * (How_ == Interpolation::CubicSpline ? 3u : 1u) +
           (How_ == Interpolation::CubicSpline ? Components_ : 0u);
  }

  [[nodiscard]] const double *OutTangent(size_t k) const {
    return Values_.data() + (k * 3u + 2u) * Components_;
  }

  [[nodiscard]] const double *InTangent(size_t k) const {
    return Values_.data() + k * 3u * Components_;
  }

  std::span<const double> Frames_;
  std::span<const double> Values_;
  size_t Components_ = 0;
  Interpolation How_ = Interpolation::Linear;
};

}
#endif
