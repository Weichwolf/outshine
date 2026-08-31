#ifndef OUTSHINE_RENDER_STAGES_VERTEXARMS_H
#define OUTSHINE_RENDER_STAGES_VERTEXARMS_H

#include <cstddef>
#include <string>
#include <vector>

#include "DrawList.h"

namespace outshine::Render {

struct VertexArm {
  VertexLayout Layout;
  bool Normal;
  bool Tangent;
  bool Uv;
  bool Uv1;
  bool Colour;
};

inline constexpr VertexArm kVertexArms[] = {
    {.Layout = VertexLayout::Position,
     .Normal = false,
     .Tangent = false,
     .Uv = false,
     .Uv1 = false,
     .Colour = false},
    {.Layout = VertexLayout::PositionUv,
     .Normal = false,
     .Tangent = false,
     .Uv = true,
     .Uv1 = false,
     .Colour = false},
    {.Layout = VertexLayout::PositionUvUv1,
     .Normal = false,
     .Tangent = false,
     .Uv = true,
     .Uv1 = true,
     .Colour = false},
    {.Layout = VertexLayout::PositionNormal,
     .Normal = true,
     .Tangent = false,
     .Uv = false,
     .Uv1 = false,
     .Colour = false},
    {.Layout = VertexLayout::PositionNormalUv,
     .Normal = true,
     .Tangent = false,
     .Uv = true,
     .Uv1 = false,
     .Colour = false},
    {.Layout = VertexLayout::PositionNormalUvUv1,
     .Normal = true,
     .Tangent = false,
     .Uv = true,
     .Uv1 = true,
     .Colour = false},
    {.Layout = VertexLayout::PositionNormalUvTangent,
     .Normal = true,
     .Tangent = true,
     .Uv = true,
     .Uv1 = false,
     .Colour = false},
    {.Layout = VertexLayout::PositionNormalUvUv1Tangent,
     .Normal = true,
     .Tangent = true,
     .Uv = true,
     .Uv1 = true,
     .Colour = false},
    {.Layout = VertexLayout::PositionColour,
     .Normal = false,
     .Tangent = false,
     .Uv = false,
     .Uv1 = false,
     .Colour = true},
    {.Layout = VertexLayout::PositionUvColour,
     .Normal = false,
     .Tangent = false,
     .Uv = true,
     .Uv1 = false,
     .Colour = true},
    {.Layout = VertexLayout::PositionUvUv1Colour,
     .Normal = false,
     .Tangent = false,
     .Uv = true,
     .Uv1 = true,
     .Colour = true},
    {.Layout = VertexLayout::PositionNormalColour,
     .Normal = true,
     .Tangent = false,
     .Uv = false,
     .Uv1 = false,
     .Colour = true},
    {.Layout = VertexLayout::PositionNormalUvColour,
     .Normal = true,
     .Tangent = false,
     .Uv = true,
     .Uv1 = false,
     .Colour = true},
    {.Layout = VertexLayout::PositionNormalUvUv1Colour,
     .Normal = true,
     .Tangent = false,
     .Uv = true,
     .Uv1 = true,
     .Colour = true},
    {.Layout = VertexLayout::PositionNormalUvTangentColour,
     .Normal = true,
     .Tangent = true,
     .Uv = true,
     .Uv1 = false,
     .Colour = true},
    {.Layout = VertexLayout::PositionNormalUvUv1TangentColour,
     .Normal = true,
     .Tangent = true,
     .Uv = true,
     .Uv1 = true,
     .Colour = true},
};

inline constexpr std::size_t kVertexArmCount = sizeof kVertexArms / sizeof kVertexArms[0];

static_assert(kVertexArmCount ==
                  static_cast<std::size_t>(VertexLayout::PositionNormalUvUv1TangentColour) + 1u,
              "every vertex layout has an arm, and the table is in the enum's own order");

constexpr bool EveryArmIsAtItsOwnIndex() {
  for (std::size_t at = 0; at < kVertexArmCount; ++at) {
    if (static_cast<std::size_t>(kVertexArms[at].Layout) != at) { return false; }
  }
  return true;
}

static_assert(EveryArmIsAtItsOwnIndex(),
              "a row sits at the index its own layout names, so the lookup is not a search");

[[nodiscard]] inline std::string ArmNamed(const VertexArm &one) {
  std::string said = "vs";
  if (one.Tangent) {
    said += "Mapped";
  } else if (one.Normal) {
    said += "Lit";
  }
  if (one.Uv && !one.Tangent) { said += "Textured"; }
  if (one.Uv1) { said += "Two"; }
  if (one.Colour) { said += "Tinted"; }
  return said;
}

[[nodiscard]] inline const char *VertexArmName(VertexLayout layout) {
  static const std::vector<std::string> named = [] {
    std::vector<std::string> made;
    made.reserve(kVertexArmCount);
    for (const VertexArm &one : kVertexArms) { made.push_back(ArmNamed(one)); }
    return made;
  }();
  const auto at = static_cast<std::size_t>(layout);
  return at < kVertexArmCount ? named[at].c_str() : "vs";
}

[[nodiscard]] inline std::string VertexArmsMsl() {
  std::string said = "\n";
  for (const VertexArm &one : kVertexArms) {
    std::string runs;
    if (one.Uv && !one.Tangent) { runs += "SUBJECT_UV_ATTRIBUTE "; }
    if (one.Uv1) { runs += "SUBJECT_UV1_ATTRIBUTE "; }
    if (one.Colour) { runs += "SUBJECT_COLOUR_ATTRIBUTE"; }
    if (runs.empty()) { runs = "SUBJECT_NO_COLOUR_ATTRIBUTE"; }
    if (one.Tangent) {
      said += "SUBJECT_MAPPED_ARM(";
    } else if (one.Normal) {
      said += "SUBJECT_LIT_ARM(";
    } else {
      said += "SUBJECT_EMITTED_ARM(";
    }
    said += ArmNamed(one);
    said += ", ";
    said += runs;
    said += ", ";
    if (!one.Tangent) { said += one.Uv ? "v.uv, " : "float2(0.0), "; }
    said += one.Uv1 ? "v.uv1, " : "float2(0.0), ";
    said += one.Colour ? "v.colour)\n" : "SUBJECT_NO_VERTEX_COLOUR)\n";
  }
  return said;
}

} // namespace outshine::Render
#endif
