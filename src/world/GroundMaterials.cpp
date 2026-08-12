#include "GroundMaterials.h"

#include "Json.h"
#include "Log.h"

#include <algorithm>
#include <cstdio>

namespace outshine::World {

int GroundMaterials::Find(const std::string &name) const {
  for (size_t i = 0; i < Mats_.size(); i++)
    if (Mats_[i].Name == name) return (int)i;
  return -1;
}

bool GroundMaterials::Load(const char *path) {
  Mats_.clear();
  Error_.clear();

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

  const Json::Ref mm = doc.Root()["moistureModel"];
  const float kWet = (float)mm["kWet"].Num(0.0);
  const Json::Ref excl = mm["exclusions"];
  const float wetLo = (float)doc.Root()["specularModel"]["edges"][(size_t)0].Num(0.05);
  const float wetHi = (float)doc.Root()["specularModel"]["edges"][(size_t)1].Num(0.85);

  const Json::Ref cls = doc.Root()["classes"];
  if (cls.GetKind() != Json::Kind::Array || cls.Size() == 0) {
    Error_ = "no classes array";
    return false;
  }

  std::vector<std::string> litterName(cls.Size());
  for (size_t i = 0; i < cls.Size(); i++) {
    const Json::Ref c = cls[i];
    Material m{};
    m.Name = c["name"].Str("?");
    m.Roughness = (float)c["roughness"].Num(0.9);
    m.Moisture = (float)c["moisture"].Num(0.0);
    m.GrainSizeM = (float)c["grainSizeM"].Num(0.002);
    m.HeightAmplitudeM = (float)c["heightAmplitudeM"].Num(0.0005);
    m.DetailCoarseM = (float)c["detailScaleM"][(size_t)0].Num(2.0);
    m.DetailFineM = (float)c["detailScaleM"][(size_t)1].Num(0.3);
    m.LitterCoverage = (float)c["litter"]["coverage"].Num(0.0);
    const Json::Ref pd = c["slope"]["plausibleDeg"];
    if (pd.Size() != 2) { Error_ = "class " + m.Name + ": slope.plausibleDeg must be a pair"; return false; }
    m.SlopeMaxDeg = (float)pd[(size_t)1].Num(90.0);
    litterName[i] = c["litter"]["class"].Str("");

    /* Cook-Torrance wants a continuous dielectric interface; a heap of grains has none, so its
     * specular is a scale and not a lobe shape (specularModel). Unknown spelling is an error and not
     * a default, because either default draws a wrong material everywhere the class wins. */
    const Json::Ref surf = c["surface"];
    if (surf.StrEquals("coherent")) m.SpecularScale = 1.0f;
    else if (surf.StrEquals("particulate")) {
      const float t = std::min(std::max((m.Moisture - wetLo) / (wetHi - wetLo), 0.0f), 1.0f);
      m.SpecularScale = t * t * (3.0f - 2.0f * t);
    } else { Error_ = "class " + m.Name + ": surface must be 'coherent' or 'particulate'"; return false; }

    bool wetExempt = false;
    for (size_t e = 0; e < excl.Size(); e++)
      if (excl[e].StrEquals(m.Name.c_str())) wetExempt = true;
    /* The dial is applied ONCE, here, so no consumer can apply it twice: the class list is static
     * today and weather has no producer. */
    const float wet = wetExempt ? 1.0f : (1.0f - kWet * m.Moisture);
    /* The triple is locked to the SHORTWAVE albedo (0.3-2.5 um) and the renderer is visible-band, so
     * the band correction is a per-class SCALE: the chromaticity is a separate source and must not
     * move. A single gain across the table is what failed before — 0.257 for grass against 0.953 for
     * asphalt is a factor 3.7 (ground-materials.json visibleBandModel.whyNotOneNumber). */
    m.VisibleRatio = (float)c["visibleBroadbandRatio"].Num(1.0);
    for (int k = 0; k < 3; k++)
      m.Albedo[k] = (float)c["albedo"][(size_t)k].Num(0.15) * wet * m.VisibleRatio;

    m.LitterClass = -1;
    Mats_.push_back(m);
  }
  for (size_t i = 0; i < Mats_.size(); i++)
    if (!litterName[i].empty()) Mats_[i].LitterClass = Find(litterName[i]);

  Log::Info("ground", "materials", {{"path", path}, {"classes", (int)Mats_.size()},
                                    {"kWet", (double)kWet}});
  return true;
}

} // namespace outshine::World
