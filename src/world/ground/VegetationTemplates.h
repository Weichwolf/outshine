#ifndef OUTSHINE_WORLD_GROUND_VEGETATIONTEMPLATES_H
#define OUTSHINE_WORLD_GROUND_VEGETATIONTEMPLATES_H

#include "math/Vec4.h"
#include "math/Vec3.h"
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "AlpineLimit.h"
#include "GroundMaterials.h"

namespace outshine::Ground {

class VegetationTemplates {
public:
  struct Row {
    Vec4f Ground;
    Vec4f Litter;
    Vec4f GroundSurf;
    Vec4f LitterSurf;
    Vec4f Mix;
    Vec4f Grass;
    Vec4f Dry;
    Vec4f Param;
    Vec4f Edge;
    int GroundClass = -1;
  };

  struct Rule {
    int Tpl = 0;
    int Rank = 0;
    float WidthM = 0.0f;
    float MaxGradient = 0.0f;
    float MinRadiusM = 0.0f;
    float ClearanceM = 0.0f;
    int Lanes = 0;
    bool Oneway = false;
  };

  struct Blade {
    Vec3f Green;
    Vec3f Dry;
  };

  [[nodiscard]] bool Load(const char *path, const GroundMaterials &mats);

  [[nodiscard]] const Row *Rows() const { return Table_.data(); }

  [[nodiscard]] size_t RowBytes() const { return Table_.size() * sizeof(Row); }

  [[nodiscard]] bool Ready() const { return !Table_.empty(); }

  [[nodiscard]] size_t TemplateCount() const { return Table_.size(); }

  [[nodiscard]] float FrictionOf(size_t tpl) const {
    return tpl < Friction_.size() ? Friction_[tpl] : 0.0f;
  }

  [[nodiscard]] const std::string &Name(size_t i) const { return Names_[i]; }

  [[nodiscard]] const std::string &Error() const { return Error_; }

  [[nodiscard]] const Rule *Find(std::string_view layer, std::string_view kind) const;

  [[nodiscard]] int UnmappedRow() const { return Unmapped_; }

  [[nodiscard]] const AlpineLimit &Limit() const { return Limit_; }

  [[nodiscard]] int RockTemplate() const { return RockTpl_; }

  [[nodiscard]] size_t RuleCount() const { return Rules_.size(); }

  struct WaterBand {
    float RunM = 0.0f;
    float ClearanceM = 0.0f;
  };

  [[nodiscard]] const std::vector<WaterBand> &WaterBands() const { return WaterBands_; }

  [[nodiscard]] const std::vector<std::string> &Layers() const { return Layers_; }

  [[nodiscard]] const std::vector<std::string> &AreaLayers() const { return AreaLayers_; }

private:
  std::vector<Row> Table_;
  std::vector<float> Friction_;
  std::vector<std::string> Names_;
  std::unordered_map<std::string, Rule> Rules_;
  std::vector<WaterBand> WaterBands_;
  std::vector<std::string> Layers_, AreaLayers_;
  AlpineLimit Limit_;
  std::string Error_;
  int Unmapped_ = 0, RockTpl_ = 0;
};

} // namespace outshine::Ground
#endif
