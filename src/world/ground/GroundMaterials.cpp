#include "GroundMaterials.h"

#include "Json.h"
#include "Log.h"

#include <algorithm>
#include <cstdio>

namespace outshine::Ground {

int GroundMaterials::Find(std::string_view name) const {
  for (size_t i = 0; i < Mats_.size(); i++) {
    if (Mats_[i].Name == name) { return static_cast<int>(i); }
  }
  return -1;
}

bool GroundMaterials::Load(const char *path) {
  Mats_.clear();
  Error_.clear();

  FILE *f = fopen(path, "rb");
  if (f == nullptr) {
    Error_ = std::string("open failed: ") + path;
    return false;
  }
  fseek(f, 0, SEEK_END);
  const long n = ftell(f);
  fseek(f, 0, SEEK_SET);
  std::string text(static_cast<size_t>(n > 0 ? n : 0), '\0');
  const size_t got = n > 0 ? fread(&text[0], 1, static_cast<size_t>(n), f) : 0;
  fclose(f);
  if (got != text.size()) {
    Error_ = "short read";
    return false;
  }

  Json doc;
  if (!doc.Parse(text.c_str(), text.size())) {
    Error_ = "parse failed";
    return false;
  }

  const Json::Ref mm = doc.Root()["moistureModel"];
  const float kWet = static_cast<float>(mm["kWet"].Num(0.0));
  const Json::Ref excl = mm["exclusions"];
  const float wetLo =
      static_cast<float>(doc.Root()["specularModel"]["edges"][static_cast<size_t>(0)].Num(0.05));
  const float wetHi =
      static_cast<float>(doc.Root()["specularModel"]["edges"][static_cast<size_t>(1)].Num(0.85));

  const std::string reference = doc.Root()["frictionModel"]["reference"].Str("");
  if (reference.empty()) {
    Error_ = "frictionModel.reference must name the class every friction is relative to";
    return false;
  }

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
    m.Roughness = static_cast<float>(c["roughness"].Num(0.9));
    const Json::Ref peak = c["peakFriction"];
    if (peak.GetKind() != Json::Kind::Number || !(peak.Num(0.0) > 0.0)) {
      Error_ = "class " + m.Name + ": peakFriction must be a positive number";
      return false;
    }
    m.PeakFriction = static_cast<float>(peak.Num(0.0));
    m.Moisture = static_cast<float>(c["moisture"].Num(0.0));
    m.GrainSizeM = static_cast<float>(c["grainSizeM"].Num(0.002));
    m.HeightAmplitudeM = static_cast<float>(c["heightAmplitudeM"].Num(0.0005));
    m.DetailCoarseM = static_cast<float>(c["detailScaleM"][static_cast<size_t>(0)].Num(2.0));
    m.DetailFineM = static_cast<float>(c["detailScaleM"][static_cast<size_t>(1)].Num(0.3));
    m.LitterCoverage = static_cast<float>(c["litter"]["coverage"].Num(0.0));
    const Json::Ref pd = c["slope"]["plausibleDeg"];
    if (pd.Size() != 2) {
      Error_ = "class " + m.Name + ": slope.plausibleDeg must be a pair";
      return false;
    }
    m.SlopeMaxDeg = static_cast<float>(pd[static_cast<size_t>(1)].Num(90.0));
    litterName[i] = c["litter"]["class"].Str("");

    const Json::Ref surf = c["surface"];
    if (surf.StrEquals("coherent")) {
      m.SpecularScale = 1.0f;
    } else if (surf.StrEquals("particulate")) {
      const float t = std::min(std::max((m.Moisture - wetLo) / (wetHi - wetLo), 0.0f), 1.0f);
      m.SpecularScale = t * t * (3.0f - 2.0f * t);
    } else {
      Error_ = "class " + m.Name + ": surface must be 'coherent' or 'particulate'";
      return false;
    }

    bool wetExempt = false;
    for (size_t e = 0; e < excl.Size(); e++) {
      if (excl[e].StrEquals(m.Name.c_str())) { wetExempt = true; }
    }

    const float wet = wetExempt ? 1.0f : (1.0f - kWet * m.Moisture);

    m.VisibleRatio = static_cast<float>(c["visibleBroadbandRatio"].Num(1.0));
    for (int k = 0; k < 3; k++) {
      m.Albedo[k] =
          static_cast<float>(c["albedo"][static_cast<size_t>(k)].Num(0.15)) * wet * m.VisibleRatio;
    }

    m.LitterClass = -1;
    Mats_.push_back(m);
  }
  const int stands = Find(reference);
  if (stands < 0) {
    Error_ = "frictionModel.reference names '" + reference + "' and no class carries that name";
    return false;
  }
  const float against = Mats_[static_cast<size_t>(stands)].PeakFriction;
  for (Material &one : Mats_) { one.FrictionFactor = one.PeakFriction / against; }

  for (size_t i = 0; i < Mats_.size(); i++) {
    if (!litterName[i].empty()) { Mats_[i].LitterClass = Find(litterName[i]); }
  }

  Log::Info("ground",
            "materials",
            {{"path", path},
             {"classes", static_cast<int>(Mats_.size())},
             {"kWet", static_cast<double>(kWet)}});
  return true;
}

} // namespace outshine::Ground
