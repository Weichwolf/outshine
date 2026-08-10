/* The ground-material table: sixteen classes, each a LINEAR
 * reflectance plus the procedural surface that makes two classes of equal reflectance look different.
 * A vegetation template REFERENCES a class by name; it never declares a colour. */
#ifndef GROUNDMATERIALS_H
#define GROUNDMATERIALS_H

#include <cstddef>
#include <string>
#include <vector>

namespace outshine::World {

class GroundMaterials {
public:
  struct Material {
    std::string Name;
    float Albedo[3];        /* linear VISIBLE-band reflectance, moisture and band ratio applied */
    float VisibleRatio;
    float Roughness;
    float Moisture;
    float SpecularScale;    /* fraction of the class that presents a coherent dielectric interface */
    float GrainSizeM;
    float HeightAmplitudeM;
    float DetailCoarseM;
    float DetailFineM;
    /* slope.plausibleDeg[1]: the steepest face this material still lies on. Above it the ground is
     * the bare-rock class and nothing stands (vegetation.json alpineLimit). */
    float SlopeMaxDeg;
    int LitterClass;        /* index into this table, or -1 */
    float LitterCoverage;
  };

  bool Load(const char *path);

  bool Ready() const { return !Mats_.empty(); }
  size_t Count() const { return Mats_.size(); }
  const Material &At(size_t i) const { return Mats_[i]; }
  int Find(const std::string &name) const;
  const std::string &Error() const { return Error_; }

private:
  std::vector<Material> Mats_;
  std::string Error_;
};

} // namespace outshine::World
#endif
