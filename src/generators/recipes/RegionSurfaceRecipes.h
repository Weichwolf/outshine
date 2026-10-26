#ifndef OUTSHINE_GENERATORS_RECIPES_REGIONSURFACERECIPES_H
#define OUTSHINE_GENERATORS_RECIPES_REGIONSURFACERECIPES_H

#include "SurfaceMaterial.h"
#include "Vec3.h"
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "Json.h"
#include "AlpineLimit.h"
#include "GroundMaterialCatalog.h"

namespace outshine::Generators {

class RegionSurfaceRecipes {
public:
  struct Rule {
    int Tpl = 0;
    int Rank = 0;
    float WidthM = 0.0f;
    float MaxGradient = 0.0f;
    float MinRadiusM = 0.0f;
    float ClearanceM = 0.0f;
    int Lanes = 0;
    bool Oneway = false;
    float SpeedMps = 0.0f;
    int Priority = 0;
    bool Sealed = false;
  };

  struct Blade {
    Vec3f Green;
    Vec3f Dry;
  };

  [[nodiscard]] bool Load(const char *path, const GroundMaterialCatalog &mats);

  [[nodiscard]] const ::outshine::Ground::SurfaceMaterial *Rows() const { return Table_.data(); }

  [[nodiscard]] size_t RowBytes() const {
    return Table_.size() * sizeof(::outshine::Ground::SurfaceMaterial);
  }

  [[nodiscard]] ::outshine::Ground::SurfacePaletteView SurfacePalette() const noexcept {
    return {.Materials = Table_, .RockMaterial = RockTpl_, .SlopeBandDeg = Limit_.SlopeBandDeg()};
  }

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
  using BladeMap = std::unordered_map<std::string, Blade>;
  [[nodiscard]] bool ReadBlades(const Json::Ref &root, BladeMap &bladeByName);
  [[nodiscard]] bool ReadSubstrate(const Json::Ref &ground,
                                   const GroundMaterialCatalog &materials,
                                   ::outshine::Ground::SurfaceMaterial &row);
  [[nodiscard]] bool ReadTemplates(const Json::Ref &root,
                                   const GroundMaterialCatalog &materials,
                                   const BladeMap &bladeByName);
  [[nodiscard]] bool ReadRules(const Json::Ref &templates);
  void ReadLayers(const Json::Ref &templates);
  [[nodiscard]] bool ReadEnvironment(const Json::Ref &root);

  std::vector<::outshine::Ground::SurfaceMaterial> Table_;
  std::vector<float> Friction_;
  std::vector<std::string> Names_;
  std::unordered_map<std::string, Rule> Rules_;
  std::vector<WaterBand> WaterBands_;
  std::vector<std::string> Layers_, AreaLayers_;
  AlpineLimit Limit_;
  std::string Error_;
  int Unmapped_ = 0, RockTpl_ = 0;
};

}
#endif
