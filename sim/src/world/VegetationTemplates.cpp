#include "VegetationTemplates.h"

#include "Json.h"
#include "Log.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>

namespace outshine::World {

namespace {

float SrgbToLinear(float s) {
  return s <= 0.04045f ? s / 12.92f : std::pow((s + 0.055f) / 1.055f, 2.4f);
}

float SrgbLinImpl(double byte255) { return SrgbToLinear((float)(byte255 / 255.0)); }

}  // namespace

bool VegetationTemplates::Load(const char *path, const GroundMaterials &mats) {
  Table_.clear();
  Names_.clear();
  Rules_.clear();
  Layers_.clear();
  AreaLayers_.clear();
  Error_.clear();
  Default_ = 0;

  FILE *f = fopen(path, "rb");
  if (!f) { Error_ = std::string("open failed: ") + path; return false; }
  fseek(f, 0, SEEK_END);
  const long n = ftell(f);
  fseek(f, 0, SEEK_SET);
  std::string text((size_t)(n > 0 ? n : 0), '\0');
  const size_t got = n > 0 ? fread(&text[0], 1, (size_t)n, f) : 0;
  fclose(f);
  if (got != text.size()) { Error_ = "short read"; return false; }

  Render::Json doc;
  if (!doc.Parse(text.c_str(), text.size())) { Error_ = "parse failed"; return false; }
  const Render::Json::Ref tpls = doc.Root()["templates"];
  if (tpls.GetKind() != Render::Json::Kind::Array || tpls.Size() == 0) {
    Error_ = "no templates array";
    return false;
  }
  if (tpls.Size() > 256) { Error_ = "more than 256 templates"; return false; }

  if (!mats.Ready()) { Error_ = "ground-material table not loaded"; return false; }

  /* Only the BLADE colours are still display values; the ground half of that hack retired with the
   * material table. Keys are NOT scaled: they address the raster's own bytes and must stay
   * bit-identical to tiles/src/style.h. */
  const float grassGain = (float)doc.Root()["grassReflectanceGain"].Num(1.0);

  Table_.reserve(tpls.Size());

  const auto fillSurf = [](float *dst, const GroundMaterials::Material &m) {
    dst[0] = m.GrainSizeM;
    dst[1] = m.HeightAmplitudeM;
    dst[2] = m.DetailCoarseM;
    dst[3] = m.DetailFineM;
  };

  for (size_t i = 0; i < tpls.Size(); i++) {
    const Render::Json::Ref t = tpls[i];
    Names_.push_back(t["name"].Str("?"));

    const Render::Json::Ref g = t["ground"], gr = t["grass"];
    const std::string gname = g["class"].Str("");
    const int gi = mats.Find(gname);
    if (gi < 0) { Error_ = "unknown ground class: " + gname; return false; }
    const GroundMaterials::Material &gm = mats.At((size_t)gi);

    /* The template may override the litter the material declares — beech litter under spruce is the
     * defect the botanist calls, and waldboden alone cannot tell the two stands apart. */
    const std::string lname = g["litterClass"].Str("");
    int li = lname.empty() ? gm.LitterClass : mats.Find(lname);
    if (!lname.empty() && li < 0) { Error_ = "unknown litter class: " + lname; return false; }
    const GroundMaterials::Material &lm = mats.At((size_t)(li >= 0 ? li : gi));

    const Render::Json::Ref bc = gr["colorSrgb"], dc = gr["drySrgb"];
    Row row{};
    for (int c = 0; c < 3; c++) {
      row.Ground[c] = gm.Albedo[c];
      row.Litter[c] = lm.Albedo[c];
      row.Grass[c]  = grassGain * SrgbLinImpl(bc[(size_t)c].Num(128.0));
      row.Dry[c]    = grassGain * SrgbLinImpl(dc[(size_t)c].Num(128.0));
    }
    row.Ground[3] = gm.Roughness;
    row.Litter[3] = lm.Roughness;
    fillSurf(row.GroundSurf, gm);
    fillSurf(row.LitterSurf, lm);
    row.Mix[0]    = li >= 0 ? (float)g["litterCoverage"].Num(gm.LitterCoverage) : 0.0f;
    row.Mix[1]    = (float)g["contrast"].Num(0.5);
    row.Mix[2]    = gm.SpecularScale;
    row.Mix[3]    = lm.SpecularScale;
    row.Grass[3]  = (float)gr["perM2"].Num(0.0);
    row.Dry[3]    = (float)gr["heightM"].Num(0.0);
    row.Param[0]  = (float)gr["heightJitter"].Num(0.5);
    row.Param[1]  = (float)gr["widthM"].Num(0.01);
    row.Param[2]  = (float)t["clutter"]["perM2"].Num(0.0);
    row.Param[3]  = (float)gr["dryFraction"].Num(0.35);
    row.Edge[0]   = (float)g["edgeReachM"].Num(0.05);
    row.Edge[1]   = (float)g["edgeConstructed"].Num(0.0);
    row.Edge[2]   = (float)t["trees"]["perM2"].Num(0.0);

    /* Beyond the cover stage's fade radius no blade is drawn and the terrain layer is the only thing
     * left showing the meadow, so its albedo has to be the SWARD's and not the floor's. The aggregate
     * is built from the TEMPLATE's own three declarations and from no shader constant - the two blade
     * colours mixed by the declared dry fraction - so retuning a colour moves the terrain with it and
     * nothing here can drift out of step with a stage it cannot see. */
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

  /* The osm rows come SECOND, so every row can name a template by index without a forward pass. */
  for (size_t i = 0; i < tpls.Size(); i++) {
    const Render::Json::Ref rows = tpls[i]["osm"];
    for (size_t k = 0; k < rows.Size(); k++) {
      const Render::Json::Ref r = rows[k];
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
      if (rule.Rank < 0) { Error_ = "osm row without rank: " + layer + "/" + kind; return false; }
      const std::string key = layer + "/" + kind;
      if (!Rules_.emplace(key, rule).second) { Error_ = "duplicate osm row: " + key; return false; }
    }
  }
  if (Rules_.empty()) { Error_ = "no osm rows declared"; return false; }

  std::unordered_map<std::string, bool> hasLine;
  for (size_t i = 0; i < tpls.Size(); i++) {
    const Render::Json::Ref rows = tpls[i]["osm"];
    for (size_t k = 0; k < rows.Size(); k++) {
      const std::string layer = rows[k]["layer"].Str("");
      auto it = hasLine.find(layer);
      const bool line = rows[k]["widthM"].Num(0.0) > 0.0;
      if (it == hasLine.end()) { Layers_.push_back(layer); hasLine.emplace(layer, line); }
      else if (line) it->second = true;
    }
  }
  for (const std::string &l : Layers_) if (!hasLine[l]) AreaLayers_.push_back(l);

  const std::string def = doc.Root()["osmDefault"].Str("");
  Default_ = -1;
  for (size_t i = 0; i < Names_.size(); i++)
    if (Names_[i] == def) Default_ = (int)i;
  if (Default_ < 0) { Error_ = "osmDefault names no template: " + def; return false; }

  Log::Info("veg", "table", {{"path", path}, {"templates", (int)Table_.size()},
                             {"osmRules", (int)Rules_.size()}, {"layers", (int)Layers_.size()},
                             {"areaLayers", (int)AreaLayers_.size()}, {"default", def}});
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

} // namespace outshine::World
