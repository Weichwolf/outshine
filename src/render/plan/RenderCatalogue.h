/* WHAT THE ENGINE CAN RENDER, as data a compiler can read. Every resource and every stage this
 * renderer owns is one row here, and the rows carry no device handle, no texture and no pipeline --
 * which is what lets a plan be checked before a device object exists. WebGPU pins a view into a bind
 * group at bind-group creation, so "create only what is declared" has to be a graph settled before
 * anything is created, and never a branch at a creation site.
 *
 * THE ENUMERATION ORDER IS THE EXECUTION ORDER, and `TopologicalOrderHolds` below is the proof: for
 * every stage, every producer of everything it reads stands earlier in the enumeration. A cyclic
 * catalogue therefore does not compile, and the derived order needs no sort at run time and no
 * second number to tie-break -- the order a consumer could get wrong has no spelling. */
#ifndef RENDERCATALOGUE_H
#define RENDERCATALOGUE_H

#include <cstddef>

namespace outshine::Render {

/* A named, typed GPU artefact of the plan. Never a string: a string defers a typo to run time, and
 * the point of the plan is that the refusal happens where the plan is written. */
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
  AoBuffer,
  SceneLinear,
  FrameTex,
  Surface,
  kCount
};

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
  BenchGround,
  Terrain,
  Buildings,
  Water,
  Models,
  Subjects,
  AmbientOcclusion,
  TemporalResolve,
  Tonemap,
  Present,
  kCount
};

/* WHERE A RESOURCE COMES FROM, and the three answers decide three different refusals.
 * `Given` is the plan's own -- a sampler, a uniform the host writes each frame -- and no stage
 * produces it. `Derived` is produced by exactly one stage and does not exist without it.
 * `Attachment` is a render target: the plan creates and clears it, and stages draw into it. */
enum class ResourceKind { Given, Derived, Attachment };

/* THE CRITERION, AS A FIELD. Does the stage's absence change the picture, or make it impossible?
 * Impossible -> `Machinery`, and the compiler pulls it backwards from the requested output.
 * Different -> `Content`, and the consumer declares it or does without. Nothing else decides this. */
enum class Provenance { Machinery, Content };

enum class PassKind { Compute, Raster };

/* WHAT A RESOURCE FALLS BACK TO when its producer is content the declaration did not name. `Alias`
 * rebinds its readers to another resource; `Neutral` means the plan creates it and fills it with the
 * value that makes its reader behave as if it were absent. A `Derived` resource with `None` and an
 * undeclared producer is a refusal naming both. */
enum class FallbackKind { None, Alias, Neutral };

/* THE STORAGE, because a picture changes when it changes and the plan's digest has to cover it.
 * `Handle` is a sampler or a uniform, which has no texel. */
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

/* HOW MANY COLOUR TARGETS ONE PASS MAY CARRY. WebGPU guarantees `maxColorAttachments = 8` and Metal
 * on this device reports the same, so eight is the floor a plan may assume rather than a number
 * chosen here. A pass whose target union exceeds it is refused where the plan is compiled. */
inline constexpr size_t kMaxColourAttachments = 8;

struct ResourceRow {
  Resource Id;
  ResourceKind Kind;
  FallbackKind Fallback;
  Resource AliasOf;   /* Resource::kCount unless Fallback == Alias */
  TexelFormat Format;
  const char *Name;
};

struct StageRow {
  Stage Id;
  Provenance From;
  PassKind Kind;
  const char *Name;
  /* `Reads` is what the stage samples or binds; `Writes` names the `Derived` resources that do not
   * exist without it; `Contributes` is its render-target set, which is what a pass IS. */
  Resource Reads[kMaxEdges];
  Resource Writes[kMaxEdges];
  Resource Contributes[kMaxEdges];
  /* THE ONE PAIR AN IMPLEMENTATION CAN FUSE. R2 admits a pair only where a single fragment writing
   * both target sets exists, because reading a target of the pass one is in is not legal WebGPU:
   * without the fused shader the rule would be a rule for producing invalid command buffers. */
  Stage FusesInto;
};

inline constexpr Resource kNoEdge = Resource::kCount;
inline constexpr Stage kNoFusion = Stage::kCount;

/* THE COLOUR TARGET SET OF ONE PASS: the union of its stages' targets, one entry per distinct
 * `Resource`, in first-seen order. The union is the type's job and not the caller's, because the
 * caller's version of it was the defect -- it compared two freshly created texture views, which are
 * different objects for the same resource, so the duplicate guard never fired and a pass of n stages
 * wrote 2n entries into an array sized for one attachment per edge.
 *
 * `Add` is the only way in, it absorbs a repeat, and it refuses rather than growing: a set that
 * cannot hold what it was handed is a plan the device would reject, and the refusal belongs where
 * the plan is compiled. Nothing can index past `Size()`, so the encoder's array cannot be overrun. */
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
    /* The exposure a plan without the meter runs at is the plan's own declared scalar, baked into
     * the tonemap it generates -- so a picture that wants a fixed exposure pays neither the meter
     * nor the hemisphere integral behind it. */
    {Resource::Meter, ResourceKind::Derived, FallbackKind::Neutral, kNoEdge, TexelFormat::Handle,
     "meter"},
    /* No neutral atlas. A lit surface samples a shadow map with a comparison sampler and a cascade
     * uniform; standing a cleared texture in for one is a second lighting path, so a plan that lights
     * a surface and declares no shadow map is refused and told which stage supplies it. */
    {Resource::ShadowAtlas, ResourceKind::Attachment, FallbackKind::None, kNoEdge,
     TexelFormat::Depth32Float, "shadowAtlas"},
    {Resource::SceneHdr, ResourceKind::Attachment, FallbackKind::None, kNoEdge,
     TexelFormat::Rgba16Float, "sceneHdr"},
    {Resource::SceneVelocity, ResourceKind::Attachment, FallbackKind::None, kNoEdge,
     TexelFormat::Rg16Float, "sceneVelocity"},
    {Resource::SceneDepth, ResourceKind::Attachment, FallbackKind::None, kNoEdge,
     TexelFormat::Depth32Float, "sceneDepth"},
    /* THE NORMAL THE BRDF RECEIVED, world-space, in the subject's own frame (board:1122). It exists
     * because the third leg of the normal comparison had no source: Cycles publishes its shading
     * normal and the glTF declares its own, and ours was inferred from where a highlight landed --
     * a 2.33 px centroid displacement on a 33.4 px dome, which is 4.2 to 10.3 degrees and structural
     * rather than precision. IT IS ATTACHED ONLY WHERE SOMETHING READS IT (board:1121), so no plan
     * that does not ask for it pays a byte. */
    {Resource::SceneShadingNormal, ResourceKind::Attachment, FallbackKind::None, kNoEdge,
     TexelFormat::Rgba16Float, "sceneShadingNormal"},
    {Resource::AoBuffer, ResourceKind::Attachment, FallbackKind::Neutral, kNoEdge,
     TexelFormat::R8Unorm, "aoBuffer"},
    /* Resolved linear radiance BEFORE the occlusion composite and BEFORE the metered exposure. A
     * picture plan without the temporal resolve reaches the tonemap through the alias rather than
     * through a full-screen blit that exists only to copy. */
    {Resource::SceneLinear, ResourceKind::Derived, FallbackKind::Alias, Resource::SceneHdr,
     TexelFormat::Rgba16Float, "sceneLinear"},
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
    {Stage::BenchGround, Provenance::Content, PassKind::Raster, "benchGround",
     {Resource::ShadowAtlas, Resource::IrradianceBuffer, Resource::CascadeUniform, kNoEdge}, {kNoEdge},
     {Resource::SceneHdr, Resource::SceneVelocity, Resource::SceneDepth, kNoEdge}, kNoFusion},
    {Stage::Terrain, Provenance::Content, PassKind::Raster, "terrain",
     {Resource::SkyViewLut, Resource::LutSampler, Resource::AtmosphereUniform,
      Resource::VegetationTable, Resource::ShadowAtlas, Resource::IrradianceBuffer,
      Resource::CascadeUniform, kNoEdge},
     {kNoEdge}, {Resource::SceneHdr, Resource::SceneVelocity, Resource::SceneDepth, kNoEdge}, kNoFusion},
    {Stage::Buildings, Provenance::Content, PassKind::Raster, "buildings",
     {Resource::ShadowAtlas, Resource::IrradianceBuffer, Resource::CascadeUniform, kNoEdge}, {kNoEdge},
     {Resource::SceneHdr, Resource::SceneVelocity, Resource::SceneDepth, kNoEdge}, kNoFusion},
    {Stage::Water, Provenance::Content, PassKind::Raster, "water",
     {Resource::ShadowAtlas, Resource::IrradianceBuffer, Resource::CascadeUniform, kNoEdge}, {kNoEdge},
     {Resource::SceneHdr, Resource::SceneVelocity, Resource::SceneDepth, kNoEdge}, kNoFusion},
    {Stage::Models, Provenance::Content, PassKind::Raster, "models",
     {Resource::ShadowAtlas, Resource::IrradianceBuffer, Resource::CascadeUniform, kNoEdge}, {kNoEdge},
     {Resource::SceneHdr, Resource::SceneVelocity, Resource::SceneDepth, kNoEdge}, kNoFusion},
    /* THE SHADING NORMAL IS ON `subjects` ALONE and that is not a shortcut: only this stage has a
     * shader that writes one, and a stage contributing a target its pipelines never write is
     * board:1121's defect in a new place. It is SAFE BY CONSTRUCTION rather than by review -- pass
     * merging requires identical contribution sets, so a geometry stage without this target can
     * never share a pass with the one that has it. */
    {Stage::Subjects, Provenance::Content, PassKind::Raster, "subjects",
     {kNoEdge}, {kNoEdge},
     {Resource::SceneHdr, Resource::SceneVelocity, Resource::SceneDepth,
      Resource::SceneShadingNormal, kNoEdge}, kNoFusion},
    {Stage::AmbientOcclusion, Provenance::Content, PassKind::Raster, "ambientOcclusion",
     {Resource::SceneDepth, Resource::AtmosphereUniform, kNoEdge}, {kNoEdge},
     {Resource::AoBuffer, kNoEdge}, kNoFusion},
    {Stage::TemporalResolve, Provenance::Content, PassKind::Raster, "temporalResolve",
     {Resource::SceneHdr, Resource::SceneVelocity, Resource::SceneDepth, Resource::LinearSampler,
      Resource::AtmosphereUniform, kNoEdge},
     {Resource::SceneLinear, kNoEdge}, {kNoEdge}, Stage::Tonemap},
    {Stage::Tonemap, Provenance::Machinery, PassKind::Raster, "tonemap",
     {Resource::SceneLinear, Resource::SceneDepth, Resource::AoBuffer, Resource::Meter,
      Resource::LinearSampler, kNoEdge},
     {kNoEdge}, {Resource::FrameTex, kNoEdge}, kNoFusion},
    {Stage::Present, Provenance::Machinery, PassKind::Raster, "present",
     {Resource::FrameTex, Resource::LinearSampler, kNoEdge}, {kNoEdge},
     {Resource::Surface, kNoEdge}, kNoFusion},
};

/* MEASURED, not derived: the settled 1280x720 reference frame against the same frame after 512
 * settle frames (`--settle N`, 2026-08-07). Pixels differing by more than two codes: 3924 at 0, 22
 * at 48, 7 at 128, and 7 at 192 / 256 / 384. 128 is where the curve reaches its floor; below it the
 * accumulator is still visibly filling, above it nothing moves. The residual 7 px is the f16
 * history's last bit and does not go to zero at any length. It is a property of the temporal stage,
 * so it stands beside its row and no test carries a settle constant of its own. */
inline constexpr int kTemporalSettleFrames = 128;

constexpr size_t kResourceCount = static_cast<size_t>(Resource::kCount);
constexpr size_t kStageCount = static_cast<size_t>(Stage::kCount);

constexpr const ResourceRow &Row(Resource r) { return kResources[static_cast<size_t>(r)]; }
constexpr const StageRow &Row(Stage s) { return kStages[static_cast<size_t>(s)]; }

constexpr bool Names(const Resource (&edges)[kMaxEdges], Resource wanted) {
  for (size_t i = 0; i < kMaxEdges && edges[i] != kNoEdge; ++i) {
    if (edges[i] == wanted) { return true; }
  }
  return false;
}

/* Anything that puts content into a resource: the single writer of a `Derived` one, or any of the
 * contributors of an `Attachment`. */
constexpr bool Produces(Stage s, Resource r) {
  return Names(Row(s).Writes, r) || Names(Row(s).Contributes, r);
}

constexpr size_t ProducerCount(Resource r) {
  size_t found = 0;
  for (size_t s = 0; s < kStageCount; ++s) {
    if (Produces(static_cast<Stage>(s), r)) { ++found; }
  }
  return found;
}

/* THE FOUR REFUSALS THAT ARE THE ENGINE'S AND NEVER THE CONSUMER'S. Each is a property of the table
 * above, so each is a compile error rather than an error string: a consumer must not be told a
 * sentence about a defect it could not have caused. */

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

/* NO CYCLE, AND NO SORT AT RUN TIME. Every producer of everything a stage reads stands earlier in
 * the enumeration, so the enumeration is a linear extension of the dependency graph and a cyclic
 * catalogue cannot be written down. */
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

/* A fused pair must stand next to each other and the second must read what the first writes, or the
 * rule would name a merge the compiler could never reach. */
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

} // namespace outshine::Render
#endif
