#include "SubjectDraw.h"
#include "math/Vec3.h"

#include "FragmentArms.h"

#include <algorithm>
#include <atomic>
#include <chrono>

#include <cstdint>
#include <span>
#include <new>

#include "Heap.h"
#include "LightVisibilityStage.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "MetalRoughBrdf.h"
#include "IridescenceLobe.h"
#include "MicrofacetEnergy.h"
#include "SheenLobe.h"
#include "NormalFromMap.h"
#include "SceneTargets.h"
#include "VertexArms.h"
#include "scene/SurfaceState.h"
#include "ShaderFile.h"

namespace outshine::Render {

constexpr size_t kPrevAnchorSlot = 52;

constexpr size_t kAnchorSlot = 48;

namespace {

constexpr SDL_GPUFrontFace kGltfFrontFace = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;

constexpr Winding kSubjectWinding = Winding::Trusted;

SDL_GPUColorTargetBlendState OverBlend() {
  SDL_GPUColorTargetBlendState blend{};
  blend.enable_blend = true;
  blend.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
  blend.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
  blend.color_blend_op = SDL_GPU_BLENDOP_ADD;
  blend.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
  blend.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
  blend.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
  return blend;
}

const char *KindName(SurfaceKind kind) {
  switch (kind) {
    case SurfaceKind::Opaque: return "OPAQUE";
    case SurfaceKind::Masked: return "MASK";
    case SurfaceKind::Blended: return "BLEND";
    case SurfaceKind::ThinTransmissive: return "a thin transmissive sheet";
    case SurfaceKind::Refractive: return "a refracting volume";
  }
  return "an undeclared surface";
}

} // namespace

namespace {

struct VertexShape {
  static constexpr uint32_t kRuns = 7;
  std::array<SDL_GPUVertexBufferDescription, kRuns> Buffers{};
  std::array<SDL_GPUVertexAttribute, kRuns> Attributes{};
  uint32_t Count = 0;
};

struct Feed {
  uint32_t Slot = 0;
  uint32_t Floats = 0;
};

SDL_GPUVertexBufferDescription Run(Feed of) {
  const uint32_t slot = of.Slot;
  const uint32_t floats = of.Floats;
  SDL_GPUVertexBufferDescription description{};
  description.slot = slot;
  description.pitch = floats * static_cast<uint32_t>(sizeof(float));
  description.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
  return description;
}

struct Attribute {
  uint32_t Location = 0;
  uint32_t Slot = 0;
};

SDL_GPUVertexAttribute At(Attribute of, SDL_GPUVertexElementFormat format) {
  const uint32_t location = of.Location;
  const uint32_t slot = of.Slot;
  SDL_GPUVertexAttribute attribute{};
  attribute.location = location;
  attribute.buffer_slot = slot;
  attribute.format = format;
  attribute.offset = 0;
  return attribute;
}

SDL_GPUVertexElementFormat FormatOf(uint32_t floats) {
  switch (floats) {
    case 2: return SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    case 4: return SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
    default: return SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
  }
}

VertexShape ShapeOf(VertexLayout layout, bool writesVelocity) {
  std::array<VertexRun, kMostVertexRuns> runs = {{}};
  const uint32_t count = RunsOf(layout, writesVelocity, runs);
  VertexShape shape;
  for (uint32_t at = 0; at < count; ++at) {
    shape.Buffers[at] = Run({.Slot = at, .Floats = runs[at].Floats});
    shape.Attributes[at] =
        At({.Location = runs[at].Location, .Slot = at}, FormatOf(runs[at].Floats));
    ++shape.Count;
  }
  return shape;
}

SDL_GPUShader *MakeShader(SDL_GPUDevice *device,
                          std::string_view source,
                          const char *entry,
                          SDL_GPUShaderStage stage) {
  return ShaderFrom(device, source, entry, stage, SubjectDraw::ShaderShape);
}

} // namespace

const char *
SubjectDraw::FragmentEntry(SurfaceDomain domain, SurfaceKind kind, VertexLayout layout) {
  return FragmentArmNamed(domain, ShadingArmOf(layout), CarriesUv(layout), kind);
}

const char *SubjectDraw::VertexEntry(VertexLayout layout) {
  return VertexArmName(layout);
}

std::string SubjectDraw::ShaderSource(const SourceOptions &options) {
  std::string ignored;
  return ShaderSource(options, ignored);
}

std::string SubjectDraw::ShaderSource(const SourceOptions &options, std::string &error) {
  ShaderText source;
  source.Begins().Adds(VelocityStaticDefine());
  VelocityStatic(source)
      .Adds("\n#define SUBJECT_WRITES_VELOCITY ")
      .Adds(options.WritesVelocity ? "1" : "0")
      .Adds("\n#define SUBJECT_WRITES_SHADING_NORMAL ")
      .Adds(options.NormalIndex >= 0 ? "1" : "0")
      .Adds("\n#define SUBJECT_NORMAL_COLOUR_INDEX ")
      .Adds(std::to_string(options.NormalIndex < 0 ? 0 : options.NormalIndex))
      .Adds("\n#define SUBJECT_WRITES_SURFACE_IDENTITY ")
      .Adds(options.IdentityIndex >= 0 ? "1" : "0")
      .Adds("\n#define SUBJECT_IDENTITY_COLOUR_INDEX ")
      .Adds(std::to_string(options.IdentityIndex < 0 ? 0 : options.IdentityIndex))
      .Adds("\n")
      .Reads("src/render/shaders/subjectBindings.msl")
      .Reads("src/render/shaders/subject.msl");
  MetalRoughBrdf(source);
  SheenLobe(source);
  IridescenceLobe(source);
  MicrofacetEnergy(source);
  source.Reads("src/render/shaders/subjectLit.msl")
      .Reads("src/render/shaders/subjectLitTextured.msl");
  NormalFromMap(source);
  return source.Reads("src/render/shaders/subjectMapped.msl")
      .Reads("src/render/shaders/subjectGround.msl")
      .Adds(VertexArmsMsl())
      .Take(error);
}

bool SubjectDraw::Configure(const Gpu &gpu, std::string &error) {
  Device = gpu.Device;
  Bound().StandsOn(gpu.Device, gpu.FiltersFloat32);

  Colours.clear();
  for (const Resource colour : gpu.SceneColours) { Colours.push_back(colour); }
  const auto attachmentIndex = [this](Resource which) -> long {
    const auto at = std::ranges::find(Colours, which);
    return at == Colours.end() ? -1 : static_cast<long>(at - Colours.begin());
  };
  WritesVelocity = attachmentIndex(Resource::SceneVelocity) >= 0;
  const bool writesVelocity = WritesVelocity;
  const long normalIndex = attachmentIndex(Resource::SceneShadingNormal);
  const long identityIndex = attachmentIndex(Resource::SceneSurfaceIdentity);

  SourceOptions options;
  options.WritesVelocity = writesVelocity;
  options.NormalIndex = normalIndex;
  options.IdentityIndex = identityIndex;
  const std::string source = ShaderSource(options, error);
  if (source.empty()) { return false; }

  Built = 0;

  const bool glass = Behind != nullptr;
  for (const SurfaceKind kind : {SurfaceKind::Opaque,
                                 SurfaceKind::Masked,
                                 SurfaceKind::Blended,
                                 SurfaceKind::ThinTransmissive,
                                 SurfaceKind::Refractive}) {
    if (!glass && (kind == SurfaceKind::ThinTransmissive || kind == SurfaceKind::Refractive)) {
      continue;
    }

    const bool blends = kind == SurfaceKind::Blended;
    std::array<SDL_GPUColorTargetDescription, kMaxColourAttachments> targets = {{}};
    targets[0].format = gpu.HdrFormat;
    if (blends) { targets[0].blend_state = OverBlend(); }

    if (writesVelocity) {
      targets[attachmentIndex(Resource::SceneVelocity)] = VelocityTarget(!blends);
    }

    if (normalIndex >= 0) {
      targets[normalIndex].format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
    }

    if (identityIndex >= 0) {
      targets[identityIndex].format = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
    }

    for (const SurfaceDomain domain : {SurfaceDomain::Subject, SurfaceDomain::Ground}) {
      for (const VertexLayoutRow &row : kVertexLayouts) {
        const VertexLayout layout = row.Layout;
        if (!DomainPresents(domain, ShadingArmOf(layout), CarriesUv(layout), kind)) { continue; }
        const VertexShape shape = ShapeOf(layout, WritesVelocity);
        const char *const entry = FragmentEntry(domain, kind, layout);
        const OwnedShader vertex(
            Device, MakeShader(Device, source, VertexEntry(layout), SDL_GPU_SHADERSTAGE_VERTEX));
        const OwnedShader fragment(Device,
                                   MakeShader(Device, source, entry, SDL_GPU_SHADERSTAGE_FRAGMENT));
        if (!vertex || !fragment) {
          error = std::string("the subject's shader did not compile at ") + VertexEntry(layout) +
                  "/" + entry + ": " + SDL_GetError();
          return false;
        }
        SDL_GPUGraphicsPipelineCreateInfo wanted{};
        wanted.vertex_shader = vertex.Get();
        wanted.fragment_shader = fragment.Get();
        wanted.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        wanted.vertex_input_state.vertex_buffer_descriptions = shape.Buffers.data();
        wanted.vertex_input_state.num_vertex_buffers = shape.Count;
        wanted.vertex_input_state.vertex_attributes = shape.Attributes.data();
        wanted.vertex_input_state.num_vertex_attributes = shape.Count;
        wanted.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        wanted.rasterizer_state.front_face = kGltfFrontFace;
        wanted.target_info.color_target_descriptions = targets.data();
        wanted.target_info.num_color_targets = static_cast<Uint32>(Colours.size());
        wanted.target_info.has_depth_stencil_target = true;
        wanted.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
        wanted.depth_stencil_state.enable_depth_test = true;
        wanted.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_GREATER;

        wanted.depth_stencil_state.enable_depth_write = !blends;

        for (const bool cullsBack : {false, true}) {
          wanted.rasterizer_state.cull_mode =
              cullsBack ? SDL_GPU_CULLMODE_BACK : SDL_GPU_CULLMODE_NONE;
          SDL_GPUGraphicsPipeline *made = SDL_CreateGPUGraphicsPipeline(Device, &wanted);
          if (made == nullptr) {
            error = std::string("the subject's pipeline was refused at ") + entry + ": " +
                    SDL_GetError();
            return false;
          }
          Pipelines[PipelineAt(domain, layout, kind, cullsBack)] = OwnedPipeline(Device, made);
          ++Built;
        }
      }
    }
  }
  return true;
}

size_t SubjectDraw::PipelineAt(SurfaceDomain domain,
                               VertexLayout layout,
                               SurfaceKind kind,
                               bool cullsBack) {
  const size_t placed =
      (static_cast<size_t>(domain) * kVertexLayoutCount) + static_cast<size_t>(layout);
  return (placed * 2u + (cullsBack ? 1u : 0u)) * kSurfaceKinds + static_cast<size_t>(kind);
}

void SubjectDraw::BindSurface(const SubjectMaterial &material) {
  SurfaceSlot slot;
  slot.Kind = material.State().Kind();
  slot.CullsBack = CullsBackFaces(material.State(), kSubjectWinding);
  slot.Domain = material.Domain;
  slot.ReadsSecondUv = material.ReadsSecondUv();
  slot.Colour = Bound().Upload(material.Colour, SubjectResidency::Transfer::Srgb, TexelKind::Value);
  slot.Normal =
      Bound().Upload(material.Normal, SubjectResidency::Transfer::Linear, TexelKind::Direction);
  slot.MetalRough =
      Bound().Upload(material.MetalRough, SubjectResidency::Transfer::Linear, TexelKind::Value);
  slot.Emissive =
      Bound().Upload(material.Emissive, SubjectResidency::Transfer::Srgb, TexelKind::Value);

  slot.SpecularStrength = Bound().Upload(
      material.SpecularStrength, SubjectResidency::Transfer::Linear, TexelKind::Value);
  slot.SpecularTint =
      Bound().Upload(material.SpecularTint, SubjectResidency::Transfer::Srgb, TexelKind::Value);

  const Material &row = material.Row;

  const auto identity = static_cast<float>(Slots.size() + 1u);

  Vec3f f0;
  DielectricF0(row, f0);

  const std::array scalars = {material.Coverage(),
                              material.State().CoverageCut(),
                              row.Metalness,
                              row.Roughness,
                              row.BaseColour[0],
                              row.BaseColour[1],
                              row.BaseColour[2],
                              row.BaseColour[3],
                              row.Emission[0],
                              row.Emission[1],
                              row.Emission[2],
                              material.NormalScale,
                              identity,
                              f0[0],
                              f0[1],
                              f0[2],
                              DielectricF90(row),
                              row.Transmission,
                              row.Thickness,
                              row.AttenuationDistance,
                              row.AttenuationColour[0],
                              row.AttenuationColour[1],
                              row.AttenuationColour[2],
                              row.SheenColour[0],
                              row.SheenColour[1],
                              row.SheenColour[2],
                              row.SheenRoughness,
                              row.Clearcoat,
                              row.ClearcoatRoughness,
                              row.Anisotropy,
                              row.AnisotropyRotationRad,
                              row.Iridescence,
                              row.IridescenceIor,
                              row.IridescenceThicknessMinNm,
                              row.IridescenceThicknessMaxNm};
  static_assert(scalars.size() == static_cast<size_t>(kSurfaceScalars),
                "the surface row and its declared length are one statement");
  std::ranges::copy(scalars, slot.Row.begin());

  const std::array<const SubjectTexture *const, kSubjectMaterialImages> images = {
      &material.Colour,
      &material.Normal,
      &material.MetalRough,
      &material.Emissive,
      &material.SpecularStrength,
      &material.SpecularTint};
  auto at = static_cast<size_t>(kSurfaceScalars);
  for (const SubjectTexture *image : images) {
    for (const double element : image->Uv.M) { slot.Row[at++] = static_cast<float>(element); }
  }

  for (const SubjectTexture *image : images) {
    slot.Row[at++] = image->Set == UvSet::Second ? 1.0f : 0.0f;
  }
  Slots.push_back(std::move(slot));
}

uint32_t SubjectDraw::ColourImages() const {
  std::vector<SDL_GPUTexture *> distinct;
  for (const SurfaceSlot &slot : Slots) {
    SDL_GPUTexture *const colour = slot.Colour.Image.Get();
    bool seen = false;
    for (const SDL_GPUTexture *const held : distinct) { seen = seen || held == colour; }
    if (!seen) { distinct.push_back(colour); }
  }
  return static_cast<uint32_t>(distinct.size());
}

uint32_t SubjectDraw::Layouts() const {
  std::vector<VertexLayout> distinct;
  for (const DrawBatch &batch : Batches) {
    bool seen = false;
    for (const VertexLayout held : distinct) { seen = seen || held == batch.Layout; }
    if (!seen) { distinct.push_back(batch.Layout); }
  }
  return static_cast<uint32_t>(distinct.size());
}

uint32_t SubjectDraw::DistinctPlacements() const {
  const size_t rows = Placed_.size() / 16u;
  if (rows == 0) { return 0; }
  uint32_t distinct = 0;
  for (size_t row = 0; row < rows; ++row) {
    bool seen = false;
    for (size_t over = 0; over < row && !seen; ++over) {
      seen = std::ranges::equal(std::span(Placed_).subspan(row * 16u, 16u),
                                std::span(Placed_).subspan(over * 16u, 16u));
    }
    if (!seen) { ++distinct; }
  }
  return distinct;
}

uint32_t SubjectDraw::Textured() const {
  uint32_t wearing = 0;
  for (const SurfaceSlot &slot : Slots) {
    if (slot.Colour.Image.Get() != nullptr) { ++wearing; }
  }
  return wearing;
}

bool SubjectDraw::SetMaterials(std::span<const SubjectMaterial> materials, std::string &error) {
  Slots.clear();
  Batches.clear();
  BatchLayout.clear();
  Bound().Shape().Indices = 0;
  if (Device == nullptr) {
    error = "the subject unit has no device, so no surface can be bound";
    return false;
  }
  if (!Bound().FiltersFloat32()) {
    error = "the device did not grant float32-filterable, and this unit's colour image is linear "
            "f32 so that the filter runs on exact linear values";
    return false;
  }
  for (size_t slot = 0; slot < materials.size(); ++slot) {
    const SurfaceKind kind = materials[slot].State().Kind();

    if ((kind == SurfaceKind::ThinTransmissive || kind == SurfaceKind::Refractive) &&
        Behind == nullptr && !GlassDrawnElsewhere_) {
      error = "surface slot " + std::to_string(slot) + " is " + KindName(kind) +
              ", and no pass of this plan draws it -- what is transmitted through a sheet or "
              "refracted by a volume is the scene behind it, so a subject carrying one needs the "
              "transmissive pass declared, and drawing it opaque instead would be a picture nobody "
              "asked for";
      Slots.clear();
      return false;
    }
    BindSurface(materials[slot]);
  }
  return true;
}

namespace {

constexpr uint32_t kPositionFloats = 3;
constexpr uint32_t kPairFloats = 2;
constexpr uint32_t kQuadFloats = 4;
constexpr uint32_t kSphereFloats = 12;
constexpr uint32_t kArgWords = 5;
constexpr uint32_t kBatchWords = 2;
constexpr SDL_GPUBufferUsageFlags kIndexUse =
    SDL_GPU_BUFFERUSAGE_INDEX | SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ;

struct RebasedRun {
  const uint32_t *From = nullptr;
  uint32_t Count = 0;
  uint32_t FirstVertex = 0;
};

void WriteRebased(const void *carrying, float *into, uint32_t floats) {
  const auto *const held = static_cast<const RebasedRun *>(carrying);
  auto *const words = reinterpret_cast<uint32_t *>(into);
  for (uint32_t at = 0; at < floats && at < held->Count; ++at) {
    words[at] = held->From[at] + held->FirstVertex;
  }
}

struct PieceRun {
  const PieceMesh *Piece = nullptr;
};

void WritePiecePositions(const void *carrying, float *into, uint32_t floats) {
  const auto *const held = static_cast<const PieceRun *>(carrying);
  uint32_t at = 0;
  for (const StoredVertex &one : held->Piece->Verts) {
    if (at + kPositionFloats > floats) { break; }
    into[at++] = one.pos[0];
    into[at++] = one.pos[1];
    into[at++] = one.pos[2];
  }
}

void WritePieceNormals(const void *carrying, float *into, uint32_t floats) {
  const auto *const held = static_cast<const PieceRun *>(carrying);
  uint32_t at = 0;
  for (const StoredVertex &one : held->Piece->Verts) {
    if (at + kPositionFloats > floats) { break; }
    const Vec3f facing = one.norm();
    into[at++] = facing[0];
    into[at++] = facing[1];
    into[at++] = facing[2];
  }
}

void WritePieceUv(const void *carrying, float *into, uint32_t floats) {
  const auto *const held = static_cast<const PieceRun *>(carrying);
  uint32_t at = 0;
  for (const StoredVertex &one : held->Piece->Verts) {
    if (at + kPairFloats > floats) { break; }
    const Vec2f uv = one.uv();
    into[at++] = uv[0];
    into[at++] = uv[1];
  }
}

} // namespace

bool SubjectDraw::RoomForStreams(std::string &error) {
  SubjectResidency &res = Bound();
  const uint32_t verts = res.VertexRoom();
  const uint32_t subject = res.SubjectVertices().First + res.SubjectVertices().Count;
  const auto bytes = [](uint32_t count, uint32_t wide) {
    return count * wide * static_cast<uint32_t>(sizeof(float));
  };
  const auto vertex = SDL_GPU_BUFFERUSAGE_VERTEX;
  using S = SubjectResidency::Stream;
  return res.Grow(S::Vertex, {.Usage = vertex, .Bytes = bytes(verts, kPositionFloats)}, error) &&
         res.Grow(S::Normal, {.Usage = vertex, .Bytes = bytes(verts, kPositionFloats)}, error) &&
         res.Grow(S::Uv, {.Usage = vertex, .Bytes = bytes(verts, kPairFloats)}, error) &&
         res.Grow(S::Colour, {.Usage = vertex, .Bytes = bytes(verts, kQuadFloats)}, error) &&
         (!WritesVelocity || res.Grow(S::Previous,
                                      {.Usage = vertex, .Bytes = bytes(verts, kPositionFloats)},
                                      error)) &&
         (subject == 0 ||
          (res.Grow(
               S::Emitted, {.Usage = vertex, .Bytes = bytes(subject, kPositionFloats)}, error) &&
           res.Grow(S::Tangent, {.Usage = vertex, .Bytes = bytes(subject, kQuadFloats)}, error) &&
           res.Grow(S::Uv1, {.Usage = vertex, .Bytes = bytes(subject, kPairFloats)}, error))) &&
         res.Grow(S::Index,
                  {.Usage = kIndexUse,
                   .Bytes = res.IndexRoom() * static_cast<uint32_t>(sizeof(uint32_t))},
                  error);
}

bool SubjectDraw::SetMesh(const SubjectMesh &mesh, std::string &error) {
  ++Reshaped_;
  Bound().DropStaged();
  Bound().Shape().Vertices = mesh.VertexCount;
  Bound().Shape().Indices = mesh.IndexCount;
  Bound().Shape().HasUv = mesh.Uv.Stands();
  Bound().Shape().HasUv1 = mesh.Uv1.Stands();
  Bound().Shape().HasNormal = mesh.Normals.Stands();
  Bound().Shape().HasTangent = mesh.Tangents.Stands();
  Bound().Shape().HasColour = mesh.Colours.Stands();
  SubjectBatches_.clear();
  SubjectJobs_.clear();
  SubjectSpheres_.clear();
  SubjectRows_ = 0;
  TablesStale_ = true;
  for (int axis = 0; axis < 3; ++axis) { Anchor[axis] = mesh.Anchor[axis]; }
  if (ModelStamp_ != Frame_) {
    ModelBefore_ = ModelStamp_ == 0u ? mesh.Model : Model;
    ModelStamp_ = Frame_;
  }
  for (int part = 0; part < 16; part++) { Model[part] = mesh.Model[part]; }
  RowsStale_ = true;
  if (Bound().Shape().Vertices == 0 || Bound().Shape().Indices == 0) {
    Bound().Shape().Indices = 0;
    if (!Borrows()) {
      Bound().GiveVertices(Bound().SubjectVertices());
      Bound().GiveIndices(Bound().SubjectIndices());
      Bound().SubjectStands({}, {});
    }
    return HandTables(error);
  }
  if (Device == nullptr) {
    Bound().Shape().Indices = 0;
    error = "the subject stage carries no device, so a mesh of " +
            std::to_string(Bound().Shape().Vertices) + " vertices has nowhere to become resident";
    return false;
  }
  {
    const char *missing = nullptr;
    if (!mesh.Verts.Stands()) {
      missing = "position run";
    } else if (mesh.Indices == nullptr) {
      missing = "index run";
    } else if (mesh.Draws == nullptr) {
      missing = "draw list";
    } else if (!mesh.Emitted.Stands()) {
      missing = "emitted-radiance run";
    }
    if (missing != nullptr) {
      error = std::string("the mesh declares ") + std::to_string(mesh.VertexCount) +
              " vertices and " + std::to_string(mesh.IndexCount) + " indices but carries no " +
              missing +
              " -- a declaration that names geometry it does not hand over draws "
              "nothing, and drawing nothing is not what it asked for";
      Bound().Shape().Indices = 0;
      return false;
    }
  }

  if (mesh.PrevVerts.Stands() && !WritesVelocity) {
    Bound().Shape().Indices = 0;
    error = "the mesh carries a previous pose and the pass attaches no velocity target, so the run "
            "would reach no shader";
    return false;
  }
  for (const DrawBatch &batch : mesh.Draws->Batches()) {
    if (batch.MaterialSlot < Slots.size() && Slots[batch.MaterialSlot].ReadsSecondUv &&
        !(CarriesUv1(batch.Layout) && Bound().Shape().HasUv1)) {
      Bound().Shape().Indices = 0;
      error = "surface slot " + std::to_string(batch.MaterialSlot) +
              " reads an image from the second uv set and the draw wearing it " +
              (Bound().Shape().HasUv1 ? "takes a vertex layout that binds no second run"
                                      : "has no second uv run at all") +
              ", and the first set is not a substitute for it";
      return false;
    }
    if (batch.MaterialSlot >= Slots.size()) {
      Bound().Shape().Indices = 0;
      error = "a draw names surface slot " + std::to_string(batch.MaterialSlot) +
              " over a table of " + std::to_string(Slots.size()) + " surfaces";
      return false;
    }
    if (batch.FirstIndex + batch.IndexCount > Bound().Shape().Indices) {
      Bound().Shape().Indices = 0;
      error = "a draw covers indices " + std::to_string(batch.FirstIndex) + " to " +
              std::to_string(batch.FirstIndex + batch.IndexCount) + " over a run of " +
              std::to_string(mesh.IndexCount);
      return false;
    }
  }
  SubjectBatches_ = mesh.Draws->Batches();
  for (const DrawBatch &batch : SubjectBatches_) {
    SubjectRows_ = std::max(SubjectRows_, batch.ModelSlot + batch.Instances);
  }

  if (Borrows()) { return HandTables(error); }

  {
    const Heap::Tagged uploading("mesh-upload");
    Bound().GiveVertices(Bound().SubjectVertices());
    Bound().GiveIndices(Bound().SubjectIndices());
    const SubjectResidency::Range v = Bound().TakeVertices(mesh.VertexCount);
    const SubjectResidency::Range i = Bound().TakeIndices(mesh.IndexCount);
    Bound().SubjectStands(v, i);
    if (!RoomForStreams(error)) {
      Bound().Shape().Indices = 0;
      return false;
    }
    const RebasedRun rebased{
        .From = mesh.Indices, .Count = mesh.IndexCount, .FirstVertex = v.First};
    std::array run = {SubjectResidency::Crossing{
        .Which = SubjectResidency::Stream::Index,
        .Usage = kIndexUse,
        .Bytes = mesh.IndexCount * static_cast<uint32_t>(sizeof(uint32_t)),
        .Offset = i.First * static_cast<uint32_t>(sizeof(uint32_t)),
        .Writes = WriteRebased,
        .Carrying = &rebased}};
    if (!Bound().Cross(run, false, error)) {
      Bound().Shape().Indices = 0;
      return false;
    }
  }
  if (!Bound().Buffer(SubjectResidency::Stream::Index)) {
    Bound().Shape().Indices = 0;
    error = std::string("the subject's index run did not reach the device: ") + SDL_GetError();
    return false;
  }
  if (!HandStreams(mesh, false, error)) { return false; }

  const std::vector<uint32_t> &jobs = mesh.Draws->ClusterJobs();
  if (!jobs.empty() && !mesh.ClusterSpheres.empty()) {
    SubjectJobs_ = jobs;
    SubjectSpheres_.assign(mesh.ClusterSpheres.begin(), mesh.ClusterSpheres.end());
  }
  return HandTables(error);
}

bool SubjectDraw::HandStreams(const SubjectPose &pose, bool deferred, std::string &error) {
  const Heap::Tagged uploading("mesh-upload");
  const uint32_t first = Bound().SubjectVertices().First;
  const uint32_t positionBytes =
      Bound().Shape().Vertices * kPositionFloats * static_cast<uint32_t>(sizeof(float));
  const uint32_t pairBytes =
      Bound().Shape().Vertices * kPairFloats * static_cast<uint32_t>(sizeof(float));
  const uint32_t quadBytes =
      Bound().Shape().Vertices * kQuadFloats * static_cast<uint32_t>(sizeof(float));
  const SubjectStream &previousPose = pose.PrevVerts.Stands() ? pose.PrevVerts : pose.Verts;
  const auto crossing = [first](SubjectResidency::Stream which,
                                const SubjectStream &stream,
                                bool carried,
                                uint32_t bytes,
                                uint32_t wide) {
    SubjectResidency::Crossing made;
    made.Which = which;
    made.Usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    made.Bytes = carried ? bytes : 0u;
    made.Offset = first * wide * static_cast<uint32_t>(sizeof(float));
    if (carried) {
      made.From = stream.From;
      made.Writes = stream.Writes;
      made.Carrying = stream.Carrying;
    }
    return made;
  };

  std::array<SubjectResidency::Crossing, 8> streams = {{
      crossing(SubjectResidency::Stream::Vertex, pose.Verts, true, positionBytes, kPositionFloats),
      crossing(
          SubjectResidency::Stream::Emitted, pose.Emitted, true, positionBytes, kPositionFloats),
      crossing(SubjectResidency::Stream::Normal,
               pose.Normals,
               Bound().Shape().HasNormal,
               positionBytes,
               kPositionFloats),
      crossing(SubjectResidency::Stream::Tangent,
               pose.Tangents,
               Bound().Shape().HasTangent,
               quadBytes,
               kQuadFloats),
      crossing(
          SubjectResidency::Stream::Uv, pose.Uv, Bound().Shape().HasUv, pairBytes, kPairFloats),
      crossing(
          SubjectResidency::Stream::Uv1, pose.Uv1, Bound().Shape().HasUv1, pairBytes, kPairFloats),
      crossing(SubjectResidency::Stream::Colour,
               pose.Colours,
               Bound().Shape().HasColour,
               quadBytes,
               kQuadFloats),
      crossing(SubjectResidency::Stream::Previous,
               previousPose,
               WritesVelocity,
               positionBytes,
               kPositionFloats),
  }};
  size_t count = 0;
  for (const SubjectResidency::Crossing &one : streams) {
    if (one.Bytes > 0 && one.Stands()) { streams[count++] = one; }
  }
  if (!HandPlacements(deferred, error)) { return false; }
  if (!Bound().Cross(
          std::span<SubjectResidency::Crossing>(streams.data(), count), deferred, error)) {
    Bound().Shape().Indices = 0;
    return false;
  }
  if (!Bound().Buffer(SubjectResidency::Stream::Vertex) ||
      !Bound().Buffer(SubjectResidency::Stream::Emitted) ||
      (WritesVelocity && !Bound().Buffer(SubjectResidency::Stream::Previous)) ||
      (Bound().Shape().HasColour && !Bound().Buffer(SubjectResidency::Stream::Colour))) {
    Bound().Shape().Indices = 0;
    error = std::string("the subject's vertex streams did not reach the device: ") + SDL_GetError();
    return false;
  }
  return true;
}

PieceId SubjectDraw::PlacePiece(const PieceMesh &piece, std::string &error) {
  if (Borrows()) { return kNoPiece; }
  if (piece.Verts.empty() || piece.Indices.size() < 3 || piece.Indices.size() % 3 != 0) {
    error = "a piece of " + std::to_string(piece.Verts.size()) + " vertices and " +
            std::to_string(piece.Indices.size()) +
            " indices is not a mesh, and placing nothing is not what was asked for";
    return kNoPiece;
  }
  if (!piece.Colours.empty() && piece.Colours.size() != piece.Verts.size() * kQuadFloats) {
    error = "a piece carries " + std::to_string(piece.Colours.size() / kQuadFloats) +
            " colours over " + std::to_string(piece.Verts.size()) + " vertices";
    return kNoPiece;
  }
  if (Device == nullptr) {
    error = "the subject stage carries no device, so a piece has nowhere to become resident";
    return kNoPiece;
  }
  const Heap::Tagged uploading("mesh-upload");
  const auto verts = static_cast<uint32_t>(piece.Verts.size());
  const auto indices = static_cast<uint32_t>(piece.Indices.size());
  SubjectResidency &res = Bound();
  const SubjectResidency::Range v = res.TakeVertices(verts);
  const SubjectResidency::Range i = res.TakeIndices(indices);
  const auto giveBack = [&res, v, i] {
    res.GiveVertices(v);
    res.GiveIndices(i);
  };
  if (!RoomForStreams(error)) {
    giveBack();
    return kNoPiece;
  }
  const PieceRun carrying{.Piece = &piece};
  const RebasedRun rebased{.From = piece.Indices.data(), .Count = indices, .FirstVertex = v.First};
  const auto floatsAt = [v](uint32_t wide) {
    return v.First * wide * static_cast<uint32_t>(sizeof(float));
  };
  const auto vertexUse = SDL_GPU_BUFFERUSAGE_VERTEX;
  const uint32_t positionBytes = verts * kPositionFloats * static_cast<uint32_t>(sizeof(float));
  std::array<SubjectResidency::Crossing, 6> crossings = {{
      {.Which = SubjectResidency::Stream::Vertex,
       .Usage = vertexUse,
       .Bytes = positionBytes,
       .Offset = floatsAt(kPositionFloats),
       .Writes = WritePiecePositions,
       .Carrying = &carrying},
      {.Which = SubjectResidency::Stream::Normal,
       .Usage = vertexUse,
       .Bytes = positionBytes,
       .Offset = floatsAt(kPositionFloats),
       .Writes = WritePieceNormals,
       .Carrying = &carrying},
      {.Which = SubjectResidency::Stream::Index,
       .Usage = kIndexUse,
       .Bytes = indices * static_cast<uint32_t>(sizeof(uint32_t)),
       .Offset = i.First * static_cast<uint32_t>(sizeof(uint32_t)),
       .Writes = WriteRebased,
       .Carrying = &rebased},
      {.Which = SubjectResidency::Stream::Previous,
       .Usage = vertexUse,
       .Bytes = WritesVelocity ? positionBytes : 0u,
       .Offset = floatsAt(kPositionFloats),
       .Writes = WritePiecePositions,
       .Carrying = &carrying},
      {.Which = SubjectResidency::Stream::Uv,
       .Usage = vertexUse,
       .Bytes = piece.Textured ? verts * kPairFloats * static_cast<uint32_t>(sizeof(float)) : 0u,
       .Offset = floatsAt(kPairFloats),
       .Writes = WritePieceUv,
       .Carrying = &carrying},
      {.Which = SubjectResidency::Stream::Colour,
       .Usage = vertexUse,
       .From = piece.Colours.data(),
       .Bytes = static_cast<uint32_t>(piece.Colours.size() * sizeof(float)),
       .Offset = floatsAt(kQuadFloats)},
  }};
  size_t count = 0;
  for (const SubjectResidency::Crossing &one : crossings) {
    if (one.Bytes > 0 && one.Stands()) { crossings[count++] = one; }
  }
  if (!res.Cross(std::span<SubjectResidency::Crossing>(crossings.data(), count), false, error)) {
    giveBack();
    return kNoPiece;
  }

  PieceId id = kNoPiece;
  if (!Spare_.empty()) {
    id = Spare_.back();
    Spare_.pop_back();
  } else {
    id = static_cast<PieceId>(Pieces_.size());
    Pieces_.emplace_back();
  }
  Piece &held = Pieces_[id];
  held.V = v;
  held.I = i;
  held.Surface = piece.Surface;
  held.Row = piece.Row;
  held.Clusters.assign(piece.Clusters.begin(), piece.Clusters.end());
  VertexRunsCarried carried;
  carried.Normal = true;
  carried.Uv = piece.Textured;
  carried.Colour = !piece.Colours.empty();
  (void)LayoutOf(carried, held.Layout);
  held.Live = true;
  ++PiecesLive_;
  PieceTriangles_ += indices / 3u;
  TablesStale_ = true;
  RowsStale_ = true;
  return id;
}

void SubjectDraw::ReleasePiece(PieceId which) {
  if (which >= Pieces_.size() || !Pieces_[which].Live) { return; }
  Piece &held = Pieces_[which];
  Bound().GiveVertices(held.V);
  Bound().GiveIndices(held.I);
  PieceTriangles_ -= held.I.Count / 3u;
  held.Live = false;
  held.Clusters.clear();
  Spare_.push_back(which);
  --PiecesLive_;
  TablesStale_ = true;
  RowsStale_ = true;
}

void SubjectDraw::WearPieces(std::span<const uint32_t> slotOfSurface) {
  if (SlotOf_.size() == slotOfSurface.size() &&
      std::equal(SlotOf_.begin(), SlotOf_.end(), slotOfSurface.begin())) {
    return;
  }
  SlotOf_.assign(slotOfSurface.begin(), slotOfSurface.end());
  TablesStale_ = true;
}

bool SubjectDraw::HandTables(std::string &error) {
  if (!TablesStale_) { return true; }
  TablesStale_ = false;
  return Retable(error);
}

bool SubjectDraw::Retable(std::string &error) {

  const Heap::Tagged uploading("mesh-cull");
  const uint32_t subjectIndexFirst = Bound().SubjectIndices().First;
  Batches.clear();
  BatchLayout.clear();
  Args_.clear();
  Jobs_ = 0;

  for (const DrawBatch &one : SubjectBatches_) {
    DrawBatch batch = one;
    batch.FirstIndex += subjectIndexFirst;
    VertexRunsCarried carried;
    carried.Uv = CarriesUv(batch.Layout) && Bound().Shape().HasUv;
    carried.Normal = CarriesNormal(batch.Layout) && Bound().Shape().HasNormal;
    carried.Tangent =
        CarriesTangent(batch.Layout) && carried.Normal && carried.Uv && Bound().Shape().HasTangent;
    carried.Uv1 = CarriesUv1(batch.Layout) && carried.Uv && Bound().Shape().HasUv1;
    carried.Colour = CarriesColour(batch.Layout) && Bound().Shape().HasColour;
    VertexLayout drawn = VertexLayout::Position;
    if (!LayoutOf(carried, drawn)) {
      Bound().Shape().Indices = 0;
      Batches.clear();
      BatchLayout.clear();
      error = "a draw's runs name no vertex layout this engine builds, and the nearest one is not "
              "an answer -- the combination is what the enumeration exists to refuse";
      return false;
    }
    Batches.push_back(batch);
    BatchLayout.push_back(drawn);
  }
  if (Borrows()) { return true; }

  std::vector<uint32_t> &jobs = TableJobs_;
  std::vector<float> &spheres = TableSpheres_;
  jobs.assign(SubjectJobs_.begin(), SubjectJobs_.end());
  spheres.assign(SubjectSpheres_.begin(), SubjectSpheres_.end());
  for (size_t at = 2; at < jobs.size(); at += DrawList::kJobWords) {
    jobs[at] += subjectIndexFirst;
  }
  const bool subjectCut = !jobs.empty() && !spheres.empty();
  if (!subjectCut) {
    jobs.clear();
    spheres.clear();
    for (DrawBatch &batch : Batches) {
      batch.FirstJob = 0;
      batch.JobCount = 0;
    }
  }

  const auto slotOf = [this](const Piece &one) {
    return one.Surface < SlotOf_.size() ? SlotOf_[one.Surface] : kNoSlot;
  };
  std::vector<uint32_t> &order = TableOrder_;
  order.clear();
  size_t clusters = 0;
  for (uint32_t at = 0; at < Pieces_.size(); ++at) {
    if (Pieces_[at].Live && slotOf(Pieces_[at]) != kNoSlot && slotOf(Pieces_[at]) < Slots.size()) {
      order.push_back(at);
      clusters += Pieces_[at].Clusters.size();
    }
  }
  jobs.reserve(jobs.size() + clusters * DrawList::kJobWords);
  spheres.reserve(spheres.size() + clusters * kSphereFloats);
  std::ranges::sort(order, [this, &slotOf](uint32_t a, uint32_t b) {
    const Piece &left = Pieces_[a];
    const Piece &right = Pieces_[b];
    if (slotOf(left) != slotOf(right)) { return slotOf(left) < slotOf(right); }
    if (left.Layout != right.Layout) { return left.Layout < right.Layout; }
    return a < b;
  });
  for (const uint32_t at : order) {
    const Piece &one = Pieces_[at];
    const auto row = static_cast<uint32_t>(Batches.size());
    DrawBatch batch{};
    batch.FirstIndex = one.I.First;
    batch.IndexCount = one.I.Count;
    batch.MaterialSlot = slotOf(one);
    batch.Layout = one.Layout;
    batch.Kind = SurfaceKind::Opaque;
    batch.Draws = 1;
    batch.ModelSlot = SubjectRows_ + at;
    batch.Instances = 1;
    batch.FirstJob = static_cast<uint32_t>(jobs.size() / DrawList::kJobWords);
    for (const DagCluster &cluster : one.Clusters) {
      const auto sphere = static_cast<uint32_t>(spheres.size() / kSphereFloats);
      spheres.insert(spheres.end(),
                     {cluster.SelfCenter[0],
                      cluster.SelfCenter[1],
                      cluster.SelfCenter[2],
                      cluster.SelfRadius,
                      cluster.ParentCenter[0],
                      cluster.ParentCenter[1],
                      cluster.ParentCenter[2],
                      cluster.ParentRadius,
                      cluster.SelfErr,
                      cluster.ParentErr,
                      0.0f,
                      0.0f});
      jobs.insert(jobs.end(), {sphere, row, one.I.First + cluster.First, cluster.Count});
    }
    batch.JobCount = static_cast<uint32_t>(jobs.size() / DrawList::kJobWords) - batch.FirstJob;
    Batches.push_back(batch);
    BatchLayout.push_back(one.Layout);
  }

  if (jobs.empty() || spheres.empty()) { return true; }
  Args_.assign(Batches.size() * kArgWords, 0u);
  std::vector<uint32_t> &rows = TableRows_;
  rows.assign(Batches.size() * kBatchWords, 0u);
  uint32_t base = 0;
  for (size_t at = 0; at < Batches.size(); ++at) {
    const DrawBatch &batch = Batches[at];
    Args_[at * kArgWords + 1u] = batch.Instances;
    Args_[at * kArgWords + 2u] = base;
    Args_[at * kArgWords + 4u] = batch.ModelSlot;
    rows[at * kBatchWords] = batch.FirstJob;
    rows[at * kBatchWords + 1u] = batch.JobCount;
    for (uint32_t one = 0; one < batch.JobCount; ++one) {
      base += jobs[(static_cast<size_t>(batch.FirstJob) + one) * DrawList::kJobWords + 3u];
    }
  }
  Jobs_ = static_cast<uint32_t>(jobs.size() / DrawList::kJobWords);
  if (base == 0) {
    Args_.clear();
    Jobs_ = 0;
    return true;
  }

  const auto read = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ;
  std::array cut = {
      SubjectResidency::Crossing{.Which = SubjectResidency::Stream::ClusterSpheres,
                                 .Usage = read,
                                 .From = spheres.data(),
                                 .Bytes = static_cast<uint32_t>(spheres.size() * sizeof(float))},
      SubjectResidency::Crossing{.Which = SubjectResidency::Stream::ClusterJobs,
                                 .Usage = read,
                                 .From = jobs.data(),
                                 .Bytes = static_cast<uint32_t>(jobs.size() * sizeof(uint32_t))},
      SubjectResidency::Crossing{.Which = SubjectResidency::Stream::ClusterBatches,
                                 .Usage = read,
                                 .From = rows.data(),
                                 .Bytes = static_cast<uint32_t>(rows.size() * sizeof(uint32_t))},
  };
  if (!Bound().Cross(cut, false, error)) {
    Args_.clear();
    Jobs_ = 0;
    return false;
  }

  constexpr SDL_GPUBufferUsageFlags kCullUse =
      SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ | SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE;
  const uint32_t jobBytes = Jobs_ * static_cast<uint32_t>(sizeof(uint32_t));
  if (!Room(SubjectResidency::Stream::ClusterKept, {.Usage = kCullUse, .Bytes = jobBytes}) ||
      !Room(SubjectResidency::Stream::ClusterSlot, {.Usage = kCullUse, .Bytes = jobBytes}) ||
      !Room(SubjectResidency::Stream::DrawIndex,
            {.Usage = SDL_GPU_BUFFERUSAGE_INDEX | SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,
             .Bytes = base * static_cast<uint32_t>(sizeof(uint32_t))}) ||
      !Room(SubjectResidency::Stream::DrawArguments,
            {.Usage = SDL_GPU_BUFFERUSAGE_INDIRECT | SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,
             .Bytes = static_cast<uint32_t>(Args_.size() * sizeof(uint32_t))})) {
    Args_.clear();
    Jobs_ = 0;
    error = std::string("the cut's compacted run found no room on the device: ") + SDL_GetError();
    return false;
  }
  return HandDrawArguments(false, error);
}

bool SubjectDraw::HandDrawArguments(bool deferred, std::string &error) {
  if (Args_.empty() || !Bound().Buffer(SubjectResidency::Stream::DrawArguments)) { return true; }
  for (size_t at = 0; at * 5u < Args_.size(); ++at) { Args_[at * 5u] = 0u; }
  const auto bytes = static_cast<uint32_t>(Args_.size() * sizeof(uint32_t));
  std::array table = {SubjectResidency::Crossing{.Which = SubjectResidency::Stream::DrawArguments,
                                                 .Usage = SDL_GPU_BUFFERUSAGE_INDIRECT,
                                                 .From = Args_.data(),
                                                 .Bytes = bytes}};
  return Bound().Cross(table, deferred, error);
}

bool SubjectDraw::HandPlacements(bool deferred, std::string &error) {
  const size_t needed = std::max(Placed_.size() / 16u, static_cast<size_t>(SubjectRows_));
  const size_t all = needed + Pieces_.size();
  if (!RowsStale_ && Rows_.size() == all * 32u) { return true; }
  RowsStale_ = false;
  if (all == 0) { return true; }
  Rows_.assign(all * 32u, 0.0f);
  static constexpr Mat4 kUnmoved = {{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}};
  for (size_t piece = 0; piece < Pieces_.size(); ++piece) {
    const Mat4 &row = Pieces_[piece].Live ? Pieces_[piece].Row : kUnmoved;
    for (size_t at = 0; at < 16u; ++at) {
      const auto held = static_cast<float>(row[at]);
      Rows_[(needed + piece) * 32u + at] = held;
      Rows_[(needed + piece) * 32u + 16u + at] = held;
    }
  }
  for (size_t row = 0; row < needed; ++row) {
    const bool placed = row * 16u + 16u <= Placed_.size();
    const double *const now = placed ? Placed_.data() + row * 16u : Model.data();
    const bool carried = placed ? row < Stamped_.size() && Stamped_[row] != 0u : ModelStamp_ != 0u;
    const double *const carriedFrom = placed ? Before_.data() + row * 16u : ModelBefore_.data();
    const double *const was = carried ? carriedFrom : now;
    for (size_t at = 0; at < 16u; ++at) {
      Rows_[row * 32u + at] = static_cast<float>(now[at]);
      Rows_[row * 32u + 16u + at] = static_cast<float>(was[at]);
    }
  }
  std::array rows = {SubjectResidency::Crossing{
      .Which = SubjectResidency::Stream::Placements,
      .Usage = SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ | SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ,
      .From = Rows_.data(),
      .Bytes = static_cast<uint32_t>(Rows_.size() * sizeof(float))}};
  return Bound().Cross(rows, deferred, error);
}

bool SubjectDraw::SetPose(const SubjectPose &pose, std::string &error) {
  ++Reshaped_;
  if (Borrows()) { return true; }
  if (Bound().Shape().Indices == 0) {
    error = "a pose arrived before any mesh, and there is no subject for it to be a pose of";
    return false;
  }
  if (pose.VertexCount != Bound().Shape().Vertices) {
    error = "the pose carries " + std::to_string(pose.VertexCount) +
            " vertices and the subject has " + std::to_string(Bound().Shape().Vertices) +
            ", so it is a different body rather than the same one moved";
    return false;
  }
  if (!pose.Verts.Stands() || !pose.Emitted.Stands()) {
    error = "a pose arrived without positions or emitted radiance, which every draw binds";
    return false;
  }
  for (int axis = 0; axis < 3; ++axis) { Anchor[axis] = pose.Anchor[axis]; }
  if (ModelStamp_ != Frame_) {
    ModelBefore_ = ModelStamp_ == 0u ? pose.Model : Model;
    ModelStamp_ = Frame_;
  }
  for (int part = 0; part < 16; part++) { Model[part] = pose.Model[part]; }
  RowsStale_ = true;
  if (!HandStreams(pose, true, error)) {
    Bound().DropStaged();
    return false;
  }
  return true;
}

bool SubjectDraw::SetLights(std::span<const SubjectLight> lights, std::string &error) {
  if (lights.size() > kMaxSubjectLights) {
    error = "the subject declares " + std::to_string(lights.size()) +
            " punctual lights over a list of " + std::to_string(kMaxSubjectLights) +
            ", and a light this unit cannot bind is a refusal rather than a light left out of the "
            "picture";
    return false;
  }
  Placed.assign(lights.begin(), lights.end());
  return true;
}

static_assert(static_cast<int>(LightKind::Directional) == 0 &&
                  static_cast<int>(LightKind::Point) == 1 && static_cast<int>(LightKind::Spot) == 2,
              "the shader reads the kind as the enum's own number, so reordering LightKind moves "
              "every light to another shape");

std::array<float, SubjectDraw::kLightFloats>
SubjectDraw::PackedLights(const FrameContext &ctx) const {
  std::array<float, kLightFloats> packed{};
  packed[0] = static_cast<float>(Placed.size());
  packed[1] = 0.0f;
  packed[2] = Shadowed_ ? 1.0f : 0.0f;
  packed[3] = 1.0f / static_cast<float>(kShadowAtlasPx);
  for (int channel = 0; channel < 3; ++channel) {
    packed[4 + channel] = static_cast<float>(IndirectLight.RadianceLinear[channel]);
    packed[8 + channel] = static_cast<float>(IndirectLight.GroundLinear[channel]);
    packed[12 + channel] = static_cast<float>(IndirectLight.UpUnit[channel]);
  }
  for (size_t at = 0; at < Placed.size(); ++at) {
    const PunctualLight &light = Placed[at].Light;
    float *entry = packed.data() + 16 + at * 4u * static_cast<size_t>(kLightVec4s);
    for (int channel = 0; channel < 3; ++channel) {
      entry[channel] = light.Colour[channel] * light.Intensity;
    }
    entry[3] = static_cast<float>(light.Kind);
    for (int axis = 0; axis < 3; ++axis) {
      entry[4 + axis] =
          static_cast<float>(Placed[at].PositionEcefM[axis] + ctx.PreViewTranslation[axis]);
    }

    entry[7] = light.RangeM > 0.0f ? 1.0f / light.RangeM : 0.0f;
    for (int axis = 0; axis < 3; ++axis) { entry[8 + axis] = light.Direction[axis]; }
    const float outer = std::cos(light.OuterConeRad);
    const float inner = std::cos(light.InnerConeRad);
    entry[12] = outer;

    entry[13] = inner > outer ? 1.0f / (inner - outer) : 0.0f;
  }
  return packed;
}

uint32_t SubjectDraw::DrawCount() const {
  uint32_t drawn = 0;
  for (const DrawBatch &batch : Batches) { drawn += batch.Draws; }
  return drawn;
}

static_assert(sizeof(SDL_GPUIndexedIndirectDrawCommand) == 5u * sizeof(uint32_t),
              "the indirect argument table is written as five uints a batch");
inline constexpr size_t kIndirectStride = sizeof(SDL_GPUIndexedIndirectDrawCommand);

void SubjectDraw::Encode(const FrameContext &ctx, const PassRecording &into) {
  if (Batches.empty() || !Bound().Buffer(SubjectResidency::Stream::Vertex) ||
      !Bound().Buffer(SubjectResidency::Stream::Index)) {
    return;
  }
  std::array<float, kUniFloats> uniform = {{}};
  const auto place = [this, &ctx, &uniform, &into] {
    for (int axis = 0; axis < 3; ++axis) {
      uniform[kAnchorSlot + axis] = static_cast<float>(Anchor[axis] + ctx.PreViewTranslation[axis]);
      uniform[kPrevAnchorSlot + axis] =
          static_cast<float>(Anchor[axis] + ctx.PrevPreViewTranslation[axis]);
    }
    for (int i = 0; i < 16; i++) { uniform[i] = ctx.Mvp[i]; }
    for (int i = 0; i < 16; i++) { uniform[16 + i] = ctx.PrevMvp[i]; }
    for (int i = 0; i < 16; i++) { uniform[32 + i] = static_cast<float>(LightFromWorld_[i]); }
    ++UniformPushes_;
    SDL_PushGPUVertexUniformData(into.Commands,
                                 0,
                                 uniform.data(),
                                 static_cast<uint32_t>(uniform.size() * sizeof(uniform[0])));
  };
  place();
  const std::array<float, kLightFloats> lights = PackedLights(ctx);
  ShadowedFrames_ += lights[2] > 0.5f ? 1u : 0u;
  SDL_PushGPUFragmentUniformData(
      into.Commands, 1, lights.data(), static_cast<uint32_t>(lights.size() * sizeof(float)));

  bool boundCut = false;
  bool anyIndex = false;

  std::array<SDL_GPUBuffer *const, 1> rows = {
      Bound().Buffer(SubjectResidency::Stream::Placements).Get()};
  SDL_BindGPUVertexStorageBuffers(into.Pass, 0, rows.data(), 1);

  size_t bound = kPipelines;
  size_t boundSlot = 0;
  bool slotBound = false;
  const bool cut = Bound().Buffer(SubjectResidency::Stream::DrawIndex) &&
                   Bound().Buffer(SubjectResidency::Stream::DrawArguments) && !Args_.empty();
  for (size_t at = 0; at < Batches.size(); ++at) {
    const DrawBatch &batch = Batches[at];
    const bool culled = cut && batch.JobCount > 0;
    const SurfaceSlot &surface = Slots[batch.MaterialSlot];

    const bool glassSlot =
        surface.Kind == SurfaceKind::ThinTransmissive || surface.Kind == SurfaceKind::Refractive;
    if (glassSlot != (Behind != nullptr)) { continue; }

    const VertexLayout wanted = BatchLayout[at];
    const bool textured = CarriesUv(wanted);
    const bool lit = CarriesNormal(wanted);
    const bool mapped = CarriesTangent(wanted);
    const bool secondUv = CarriesUv1(wanted);
    const bool tinted = CarriesColour(wanted);

    const size_t wantedPipeline =
        PipelineAt(surface.Domain, wanted, surface.Kind, surface.CullsBack);
    if (wantedPipeline != bound) {
      SDL_BindGPUGraphicsPipeline(into.Pass, Pipelines[wantedPipeline].Get());

      std::array<SDL_GPUBufferBinding, VertexShape::kRuns> runs = {{}};
      uint32_t count = 0;
      runs[count++] = SDL_GPUBufferBinding{
          .buffer = Bound().Buffer(SubjectResidency::Stream::Vertex).Get(), .offset = 0};
      if (textured) {
        runs[count++] = SDL_GPUBufferBinding{
            .buffer = Bound().Buffer(SubjectResidency::Stream::Uv).Get(), .offset = 0};
      }
      if (secondUv) {
        runs[count++] = SDL_GPUBufferBinding{
            .buffer = Bound().Buffer(SubjectResidency::Stream::Uv1).Get(), .offset = 0};
      }
      runs[count++] = SDL_GPUBufferBinding{
          .buffer = lit ? Bound().Buffer(SubjectResidency::Stream::Normal).Get()
                        : Bound().Buffer(SubjectResidency::Stream::Emitted).Get(),
          .offset = 0};
      if (mapped) {
        runs[count++] = SDL_GPUBufferBinding{
            .buffer = Bound().Buffer(SubjectResidency::Stream::Tangent).Get(), .offset = 0};
      }
      if (tinted) {
        runs[count++] = SDL_GPUBufferBinding{
            .buffer = Bound().Buffer(SubjectResidency::Stream::Colour).Get(), .offset = 0};
      }

      if (WritesVelocity) {
        runs[count++] = SDL_GPUBufferBinding{
            .buffer = Bound().Buffer(SubjectResidency::Stream::Previous).Get(), .offset = 0};
      }
      SDL_BindGPUVertexBuffers(into.Pass, 0, runs.data(), count);
      bound = wantedPipeline;
    }
    if (!slotBound || boundSlot != batch.MaterialSlot) {
      const std::array<SDL_GPUTextureSamplerBinding, kSubjectImages> images = {
          {{.texture = surface.Colour.Image.Get(), .sampler = surface.Colour.Sample.Get()},
           {.texture = surface.Normal.Image.Get(), .sampler = surface.Normal.Sample.Get()},
           {.texture = surface.MetalRough.Image.Get(), .sampler = surface.MetalRough.Sample.Get()},
           {.texture = surface.Emissive.Image.Get(), .sampler = surface.Emissive.Sample.Get()},
           {.texture = surface.SpecularStrength.Image.Get(),
            .sampler = surface.SpecularStrength.Sample.Get()},
           {.texture = surface.SpecularTint.Image.Get(),
            .sampler = surface.SpecularTint.Sample.Get()},

           {.texture = Behind != nullptr ? Behind : surface.Colour.Image.Get(),
            .sampler = BehindSampler != nullptr ? BehindSampler : surface.Colour.Sample.Get()},

           {.texture = Atlas_ != nullptr ? Atlas_ : surface.Colour.Image.Get(),
            .sampler = AtlasSampler_ != nullptr ? AtlasSampler_ : surface.Colour.Sample.Get()}}};
      SDL_BindGPUFragmentSamplers(into.Pass, 0, images.data(), kSubjectImages);
      if (SkyIrradiance_ != nullptr && GroundClasses_ != nullptr && GroundPalette_ != nullptr) {
        std::array<SDL_GPUBuffer *const, kSubjectFragmentStorage> storage = {
            SkyIrradiance_, GroundClasses_, GroundPalette_};
        SDL_BindGPUFragmentStorageBuffers(into.Pass, 0, storage.data(), kSubjectFragmentStorage);
      }
      SDL_PushGPUFragmentUniformData(into.Commands,
                                     0,
                                     surface.Row.data(),
                                     static_cast<uint32_t>(surface.Row.size() * sizeof(float)));
      boundSlot = batch.MaterialSlot;
      slotBound = true;
    }
    if (!anyIndex || boundCut != culled) {
      const SDL_GPUBufferBinding indices{
          .buffer = culled ? Bound().Buffer(SubjectResidency::Stream::DrawIndex).Get()
                           : Bound().Buffer(SubjectResidency::Stream::Index).Get(),
          .offset = 0};
      SDL_BindGPUIndexBuffer(into.Pass, &indices, SDL_GPU_INDEXELEMENTSIZE_32BIT);
      boundCut = culled;
      anyIndex = true;
    }
    if (culled) {
      SDL_DrawGPUIndexedPrimitivesIndirect(
          into.Pass,
          Bound().Buffer(SubjectResidency::Stream::DrawArguments).Get(),
          static_cast<Uint32>(at * kIndirectStride),
          1u);
      continue;
    }
    SDL_DrawGPUIndexedPrimitives(
        into.Pass, batch.IndexCount, batch.Instances, batch.FirstIndex, 0, batch.ModelSlot);
  }
}
} // namespace outshine::Render
