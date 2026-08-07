#include "VegetationTemplates.h"

#include "Json.h"
#include "Log.h"

#include <cstdio>
#include <cstdlib>
#include <limits>

namespace outshine::World {

namespace {

struct KeyColor { float G[3]; int Tpl; };

float SrgbLinImpl(double byte255) {
  return VegetationTemplates::SrgbToLinear((float)(byte255 / 255.0));
}

}  // namespace

bool VegetationTemplates::Load(const char *path, const GroundMaterials &mats) {
  Table_.clear();
  Names_.clear();
  Lut_.clear();
  Error_.clear();
  Collisions_ = 0;

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

  std::vector<KeyColor> keys;
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

    const Render::Json::Ref ks = t["keySrgb"];
    for (size_t k = 0; k < ks.Size(); k++) {
      KeyColor kc{};
      kc.Tpl = (int)i;
      for (int c = 0; c < 3; c++)
        kc.G[c] = std::sqrt(SrgbLinImpl(ks[k][(size_t)c].Num(0.0)));
      keys.push_back(kc);
    }
  }
  if (keys.empty()) { Error_ = "no key colours declared"; return false; }

  /* Every cell to the NEAREST declared key. A cell that holds two templates' keys at once is the one
   * failure mode of an 8-bit index, so it is counted rather than resolved quietly. */
  std::vector<int> holder((size_t)kLutCells, -1);
  for (const KeyColor &k : keys) {
    const int cell = (int)(k.G[2] * kLutSide) * kLutSide * kLutSide +
                     (int)(k.G[1] * kLutSide) * kLutSide + (int)(k.G[0] * kLutSide);
    const int c = cell < 0 ? 0 : (cell > kLutCells - 1 ? kLutCells - 1 : cell);
    if (holder[(size_t)c] >= 0 && holder[(size_t)c] != k.Tpl) Collisions_++;
    holder[(size_t)c] = k.Tpl;
  }

  Lut_.assign((size_t)kLutCells, 0u);
  for (int b = 0; b < kLutSide; b++)
    for (int g = 0; g < kLutSide; g++)
      for (int r = 0; r < kLutSide; r++) {
        const size_t cell = (size_t)((b * kLutSide + g) * kLutSide + r);
        if (holder[cell] >= 0) { Lut_[cell] = (uint32_t)holder[cell]; continue; }
        const float c[3] = {(r + 0.5f) / kLutSide, (g + 0.5f) / kLutSide, (b + 0.5f) / kLutSide};
        float best = 1e30f;
        int win = 0;
        for (const KeyColor &k : keys) {
          const float d = (k.G[0] - c[0]) * (k.G[0] - c[0]) + (k.G[1] - c[1]) * (k.G[1] - c[1]) +
                          (k.G[2] - c[2]) * (k.G[2] - c[2]);
          if (d < best) { best = d; win = k.Tpl; }
        }
        Lut_[cell] = (uint32_t)win;
      }

  Log::Info("veg", "table", {{"path", path}, {"templates", (int)Table_.size()},
                             {"keys", (int)keys.size()}, {"lutCells", kLutCells},
                             {"collisions", Collisions_}});
  return true;
}

} // namespace outshine::World
