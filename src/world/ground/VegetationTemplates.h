#ifndef OUTSHINE_WORLD_GROUND_VEGETATIONTEMPLATES_H
#define OUTSHINE_WORLD_GROUND_VEGETATIONTEMPLATES_H

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
    float Ground[4];
    float Litter[4];
    float GroundSurf[4];
    float LitterSurf[4];
    float Mix[4];
    float Grass[4];
    float Dry[4];
    float Param[4];
    float Edge[4];
  };

  struct Rule {
    int Tpl = 0;
    int Rank = 0;
    float WidthM = 0.0f;
    float MaxGradient = 0.0f;
    float MinRadiusM = 0.0f;
    int Lanes = 0;
    bool Oneway = false;
  };

  struct Blade {
    float Green[3];
    float Dry[3];
  };

  [[nodiscard]] bool Load(const char *path, const GroundMaterials &mats);

  const Row *Rows() const { return Table_.data(); }
  size_t RowBytes() const { return Table_.size() * sizeof(Row); }
  [[nodiscard]] bool Ready() const { return !Table_.empty(); }
  size_t TemplateCount() const { return Table_.size(); }
  [[nodiscard]] float FrictionOf(size_t tpl) const {
    return tpl < Friction_.size() ? Friction_[tpl] : 0.0f;
  }
  const std::string &Name(size_t i) const { return Names_[i]; }
  const std::string &Error() const { return Error_; }

  const Rule *Find(std::string_view layer, std::string_view kind) const;

  int UnmappedRow() const { return Unmapped_; }

  const AlpineLimit &Limit() const { return Limit_; }
  int RockTemplate() const { return RockTpl_; }
  size_t RuleCount() const { return Rules_.size(); }

  const std::vector<std::string> &Layers() const { return Layers_; }

  const std::vector<std::string> &AreaLayers() const { return AreaLayers_; }

private:
  std::vector<Row> Table_;
  std::vector<float> Friction_;
  std::vector<std::string> Names_;
  std::unordered_map<std::string, Rule> Rules_;
  std::vector<std::string> Layers_, AreaLayers_;
  AlpineLimit Limit_;
  std::string Error_;
  int Unmapped_ = 0, RockTpl_ = 0;
};

}
#endif
