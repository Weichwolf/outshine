#ifndef OUTSHINE_WORLD_GROUND_GROUNDMATERIALS_H
#define OUTSHINE_WORLD_GROUND_GROUNDMATERIALS_H

#include <string_view>
#include <cstddef>
#include <string>
#include <vector>

namespace outshine::Ground {

class GroundMaterials {
public:
  struct Material {
    std::string Name;
    float Albedo[3];
    float VisibleRatio;
    float Roughness;
    float Moisture;
    float SpecularScale;
    float GrainSizeM;
    float HeightAmplitudeM;
    float DetailCoarseM;
    float DetailFineM;

    float SlopeMaxDeg;
    int LitterClass;
    float LitterCoverage;
  };

  [[nodiscard]] bool Load(const char *path);

  [[nodiscard]] bool Ready() const { return !Mats_.empty(); }
  size_t Count() const { return Mats_.size(); }
  const Material &At(size_t i) const { return Mats_[i]; }
  int Find(std::string_view name) const;
  const std::string &Error() const { return Error_; }

private:
  std::vector<Material> Mats_;
  std::string Error_;
};

}
#endif
