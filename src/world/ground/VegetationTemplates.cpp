#include <format>
#include "VegetationTemplates.h"

#include "Json.h"
#include "Log.h"

#include "ReadTextFile.h"
#include <cstddef>
#include <expected>
#include <string>
#include <type_traits>
#include <utility>
#include <unordered_map>
#include <string_view>

namespace outshine::Ground {

constexpr double kCoverUnsaid = 0.35;

constexpr double kLitterUnsaid = 0.01;

constexpr double kEdgeReachUnsaidM = 0.05;

bool VegetationTemplates::Load(const char *path, const GroundMaterials &mats) {
  constexpr size_t kCatalogByteLimit = size_t{1024} * 1024u;
  const auto text = ReadTextFile(path != nullptr ? std::string_view(path) : std::string_view{},
                                 kCatalogByteLimit);
  if (!text) {
    Error_ = text.error();
    return false;
  }
  Json doc;
  if (!doc.Parse(text->data(), text->size())) {
    Error_ = "parse failed";
    return false;
  }
  const auto root = doc.Root();
  const auto tpls = root["templates"];
  if (tpls.GetKind() != Json::Kind::Array || tpls.Size() == 0) {
    Error_ = "no templates array";
    return false;
  }
  constexpr size_t kMaximumTemplates = 256;
  if (tpls.Size() > kMaximumTemplates) {
    Error_ = "more than 256 templates";
    return false;
  }
  if (!mats.Ready()) {
    Error_ = "ground-material table not loaded";
    return false;
  }
  VegetationTemplates candidate;
  BladeMap blades;
  if (!candidate.ReadBlades(root, blades) || !candidate.ReadTemplates(root, mats, blades) ||
      !candidate.ReadRules(tpls) || !candidate.ReadEnvironment(root)) {
    Error_ = std::move(candidate.Error_);
    return false;
  }
  candidate.ReadLayers(tpls);
  static_assert(std::is_nothrow_move_assignable_v<VegetationTemplates>);
  *this = std::move(candidate);
  Log::Info(LogTag::Veg,
            "table",
            {{"path", path},
             {"classRows", static_cast<int>(Table_.size())},
             {"osmRules", static_cast<int>(Rules_.size())},
             {"layers", static_cast<int>(Layers_.size())},
             {"areaLayers", static_cast<int>(AreaLayers_.size())},
             {"unmappedRow", static_cast<double>(Unmapped_)},
             {"rockTemplate", Limit_.RockTemplateName()},
             {"slopeBandDeg", static_cast<double>(Limit_.SlopeBandDeg())}});
  return true;
}

bool VegetationTemplates::ReadBlades(const Json::Ref &root, BladeMap &bladeByName) {
  const Json::Ref blades = root["bladeClasses"];
  for (size_t i = 0; i < blades.Size(); i++) {
    const Json::Ref b = blades[i];
    Blade bl{};
    for (int c = 0; c < 3; c++) {
      bl.Green[c] = static_cast<float>(b["greenLinear"][static_cast<size_t>(c)].Num(-1.0));
      bl.Dry[c] = static_cast<float>(b["dryLinear"][static_cast<size_t>(c)].Num(-1.0));
      if (bl.Green[c] < 0.0f || bl.Dry[c] < 0.0f) {
        Error_ = "blade class without greenLinear/dryLinear: " + b["name"].Str("?");
        return false;
      }
    }
    if (!bladeByName.emplace(b["name"].Str("?"), bl).second) {
      Error_ = "duplicate blade class: " + b["name"].Str("?");
      return false;
    }
  }
  if (bladeByName.empty()) {
    Error_ = "no bladeClasses declared";
    return false;
  }

  return true;
}

bool VegetationTemplates::ReadSubstrate(const Json::Ref &ground,
                                        const GroundMaterials &materials,
                                        Row &row) {
  const auto fillSurf = [](Vec4f &dst, const GroundMaterials::Material &m) {
    dst[0] = m.GrainSizeM;
    dst[1] = m.HeightAmplitudeM;
    dst[2] = m.DetailCoarseM;
    dst[3] = m.DetailFineM;
  };

  const std::string gname = ground["class"].Str("");
  const int gi = materials.Find(gname);
  if (gi < 0) {
    Error_ = "unknown ground class: " + gname;
    return false;
  }
  const GroundMaterials::Material &gm = materials.At(static_cast<size_t>(gi));

  const std::string lname = ground["litterClass"].Str("");
  const int li = lname.empty() ? gm.LitterClass : materials.Find(lname);
  if (!lname.empty() && li < 0) {
    Error_ = "unknown litter class: " + lname;
    return false;
  }
  const GroundMaterials::Material &lm = materials.At(static_cast<size_t>(li >= 0 ? li : gi));
  row.GroundClass = gi;
  for (int c = 0; c < 3; c++) {
    row.Ground[c] = gm.Albedo[c];
    row.Litter[c] = lm.Albedo[c];
  }
  row.Ground[3] = gm.Roughness;
  row.Litter[3] = lm.Roughness;
  fillSurf(row.GroundSurf, gm);
  fillSurf(row.LitterSurf, lm);
  row.Mix[0] = li >= 0 ? static_cast<float>(ground["litterCoverage"].Num(gm.LitterCoverage)) : 0.0f;
  row.Mix[1] = static_cast<float>(ground["contrast"].Num(0.5));
  row.Mix[2] = gm.SpecularScale;
  row.Mix[3] = lm.SpecularScale;
  row.Edge[0] = static_cast<float>(ground["edgeReachM"].Num(kEdgeReachUnsaidM));
  row.Edge[1] = static_cast<float>(ground["edgeConstructed"].Num(0.0));
  row.Edge[3] = gm.SlopeMaxDeg;
  Friction_.push_back(gm.FrictionFactor);
  return true;
}

bool VegetationTemplates::ReadTemplates(const Json::Ref &root,
                                        const GroundMaterials &materials,
                                        const BladeMap &bladeByName) {
  const auto tpls = root["templates"];
  Table_.reserve(tpls.Size() + 1);

  for (size_t i = 0; i < tpls.Size(); i++) {
    const Json::Ref t = tpls[i];
    Names_.push_back(t["name"].Str("?"));

    const Json::Ref g = t["ground"];
    const Json::Ref gr = t["grass"];
    const std::string bname = gr["class"].Str("");
    const auto bit = bladeByName.find(bname);
    if (bit == bladeByName.end()) {
      Error_ = "unknown blade class: " + bname;
      return false;
    }

    Row row{};
    if (!ReadSubstrate(g, materials, row)) { return false; }
    for (int c = 0; c < 3; c++) {
      row.Grass[c] = bit->second.Green[c];
      row.Dry[c] = bit->second.Dry[c];
    }
    row.Grass[3] = static_cast<float>(gr["perM2"].Num(0.0));
    row.Dry[3] = static_cast<float>(gr["heightM"].Num(0.0));
    row.Param[0] = static_cast<float>(gr["heightJitter"].Num(0.5));
    row.Param[1] = static_cast<float>(gr["widthM"].Num(kLitterUnsaid));
    row.Param[2] = static_cast<float>(t["clutter"]["perM2"].Num(0.0));
    row.Param[3] = static_cast<float>(gr["dryFraction"].Num(kCoverUnsaid));
    row.Edge[2] = static_cast<float>(t["trees"]["perM2"].Num(0.0));

    const float closure = static_cast<float>(g["swardClosure"].Num(0.0));
    if (closure > 0.0f) {
      for (int c = 0; c < 3; c++) {
        const float sward = row.Grass[c] + (row.Dry[c] - row.Grass[c]) * row.Param[3];
        row.Ground[c] += (sward - row.Ground[c]) * closure;
        row.Litter[c] += (sward - row.Litter[c]) * closure;
      }
    }
    Table_.push_back(row);
  }

  {
    const Json::Ref u = root["unmapped"];
    if (u.GetKind() != Json::Kind::Object) {
      Error_ = "no unmapped substrate declared";
      return false;
    }
    Row row{};
    if (!ReadSubstrate(u, materials, row)) { return false; }
    Unmapped_ = static_cast<int>(Table_.size());
    Names_.emplace_back("unmapped");
    Table_.push_back(row);
  }
  return true;
}

bool VegetationTemplates::ReadRules(const Json::Ref &templates) {

  for (size_t i = 0; i < templates.Size(); i++) {
    const Json::Ref rows = templates[i]["osm"];
    for (size_t k = 0; k < rows.Size(); k++) {
      const Json::Ref r = rows[k];
      const std::string layer = r["layer"].Str("");
      const std::string kind = r["kind"].Str("");
      if (layer.empty() || kind.empty()) {
        Error_ = "osm row without layer or kind on template " + Names_[i];
        return false;
      }
      Rule rule{};
      rule.Tpl = static_cast<int>(i);
      rule.Rank = static_cast<int>(r["rank"].Num(-1.0));
      rule.WidthM = static_cast<float>(r["widthM"].Num(0.0));
      rule.MaxGradient = static_cast<float>(r["maxGradient"].Num(0.0));
      rule.MinRadiusM = static_cast<float>(r["minRadiusM"].Num(0.0));
      rule.ClearanceM = static_cast<float>(r["clearanceM"].Num(0.0));
      rule.Lanes = static_cast<int>(r["lanes"].Num(0.0));
      rule.Oneway = r["oneway"].Num(0.0) > 0.5;
      rule.SpeedMps = static_cast<float>(r["speedMps"].Num(0.0));
      rule.Priority = static_cast<int>(r["priority"].Num(0.0));
      rule.Sealed = r["sealed"].Num(0.0) > 0.5;
      if (rule.Rank < 0) {
        Error_ = std::format("osm row without rank: {}/{}", layer, kind);
        return false;
      }
      const std::string key = std::format("{}/{}", layer, kind);
      if (!Rules_.emplace(key, rule).second) {
        Error_ = "duplicate osm row: " + key;
        return false;
      }
    }
  }
  if (Rules_.empty()) {
    Error_ = "no osm rows declared";
    return false;
  }

  return true;
}

void VegetationTemplates::ReadLayers(const Json::Ref &templates) {
  std::unordered_map<std::string, bool> hasLine;
  for (size_t i = 0; i < templates.Size(); i++) {
    const Json::Ref rows = templates[i]["osm"];
    for (size_t k = 0; k < rows.Size(); k++) {
      const std::string layer = rows[k]["layer"].Str("");
      const auto it = hasLine.find(layer);
      const bool line = rows[k]["widthM"].Num(0.0) > 0.0;
      if (it == hasLine.end()) {
        Layers_.push_back(layer);
        hasLine.emplace(layer, line);
      } else if (line) {
        it->second = true;
      }
    }
  }
  for (const std::string &l : Layers_) {
    if (!hasLine[l]) { AreaLayers_.push_back(l); }
  }
}

bool VegetationTemplates::ReadEnvironment(const Json::Ref &root) {
  {
    const Json::Ref bands = root["waterClearance"];
    for (size_t i = 0; i < bands.Size(); i++) {
      WaterBands_.push_back(
          WaterBand{.RunM = static_cast<float>(bands[i]["runM"].Num(0.0)),
                    .ClearanceM = static_cast<float>(bands[i]["clearanceM"].Num(0.0))});
    }
  }

  if (!Limit_.Load(root)) {
    Error_ = Limit_.Error();
    return false;
  }
  RockTpl_ = -1;
  for (size_t i = 0; i < Names_.size(); i++) {
    if (Names_[i] == Limit_.RockTemplateName()) { RockTpl_ = static_cast<int>(i); }
  }
  if (RockTpl_ < 0) {
    Error_ = "alpineLimit.rockTemplate names no template: " + Limit_.RockTemplateName();
    return false;
  }

  return true;
}

const VegetationTemplates::Rule *VegetationTemplates::Find(std::string_view layer,
                                                           std::string_view kind) const {
  std::string key;
  key.reserve(layer.size() + kind.size() + 1);
  key.append(layer).append("/").append(kind);
  auto it = Rules_.find(key);
  if (it != Rules_.end()) { return &it->second; }
  key.assign(layer).append("/*");
  it = Rules_.find(key);
  return it != Rules_.end() ? &it->second : nullptr;
}

}
