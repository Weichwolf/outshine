#ifndef OUTSHINE_GENERATORS_MESHED_H
#define OUTSHINE_GENERATORS_MESHED_H

#include <string>

#include <Geometry.h>

namespace outshine::Generators {

inline constexpr size_t kSoupFloatsPerVertex = 8;

class Meshed {
public:
  [[nodiscard]] bool Take(std::string named, MaterialInstance material, const float *soup, size_t floats);
  [[nodiscard]] size_t Parts() const { return (size_t)Held_.parts(); }
  [[nodiscard]] Geometry Handed() { return std::move(Held_); }
  [[nodiscard]] const std::string &Error() const { return Error_; }

private:
  Geometry Held_;
  std::string Error_;
};

}
#endif
