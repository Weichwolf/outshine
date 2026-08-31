#ifndef OUTSHINE_RENDER_STAGES_FRAGMENTARMS_H
#define OUTSHINE_RENDER_STAGES_FRAGMENTARMS_H

#include <cstddef>

#include <SurfaceState.h>

#include "DrawList.h"

namespace outshine::Render {

enum class ShadingArm : std::uint8_t { Flat, Lit, Mapped };

inline constexpr std::size_t kShadingArms = 3;
inline constexpr std::size_t kSurfaceKinds = 5;
inline constexpr std::size_t kFragmentArms = kShadingArms * 2u * kSurfaceKinds;

struct FragmentArm {
  ShadingArm Shading;
  bool Textured;
  SurfaceKind Kind;
  const char *Entry;
};

[[nodiscard]] constexpr std::size_t
FragmentArmAt(ShadingArm shading, bool textured, SurfaceKind kind) {
  return ((static_cast<std::size_t>(shading) * 2u) + (textured ? 1u : 0u)) * kSurfaceKinds +
         static_cast<std::size_t>(kind);
}

inline constexpr FragmentArm kFragmentArmRows[kFragmentArms] = {
    {.Shading = ShadingArm::Flat, .Textured = false, .Kind = SurfaceKind::Opaque, .Entry = "fs"},
    {.Shading = ShadingArm::Flat,
     .Textured = false,
     .Kind = SurfaceKind::Masked,
     .Entry = "fsMasked"},
    {.Shading = ShadingArm::Flat,
     .Textured = false,
     .Kind = SurfaceKind::Blended,
     .Entry = "fsBlended"},
    {.Shading = ShadingArm::Flat,
     .Textured = false,
     .Kind = SurfaceKind::ThinTransmissive,
     .Entry = "fsTransmissive"},
    {.Shading = ShadingArm::Flat,
     .Textured = false,
     .Kind = SurfaceKind::Refractive,
     .Entry = "fsTransmissive"},

    {.Shading = ShadingArm::Flat,
     .Textured = true,
     .Kind = SurfaceKind::Opaque,
     .Entry = "fsTextured"},
    {.Shading = ShadingArm::Flat,
     .Textured = true,
     .Kind = SurfaceKind::Masked,
     .Entry = "fsMaskedTextured"},
    {.Shading = ShadingArm::Flat,
     .Textured = true,
     .Kind = SurfaceKind::Blended,
     .Entry = "fsBlendedTextured"},
    {.Shading = ShadingArm::Flat,
     .Textured = true,
     .Kind = SurfaceKind::ThinTransmissive,
     .Entry = "fsTransmissive"},
    {.Shading = ShadingArm::Flat,
     .Textured = true,
     .Kind = SurfaceKind::Refractive,
     .Entry = "fsTransmissive"},

    {.Shading = ShadingArm::Lit, .Textured = false, .Kind = SurfaceKind::Opaque, .Entry = "fsLit"},
    {.Shading = ShadingArm::Lit,
     .Textured = false,
     .Kind = SurfaceKind::Masked,
     .Entry = "fsLitMasked"},
    {.Shading = ShadingArm::Lit,
     .Textured = false,
     .Kind = SurfaceKind::Blended,
     .Entry = "fsLitBlended"},
    {.Shading = ShadingArm::Lit,
     .Textured = false,
     .Kind = SurfaceKind::ThinTransmissive,
     .Entry = "fsLitTransmissive"},
    {.Shading = ShadingArm::Lit,
     .Textured = false,
     .Kind = SurfaceKind::Refractive,
     .Entry = "fsLitTransmissive"},

    {.Shading = ShadingArm::Lit,
     .Textured = true,
     .Kind = SurfaceKind::Opaque,
     .Entry = "fsLitTextured"},
    {.Shading = ShadingArm::Lit,
     .Textured = true,
     .Kind = SurfaceKind::Masked,
     .Entry = "fsLitMaskedTextured"},
    {.Shading = ShadingArm::Lit,
     .Textured = true,
     .Kind = SurfaceKind::Blended,
     .Entry = "fsLitBlendedTextured"},
    {.Shading = ShadingArm::Lit,
     .Textured = true,
     .Kind = SurfaceKind::ThinTransmissive,
     .Entry = "fsLitTransmissiveTextured"},
    {.Shading = ShadingArm::Lit,
     .Textured = true,
     .Kind = SurfaceKind::Refractive,
     .Entry = "fsLitTransmissiveTextured"},

    {.Shading = ShadingArm::Mapped,
     .Textured = false,
     .Kind = SurfaceKind::Opaque,
     .Entry = "fsMapped"},
    {.Shading = ShadingArm::Mapped,
     .Textured = false,
     .Kind = SurfaceKind::Masked,
     .Entry = "fsMappedMasked"},
    {.Shading = ShadingArm::Mapped,
     .Textured = false,
     .Kind = SurfaceKind::Blended,
     .Entry = "fsMappedBlended"},
    {.Shading = ShadingArm::Mapped,
     .Textured = false,
     .Kind = SurfaceKind::ThinTransmissive,
     .Entry = "fsMappedTransmissive"},
    {.Shading = ShadingArm::Mapped,
     .Textured = false,
     .Kind = SurfaceKind::Refractive,
     .Entry = "fsMappedTransmissive"},

    {.Shading = ShadingArm::Mapped,
     .Textured = true,
     .Kind = SurfaceKind::Opaque,
     .Entry = "fsMapped"},
    {.Shading = ShadingArm::Mapped,
     .Textured = true,
     .Kind = SurfaceKind::Masked,
     .Entry = "fsMappedMasked"},
    {.Shading = ShadingArm::Mapped,
     .Textured = true,
     .Kind = SurfaceKind::Blended,
     .Entry = "fsMappedBlended"},
    {.Shading = ShadingArm::Mapped,
     .Textured = true,
     .Kind = SurfaceKind::ThinTransmissive,
     .Entry = "fsMappedTransmissive"},
    {.Shading = ShadingArm::Mapped,
     .Textured = true,
     .Kind = SurfaceKind::Refractive,
     .Entry = "fsMappedTransmissive"},
};

constexpr bool EveryFragmentArmIsAtItsOwnIndex() {
  for (std::size_t at = 0; at < kFragmentArms; ++at) {
    const FragmentArm &one = kFragmentArmRows[at];
    if (FragmentArmAt(one.Shading, one.Textured, one.Kind) != at) { return false; }
    if (one.Entry == nullptr) { return false; }
  }
  return true;
}

static_assert(EveryFragmentArmIsAtItsOwnIndex(),
              "a fragment arm's index is derived from its own axes, so the table is total over "
              "them and no combination falls through to a default");

[[nodiscard]] constexpr ShadingArm ShadingArmOf(VertexLayout layout) {
  if (CarriesTangent(layout)) { return ShadingArm::Mapped; }
  if (CarriesNormal(layout)) { return ShadingArm::Lit; }
  return ShadingArm::Flat;
}

[[nodiscard]] constexpr const char *
FragmentArmNamed(ShadingArm shading, bool textured, SurfaceKind kind) {
  return kFragmentArmRows[FragmentArmAt(shading, textured, kind)].Entry;
}

} // namespace outshine::Render
#endif
