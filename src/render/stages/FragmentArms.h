#ifndef OUTSHINE_RENDER_STAGES_FRAGMENTARMS_H
#define OUTSHINE_RENDER_STAGES_FRAGMENTARMS_H

#include <cstddef>

#include <SurfaceState.h>

#include "DrawList.h"
#include "SubjectTypes.h"

namespace outshine::Render {

enum class ShadingArm : std::uint8_t { Flat, Lit, Mapped };

inline constexpr std::size_t kShadingArms = 3;
inline constexpr std::size_t kSurfaceKinds = 5;
inline constexpr std::size_t kFragmentArms = kSurfaceDomains * kShadingArms * 2u * kSurfaceKinds;

struct FragmentArm {
  SurfaceDomain Domain;
  ShadingArm Shading;
  bool Textured;
  SurfaceKind Kind;
  const char *Entry;
};

[[nodiscard]] constexpr std::size_t
FragmentArmAt(SurfaceDomain domain, ShadingArm shading, bool textured, SurfaceKind kind) {
  const std::size_t shaded =
      (static_cast<std::size_t>(domain) * kShadingArms) + static_cast<std::size_t>(shading);
  return ((shaded * 2u) + (textured ? 1u : 0u)) * kSurfaceKinds + static_cast<std::size_t>(kind);
}

[[nodiscard]] constexpr bool
DomainPresents(SurfaceDomain domain, ShadingArm shading, bool textured, SurfaceKind kind) {
  if (domain == SurfaceDomain::Subject) { return true; }
  return shading == ShadingArm::Lit && !textured && kind == SurfaceKind::Opaque;
}

inline constexpr FragmentArm kFragmentArmRows[kFragmentArms] = {
    {.Domain = SurfaceDomain::Subject,
     .Shading = ShadingArm::Flat,
     .Textured = false,
     .Kind = SurfaceKind::Opaque,
     .Entry = "fs"},
    {.Domain = SurfaceDomain::Subject,
     .Shading = ShadingArm::Flat,
     .Textured = false,
     .Kind = SurfaceKind::Masked,
     .Entry = "fsMasked"},
    {.Domain = SurfaceDomain::Subject,
     .Shading = ShadingArm::Flat,
     .Textured = false,
     .Kind = SurfaceKind::Blended,
     .Entry = "fsBlended"},
    {.Domain = SurfaceDomain::Subject,
     .Shading = ShadingArm::Flat,
     .Textured = false,
     .Kind = SurfaceKind::ThinTransmissive,
     .Entry = "fsTransmissive"},
    {.Domain = SurfaceDomain::Subject,
     .Shading = ShadingArm::Flat,
     .Textured = false,
     .Kind = SurfaceKind::Refractive,
     .Entry = "fsTransmissive"},

    {.Domain = SurfaceDomain::Subject,
     .Shading = ShadingArm::Flat,
     .Textured = true,
     .Kind = SurfaceKind::Opaque,
     .Entry = "fsTextured"},
    {.Domain = SurfaceDomain::Subject,
     .Shading = ShadingArm::Flat,
     .Textured = true,
     .Kind = SurfaceKind::Masked,
     .Entry = "fsMaskedTextured"},
    {.Domain = SurfaceDomain::Subject,
     .Shading = ShadingArm::Flat,
     .Textured = true,
     .Kind = SurfaceKind::Blended,
     .Entry = "fsBlendedTextured"},
    {.Domain = SurfaceDomain::Subject,
     .Shading = ShadingArm::Flat,
     .Textured = true,
     .Kind = SurfaceKind::ThinTransmissive,
     .Entry = "fsTransmissive"},
    {.Domain = SurfaceDomain::Subject,
     .Shading = ShadingArm::Flat,
     .Textured = true,
     .Kind = SurfaceKind::Refractive,
     .Entry = "fsTransmissive"},

    {.Domain = SurfaceDomain::Subject,
     .Shading = ShadingArm::Lit,
     .Textured = false,
     .Kind = SurfaceKind::Opaque,
     .Entry = "fsLit"},
    {.Domain = SurfaceDomain::Subject,
     .Shading = ShadingArm::Lit,
     .Textured = false,
     .Kind = SurfaceKind::Masked,
     .Entry = "fsLitMasked"},
    {.Domain = SurfaceDomain::Subject,
     .Shading = ShadingArm::Lit,
     .Textured = false,
     .Kind = SurfaceKind::Blended,
     .Entry = "fsLitBlended"},
    {.Domain = SurfaceDomain::Subject,
     .Shading = ShadingArm::Lit,
     .Textured = false,
     .Kind = SurfaceKind::ThinTransmissive,
     .Entry = "fsLitTransmissive"},
    {.Domain = SurfaceDomain::Subject,
     .Shading = ShadingArm::Lit,
     .Textured = false,
     .Kind = SurfaceKind::Refractive,
     .Entry = "fsLitTransmissive"},

    {.Domain = SurfaceDomain::Subject,
     .Shading = ShadingArm::Lit,
     .Textured = true,
     .Kind = SurfaceKind::Opaque,
     .Entry = "fsLitTextured"},
    {.Domain = SurfaceDomain::Subject,
     .Shading = ShadingArm::Lit,
     .Textured = true,
     .Kind = SurfaceKind::Masked,
     .Entry = "fsLitMaskedTextured"},
    {.Domain = SurfaceDomain::Subject,
     .Shading = ShadingArm::Lit,
     .Textured = true,
     .Kind = SurfaceKind::Blended,
     .Entry = "fsLitBlendedTextured"},
    {.Domain = SurfaceDomain::Subject,
     .Shading = ShadingArm::Lit,
     .Textured = true,
     .Kind = SurfaceKind::ThinTransmissive,
     .Entry = "fsLitTransmissiveTextured"},
    {.Domain = SurfaceDomain::Subject,
     .Shading = ShadingArm::Lit,
     .Textured = true,
     .Kind = SurfaceKind::Refractive,
     .Entry = "fsLitTransmissiveTextured"},

    {.Domain = SurfaceDomain::Subject,
     .Shading = ShadingArm::Mapped,
     .Textured = false,
     .Kind = SurfaceKind::Opaque,
     .Entry = "fsMapped"},
    {.Domain = SurfaceDomain::Subject,
     .Shading = ShadingArm::Mapped,
     .Textured = false,
     .Kind = SurfaceKind::Masked,
     .Entry = "fsMappedMasked"},
    {.Domain = SurfaceDomain::Subject,
     .Shading = ShadingArm::Mapped,
     .Textured = false,
     .Kind = SurfaceKind::Blended,
     .Entry = "fsMappedBlended"},
    {.Domain = SurfaceDomain::Subject,
     .Shading = ShadingArm::Mapped,
     .Textured = false,
     .Kind = SurfaceKind::ThinTransmissive,
     .Entry = "fsMappedTransmissive"},
    {.Domain = SurfaceDomain::Subject,
     .Shading = ShadingArm::Mapped,
     .Textured = false,
     .Kind = SurfaceKind::Refractive,
     .Entry = "fsMappedTransmissive"},

    {.Domain = SurfaceDomain::Subject,
     .Shading = ShadingArm::Mapped,
     .Textured = true,
     .Kind = SurfaceKind::Opaque,
     .Entry = "fsMapped"},
    {.Domain = SurfaceDomain::Subject,
     .Shading = ShadingArm::Mapped,
     .Textured = true,
     .Kind = SurfaceKind::Masked,
     .Entry = "fsMappedMasked"},
    {.Domain = SurfaceDomain::Subject,
     .Shading = ShadingArm::Mapped,
     .Textured = true,
     .Kind = SurfaceKind::Blended,
     .Entry = "fsMappedBlended"},
    {.Domain = SurfaceDomain::Subject,
     .Shading = ShadingArm::Mapped,
     .Textured = true,
     .Kind = SurfaceKind::ThinTransmissive,
     .Entry = "fsMappedTransmissive"},
    {.Domain = SurfaceDomain::Subject,
     .Shading = ShadingArm::Mapped,
     .Textured = true,
     .Kind = SurfaceKind::Refractive,
     .Entry = "fsMappedTransmissive"},

    {.Domain = SurfaceDomain::Ground,
     .Shading = ShadingArm::Flat,
     .Textured = false,
     .Kind = SurfaceKind::Opaque,
     .Entry = nullptr},
    {.Domain = SurfaceDomain::Ground,
     .Shading = ShadingArm::Flat,
     .Textured = false,
     .Kind = SurfaceKind::Masked,
     .Entry = nullptr},
    {.Domain = SurfaceDomain::Ground,
     .Shading = ShadingArm::Flat,
     .Textured = false,
     .Kind = SurfaceKind::Blended,
     .Entry = nullptr},
    {.Domain = SurfaceDomain::Ground,
     .Shading = ShadingArm::Flat,
     .Textured = false,
     .Kind = SurfaceKind::ThinTransmissive,
     .Entry = nullptr},
    {.Domain = SurfaceDomain::Ground,
     .Shading = ShadingArm::Flat,
     .Textured = false,
     .Kind = SurfaceKind::Refractive,
     .Entry = nullptr},

    {.Domain = SurfaceDomain::Ground,
     .Shading = ShadingArm::Flat,
     .Textured = true,
     .Kind = SurfaceKind::Opaque,
     .Entry = nullptr},
    {.Domain = SurfaceDomain::Ground,
     .Shading = ShadingArm::Flat,
     .Textured = true,
     .Kind = SurfaceKind::Masked,
     .Entry = nullptr},
    {.Domain = SurfaceDomain::Ground,
     .Shading = ShadingArm::Flat,
     .Textured = true,
     .Kind = SurfaceKind::Blended,
     .Entry = nullptr},
    {.Domain = SurfaceDomain::Ground,
     .Shading = ShadingArm::Flat,
     .Textured = true,
     .Kind = SurfaceKind::ThinTransmissive,
     .Entry = nullptr},
    {.Domain = SurfaceDomain::Ground,
     .Shading = ShadingArm::Flat,
     .Textured = true,
     .Kind = SurfaceKind::Refractive,
     .Entry = nullptr},

    {.Domain = SurfaceDomain::Ground,
     .Shading = ShadingArm::Lit,
     .Textured = false,
     .Kind = SurfaceKind::Opaque,
     .Entry = "fsGroundLit"},
    {.Domain = SurfaceDomain::Ground,
     .Shading = ShadingArm::Lit,
     .Textured = false,
     .Kind = SurfaceKind::Masked,
     .Entry = nullptr},
    {.Domain = SurfaceDomain::Ground,
     .Shading = ShadingArm::Lit,
     .Textured = false,
     .Kind = SurfaceKind::Blended,
     .Entry = nullptr},
    {.Domain = SurfaceDomain::Ground,
     .Shading = ShadingArm::Lit,
     .Textured = false,
     .Kind = SurfaceKind::ThinTransmissive,
     .Entry = nullptr},
    {.Domain = SurfaceDomain::Ground,
     .Shading = ShadingArm::Lit,
     .Textured = false,
     .Kind = SurfaceKind::Refractive,
     .Entry = nullptr},

    {.Domain = SurfaceDomain::Ground,
     .Shading = ShadingArm::Lit,
     .Textured = true,
     .Kind = SurfaceKind::Opaque,
     .Entry = nullptr},
    {.Domain = SurfaceDomain::Ground,
     .Shading = ShadingArm::Lit,
     .Textured = true,
     .Kind = SurfaceKind::Masked,
     .Entry = nullptr},
    {.Domain = SurfaceDomain::Ground,
     .Shading = ShadingArm::Lit,
     .Textured = true,
     .Kind = SurfaceKind::Blended,
     .Entry = nullptr},
    {.Domain = SurfaceDomain::Ground,
     .Shading = ShadingArm::Lit,
     .Textured = true,
     .Kind = SurfaceKind::ThinTransmissive,
     .Entry = nullptr},
    {.Domain = SurfaceDomain::Ground,
     .Shading = ShadingArm::Lit,
     .Textured = true,
     .Kind = SurfaceKind::Refractive,
     .Entry = nullptr},

    {.Domain = SurfaceDomain::Ground,
     .Shading = ShadingArm::Mapped,
     .Textured = false,
     .Kind = SurfaceKind::Opaque,
     .Entry = nullptr},
    {.Domain = SurfaceDomain::Ground,
     .Shading = ShadingArm::Mapped,
     .Textured = false,
     .Kind = SurfaceKind::Masked,
     .Entry = nullptr},
    {.Domain = SurfaceDomain::Ground,
     .Shading = ShadingArm::Mapped,
     .Textured = false,
     .Kind = SurfaceKind::Blended,
     .Entry = nullptr},
    {.Domain = SurfaceDomain::Ground,
     .Shading = ShadingArm::Mapped,
     .Textured = false,
     .Kind = SurfaceKind::ThinTransmissive,
     .Entry = nullptr},
    {.Domain = SurfaceDomain::Ground,
     .Shading = ShadingArm::Mapped,
     .Textured = false,
     .Kind = SurfaceKind::Refractive,
     .Entry = nullptr},

    {.Domain = SurfaceDomain::Ground,
     .Shading = ShadingArm::Mapped,
     .Textured = true,
     .Kind = SurfaceKind::Opaque,
     .Entry = nullptr},
    {.Domain = SurfaceDomain::Ground,
     .Shading = ShadingArm::Mapped,
     .Textured = true,
     .Kind = SurfaceKind::Masked,
     .Entry = nullptr},
    {.Domain = SurfaceDomain::Ground,
     .Shading = ShadingArm::Mapped,
     .Textured = true,
     .Kind = SurfaceKind::Blended,
     .Entry = nullptr},
    {.Domain = SurfaceDomain::Ground,
     .Shading = ShadingArm::Mapped,
     .Textured = true,
     .Kind = SurfaceKind::ThinTransmissive,
     .Entry = nullptr},
    {.Domain = SurfaceDomain::Ground,
     .Shading = ShadingArm::Mapped,
     .Textured = true,
     .Kind = SurfaceKind::Refractive,
     .Entry = nullptr},
};

constexpr bool EveryFragmentArmIsAtItsOwnIndex() {
  for (std::size_t at = 0; at < kFragmentArms; ++at) {
    const FragmentArm &one = kFragmentArmRows[at];
    if (FragmentArmAt(one.Domain, one.Shading, one.Textured, one.Kind) != at) { return false; }
    if (DomainPresents(one.Domain, one.Shading, one.Textured, one.Kind) != (one.Entry != nullptr)) {
      return false;
    }
  }
  return true;
}

static_assert(EveryFragmentArmIsAtItsOwnIndex(),
              "a fragment arm's index is derived from its own axes, so the table is total over "
              "them and no combination falls through to a default -- and a row carries an entry "
              "exactly when its domain presents it");

[[nodiscard]] constexpr ShadingArm ShadingArmOf(VertexLayout layout) {
  if (CarriesTangent(layout)) { return ShadingArm::Mapped; }
  if (CarriesNormal(layout)) { return ShadingArm::Lit; }
  return ShadingArm::Flat;
}

[[nodiscard]] constexpr const char *
FragmentArmNamed(SurfaceDomain domain, ShadingArm shading, bool textured, SurfaceKind kind) {
  return kFragmentArmRows[FragmentArmAt(domain, shading, textured, kind)].Entry;
}

} // namespace outshine::Render
#endif
