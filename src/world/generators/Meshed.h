#ifndef OUTSHINE_WORLD_GENERATORS_MESHED_H
#define OUTSHINE_WORLD_GENERATORS_MESHED_H

#include <cstdint>
#include <string>
#include <vector>

#include <Geometry.h>

namespace outshine::Generators {

inline constexpr size_t kSoupFloatsPerVertex = 8;

class Meshed {
public:
  [[nodiscard]] bool Take(std::string named, int material, const float *soup, size_t floats);

  [[nodiscard]] size_t Parts() const { return Named_.size(); }
  [[nodiscard]] Geometry Handed();

  [[nodiscard]] const std::string &Error() const { return Error_; }

private:
  struct Reach {
    size_t FirstVertex = 0;
    size_t VertexCount = 0;
    size_t FirstIndex = 0;
    size_t IndexCount = 0;
    int Material = -1;
  };

  std::vector<std::string> Named_;
  std::vector<Reach> Reaches_;
  std::vector<float> PositionsM_;
  std::vector<float> Uv_;
  std::vector<float> NormalM_;
  std::vector<uint32_t> Index_;
  std::vector<Part> Handed_;
  std::string Error_;
};

}

#endif
