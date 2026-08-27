#ifndef OUTSHINE_RENDER_PLAN_RENDERCATALOGUE_H
#define OUTSHINE_RENDER_PLAN_RENDERCATALOGUE_H

#include <cstddef>

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
  AoBuffer,
  SceneLinear,

  OverlayAtlas,
  FrameTex,
  Surface,
  kCount
};

[[nodiscard]] constexpr bool CarriesSceneRadiance(Resource resource) {
  switch (resource) {

    case Resource::SceneHdr:
    case Resource::SceneTransmissive:
    case Resource::SceneComposited:
    case Resource::SceneLinear:
      return true;
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

    case Resource::OverlayAtlas:
    case Resource::FrameTex:
    case Resource::Surface:
    case Resource::kCount:
      return false;
  }
  return false;
}

enum class Stage {
  MediumTransmittance,
  MediumMultiScatter,
  MediumRadiance,
  Irradiance,
  AutoExposure,
  LightVisibility,
  Sky,
  Sun,
  Moon,
  Stars,
  Subjects,
  SubjectsTransmissive,
  CompositeTransmission,
  AmbientOcclusion,
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
};

struct StageRow {
  Stage Id;
  Provenance From;
  PassKind Kind;
  const char *Name;

  Resource Reads[kMaxEdges];
  Resource Writes[kMaxEdges];
  Resource Contributes[kMaxEdges];

  Stage FusesInto;
};

inline constexpr Resource kNoEdge = Resource::kCount;
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
    {Resource::LinearSampler, ResourceKind::Given, FallbackKind::None, kNoEdge, TexelFormat::Handle,
     "linearSampler"},
    {Resource::LutSampler, ResourceKind::Given, FallbackKind::None, kNoEdge, TexelFormat::Handle,
     "lutSampler"},
    {Resource::AtmosphereUniform, ResourceKind::Given, FallbackKind::None, kNoEdge,
     TexelFormat::Handle, "atmosphere"},
    {Resource::CascadeUniform, ResourceKind::Given, FallbackKind::None, kNoEdge, TexelFormat::Handle,
     "cascades"},
    {Resource::VegetationTable, ResourceKind::Given, FallbackKind::None, kNoEdge, TexelFormat::Handle,
     "vegetation"},
    {Resource::TransmittanceLut, ResourceKind::Derived, FallbackKind::None, kNoEdge,
     TexelFormat::Rgba16Float, "transmittanceLut"},
    {Resource::MultiScatterLut, ResourceKind::Derived, FallbackKind::None, kNoEdge,
     TexelFormat::Rgba16Float, "multiScatterLut"},
    {Resource::SkyViewLut, ResourceKind::Derived, FallbackKind::None, kNoEdge,
     TexelFormat::Rgba16Float, "skyViewLut"},
    {Resource::IrradianceBuffer, ResourceKind::Derived, FallbackKind::None, kNoEdge,
     TexelFormat::Handle, "irradiance"},

    {Resource::Meter, ResourceKind::Derived, FallbackKind::Neutral, kNoEdge, TexelFormat::Handle,
     "meter"},

    {Resource::ShadowAtlas, ResourceKind::Attachment, FallbackKind::Neutral, kNoEdge,
     TexelFormat::Depth32Float, "shadowAtlas"},
    {Resource::SceneHdr, ResourceKind::Attachment, FallbackKind::None, kNoEdge,
     TexelFormat::Rgba16Float, "sceneHdr"},
    {Resource::SceneVelocity, ResourceKind::Attachment, FallbackKind::None, kNoEdge,
     TexelFormat::Rg16Float, "sceneVelocity"},
    {Resource::SceneDepth, ResourceKind::Attachment, FallbackKind::None, kNoEdge,
     TexelFormat::Depth32Float, "sceneDepth"},

    {Resource::SceneShadingNormal, ResourceKind::Attachment, FallbackKind::None, kNoEdge,
     TexelFormat::Rgba16Float, "sceneShadingNormal"},

    {Resource::SceneSurfaceIdentity, ResourceKind::Attachment, FallbackKind::None, kNoEdge,
     TexelFormat::Rgba32Float, "sceneSurfaceIdentity"},

    {Resource::SceneTransmissive, ResourceKind::Attachment, FallbackKind::None, kNoEdge,
     TexelFormat::Rgba16Float, "sceneTransmissive"},

    {Resource::SceneComposited, ResourceKind::Derived, FallbackKind::Alias, Resource::SceneHdr,
     TexelFormat::Rgba16Float, "sceneComposited"},
    {Resource::AoBuffer, ResourceKind::Attachment, FallbackKind::Neutral, kNoEdge,
     TexelFormat::R8Unorm, "aoBuffer"},

    {Resource::SceneLinear, ResourceKind::Derived, FallbackKind::Alias, Resource::SceneComposited,
     TexelFormat::Rgba16Float, "sceneLinear"},
    {Resource::OverlayAtlas, ResourceKind::Given, FallbackKind::None, kNoEdge, TexelFormat::Handle,
     "overlayAtlas"},
    {Resource::FrameTex, ResourceKind::Attachment, FallbackKind::None, kNoEdge,
     TexelFormat::Rgba8UnormSrgb, "frameTex"},
    {Resource::Surface, ResourceKind::Attachment, FallbackKind::None, kNoEdge,
     TexelFormat::Rgba8UnormSrgb, "surface"},
};

inline constexpr StageRow kStages[] = {
    {Stage::MediumTransmittance, Provenance::Machinery, PassKind::Compute, "mediumTransmittance",
     {kNoEdge}, {Resource::TransmittanceLut, kNoEdge}, {kNoEdge}, kNoFusion},
    {Stage::MediumMultiScatter, Provenance::Machinery, PassKind::Compute, "mediumMultiScatter",
     {Resource::TransmittanceLut, kNoEdge}, {Resource::MultiScatterLut, kNoEdge}, {kNoEdge}, kNoFusion},
    {Stage::MediumRadiance, Provenance::Machinery, PassKind::Compute, "mediumRadiance",
     {Resource::TransmittanceLut, Resource::MultiScatterLut, Resource::LutSampler,
      Resource::AtmosphereUniform, kNoEdge},
     {Resource::SkyViewLut, kNoEdge}, {kNoEdge}, kNoFusion},
    {Stage::Irradiance, Provenance::Machinery, PassKind::Compute, "irradiance",
     {Resource::SkyViewLut, Resource::TransmittanceLut, Resource::LutSampler,
      Resource::AtmosphereUniform, kNoEdge},
     {Resource::IrradianceBuffer, kNoEdge}, {kNoEdge}, kNoFusion},
    {Stage::AutoExposure, Provenance::Content, PassKind::Compute, "autoExposure",
     {Resource::IrradianceBuffer, kNoEdge}, {Resource::Meter, kNoEdge}, {kNoEdge}, kNoFusion},
    {Stage::LightVisibility, Provenance::Content, PassKind::Raster, "lightVisibility",
     {kNoEdge}, {kNoEdge}, {Resource::ShadowAtlas, kNoEdge}, kNoFusion},
    {Stage::Sky, Provenance::Content, PassKind::Raster, "sky",
     {Resource::SkyViewLut, Resource::LutSampler, Resource::AtmosphereUniform, kNoEdge}, {kNoEdge},
     {Resource::SceneHdr, Resource::SceneVelocity, Resource::SceneDepth, kNoEdge}, kNoFusion},
    {Stage::Sun, Provenance::Content, PassKind::Raster, "sun",
     {Resource::TransmittanceLut, Resource::LutSampler, Resource::AtmosphereUniform, kNoEdge}, {kNoEdge},
     {Resource::SceneHdr, Resource::SceneVelocity, Resource::SceneDepth, kNoEdge}, kNoFusion},
    {Stage::Moon, Provenance::Content, PassKind::Raster, "moon",
     {Resource::LutSampler, Resource::AtmosphereUniform, kNoEdge}, {kNoEdge},
     {Resource::SceneHdr, Resource::SceneVelocity, Resource::SceneDepth, kNoEdge}, kNoFusion},
    {Stage::Stars, Provenance::Content, PassKind::Raster, "stars",
     {kNoEdge}, {kNoEdge},
     {Resource::SceneHdr, Resource::SceneVelocity, Resource::SceneDepth, kNoEdge}, kNoFusion},
    {Stage::Subjects, Provenance::Content, PassKind::Raster, "subjects",
     {Resource::ShadowAtlas, Resource::LutSampler, kNoEdge}, {kNoEdge},
     {Resource::SceneHdr, Resource::SceneVelocity, Resource::SceneDepth,
      Resource::SceneShadingNormal, Resource::SceneSurfaceIdentity, kNoEdge}, kNoFusion},

    {Stage::SubjectsTransmissive, Provenance::Content, PassKind::Raster, "subjectsTransmissive",
     {Resource::SceneHdr, Resource::LinearSampler, kNoEdge}, {kNoEdge},
     {Resource::SceneTransmissive, Resource::SceneVelocity, Resource::SceneDepth, kNoEdge},
     kNoFusion},
    {Stage::CompositeTransmission, Provenance::Content, PassKind::Raster, "compositeTransmission",
     {Resource::SceneHdr, Resource::SceneTransmissive, Resource::LinearSampler, kNoEdge},
     {Resource::SceneComposited, kNoEdge}, {kNoEdge}, kNoFusion},
    {Stage::AmbientOcclusion, Provenance::Content, PassKind::Raster, "ambientOcclusion",
     {Resource::SceneDepth, Resource::AtmosphereUniform, kNoEdge}, {kNoEdge},
     {Resource::AoBuffer, kNoEdge}, kNoFusion},
    {Stage::TemporalResolve, Provenance::Content, PassKind::Raster, "temporalResolve",
     {Resource::SceneComposited, Resource::SceneVelocity, Resource::SceneDepth, Resource::LinearSampler,
      Resource::AtmosphereUniform, kNoEdge},
     {Resource::SceneLinear, kNoEdge}, {kNoEdge}, Stage::Tonemap},
    {Stage::Tonemap, Provenance::Machinery, PassKind::Raster, "tonemap",
     {Resource::SceneLinear, Resource::SceneDepth, Resource::AoBuffer, Resource::Meter,
      Resource::LinearSampler, kNoEdge},
     {kNoEdge}, {Resource::FrameTex, kNoEdge}, kNoFusion},

    {Stage::Overlay, Provenance::Content, PassKind::Raster, "overlay",
     {Resource::OverlayAtlas, Resource::LinearSampler, kNoEdge}, {kNoEdge},
     {Resource::FrameTex, kNoEdge}, kNoFusion},
    {Stage::Present, Provenance::Machinery, PassKind::Raster, "present",
     {Resource::FrameTex, Resource::LinearSampler, kNoEdge}, {kNoEdge},
     {Resource::Surface, kNoEdge}, kNoFusion},
};

inline constexpr int kTemporalSettleFrames = 128;

constexpr size_t kResourceCount = static_cast<size_t>(Resource::kCount);
constexpr size_t kStageCount = static_cast<size_t>(Stage::kCount);

[[nodiscard]] constexpr const ResourceRow &Row(Resource r) { return kResources[static_cast<size_t>(r)]; }
[[nodiscard]] constexpr const StageRow &Row(Stage s) { return kStages[static_cast<size_t>(s)]; }

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
  for (size_t s = 0; s < kStageCount; ++s) {
    const StageRow &row = kStages[s];
    for (size_t e = 0; e < kMaxEdges && row.Reads[e] != kNoEdge; ++e) {
      if (Row(row.Reads[e]).Kind == ResourceKind::Given) { continue; }
      if (ProducerCount(row.Reads[e]) == 0) { return false; }
    }
  }
  return true;
}

constexpr bool EveryDerivedResourceHasOneWriter() {
  for (size_t r = 0; r < kResourceCount; ++r) {
    const Resource id = static_cast<Resource>(r);
    size_t writers = 0;
    for (size_t s = 0; s < kStageCount; ++s) {
      if (Names(kStages[s].Writes, id)) { ++writers; }
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
static_assert(EveryDerivedResourceHasOneWriter(),
              "a derived resource has exactly one writer -- two writers of one LUT is how this tree "
              "once carried two independently fitted exposure scales");
static_assert(EveryAttachmentHasAContributor(),
              "no attachment of the catalogue is unreachable: something can draw into each");
static_assert(TopologicalOrderHolds(),
              "the stage enumeration is a linear extension of the read/write graph, so there is no "
              "cycle and the derived order needs no sort");
static_assert(EveryFusionIsAdjacentAndFed(),
              "a declared fusion names the next stage and that stage reads what this one writes");

}
#endif
