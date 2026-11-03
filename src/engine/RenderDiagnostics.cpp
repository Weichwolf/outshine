#include "TerrainLattice.h"
#include "EngineState.h"
#include "Heap.h"
#include "RuntimeScene.h"
#include "Units.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace outshine {
namespace {
constexpr double kInvalidVelocityThresholdNdc = -1.0e3;

void InspectShadow(EngineRenderState &picture, Core::DiagnosticLedger &published) {
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
    published.RecordMetric("the shadow atlas, least depth", least, "");
    published.RecordMetric("the shadow atlas, most depth", most, "");
    published.RecordMetric("texels above the clear", written, "texels");
    published.RecordMetric(
        "the shadow radius it stood on", picture.Standing->ShadowRadiusStanding(), "m");
  }
}

void InspectCulling(EngineRenderState &picture, Core::DiagnosticLedger &published) {
  Render::KeptDraws kept;
  const bool available = picture.Device.ReadGeometrySelection(
                             kept, Render::GeometryReadback::Counters) == Render::ReadState::Ready;
  published.RecordMetric("cull snapshot: available", available ? 1.0 : 0.0, "");
  if (available) {
    published.RecordMetric(
        "cull: indices the geometry cull kept", static_cast<double>(kept.Indices), "indices");
    published.RecordMetric(
        "cull: batches that kept any", static_cast<double>(kept.Batches), "batches");
    const std::array values{
        std::pair{"root pass visits", kept.RootWork.Visited},
        std::pair{"expanded roots", kept.RootWork.Accepted},
        std::pair{"chunk queue requests", kept.RootWork.Requested},
        std::pair{"chunk queue overflow", kept.RootWork.Overflow},
        std::pair{"logical clusters under kept roots", kept.RootWork.LogicalWork},
        std::pair{"leaf visits", kept.ClusterWork.Visited},
        std::pair{"selected clusters", kept.ClusterWork.Accepted},
        std::pair{"cluster queue requests", kept.ClusterWork.Requested},
        std::pair{"cluster queue overflow", kept.ClusterWork.Overflow},
        std::pair{"selected index references", kept.ClusterWork.LogicalWork}};
    for (const auto &[name, value] : values) {
      published.RecordMetric("cull snapshot: " + std::string(name), static_cast<double>(value), "");
    }
    published.RecordMetric(
        "cull snapshot: readback payload", static_cast<double>(kept.ReadbackBytes), "bytes");
  }
}

void InspectGround(const EngineRenderState &picture, Core::DiagnosticLedger &published) {
  const auto ground = picture.Device.ReadGroundDrawArguments();
  published.RecordMetric("ground: draw arguments available", ground.has_value() ? 1 : 0, "");
  if (!ground) { return; }
  static constexpr std::array patchNames{"ground: LOD0 patch instances",
                                         "ground: LOD1 patch instances",
                                         "ground: LOD2 patch instances",
                                         "ground: LOD3 patch instances"};
  static constexpr std::array indexNames{"ground: LOD0 index references",
                                         "ground: LOD1 index references",
                                         "ground: LOD2 index references",
                                         "ground: LOD3 index references"};
  uint64_t indices = 0;
  for (size_t i = 0; i < ground->Patches.size(); ++i) {
    const auto &row = ground->Patches[i];
    const auto referenced = uint64_t{row.IndexCount} * row.InstanceCount;
    published.RecordMetric(patchNames[i], row.InstanceCount, "instances");
    published.RecordMetric(indexNames[i], static_cast<double>(referenced), "indices");
    indices += referenced;
  }
  const auto skirts = uint64_t{Render::TerrainLattice::kSkirtQuads} * 6u * ground->SkirtInstances;
  published.RecordMetric("ground: skirt instances", ground->SkirtInstances, "instances");
  published.RecordMetric("ground: skirt index references", static_cast<double>(skirts), "indices");
  published.RecordMetric(
      "ground: colour draw triangles", static_cast<double>(indices + skirts) / 3.0, "triangles");
}

void InspectIrradiance(EngineRenderState &picture, Core::DiagnosticLedger &published) {
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
        published.RecordMetric(kSky[at], picture.Standing->AmbientStood()[at], "");
        published.RecordMetric(kGround[at], picture.Standing->GroundStood()[at], "");
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
      published.RecordMetric(kNamed[at], static_cast<double>(held[at]), "");
    }
  }
}

void InspectVelocity(EngineRenderState &picture, Core::DiagnosticLedger &published) {
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
    published.RecordMetric("pixels the velocity target says moved", moving, "px");
    published.RecordMetric("the furthest any of them moved", furthest, "ndc");
  }
}

void InspectLinearColour(EngineRenderState &picture, Core::DiagnosticLedger &published) {
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
    published.RecordMetric("the brightest the scene's linear buffer reached", brightest, "");
  }
}

void InspectDepth(EngineRenderState &picture, Core::DiagnosticLedger &published) {
  std::vector<float> depth;
  if (picture.Device.ReadDepth(depth) != Render::ReadState::Ready) { return; }
  const auto covered = std::ranges::count_if(depth, [](float value) { return value > 0.0f; });
  published.RecordMetric("render: pixels covered by geometry", static_cast<double>(covered), "px");
}

void InspectPresentedColour(EngineRenderState &picture, Core::DiagnosticLedger &published) {
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
    published.RecordMetric("the brightest the presented frame shows", peak, "of 255");
  }
}

}

void Engine::State::Inspected() {
  if (!Picture.Standing) { return; }
  static const Heap::Tag kFrameMeasurementsTag("frame-measures");
  const Heap::Tagged measuring(kFrameMeasurementsTag);
  const uint64_t structures = World.Pieces.Digest();
  size_t structureTiles = 0;
  World.Pieces.ForEachDigest(
      [&structureTiles](Render::GeometryPageResidency::DigestRecord) { ++structureTiles; });
  Published.RecordMetric(
      "world: live structure digest high half", static_cast<double>(structures >> 32u), "digest");
  Published.RecordMetric("world: live structure digest low half",
                         static_cast<double>(static_cast<uint32_t>(structures)),
                         "digest");
  Published.RecordMetric(
      "world: live structure tiles", static_cast<double>(structureTiles), "tiles");
  Published.RecordMetric(
      "render: live native pieces", static_cast<double>(Picture.Device.PiecesStanding()), "pieces");
  Published.RecordMetric("render: live piece sources",
                         static_cast<double>(Picture.Device.PieceSourceCount()),
                         "pieces");
  Published.RecordMetric("render: live native triangles",
                         static_cast<double>(Picture.Device.PieceTriangles()),
                         "triangles");
  InspectShadow(Picture, Published);
  InspectCulling(Picture, Published);
  InspectGround(Picture, Published);
  InspectIrradiance(Picture, Published);
  InspectVelocity(Picture, Published);
  Published.RecordMetric("the exposure the picture applied",
                         static_cast<double>(Picture.Device.ExposureApplied()),
                         "1/(cd/m2)");
  InspectLinearColour(Picture, Published);
  InspectDepth(Picture, Published);
  InspectPresentedColour(Picture, Published);
}
}
