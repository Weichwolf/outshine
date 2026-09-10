#include "GroundMaterials.h"
#include "Json.h"
#include "Log.h"
#include "ReadTextFile.h"
#include <algorithm>
#include <cstddef>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace outshine::Ground {
namespace {
constexpr double kRoughnessUnsaid = 0.9;
constexpr double kGrainSizeUnsaidM = 0.002;
constexpr double kHeightAmplitudeUnsaidM = 0.0005;
constexpr double kDetailCoarseUnsaidM = 2.0;
constexpr double kDetailFineUnsaidM = 0.3;
constexpr double kSlopeMaxUnsaidDeg = 90.0;
constexpr double kAlbedoUnsaid = 0.15;

constexpr size_t kCatalogByteLimit = size_t{1024} * 1024u;

struct MoistureModel {
  float Wet;
  float Low;
  float High;
  Json::Ref Exclusions;
};

struct ParsedMaterial {
  GroundMaterials::Material Value;
  std::string Litter;
};

int FindMaterial(std::span<const GroundMaterials::Material> materials, std::string_view name) {
  for (size_t i = 0; i < materials.size(); ++i) {
    if (materials[i].Name == name) { return static_cast<int>(i); }
  }
  return -1;
}

std::expected<ParsedMaterial, std::string> ReadMaterial(Json::Ref c, const MoistureModel &model) {
  const float kWet = model.Wet;
  const float wetLo = model.Low;
  const float wetHi = model.High;
  const Json::Ref excl = model.Exclusions;
  ParsedMaterial parsed{};
  auto &m = parsed.Value;
  m.Name = c["name"].Str("");
  if (m.Name.empty()) { return std::unexpected("material class must have a nonempty name"); }
  m.Roughness = static_cast<float>(c["roughness"].Num(kRoughnessUnsaid));
  const Json::Ref peak = c["peakFriction"];
  if (peak.GetKind() != Json::Kind::Number || !(peak.Num(0.0) > 0.0)) {
    return std::unexpected("class " + m.Name + ": peakFriction must be a positive number");
  }
  m.PeakFriction = static_cast<float>(peak.Num(0.0));
  m.Moisture = static_cast<float>(c["moisture"].Num(0.0));
  m.GrainSizeM = static_cast<float>(c["grainSizeM"].Num(kGrainSizeUnsaidM));
  m.HeightAmplitudeM = static_cast<float>(c["heightAmplitudeM"].Num(kHeightAmplitudeUnsaidM));
  m.DetailCoarseM =
      static_cast<float>(c["detailScaleM"][static_cast<size_t>(0)].Num(kDetailCoarseUnsaidM));
  m.DetailFineM =
      static_cast<float>(c["detailScaleM"][static_cast<size_t>(1)].Num(kDetailFineUnsaidM));
  m.LitterCoverage = static_cast<float>(c["litter"]["coverage"].Num(0.0));
  const Json::Ref pd = c["slope"]["plausibleDeg"];
  if (pd.Size() != 2) {
    return std::unexpected("class " + m.Name + ": slope.plausibleDeg must be a pair");
  }
  m.SlopeMaxDeg = static_cast<float>(pd[static_cast<size_t>(1)].Num(kSlopeMaxUnsaidDeg));
  parsed.Litter = c["litter"]["class"].Str("");

  const Json::Ref surf = c["surface"];
  if (surf.StrEquals("coherent")) {
    m.SpecularScale = 1.0f;
  } else if (surf.StrEquals("particulate")) {
    const float t = std::min(std::max((m.Moisture - wetLo) / (wetHi - wetLo), 0.0f), 1.0f);
    m.SpecularScale = t * t * (3.0f - 2.0f * t);
  } else {
    return std::unexpected("class " + m.Name + ": surface must be 'coherent' or 'particulate'");
  }

  bool wetExempt = false;
  for (size_t e = 0; e < excl.Size(); e++) {
    if (excl[e].StrEquals(m.Name.c_str())) { wetExempt = true; }
  }

  const float wet = wetExempt ? 1.0f : (1.0f - kWet * m.Moisture);

  m.VisibleRatio = static_cast<float>(c["visibleBroadbandRatio"].Num(1.0));
  for (int k = 0; k < 3; k++) {
    m.Albedo[k] = static_cast<float>(c["albedo"][static_cast<size_t>(k)].Num(kAlbedoUnsaid)) * wet *
                  m.VisibleRatio;
  }

  m.LitterClass = -1;
  return parsed;
}

struct NamedMaterial {
  std::string_view Name;
  int Index;
};

std::expected<void, std::string> ResolveMaterials(std::span<GroundMaterials::Material> materials,
                                                  std::span<const std::string> litterNames,
                                                  std::string_view reference) {
  std::vector<NamedMaterial> names;
  names.reserve(materials.size());
  for (size_t i = 0; i < materials.size(); ++i) {
    names.push_back({.Name = materials[i].Name, .Index = static_cast<int>(i)});
  }
  std::ranges::sort(names, {}, &NamedMaterial::Name);
  if (std::ranges::adjacent_find(names, {}, &NamedMaterial::Name) != names.end()) {
    return std::unexpected("material class names must be unique");
  }
  const auto find = [&names](std::string_view name) {
    const auto found = std::ranges::lower_bound(names, name, {}, &NamedMaterial::Name);
    return found != names.end() && found->Name == name ? found->Index : -1;
  };
  const int stands = find(reference);
  if (stands < 0) {
    return std::unexpected("frictionModel.reference names an unknown class: " +
                           std::string(reference));
  }
  const float against = materials[static_cast<size_t>(stands)].PeakFriction;
  for (size_t i = 0; i < materials.size(); ++i) {
    auto &material = materials[i];
    material.FrictionFactor = material.PeakFriction / against;
    if (litterNames[i].empty()) { continue; }
    material.LitterClass = find(litterNames[i]);
    if (material.LitterClass < 0) {
      return std::unexpected("unknown litter class: " + litterNames[i]);
    }
  }
  return {};
}

std::expected<std::vector<GroundMaterials::Material>, std::string> ReadCatalog(const Json &doc) {
  const Json::Ref mm = doc.Root()["moistureModel"];
  const float kWet = static_cast<float>(mm["kWet"].Num(0.0));
  const Json::Ref excl = mm["exclusions"];
  const float wetLo =
      static_cast<float>(doc.Root()["specularModel"]["edges"][static_cast<size_t>(0)].Num(0.05));
  const float wetHi =
      static_cast<float>(doc.Root()["specularModel"]["edges"][static_cast<size_t>(1)].Num(0.85));

  const std::string reference = doc.Root()["frictionModel"]["reference"].Str("");
  if (reference.empty()) {
    return std::unexpected(
        "frictionModel.reference must name the class every friction is relative to");
  }

  const Json::Ref cls = doc.Root()["classes"];
  if (cls.GetKind() != Json::Kind::Array || cls.Size() == 0) {
    return std::unexpected("no classes array");
  }

  const MoistureModel model{.Wet = kWet, .Low = wetLo, .High = wetHi, .Exclusions = excl};
  std::vector<GroundMaterials::Material> materials;
  std::vector<std::string> litterName;
  materials.reserve(cls.Size());
  litterName.reserve(cls.Size());
  for (size_t i = 0; i < cls.Size(); ++i) {
    auto parsed = ReadMaterial(cls[i], model);
    if (!parsed) { return std::unexpected(std::move(parsed.error())); }
    materials.push_back(std::move(parsed->Value));
    litterName.push_back(std::move(parsed->Litter));
  }
  if (auto resolved = ResolveMaterials(materials, litterName, reference); !resolved) {
    return std::unexpected(std::move(resolved.error()));
  }

  return materials;
}
}

int GroundMaterials::Find(std::string_view name) const {
  return FindMaterial(Mats_, name);
}

bool GroundMaterials::Load(const char *path) {
  Error_.clear();
  if (path == nullptr) {
    Error_ = "material catalog path is null";
    return false;
  }
  const auto text = ReadTextFile(path, kCatalogByteLimit);
  if (!text) {
    Error_ = text.error();
    return false;
  }
  Json doc;
  if (!doc.Parse(text->data(), text->size())) {
    Error_ = "material catalog JSON parse failed";
    return false;
  }
  auto catalog = ReadCatalog(doc);
  if (!catalog) {
    Error_ = std::move(catalog.error());
    return false;
  }
  Mats_ = std::move(*catalog);
  Log::Info(
      LogTag::Ground,
      "materials",
      {{"path", path},
       {"classes", static_cast<int>(Mats_.size())},
       {"kWet",
        static_cast<double>(static_cast<float>(doc.Root()["moistureModel"]["kWet"].Num(0.0)))}});
  return true;
}
}
