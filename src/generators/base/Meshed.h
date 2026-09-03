#ifndef OUTSHINE_GENERATORS_BASE_MESHED_H
#define OUTSHINE_GENERATORS_BASE_MESHED_H

#include <string>

#include <scene/Geometry.h>

namespace outshine::Generators {

inline constexpr size_t kSoupFloatsPerVertex = 8;

class Meshed {
public:
  [[nodiscard]] bool
  Take(const std::string &named, MaterialInstance material, const float *soup, size_t floats);

  [[nodiscard]] size_t Parts() const { return static_cast<size_t>(Held_.parts()); }

  [[nodiscard]] Geometry Handed() { return std::move(Held_); }

  [[nodiscard]] const std::string &Error() const { return Error_; }

private:
  Geometry Held_;
  std::string Error_;
};

} // namespace outshine::Generators
#endif
