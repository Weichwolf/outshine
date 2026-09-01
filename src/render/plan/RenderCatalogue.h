#ifndef OUTSHINE_RENDER_PLAN_RENDERCATALOGUE_H
#define OUTSHINE_RENDER_PLAN_RENDERCATALOGUE_H

#include <cstddef>
#include <cstdint>

#include <SDL3/SDL_gpu.h>

namespace outshine::Render {

enum class Resource {
  LinearSampler,
  LutSampler,
  AtmosphereUniform,
  CascadeUniform,
  VegetationTable,
  TransmittanceLut,
  MultiScatterLut,
  SkyViewLut,
  IrradianceBuffer,
  Meter,
  ShadowAtlas,
  SceneHdr,
  SceneVelocity,
  SceneDepth,
  SceneShadingNormal,
  SceneSurfaceIdentity,
  SceneTransmissive,
  SceneComposited,
  SceneAerial,
  AoBuffer,
  SceneLinear,
  DepthPyramid,

  OverlayAtlas,
  FrameTex,
  Surface,

  ClusterSphere,
  ClusterIndex,
  ClusterJobs,
  ClusterBatches,
  ClusterKept,
  ClusterSlot,
  DrawIndex,
  DrawArguments,
  kCount
};

[[nodiscard]] constexpr bool CarriesSceneRadiance(Resource resource) {
  switch (resource) {
    case Resource::SceneHdr:
    case Resource::SceneTransmissive:
    case Resource::SceneAerial:
    case Resource::SceneComposited:
    case Resource::SceneLinear: return true;
    case Resource::LinearSampler:
    case Resource::LutSampler:
    case Resource::AtmosphereUniform:
    case Resource::CascadeUniform:
    case Resource::VegetationTable:
    case Resource::TransmittanceLut:
    case Resource::MultiScatterLut:
    case Resource::SkyViewLut:
    case Resource::IrradianceBuffer:
    case Resource::Meter:
    case Resource::ShadowAtlas:
    case Resource::SceneVelocity:
    case Resource::SceneDepth:
    case Resource::SceneShadingNormal:
    case Resource::SceneSurfaceIdentity:
    case Resource::AoBuffer:
    case Resource::DepthPyramid:

    case Resource::OverlayAtlas:
    case Resource::FrameTex:
    case Resource::Surface:
    case Resource::ClusterSphere:
    case Resource::ClusterIndex:
    case Resource::ClusterJobs:
    case Resource::ClusterBatches:
    case Resource::ClusterKept:
    case Resource::ClusterSlot:
    case Resource::DrawIndex:
    case Resource::DrawArguments:
    case Resource::kCount: return false;
  }
  return false;
}

enum class Stage {
  MediumTransmittance,
  MediumMultiScatter,
  MediumRadiance,
  Irradiance,
  AutoExposure,

  SubjectCull,
  SubjectScan,
  SubjectCompact,
  LightVisibility,
  Sky,
  Sun,
  Moon,
  Stars,
  Subjects,
  SubjectsTransmissive,
  CompositeTransmission,
  AerialPerspective,
  AmbientOcclusion,
  DepthPyramid,
  TemporalResolve,
  Tonemap,
  Overlay,
  Present,
  kCount
};

enum class ResourceKind { Given, Derived, Attachment };

enum class Provenance { Machinery, Content };

enum class PassKind { Compute, Raster };

enum class FallbackKind { None, Alias, Neutral };

enum class TexelFormat {
  Handle,
  Table,
  Rgba16Float,
  Rgba32Float,
  Rg16Float,
  R8Unorm,
  Rgba8UnormSrgb,
  Depth32Float
};

inline constexpr size_t kMaxEdges = 8;

inline constexpr size_t kMaxColourAttachments = 8;

struct ResourceRow {
  Resource Id;
  ResourceKind Kind;
  FallbackKind Fallback;
  Resource AliasOf;
  TexelFormat Format;
  const char *Name;

  uint32_t Stride = 0;
};

inline constexpr Resource kNoEdge = Resource::kCount;

struct StageRow {
  Stage Id;
  Provenance From;
  PassKind Kind;
  const char *Name;

  Resource Reads[kMaxEdges];
  Resource Writes[kMaxEdges];
  Resource Contributes[kMaxEdges];

  Stage FusesInto;

  Resource ReadsLastFrame[kMaxEdges] = {
      kNoEdge, kNoEdge, kNoEdge, kNoEdge, kNoEdge, kNoEdge, kNoEdge, kNoEdge};
};

inline constexpr Stage kNoFusion = Stage::kCount;

class AttachmentSet {
public:
  [[nodiscard]] constexpr bool Add(Resource resource) {
    for (size_t i = 0; i < Count_; ++i) {
      if (Items_[i] == resource) { return true; }
    }
    if (Count_ == kMaxColourAttachments) { return false; }
    Items_[Count_++] = resource;
    return true;
  }

  [[nodiscard]] constexpr size_t Size() const { return Count_; }

  [[nodiscard]] constexpr bool Empty() const { return Count_ == 0; }

  [[nodiscard]] constexpr const Resource *begin() const { return Items_; }

  [[nodiscard]] constexpr const Resource *end() const { return Items_ + Count_; }

private:
  Resource Items_[kMaxColourAttachments] = {};
  size_t Count_ = 0;
};

inline constexpr ResourceRow kResources[] = {
    {.Id = Resource::LinearSampler,
     .Kind = ResourceKind::Given,
     .Fallback = FallbackKind::None,
     .AliasOf = kNoEdge,
     .Format = TexelFormat::Handle,
     .Name = "linearSampler"},
    {.Id = Resource::LutSampler,
     .Kind = ResourceKind::Given,
     .Fallback = FallbackKind::None,
     .AliasOf = kNoEdge,
     .Format = TexelFormat::Handle,
     .Name = "lutSampler"},
    {.Id = Resource::AtmosphereUniform,
     .Kind = ResourceKind::Given,
     .Fallback = FallbackKind::None,
     .AliasOf = kNoEdge,
     .Format = TexelFormat::Handle,
     .Name = "atmosphere"},
    {.Id = Resource::CascadeUniform,
     .Kind = ResourceKind::Given,
     .Fallback = FallbackKind::None,
     .AliasOf = kNoEdge,
     .Format = TexelFormat::Handle,
     .Name = "cascades"},
    {.Id = Resource::VegetationTable,
     .Kind = ResourceKind::Given,
     .Fallback = FallbackKind::None,
     .AliasOf = kNoEdge,
     .Format = TexelFormat::Handle,
     .Name = "vegetation"},
    {.Id = Resource::TransmittanceLut,
     .Kind = ResourceKind::Derived,
     .Fallback = FallbackKind::None,
     .AliasOf = kNoEdge,
     .Format = TexelFormat::Rgba16Float,
     .Name = "transmittanceLut"},
    {.Id = Resource::MultiScatterLut,
     .Kind = ResourceKind::Derived,
     .Fallback = FallbackKind::None,
     .AliasOf = kNoEdge,
     .Format = TexelFormat::Rgba16Float,
     .Name = "multiScatterLut"},
    {.Id = Resource::SkyViewLut,
     .Kind = ResourceKind::Derived,
     .Fallback = FallbackKind::None,
     .AliasOf = kNoEdge,
     .Format = TexelFormat::Rgba16Float,
     .Name = "skyViewLut"},
    {.Id = Resource::IrradianceBuffer,
     .Kind = ResourceKind::Derived,
     .Fallback = FallbackKind::None,
     .AliasOf = kNoEdge,
     .Format = TexelFormat::Handle,
     .Name = "irradiance"},

    {.Id = Resource::Meter,
     .Kind = ResourceKind::Derived,
     .Fallback = FallbackKind::Neutral,
     .AliasOf = kNoEdge,
     .Format = TexelFormat::Handle,
     .Name = "meter"},

    {.Id = Resource::ShadowAtlas,
     .Kind = ResourceKind::Attachment,
     .Fallback = FallbackKind::Neutral,
     .AliasOf = kNoEdge,
     .Format = TexelFormat::Depth32Float,
     .Name = "shadowAtlas"},
    {.Id = Resource::SceneHdr,
     .Kind = ResourceKind::Attachment,
     .Fallback = FallbackKind::None,
     .AliasOf = kNoEdge,
     .Format = TexelFormat::Rgba16Float,
     .Name = "sceneHdr"},
    {.Id = Resource::SceneVelocity,
     .Kind = ResourceKind::Attachment,
     .Fallback = FallbackKind::None,
     .AliasOf = kNoEdge,
     .Format = TexelFormat::Rg16Float,
     .Name = "sceneVelocity"},
    {.Id = Resource::SceneDepth,
     .Kind = ResourceKind::Attachment,
     .Fallback = FallbackKind::None,
     .AliasOf = kNoEdge,
     .Format = TexelFormat::Depth32Float,
     .Name = "sceneDepth"},

    {.Id = Resource::SceneShadingNormal,
     .Kind = ResourceKind::Attachment,
     .Fallback = FallbackKind::None,
     .AliasOf = kNoEdge,
     .Format = TexelFormat::Rgba16Float,
     .Name = "sceneShadingNormal"},

    {.Id = Resource::SceneSurfaceIdentity,
     .Kind = ResourceKind::Attachment,
     .Fallback = FallbackKind::None,
     .AliasOf = kNoEdge,
     .Format = TexelFormat::Rgba32Float,
     .Name = "sceneSurfaceIdentity"},

    {.Id = Resource::SceneTransmissive,
     .Kind = ResourceKind::Attachment,
     .Fallback = FallbackKind::None,
     .AliasOf = kNoEdge,
     .Format = TexelFormat::Rgba16Float,
     .Name = "sceneTransmissive"},

    {.Id = Resource::SceneComposited,
     .Kind = ResourceKind::Derived,
     .Fallback = FallbackKind::Alias,
     .AliasOf = Resource::SceneHdr,
     .Format = TexelFormat::Rgba16Float,
     .Name = "sceneComposited"},
    {.Id = Resource::SceneAerial,
     .Kind = ResourceKind::Derived,
     .Fallback = FallbackKind::Alias,
     .AliasOf = Resource::SceneComposited,
     .Format = TexelFormat::Rgba16Float,
     .Name = "sceneAerial"},
    {.Id = Resource::AoBuffer,
     .Kind = ResourceKind::Attachment,
     .Fallback = FallbackKind::Neutral,
     .AliasOf = kNoEdge,
     .Format = TexelFormat::R8Unorm,
     .Name = "aoBuffer"},

    {.Id = Resource::SceneLinear,
     .Kind = ResourceKind::Derived,
     .Fallback = FallbackKind::Alias,
     .AliasOf = Resource::SceneAerial,
     .Format = TexelFormat::Rgba16Float,
     .Name = "sceneLinear"},
    {.Id = Resource::DepthPyramid,
     .Kind = ResourceKind::Derived,
     .Fallback = FallbackKind::None,
     .AliasOf = kNoEdge,
     .Format = TexelFormat::Table,
     .Name = "depthPyramid",
     .Stride = static_cast<uint32_t>(sizeof(float))},
    {.Id = Resource::OverlayAtlas,
     .Kind = ResourceKind::Given,
     .Fallback = FallbackKind::None,
     .AliasOf = kNoEdge,
     .Format = TexelFormat::Handle,
     .Name = "overlayAtlas"},
    {.Id = Resource::FrameTex,
     .Kind = ResourceKind::Attachment,
     .Fallback = FallbackKind::None,
     .AliasOf = kNoEdge,
     .Format = TexelFormat::Rgba8UnormSrgb,
     .Name = "frameTex"},
    {.Id = Resource::Surface,
     .Kind = ResourceKind::Attachment,
     .Fallback = FallbackKind::None,
     .AliasOf = kNoEdge,
     .Format = TexelFormat::Rgba8UnormSrgb,
     .Name = "surface"},

    {.Id = Resource::ClusterSphere,
     .Kind = ResourceKind::Given,
     .Fallback = FallbackKind::None,
     .AliasOf = kNoEdge,
     .Format = TexelFormat::Table,
     .Name = "clusterSphere",
     .Stride = 4u * static_cast<uint32_t>(sizeof(float))},
    {.Id = Resource::ClusterIndex,
     .Kind = ResourceKind::Given,
     .Fallback = FallbackKind::None,
     .AliasOf = kNoEdge,
     .Format = TexelFormat::Table,
     .Name = "clusterIndex",
     .Stride = static_cast<uint32_t>(sizeof(uint32_t))},
    {.Id = Resource::ClusterJobs,
     .Kind = ResourceKind::Given,
     .Fallback = FallbackKind::None,
     .AliasOf = kNoEdge,
     .Format = TexelFormat::Table,
     .Name = "clusterJobs",
     .Stride = 4u * static_cast<uint32_t>(sizeof(uint32_t))},
    {.Id = Resource::ClusterBatches,
     .Kind = ResourceKind::Given,
     .Fallback = FallbackKind::None,
     .AliasOf = kNoEdge,
     .Format = TexelFormat::Table,
     .Name = "clusterBatches",
     .Stride = 2u * static_cast<uint32_t>(sizeof(uint32_t))},
    {.Id = Resource::ClusterKept,
     .Kind = ResourceKind::Derived,
     .Fallback = FallbackKind::None,
     .AliasOf = kNoEdge,
     .Format = TexelFormat::Table,
     .Name = "clusterKept",
     .Stride = static_cast<uint32_t>(sizeof(uint32_t))},
    {.Id = Resource::ClusterSlot,
     .Kind = ResourceKind::Derived,
     .Fallback = FallbackKind::None,
     .AliasOf = kNoEdge,
     .Format = TexelFormat::Table,
     .Name = "clusterSlot",
     .Stride = static_cast<uint32_t>(sizeof(uint32_t))},
    {.Id = Resource::DrawIndex,
     .Kind = ResourceKind::Derived,
     .Fallback = FallbackKind::None,
     .AliasOf = kNoEdge,
     .Format = TexelFormat::Table,
     .Name = "drawIndex",
     .Stride = static_cast<uint32_t>(sizeof(uint32_t))},
    {.Id = Resource::DrawArguments,
     .Kind = ResourceKind::Derived,
     .Fallback = FallbackKind::None,
     .AliasOf = kNoEdge,
     .Format = TexelFormat::Table,
     .Name = "drawArguments",
     .Stride = static_cast<uint32_t>(sizeof(SDL_GPUIndexedIndirectDrawCommand))},
};

[[nodiscard]] constexpr bool IsBuffer(const ResourceRow &row) {
  return row.Format == TexelFormat::Table;
}

[[nodiscard]] constexpr bool ElementsAreStated() {
  for (const ResourceRow &row : kResources) {
    if (IsBuffer(row) != (row.Stride > 0u)) { return false; }
  }
  return true;
}

static_assert(ElementsAreStated(),
              "a table states a stride and a picture states a texel format -- one row states the "
              "wrong one of the two");

inline constexpr StageRow kStages[] = {
    {.Id = Stage::MediumTransmittance,
     .From = Provenance::Machinery,
     .Kind = PassKind::Compute,
     .Name = "mediumTransmittance",
     .Reads = {kNoEdge},
     .Writes = {Resource::TransmittanceLut, kNoEdge},
     .Contributes = {kNoEdge},
     .FusesInto = kNoFusion},
    {.Id = Stage::MediumMultiScatter,
     .From = Provenance::Machinery,
     .Kind = PassKind::Compute,
     .Name = "mediumMultiScatter",
     .Reads = {Resource::TransmittanceLut, kNoEdge},
     .Writes = {Resource::MultiScatterLut, kNoEdge},
     .Contributes = {kNoEdge},
     .FusesInto = kNoFusion},
    {.Id = Stage::MediumRadiance,
     .From = Provenance::Machinery,
     .Kind = PassKind::Compute,
     .Name = "mediumRadiance",
     .Reads = {Resource::TransmittanceLut,
               Resource::MultiScatterLut,
               Resource::LutSampler,
               Resource::AtmosphereUniform,
               kNoEdge},
     .Writes = {Resource::SkyViewLut, kNoEdge},
     .Contributes = {kNoEdge},
     .FusesInto = kNoFusion},
    {.Id = Stage::Irradiance,
     .From = Provenance::Machinery,
     .Kind = PassKind::Compute,
     .Name = "irradiance",
     .Reads = {Resource::SkyViewLut,
               Resource::TransmittanceLut,
               Resource::LutSampler,
               Resource::AtmosphereUniform,
               kNoEdge},
     .Writes = {Resource::IrradianceBuffer, kNoEdge},
     .Contributes = {kNoEdge},
     .FusesInto = kNoFusion},
    {.Id = Stage::AutoExposure,
     .From = Provenance::Content,
     .Kind = PassKind::Compute,
     .Name = "autoExposure",
     .Reads = {Resource::IrradianceBuffer, kNoEdge},
     .Writes = {Resource::Meter, kNoEdge},
     .Contributes = {kNoEdge},
     .FusesInto = kNoFusion},
    {.Id = Stage::SubjectCull,
     .From = Provenance::Machinery,
     .Kind = PassKind::Compute,
     .Name = "subjectCull",
     .Reads = {Resource::ClusterSphere, Resource::ClusterJobs, kNoEdge},
     .Writes = {Resource::ClusterKept, kNoEdge},
     .Contributes = {kNoEdge},
     .FusesInto = kNoFusion,
     .ReadsLastFrame = {Resource::DepthPyramid, kNoEdge}},
    {.Id = Stage::SubjectScan,
     .From = Provenance::Machinery,
     .Kind = PassKind::Compute,
     .Name = "subjectScan",
     .Reads = {Resource::ClusterKept, Resource::ClusterBatches, kNoEdge},
     .Writes = {Resource::ClusterSlot, Resource::DrawArguments, kNoEdge},
     .Contributes = {kNoEdge},
     .FusesInto = kNoFusion},
    {.Id = Stage::SubjectCompact,
     .From = Provenance::Machinery,
     .Kind = PassKind::Compute,
     .Name = "subjectCompact",
     .Reads = {Resource::ClusterSlot,
               Resource::ClusterIndex,
               Resource::ClusterJobs,
               Resource::DrawArguments,
               kNoEdge},
     .Writes = {Resource::DrawIndex, kNoEdge},
     .Contributes = {kNoEdge},
     .FusesInto = kNoFusion},
    {.Id = Stage::LightVisibility,
     .From = Provenance::Content,
     .Kind = PassKind::Raster,
     .Name = "lightVisibility",
     .Reads = {kNoEdge},
     .Writes = {kNoEdge},
     .Contributes = {Resource::ShadowAtlas, kNoEdge},
     .FusesInto = kNoFusion},
    {.Id = Stage::Sky,
     .From = Provenance::Content,
     .Kind = PassKind::Raster,
     .Name = "sky",
     .Reads = {Resource::SkyViewLut, Resource::LutSampler, Resource::AtmosphereUniform, kNoEdge},
     .Writes = {kNoEdge},
     .Contributes = {Resource::SceneHdr, Resource::SceneVelocity, Resource::SceneDepth, kNoEdge},
     .FusesInto = kNoFusion},
    {.Id = Stage::Sun,
     .From = Provenance::Content,
     .Kind = PassKind::Raster,
     .Name = "sun",
     .Reads =
         {Resource::TransmittanceLut, Resource::LutSampler, Resource::AtmosphereUniform, kNoEdge},
     .Writes = {kNoEdge},
     .Contributes = {Resource::SceneHdr, Resource::SceneVelocity, Resource::SceneDepth, kNoEdge},
     .FusesInto = kNoFusion},
    {.Id = Stage::Moon,
     .From = Provenance::Content,
     .Kind = PassKind::Raster,
     .Name = "moon",
     .Reads = {Resource::LutSampler, Resource::AtmosphereUniform, kNoEdge},
     .Writes = {kNoEdge},
     .Contributes = {Resource::SceneHdr, Resource::SceneVelocity, Resource::SceneDepth, kNoEdge},
     .FusesInto = kNoFusion},
    {.Id = Stage::Stars,
     .From = Provenance::Content,
     .Kind = PassKind::Raster,
     .Name = "stars",
     .Reads = {kNoEdge},
     .Writes = {kNoEdge},
     .Contributes = {Resource::SceneHdr, Resource::SceneVelocity, Resource::SceneDepth, kNoEdge},
     .FusesInto = kNoFusion},
    {.Id = Stage::Subjects,
     .From = Provenance::Content,
     .Kind = PassKind::Raster,
     .Name = "subjects",
     .Reads = {Resource::ShadowAtlas,
               Resource::LutSampler,
               Resource::DrawIndex,
               Resource::DrawArguments,
               Resource::IrradianceBuffer,
               kNoEdge},
     .Writes = {kNoEdge},
     .Contributes = {Resource::SceneHdr,
                     Resource::SceneVelocity,
                     Resource::SceneDepth,
                     Resource::SceneShadingNormal,
                     Resource::SceneSurfaceIdentity,
                     kNoEdge},
     .FusesInto = kNoFusion},

    {.Id = Stage::SubjectsTransmissive,
     .From = Provenance::Content,
     .Kind = PassKind::Raster,
     .Name = "subjectsTransmissive",
     .Reads = {Resource::SceneHdr, Resource::LinearSampler, kNoEdge},
     .Writes = {kNoEdge},
     .Contributes =
         {Resource::SceneTransmissive, Resource::SceneVelocity, Resource::SceneDepth, kNoEdge},
     .FusesInto = kNoFusion},
    {.Id = Stage::CompositeTransmission,
     .From = Provenance::Content,
     .Kind = PassKind::Raster,
     .Name = "compositeTransmission",
     .Reads = {Resource::SceneHdr, Resource::SceneTransmissive, Resource::LinearSampler, kNoEdge},
     .Writes = {Resource::SceneComposited, kNoEdge},
     .Contributes = {kNoEdge},
     .FusesInto = kNoFusion},
    {.Id = Stage::AerialPerspective,
     .From = Provenance::Content,
     .Kind = PassKind::Raster,
     .Name = "aerialPerspective",
     .Reads = {Resource::SceneComposited,
               Resource::SceneDepth,
               Resource::SkyViewLut,
               Resource::TransmittanceLut,
               Resource::LinearSampler,
               Resource::LutSampler,
               Resource::AtmosphereUniform,
               kNoEdge},
     .Writes = {Resource::SceneAerial, kNoEdge},
     .Contributes = {kNoEdge},
     .FusesInto = kNoFusion},
    {.Id = Stage::AmbientOcclusion,
     .From = Provenance::Content,
     .Kind = PassKind::Raster,
     .Name = "ambientOcclusion",
     .Reads = {Resource::SceneDepth, Resource::AtmosphereUniform, kNoEdge},
     .Writes = {kNoEdge},
     .Contributes = {Resource::AoBuffer, kNoEdge},
     .FusesInto = kNoFusion},
    {.Id = Stage::DepthPyramid,
     .From = Provenance::Machinery,
     .Kind = PassKind::Compute,
     .Name = "depthPyramid",
     .Reads = {Resource::SceneDepth, Resource::LinearSampler, kNoEdge},
     .Writes = {Resource::DepthPyramid, kNoEdge},
     .Contributes = {kNoEdge},
     .FusesInto = kNoFusion},

    {.Id = Stage::TemporalResolve,
     .From = Provenance::Content,
     .Kind = PassKind::Raster,
     .Name = "temporalResolve",
     .Reads = {Resource::SceneAerial,
               Resource::SceneVelocity,
               Resource::SceneDepth,
               Resource::LinearSampler,
               Resource::AtmosphereUniform,
               kNoEdge},
     .Writes = {Resource::SceneLinear, kNoEdge},
     .Contributes = {kNoEdge},
     .FusesInto = Stage::Tonemap},
    {.Id = Stage::Tonemap,
     .From = Provenance::Machinery,
     .Kind = PassKind::Raster,
     .Name = "tonemap",
     .Reads = {Resource::SceneLinear,
               Resource::SceneDepth,
               Resource::AoBuffer,
               Resource::Meter,
               Resource::LinearSampler,
               kNoEdge},
     .Writes = {kNoEdge},
     .Contributes = {Resource::FrameTex, kNoEdge},
     .FusesInto = kNoFusion},

    {.Id = Stage::Overlay,
     .From = Provenance::Content,
     .Kind = PassKind::Raster,
     .Name = "overlay",
     .Reads = {Resource::OverlayAtlas, Resource::LinearSampler, kNoEdge},
     .Writes = {kNoEdge},
     .Contributes = {Resource::FrameTex, kNoEdge},
     .FusesInto = kNoFusion},
    {.Id = Stage::Present,
     .From = Provenance::Machinery,
     .Kind = PassKind::Raster,
     .Name = "present",
     .Reads = {Resource::FrameTex, Resource::LinearSampler, kNoEdge},
     .Writes = {kNoEdge},
     .Contributes = {Resource::Surface, kNoEdge},
     .FusesInto = kNoFusion},
};

inline constexpr int kTemporalSettleFrames = 128;

constexpr size_t kResourceCount = static_cast<size_t>(Resource::kCount);
constexpr size_t kStageCount = static_cast<size_t>(Stage::kCount);

[[nodiscard]] constexpr const ResourceRow &Row(Resource r) {
  return kResources[static_cast<size_t>(r)];
}

[[nodiscard]] constexpr const StageRow &Row(Stage s) {
  return kStages[static_cast<size_t>(s)];
}

[[nodiscard]] constexpr bool Names(const Resource (&edges)[kMaxEdges], Resource wanted) {
  for (size_t i = 0; i < kMaxEdges && edges[i] != kNoEdge; ++i) {
    if (edges[i] == wanted) { return true; }
  }
  return false;
}

[[nodiscard]] constexpr bool Produces(Stage s, Resource r) {
  return Names(Row(s).Writes, r) || Names(Row(s).Contributes, r);
}

[[nodiscard]] constexpr size_t ProducerCount(Resource r) {
  size_t found = 0;
  for (size_t s = 0; s < kStageCount; ++s) {
    if (Produces(static_cast<Stage>(s), r)) { ++found; }
  }
  return found;
}

constexpr bool EveryRowIsAtItsOwnIndex() {
  for (size_t i = 0; i < kResourceCount; ++i) {
    if (static_cast<size_t>(kResources[i].Id) != i) { return false; }
  }
  for (size_t i = 0; i < kStageCount; ++i) {
    if (static_cast<size_t>(kStages[i].Id) != i) { return false; }
  }
  return true;
}

constexpr bool EveryReadHasAProducer() {
  for (const auto &row : kStages) {
    for (size_t e = 0; e < kMaxEdges && row.Reads[e] != kNoEdge; ++e) {
      if (Row(row.Reads[e]).Kind == ResourceKind::Given) { continue; }
      if (ProducerCount(row.Reads[e]) == 0) { return false; }
    }
  }
  return true;
}

constexpr bool EveryDerivedResourceHasOneWriter() {
  for (size_t r = 0; r < kResourceCount; ++r) {
    const auto id = static_cast<Resource>(r);
    size_t writers = 0;
    for (const auto &kStage : kStages) {
      if (Names(kStage.Writes, id)) { ++writers; }
    }
    const ResourceKind kind = kResources[r].Kind;
    if (kind == ResourceKind::Derived && writers != 1) { return false; }
    if (kind != ResourceKind::Derived && writers != 0) { return false; }
  }
  return true;
}

constexpr bool EveryAttachmentHasAContributor() {
  for (size_t r = 0; r < kResourceCount; ++r) {
    if (kResources[r].Kind != ResourceKind::Attachment) { continue; }
    if (ProducerCount(static_cast<Resource>(r)) == 0) { return false; }
  }
  return true;
}

constexpr bool TopologicalOrderHolds() {
  for (size_t s = 0; s < kStageCount; ++s) {
    const StageRow &row = kStages[s];
    for (size_t e = 0; e < kMaxEdges && row.Reads[e] != kNoEdge; ++e) {
      for (size_t p = s; p < kStageCount; ++p) {
        if (Produces(static_cast<Stage>(p), row.Reads[e])) { return false; }
      }
    }
  }
  return true;
}

constexpr bool EveryFusionIsAdjacentAndFed() {
  for (size_t s = 0; s < kStageCount; ++s) {
    const StageRow &row = kStages[s];
    if (row.FusesInto == kNoFusion) { continue; }
    if (static_cast<size_t>(row.FusesInto) != s + 1) { return false; }
    bool fed = false;
    for (size_t e = 0; e < kMaxEdges && row.Writes[e] != kNoEdge; ++e) {
      if (Names(Row(row.FusesInto).Reads, row.Writes[e])) { fed = true; }
    }
    if (!fed) { return false; }
  }
  return true;
}

static_assert(sizeof kResources / sizeof kResources[0] == kResourceCount,
              "every resource of the enumeration carries a row and no row is orphaned");
static_assert(sizeof kStages / sizeof kStages[0] == kStageCount,
              "every stage of the enumeration carries a row and no row is orphaned");
static_assert(EveryRowIsAtItsOwnIndex(),
              "a row sits at the index its own id names, so Row() is a lookup and not a search");
static_assert(EveryReadHasAProducer(),
              "no stage reads a resource that nothing in the catalogue produces");
static_assert(
    EveryDerivedResourceHasOneWriter(),
    "a derived resource has exactly one writer -- two writers of one LUT is how this tree "
    "once carried two independently fitted exposure scales");
static_assert(EveryAttachmentHasAContributor(),
              "no attachment of the catalogue is unreachable: something can draw into each");
static_assert(TopologicalOrderHolds(),
              "the stage enumeration is a linear extension of the read/write graph, so there is no "
              "cycle and the derived order needs no sort");
static_assert(EveryFusionIsAdjacentAndFed(),
              "a declared fusion names the next stage and that stage reads what this one writes");

} // namespace outshine::Render
#endif
