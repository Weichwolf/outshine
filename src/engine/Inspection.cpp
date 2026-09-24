#include "EngineHeld.h"
#include "Heap.h"
#include "RuntimeScene.h"
#include "math/Units.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace outshine {
namespace {
constexpr double kInvalidVelocityThresholdNdc = -1.0e3;

void InspectShadow(Seen &picture, Core::Ledger &published) {
  std::vector<float> depth;
  if (picture.Device.ReadShadowAtlas(depth) == Render::ReadState::Ready) {
    double least = kBeyondAnyCoordinate;
    double most = -kBeyondAnyCoordinate;
    double written = 0.0;
    for (const float one : depth) {
      least = std::min(static_cast<double>(one), least);
      most = std::max(static_cast<double>(one), most);
      if (one > 0.0f) { written += 1.0; }
    }
    published.Places("the shadow atlas, least depth", least, "");
    published.Places("the shadow atlas, most depth", most, "");
    published.Places("texels above the clear", written, "texels");
    published.Places(
        "the shadow radius it stood on", picture.Standing->ShadowRadiusStanding(), "m");
  }
}

void InspectCulling(Seen &picture, Core::Ledger &published) {
  Render::KeptDraws kept;
  if (picture.Device.ReadKeptIndices(kept) == Render::ReadState::Ready) {
    published.Places(
        "cull: indices the subject cull kept", static_cast<double>(kept.Indices), "indices");
    published.Places("cull: batches that kept any", static_cast<double>(kept.Batches), "batches");
  }
}

void InspectIrradiance(Seen &picture, Core::Ledger &published) {
  std::array<float, Render::kIrradianceFloats> held = {{}};
  if (picture.Device.ReadSkyIrradiance(held) == Render::ReadState::Ready) {
    picture.Standing->ReadIrradiance(held);
    {
      static const std::array<const char *const, 3> kSky = {"the ambient the sky casts, red",
                                                            "the ambient the sky casts, green",
                                                            "the ambient the sky casts, blue"};
      static const std::array<const char *const, 3> kGround = {
          "the ambient the ground bounces, red",
          "the ambient the ground bounces, green",
          "the ambient the ground bounces, blue"};
      for (size_t at = 0; at < 3; ++at) {
        published.Places(kSky[at], picture.Standing->AmbientStood()[at], "");
        published.Places(kGround[at], picture.Standing->GroundStood()[at], "");
      }
    }
    static const std::array<const char *const, Render::kIrradianceFloats> kNamed = {
        "the device's sky irradiance, red",
        "the device's sky irradiance, green",
        "the device's sky irradiance, blue",
        "the device's transmittance toward the sun, red",
        "the device's transmittance toward the sun, green",
        "the device's transmittance toward the sun, blue"};
    for (size_t at = 0; at < Render::kIrradianceFloats; ++at) {
      published.Places(kNamed[at], static_cast<double>(held[at]), "");
    }
  }
}

void InspectVelocity(Seen &picture, Core::Ledger &published) {
  std::vector<float> velocity;
  if (picture.Device.ReadSceneVelocity(velocity) == Render::ReadState::Ready) {
    double moving = 0.0;
    double furthest = 0.0;
    for (size_t at = 0; at + 1 < velocity.size(); at += 2) {
      const auto across = static_cast<double>(velocity[at]);
      const auto down = static_cast<double>(velocity[at + 1]);
      if (across <= kInvalidVelocityThresholdNdc || down <= kInvalidVelocityThresholdNdc) {
        continue;
      }
      const double moved = std::sqrt(across * across + down * down);
      if (moved > 0.0) { moving += 1.0; }
      furthest = std::max(moved, furthest);
    }
    published.Places("pixels the velocity target says moved", moving, "px");
    published.Places("the furthest any of them moved", furthest, "ndc");
  }
}

void InspectLinearColour(Seen &picture, Core::Ledger &published) {
  std::vector<float> linear;
  if (picture.Device.ReadSceneLinear(linear) == Render::ReadState::Ready) {
    double brightest = 0.0;
    for (size_t at = 0; at + 3 < linear.size(); at += 4) {
      for (int channel = 0; channel < 3; ++channel) {
        brightest = static_cast<double>(linear[at + channel]) > brightest
                        ? static_cast<double>(linear[at + channel])
                        : brightest;
      }
    }
    published.Places("the brightest the scene's linear buffer reached", brightest, "");
  }
}

void InspectPresentedColour(Seen &picture, Core::Ledger &published) {
  std::vector<uint8_t> shown;
  if (picture.Device.ReadPixels(shown) == Render::ReadState::Ready) {
    double peak = 0.0;
    for (size_t at = 0; at + 3 < shown.size(); at += 4) {
      for (int channel = 0; channel < 3; ++channel) {
        peak = static_cast<double>(shown[at + channel]) > peak
                   ? static_cast<double>(shown[at + channel])
                   : peak;
      }
    }
    published.Places("the brightest the presented frame shows", peak, "of 255");
  }
}

}

void Engine::State::Inspected() {
  if (!Picture.Standing) { return; }
  static const Heap::Tag kFrameMeasurementsTag("frame-measures");
  const Heap::Tagged measuring(kFrameMeasurementsTag);
  InspectShadow(Picture, Published);
  InspectCulling(Picture, Published);
  InspectIrradiance(Picture, Published);
  InspectVelocity(Picture, Published);
  Published.Places("the exposure the picture applied",
                   static_cast<double>(Picture.Device.ExposureApplied()),
                   "1/(cd/m2)");
  InspectLinearColour(Picture, Published);
  InspectPresentedColour(Picture, Published);
}
}
