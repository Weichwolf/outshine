#include "SubjectDraw.h"
#include "SubjectLightingUniform.h"
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>

namespace outshine::Render {
bool SubjectDraw::SetLights(std::span<const SubjectLight> lights, std::string &error) {
  if (lights.size() > kMaxSubjectLights && Binding_ != nullptr) {
    const auto prepared = PrepareSpatialLights();
    if (!prepared) {
      error = prepared.error().Message;
      return false;
    }
  }
  if (!Lights_) { Lights_ = std::make_shared<SubjectLights>(); }
  if (Binding_ != nullptr && Binding().Device != nullptr) {
    const auto attached = Lights_->Attach(*Binding().Device, Binding().Profile.Uploads);
    if (!attached) {
      error = attached.error().Message;
      return false;
    }
  }
  const auto changed = Lights_->Set(lights);
  if (!changed) {
    error = changed.error().Message;
    return false;
  }
  if (Binding_ != nullptr) {
    Ground_.UseSpatialLights(SpatialLights() && !Binding().CapturesMaterial);
  }
  return true;
}

static_assert(static_cast<int>(LightKind::Directional) == 0 &&
                  static_cast<int>(LightKind::Point) == 1 && static_cast<int>(LightKind::Spot) == 2,
              "the shader reads the kind as the enum's own number, so reordering LightKind moves "
              "every light to another shape");

namespace {
Vec4f LightVector(const auto &source, float tail = 0) {
  return {{static_cast<float>(source[0]),
           static_cast<float>(source[1]),
           static_cast<float>(source[2]),
           tail}};
}
}

std::array<float, SubjectDraw::kLightFloats>
SubjectDraw::PackedLights(const FrameContext &ctx) const {
  SubjectLightingUniform packed{};
  const auto Placed = PlacedLights();
  const bool shadowSource = Shadowed_ && ShadowLightIndex_ < Placed.size() &&
                            Placed[ShadowLightIndex_].Light.Kind == LightKind::Directional;
  float shadowOwner = 0;
  if (shadowSource) {
    const auto id = static_cast<uint32_t>(ShadowLightIndex_ + 1);
    shadowOwner = SpatialLights() ? std::bit_cast<float>(id) : static_cast<float>(id);
  }
  packed.Count = {
      {static_cast<float>(Placed.size()), 0, shadowOwner, static_cast<float>(ShadowRegionCount_)}};
  packed.ShadowRegions = ShadowRegions_;
  packed.Environment =
      LightVector(IndirectLight.RadianceLinear,
                  Reflections_.Atlas != nullptr ? static_cast<float>(IndirectLight.SkyLux) : 0.0f);
  packed.Bounced = LightVector(IndirectLight.GroundLinear);
  packed.Up = LightVector(IndirectLight.UpUnit);
  packed.SkyGround =
      LightVector(IndirectLight.GroundAlbedo, static_cast<float>(IndirectLight.CosSunZenith));
  packed.ViewPosition = ctx.ViewPosition;
  packed.SkyUp = LightVector(Reflections_.Up);
  packed.SkyToSun = LightVector(Reflections_.ToSun);
  for (size_t at = 0; !SpatialLights() && at < Placed.size(); ++at) {
    const PunctualLight &light = Placed[at].Light;
    auto &entry = packed.Items[at];
    entry.Tint = LightVector(light.Colour, static_cast<float>(light.Kind));
    for (int channel = 0; channel < 3; ++channel) { entry.Tint[channel] *= light.Intensity; }
    for (int axis = 0; axis < 3; ++axis) {
      entry.Place[axis] =
          static_cast<float>(Placed[at].PositionEcefM[axis] + ctx.PreViewTranslation[axis]);
    }
    entry.Place[3] = light.RangeM > 0.0f ? 1.0f / light.RangeM : 0.0f;
    entry.Beam = LightVector(light.Direction);
    const float outer = std::cos(light.OuterConeRad);
    const float inner = std::cos(light.InnerConeRad);
    entry.Cone = {{outer, inner > outer ? 1.0f / (inner - outer) : 0.0f, 0, 0}};
  }
  return std::bit_cast<std::array<float, kLightFloats>>(packed);
}

}
