#include "VegetationTemplates.h"

#include "Json.h"
#include "Log.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>

namespace outshine::Ground {

bool VegetationTemplates::Load(const char *path, const GroundMaterials &mats) {
  Table_.clear();
  Names_.clear();
  Rules_.clear();
  Layers_.clear();
  AreaLayers_.clear();
  Error_.clear();
  Unmapped_ = 0;

  FILE *f = fopen(path, "rb");
  if (!f) { Error_ = std::string("open failed: ") + path; return false; }
  fseek(f, 0, SEEK_END);
  const long n = ftell(f);
  fseek(f, 0, SEEK_SET);
  std::string text((size_t)(n > 0 ? n : 0), '\0');
  const size_t got = n > 0 ? fread(&text[0], 1, (size_t)n, f) : 0;
  fclose(f);
  if (got != text.size()) { Error_ = "short read"; return false; }

  Json doc;
  if (!doc.Parse(text.c_str(), text.size())) { Error_ = "parse failed"; return false; }
  const Json::Ref tpls = doc.Root()["templates"];
  if (tpls.GetKind() != Json::Kind::Array || tpls.Size() == 0) {
    Error_ = "no templates array";
    return false;
  }
  if (tpls.Size() > 256) { Error_ = "more than 256 templates"; return false; }

  if (!mats.Ready()) { Error_ = "ground-material table not loaded"; return false; }

  const Json::Ref blades = doc.Root()["bladeClasses"];
  std::unordered_map<std::string, Blade> bladeByName;
  for (size_t i = 0; i < blades.Size(); i++) {
    const Json::Ref b = blades[i];
    Blade bl{};
    for (int c = 0; c < 3; c++) {
      bl.Green[c] = (float)b["greenLinear"][(size_t)c].Num(-1.0);
      bl.Dry[c] = (float)b["dryLinear"][(size_t)c].Num(-1.0);
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
  if (bladeByName.empty()) { Error_ = "no bladeClasses declared"; return false; }

  Table_.reserve(tpls.Size() + 1);

  const auto fillSurf = [](float *dst, const GroundMaterials::Material &m) {
    dst[0] = m.GrainSizeM;
    dst[1] = m.HeightAmplitudeM;
    dst[2] = m.DetailCoarseM;
    dst[3] = m.DetailFineM;
  };

  const auto substrate = [&](const Json::Ref &g, Row *row) -> bool {
    const std::string gname = g["class"].Str("");
    const int gi = mats.Find(gname);
    if (gi < 0) { Error_ = "unknown ground class: " + gname; return false; }
    const GroundMaterials::Material &gm = mats.At((size_t)gi);

    const std::string lname = g["litterClass"].Str("");
    const int li = lname.empty() ? gm.LitterClass : mats.Find(lname);
    if (!lname.empty() && li < 0) { Error_ = "unknown litter class: " + lname; return false; }
    const GroundMaterials::Material &lm = mats.At((size_t)(li >= 0 ? li : gi));
    for (int c = 0; c < 3; c++) {
      row->Ground[c] = gm.Albedo[c];
      row->Litter[c] = lm.Albedo[c];
    }
    row->Ground[3] = gm.Roughness;
    row->Litter[3] = lm.Roughness;
    fillSurf(row->GroundSurf, gm);
    fillSurf(row->LitterSurf, lm);
    row->Mix[0] = li >= 0 ? (float)g["litterCoverage"].Num(gm.LitterCoverage) : 0.0f;
    row->Mix[1] = (float)g["contrast"].Num(0.5);
    row->Mix[2] = gm.SpecularScale;
    row->Mix[3] = lm.SpecularScale;
    row->Edge[0] = (float)g["edgeReachM"].Num(0.05);
    row->Edge[1] = (float)g["edgeConstructed"].Num(0.0);
    row->Edge[3] = gm.SlopeMaxDeg;
    Friction_.push_back(gm.FrictionFactor);
    return true;
  };

  for (size_t i = 0; i < tpls.Size(); i++) {
    const Json::Ref t = tpls[i];
    Names_.push_back(t["name"].Str("?"));

    const Json::Ref g = t["ground"], gr = t["grass"];
    const std::string bname = gr["class"].Str("");
    auto bit = bladeByName.find(bname);
    if (bit == bladeByName.end()) { Error_ = "unknown blade class: " + bname; return false; }

    Row row{};
    if (!substrate(g, &row)) return false;
    for (int c = 0; c < 3; c++) {
      row.Grass[c]  = bit->second.Green[c];
      row.Dry[c]    = bit->second.Dry[c];
    }
    row.Grass[3]  = (float)gr["perM2"].Num(0.0);
    row.Dry[3]    = (float)gr["heightM"].Num(0.0);
    row.Param[0]  = (float)gr["heightJitter"].Num(0.5);
    row.Param[1]  = (float)gr["widthM"].Num(0.01);
    row.Param[2]  = (float)t["clutter"]["perM2"].Num(0.0);
    row.Param[3]  = (float)gr["dryFraction"].Num(0.35);
    row.Edge[2]   = (float)t["trees"]["perM2"].Num(0.0);

    const float closure = (float)g["swardClosure"].Num(0.0);
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
    const Json::Ref u = doc.Root()["unmapped"];
    if (u.GetKind() != Json::Kind::Object) { Error_ = "no unmapped substrate declared"; return false; }
    Row row{};
    if (!substrate(u, &row)) return false;
    Unmapped_ = (int)Table_.size();
    Names_.push_back("unmapped");
    Table_.push_back(row);
  }

  for (size_t i = 0; i < tpls.Size(); i++) {
    const Json::Ref rows = tpls[i]["osm"];
    for (size_t k = 0; k < rows.Size(); k++) {
      const Json::Ref r = rows[k];
      const std::string layer = r["layer"].Str("");
      const std::string kind = r["kind"].Str("");
      if (layer.empty() || kind.empty()) {
        Error_ = "osm row without layer or kind on template " + Names_[i];
        return false;
      }
      Rule rule{};
      rule.Tpl = (int)i;
      rule.Rank = (int)r["rank"].Num(-1.0);
      rule.WidthM = (float)r["widthM"].Num(0.0);
      rule.MaxGradient = (float)r["maxGradient"].Num(0.0);
      rule.MinRadiusM = (float)r["minRadiusM"].Num(0.0);
      rule.Lanes = (int)r["lanes"].Num(0.0);
      rule.Oneway = r["oneway"].Num(0.0) > 0.5;
      if (rule.Rank < 0) { Error_ = "osm row without rank: " + layer + "/" + kind; return false; }
      const std::string key = layer + "/" + kind;
      if (!Rules_.emplace(key, rule).second) { Error_ = "duplicate osm row: " + key; return false; }
    }
  }
  if (Rules_.empty()) { Error_ = "no osm rows declared"; return false; }

  std::unordered_map<std::string, bool> hasLine;
  for (size_t i = 0; i < tpls.Size(); i++) {
    const Json::Ref rows = tpls[i]["osm"];
    for (size_t k = 0; k < rows.Size(); k++) {
      const std::string layer = rows[k]["layer"].Str("");
      auto it = hasLine.find(layer);
      const bool line = rows[k]["widthM"].Num(0.0) > 0.0;
      if (it == hasLine.end()) { Layers_.push_back(layer); hasLine.emplace(layer, line); }
      else if (line) it->second = true;
    }
  }
  for (const std::string &l : Layers_) if (!hasLine[l]) AreaLayers_.push_back(l);

  if (!Limit_.Load(doc.Root())) { Error_ = Limit_.Error(); return false; }
  RockTpl_ = -1;
  for (size_t i = 0; i < Names_.size(); i++)
    if (Names_[i] == Limit_.RockTemplateName()) RockTpl_ = (int)i;
  if (RockTpl_ < 0) {
    Error_ = "alpineLimit.rockTemplate names no template: " + Limit_.RockTemplateName();
    return false;
  }

  Log::Info("veg", "table", {{"path", path}, {"classRows", (int)Table_.size()},
                             {"osmRules", (int)Rules_.size()}, {"layers", (int)Layers_.size()},
                             {"areaLayers", (int)AreaLayers_.size()},
                             {"unmappedRow", (double)Unmapped_},
                             {"rockTemplate", Limit_.RockTemplateName()},
                             {"slopeBandDeg", (double)Limit_.SlopeBandDeg()}});
  return true;
}

const VegetationTemplates::Rule *VegetationTemplates::Find(std::string_view layer,
                                                           std::string_view kind) const {
  std::string key;
  key.reserve(layer.size() + kind.size() + 1);
  key.append(layer).append("/").append(kind);
  auto it = Rules_.find(key);
  if (it != Rules_.end()) return &it->second;
  key.assign(layer).append("/*");
  it = Rules_.find(key);
  return it != Rules_.end() ? &it->second : nullptr;
}

}
