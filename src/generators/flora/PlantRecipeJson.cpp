#include "PlantRecipeJson.h"
#include "PlantRecipeLayout.h"
#include "Json.h"
#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <string_view>
#include <system_error>
#include <utility>

namespace outshine::Generators {
namespace {
template <class Record> struct NumberField {
  const char *Name;
  double Record::*Member;
};

constexpr std::array kLevelNumbers{
    NumberField<PlantLevel>{.Name = "length", .Member = &PlantLevel::LengthM},
    NumberField<PlantLevel>{.Name = "base_radius", .Member = &PlantLevel::BaseRadiusM},
    NumberField<PlantLevel>{.Name = "tip_radius", .Member = &PlantLevel::TipRadiusM},
    NumberField<PlantLevel>{.Name = "tip_scale", .Member = &PlantLevel::TipScale},
    NumberField<PlantLevel>{.Name = "attachment_jitter", .Member = &PlantLevel::AttachmentJitter},
    NumberField<PlantLevel>{.Name = "azimuth_variation",
                            .Member = &PlantLevel::AzimuthVariationRad},
    NumberField<PlantLevel>{.Name = "upward_bias", .Member = &PlantLevel::UpwardBias},
    NumberField<PlantLevel>{.Name = "first", .Member = &PlantLevel::First},
    NumberField<PlantLevel>{.Name = "last", .Member = &PlantLevel::Last},
    NumberField<PlantLevel>{.Name = "angle", .Member = &PlantLevel::AngleRad},
    NumberField<PlantLevel>{.Name = "angle_variation", .Member = &PlantLevel::AngleVariationRad},
    NumberField<PlantLevel>{.Name = "bend", .Member = &PlantLevel::BendRad},
    NumberField<PlantLevel>{.Name = "bend_variation", .Member = &PlantLevel::BendVariationRad},
    NumberField<PlantLevel>{.Name = "leaf_first", .Member = &PlantLevel::LeafFirst},
    NumberField<PlantLevel>{.Name = "leaf_last", .Member = &PlantLevel::LeafLast},
    NumberField<PlantLevel>{.Name = "leaf_angle", .Member = &PlantLevel::LeafAngleRad},
    NumberField<PlantLevel>{.Name = "leaf_sky_alignment", .Member = &PlantLevel::LeafSkyAlignment},
};
constexpr std::array kBladeNumbers{
    NumberField<PlantBlade>{.Name = "length", .Member = &PlantBlade::LengthM},
    NumberField<PlantBlade>{.Name = "width", .Member = &PlantBlade::WidthM},
    NumberField<PlantBlade>{.Name = "widest", .Member = &PlantBlade::Widest},
    NumberField<PlantBlade>{.Name = "lobe_depth", .Member = &PlantBlade::LobeDepth},
    NumberField<PlantBlade>{.Name = "base_fill", .Member = &PlantBlade::BaseFill},
    NumberField<PlantBlade>{.Name = "bend", .Member = &PlantBlade::BendRad},
    NumberField<PlantBlade>{.Name = "fold", .Member = &PlantBlade::Fold},
    NumberField<PlantBlade>{.Name = "twist", .Member = &PlantBlade::TwistRad},
    NumberField<PlantBlade>{.Name = "petiole", .Member = &PlantBlade::PetioleM},
};

template <class Integer> bool ReadInteger(const Json::Ref &value, Integer &target) {
  if (value.GetKind() != Json::Kind::Number) { return false; }
  const auto text = value.Source();
  const auto result = std::from_chars(text.data(), text.data() + text.size(), target);
  return result.ec == std::errc{} && result.ptr == text.data() + text.size();
}

bool ReadOptional(const Json::Ref &value, std::optional<double> &target) {
  if (value.GetKind() == Json::Kind::Null) {
    target.reset();
    return true;
  }
  if (value.GetKind() != Json::Kind::Number || !std::isfinite(value.Num())) { return false; }
  target = value.Num();
  return true;
}

template <class Record, size_t N>
bool ReadNumbers(const Json::Ref &row,
                 Record &target,
                 const std::array<NumberField<Record>, N> &fields) {
  for (const auto &field : fields) {
    const auto value = row[field.Name];
    if (value.GetKind() != Json::Kind::Number || !std::isfinite(value.Num())) { return false; }
    target.*field.Member = value.Num();
  }
  return true;
}

bool ReadLevel(const Json::Ref &row, bool extended, PlantLevel &level) {
  constexpr std::array kShapes{"taper", "rounded"};
  constexpr std::array kArrangements{"spiral", "distichous", "opposite", "decussate", "whorled"};
  if (row.GetKind() != Json::Kind::Object ||
      row.Size() != kLevelNumbers.size() + 4 + (extended ? 2 : 0) ||
      !ReadNumbers(row, level, kLevelNumbers) || !ReadInteger(row["branches"], level.Branches) ||
      !ReadOptional(row["leaf_internode"], level.LeafInternodeM)) {
    return false;
  }
  size_t shape = 0;
  while (shape < kShapes.size() && !row["length_shape"].StrEquals(kShapes[shape])) { ++shape; }
  size_t arrangement = 0;
  while (arrangement < kArrangements.size() &&
         !row["arrangement"].StrEquals(kArrangements[arrangement])) {
    ++arrangement;
  }
  if (shape == kShapes.size() || arrangement == kArrangements.size()) { return false; }
  level.Shape = static_cast<PlantBranchShape>(shape);
  level.Arrangement = static_cast<PlantArrangement>(arrangement);
  return !extended || (ReadInteger(row["whorl_size"], level.WhorlSize) &&
                       ReadOptional(row["angle_tip"], level.TipAngleRad));
}

bool ReadBlade(const Json::Ref &row, PlantBlade &blade) {
  return row.GetKind() == Json::Kind::Object && row.Size() == kBladeNumbers.size() + 2 &&
         ReadNumbers(row, blade, kBladeNumbers) && ReadInteger(row["lobes"], blade.Lobes) &&
         ReadInteger(row["fold_power"], blade.FoldPower);
}
}

std::expected<PlantRecipe, std::string_view> ReadPlantRecipe(std::string_view text,
                                                             size_t maximumBytes) {
  if (text.empty() || text.size() > maximumBytes) {
    return std::unexpected("plant recipe exceeds its input byte budget");
  }
  Json document;
  if (!document.Parse(text.data(), text.size())) {
    return std::unexpected("plant recipe requires valid JSON");
  }
  const auto root = document.Root();
  const bool extended = root["version"].StrEquals("branching-plant-3");
  const auto levels = root["levels"];
  const auto blades = root["blades"];
  PlantRecipe result;
  if (root.GetKind() != Json::Kind::Object || root.Size() != 4 ||
      (!extended && !root["version"].StrEquals("branching-plant-2")) ||
      !ReadInteger(root["seed"], result.Seed) || levels.GetKind() != Json::Kind::Array ||
      levels.Size() == 0 || levels.Size() > 8 || blades.GetKind() != Json::Kind::Array ||
      blades.Size() > kPlantRecipeMostBranches || (!extended && blades.Size() == 0)) {
    return std::unexpected("plant recipe requires a supported complete versioned envelope");
  }
  result.Levels.resize(levels.Size());
  result.Blades.resize(blades.Size());
  for (size_t at = 0; at < levels.Size(); ++at) {
    if (!ReadLevel(levels[at], extended, result.Levels[at])) {
      return std::unexpected("plant recipe has an incomplete or unsupported level");
    }
  }
  for (size_t at = 0; at < blades.Size(); ++at) {
    if (!ReadBlade(blades[at], result.Blades[at])) {
      return std::unexpected("plant recipe has an incomplete or unsupported blade");
    }
  }
  if (!ValidPlantRecipe(result)) { return std::unexpected("plant recipe parameters are invalid"); }
  return result;
}
}
