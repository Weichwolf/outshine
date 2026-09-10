#include "GroundMaterials.h"
#include "Json.h"
#include "Log.h"
#include "ReadTextFile.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cmath>
#include <limits>
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
constexpr double kMaximumSlopeDeg = 90.0;
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

bool IsPositiveFloat(double value) {
  return std::isfinite(value) && value >= std::numeric_limits<float>::denorm_min() &&
         value <= std::numeric_limits<float>::max();
}

std::expected<float, std::string>
ReadFactor(Json::Ref value, double fallback, std::string_view name, double maximum = 1.0) {
  if (!value.Valid()) { return static_cast<float>(fallback); }
  const double number = value.Num(-1.0);
  if (value.GetKind() != Json::Kind::Number || !std::isfinite(number) || number < 0.0 ||
      number > maximum) {
    return std::unexpected(std::string(name) +
                           " must be finite, nonnegative and within its allowed range");
  }
  return static_cast<float>(number);
}

std::expected<MoistureModel, std::string> ReadMoistureModel(Json::Ref root) {
  const auto moisture = root["moistureModel"];
  const auto specular = root["specularModel"];
  for (const auto model : {moisture, specular}) {
    if (model.Valid() && model.GetKind() != Json::Kind::Object) {
      return std::unexpected("moistureModel and specularModel must be objects");
    }
  }
  const auto edges = specular["edges"];
  if (edges.Valid() && (edges.GetKind() != Json::Kind::Array || edges.Size() != 2)) {
    return std::unexpected("specularModel.edges must be a pair");
  }
  const auto wet = ReadFactor(moisture["kWet"], 0.0, "moistureModel.kWet");
  if (!wet) { return std::unexpected(wet.error()); }
  const auto low = ReadFactor(edges[size_t{0}], 0.05, "specularModel.edges[0]");
  if (!low) { return std::unexpected(low.error()); }
  const auto high = ReadFactor(edges[size_t{1}], 0.85, "specularModel.edges[1]");
  if (!high) { return std::unexpected(high.error()); }
  if (*low >= *high) {
    return std::unexpected("specularModel.edges must remain strictly increasing as floats");
  }
  return MoistureModel{
      .Wet = *wet, .Low = *low, .High = *high, .Exclusions = moisture["exclusions"]};
}

float MoistureSpecular(float moisture, float low, float high) {
  if (moisture <= low) { return 0.0f; }
  if (moisture >= high) { return 1.0f; }
  const float t = (moisture - low) / (high - low);
  return t * t * (3.0f - 2.0f * t);
}

int FindMaterial(std::span<const GroundMaterials::Material> materials, std::string_view name) {
  for (size_t i = 0; i < materials.size(); ++i) {
    if (materials[i].Name == name) { return static_cast<int>(i); }
  }
  return -1;
}

std::expected<void, std::string>
ReadOpticalValues(Json::Ref source, GroundMaterials::Material &material, float wet) {
  const auto roughness = ReadFactor(source["roughness"], kRoughnessUnsaid, "roughness");
  if (!roughness) { return std::unexpected(roughness.error()); }
  const auto litter = source["litter"];
  if (litter.Valid() && litter.GetKind() != Json::Kind::Object) {
    return std::unexpected("litter must be an object");
  }
  const auto coverage = ReadFactor(litter["coverage"], 0.0, "litter.coverage");
  if (!coverage) { return std::unexpected(coverage.error()); }
  const auto ratio = ReadFactor(source["visibleBroadbandRatio"],
                                1.0,
                                "visibleBroadbandRatio",
                                std::numeric_limits<float>::max());
  if (!ratio) { return std::unexpected(ratio.error()); }
  const auto albedo = source["albedo"];
  if (albedo.Valid() && (albedo.GetKind() != Json::Kind::Array || albedo.Size() != 3)) {
    return std::unexpected("albedo must be an RGB triplet");
  }
  for (size_t channel = 0; channel < 3; ++channel) {
    const auto value = ReadFactor(albedo[channel], kAlbedoUnsaid, "albedo channel");
    if (!value) { return std::unexpected(value.error()); }
    const float attenuated = *value * wet;
    if (static_cast<double>(attenuated) * *ratio > 1.0) {
      return std::unexpected("rendered albedo must not exceed one");
    }
    material.Albedo[static_cast<int>(channel)] = attenuated * *ratio;
  }
  material.Roughness = *roughness;
  material.LitterCoverage = *coverage;
  material.VisibleRatio = *ratio;
  return {};
}

std::expected<std::array<float, 2>, std::string> ReadDetailScales(Json::Ref source) {
  const auto scales = source["detailScaleM"];
  if (!scales.Valid()) {
    return std::array{static_cast<float>(kDetailCoarseUnsaidM),
                      static_cast<float>(kDetailFineUnsaidM)};
  }
  if (scales.GetKind() != Json::Kind::Array || scales.Size() != 2) {
    return std::unexpected("detailScaleM must be a pair");
  }
  std::array<float, 2> values{};
  for (size_t i = 0; i < values.size(); ++i) {
    const auto value = scales[i];
    const double number = value.Num(0.0);
    if (value.GetKind() != Json::Kind::Number || !IsPositiveFloat(number)) {
      return std::unexpected("detailScaleM must contain positive representable float meters");
    }
    values[i] = static_cast<float>(number);
  }
  return values;
}

std::expected<float, std::string> ReadSlopeLimit(Json::Ref source) {
  const auto range = source["slope"]["plausibleDeg"];
  if (range.GetKind() != Json::Kind::Array || range.Size() != 2) {
    return std::unexpected("slope.plausibleDeg must be a pair");
  }
  const auto low = range[size_t{0}];
  const auto high = range[size_t{1}];
  const double minimum = low.Num(-1.0);
  const double maximum = high.Num(-1.0);
  if (low.GetKind() != Json::Kind::Number || high.GetKind() != Json::Kind::Number ||
      !std::isfinite(minimum) || !std::isfinite(maximum) || minimum < 0.0 ||
      maximum > kMaximumSlopeDeg || minimum > maximum) {
    return std::unexpected("slope.plausibleDeg must satisfy 0 <= min <= max <= 90 degrees");
  }
  return static_cast<float>(maximum);
}

std::expected<void, std::string> ReadMetricValues(Json::Ref source,
                                                  GroundMaterials::Material &material) {
  const auto grain = ReadFactor(
      source["grainSizeM"], kGrainSizeUnsaidM, "grainSizeM", std::numeric_limits<float>::max());
  if (!grain) { return std::unexpected(grain.error()); }
  const auto height = ReadFactor(source["heightAmplitudeM"],
                                 kHeightAmplitudeUnsaidM,
                                 "heightAmplitudeM",
                                 std::numeric_limits<float>::max());
  if (!height) { return std::unexpected(height.error()); }
  const auto detail = ReadDetailScales(source);
  if (!detail) { return std::unexpected(detail.error()); }
  const auto slope = ReadSlopeLimit(source);
  if (!slope) { return std::unexpected(slope.error()); }
  material.GrainSizeM = *grain;
  material.HeightAmplitudeM = *height;
  material.DetailCoarseM = (*detail)[0];
  material.DetailFineM = (*detail)[1];
  material.SlopeMaxDeg = *slope;
  return {};
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
  const Json::Ref peak = c["peakFriction"];
  if (peak.GetKind() != Json::Kind::Number || !IsPositiveFloat(peak.Num(0.0))) {
    return std::unexpected("class " + m.Name +
                           ": peakFriction must be a positive representable float");
  }
  m.PeakFriction = static_cast<float>(peak.Num(0.0));
  const auto moisture = ReadFactor(c["moisture"], 0.0, "class " + m.Name + ": moisture");
  if (!moisture) { return std::unexpected(moisture.error()); }
  m.Moisture = *moisture;
  if (auto metric = ReadMetricValues(c, m); !metric) {
    return std::unexpected("class " + m.Name + ": " + metric.error());
  }
  parsed.Litter = c["litter"]["class"].Str("");

  const Json::Ref surf = c["surface"];
  if (surf.StrEquals("coherent")) {
    m.SpecularScale = 1.0f;
  } else if (surf.StrEquals("particulate")) {
    m.SpecularScale = MoistureSpecular(m.Moisture, wetLo, wetHi);
  } else {
    return std::unexpected("class " + m.Name + ": surface must be 'coherent' or 'particulate'");
  }

  bool wetExempt = false;
  for (size_t e = 0; e < excl.Size(); e++) {
    if (excl[e].StrEquals(m.Name.c_str())) { wetExempt = true; }
  }

  const float wet = wetExempt ? 1.0f : (1.0f - kWet * m.Moisture);

  if (auto optical = ReadOpticalValues(c, m, wet); !optical) {
    return std::unexpected("class " + m.Name + ": " + optical.error());
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
    const double ratio = static_cast<double>(material.PeakFriction) / against;
    if (!IsPositiveFloat(ratio)) {
      return std::unexpected("class " + material.Name + ": relative friction exceeds float range");
    }
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
  const auto model = ReadMoistureModel(doc.Root());
  if (!model) { return std::unexpected(model.error()); }

  const std::string reference = doc.Root()["frictionModel"]["reference"].Str("");
  if (reference.empty()) {
    return std::unexpected(
        "frictionModel.reference must name the class every friction is relative to");
  }

  const Json::Ref cls = doc.Root()["classes"];
  if (cls.GetKind() != Json::Kind::Array || cls.Size() == 0) {
    return std::unexpected("no classes array");
  }

  std::vector<GroundMaterials::Material> materials;
  std::vector<std::string> litterName;
  materials.reserve(cls.Size());
  litterName.reserve(cls.Size());
  for (size_t i = 0; i < cls.Size(); ++i) {
    auto parsed = ReadMaterial(cls[i], *model);
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
