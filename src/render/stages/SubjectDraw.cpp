#include "SubjectDraw.h"

#include <atomic>
#include <chrono>

#include <span>
#include <new>

#include "Heap.h"
#include "LightVisibilityStage.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

#include "MetalRoughBrdf.h"
#include "IridescenceLobe.h"
#include "MicrofacetEnergy.h"
#include "SheenLobe.h"
#include "NormalFromMap.h"
#include "SceneTargets.h"
#include "ShaderPrelude.h"
#include "SurfaceState.h"
#include "ShaderFile.h"

namespace outshine::Render {

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
  SDL_GPUVertexBufferDescription Buffers[kRuns];
  SDL_GPUVertexAttribute Attributes[kRuns];
  uint32_t Count = 0;
};

SDL_GPUVertexBufferDescription Run(uint32_t slot, uint32_t floats) {
  SDL_GPUVertexBufferDescription description{};
  description.slot = slot;
  description.pitch = floats * (uint32_t)sizeof(float);
  description.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
  return description;
}

SDL_GPUVertexAttribute At(uint32_t location, uint32_t slot, SDL_GPUVertexElementFormat format) {
  SDL_GPUVertexAttribute attribute{};
  attribute.location = location;
  attribute.buffer_slot = slot;
  attribute.format = format;
  attribute.offset = 0;
  return attribute;
}

SDL_GPUVertexElementFormat FormatOf(uint32_t floats) {
  return floats == 2   ? SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2
         : floats == 4 ? SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4
                       : SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
}

VertexShape ShapeOf(VertexLayout layout, bool writesVelocity) {
  VertexRun runs[kMostVertexRuns] = {};
  const uint32_t count = RunsOf(layout, writesVelocity, runs);
  VertexShape shape;
  for (uint32_t at = 0; at < count; ++at) {
    shape.Buffers[at] = Run(at, runs[at].Floats);
    shape.Attributes[at] = At(runs[at].Location, at, FormatOf(runs[at].Floats));
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

const char *SubjectDraw::FragmentEntry(SurfaceKind kind, VertexLayout layout) {
  const bool textured = CarriesUv(layout);
  if (CarriesTangent(layout)) {
    switch (kind) {
      case SurfaceKind::Masked: return "fsMappedMasked";
      case SurfaceKind::Blended: return "fsMappedBlended";
      case SurfaceKind::ThinTransmissive:
      case SurfaceKind::Refractive: return "fsMappedTransmissive";
      case SurfaceKind::Opaque: break;
    }
    return "fsMapped";
  }
  if (CarriesNormal(layout)) {
    switch (kind) {
      case SurfaceKind::Masked: return textured ? "fsLitMaskedTextured" : "fsLitMasked";
      case SurfaceKind::Blended: return textured ? "fsLitBlendedTextured" : "fsLitBlended";
      case SurfaceKind::ThinTransmissive:
      case SurfaceKind::Refractive:
        return textured ? "fsLitTransmissiveTextured" : "fsLitTransmissive";
      case SurfaceKind::Opaque: break;
    }
    return textured ? "fsLitTextured" : "fsLit";
  }
  switch (kind) {
    case SurfaceKind::Masked: return textured ? "fsMaskedTextured" : "fsMasked";
    case SurfaceKind::Blended: return textured ? "fsBlendedTextured" : "fsBlended";
    case SurfaceKind::ThinTransmissive:
    case SurfaceKind::Refractive: return "fsTransmissive";
    case SurfaceKind::Opaque: break;
  }
  return textured ? "fsTextured" : "fs";
}

const char *SubjectDraw::VertexEntry(VertexLayout layout) {
  switch (layout) {
    case VertexLayout::Position: return "vs";
    case VertexLayout::PositionUv: return "vsTextured";
    case VertexLayout::PositionUvUv1: return "vsTexturedTwo";
    case VertexLayout::PositionNormal: return "vsLit";
    case VertexLayout::PositionNormalUv: return "vsLitTextured";
    case VertexLayout::PositionNormalUvUv1: return "vsLitTexturedTwo";
    case VertexLayout::PositionNormalUvTangent: return "vsMapped";
    case VertexLayout::PositionNormalUvUv1Tangent: return "vsMappedTwo";
    case VertexLayout::PositionColour: return "vsTinted";
    case VertexLayout::PositionUvColour: return "vsTexturedTinted";
    case VertexLayout::PositionUvUv1Colour: return "vsTexturedTwoTinted";
    case VertexLayout::PositionNormalColour: return "vsLitTinted";
    case VertexLayout::PositionNormalUvColour: return "vsLitTexturedTinted";
    case VertexLayout::PositionNormalUvUv1Colour: return "vsLitTexturedTwoTinted";
    case VertexLayout::PositionNormalUvTangentColour: return "vsMappedTinted";
    case VertexLayout::PositionNormalUvUv1TangentColour: return "vsMappedTwoTinted";
  }
  return "vs";
}

std::string SubjectDraw::ShaderSource(const SourceOptions &options) {
  std::string ignored;
  return ShaderSource(options, ignored);
}

std::string SubjectDraw::ShaderSource(const SourceOptions &options, std::string &error) {
  std::string bindings, body, lit, litTextured, mapped;
  if (!LoadShaderText("src/render/shaders/subjectBindings.msl", bindings, error) ||
      !LoadShaderText("src/render/shaders/subject.msl", body, error) ||
      !LoadShaderText("src/render/shaders/subjectLit.msl", lit, error) ||
      !LoadShaderText("src/render/shaders/subjectLitTextured.msl", litTextured, error) ||
      !LoadShaderText("src/render/shaders/subjectMapped.msl", mapped, error)) {
    return std::string();
  }
  const std::string brdf = MetalRoughBrdfMsl(error);
  const std::string sheen = SheenLobeMsl(error);
  const std::string iridescence = IridescenceLobeMsl(error);
  const std::string energy = MicrofacetEnergyMsl(error);
  const std::string normalMap = NormalFromMapMsl(error);
  if (brdf.empty() || sheen.empty() || iridescence.empty() || energy.empty() || normalMap.empty()) {
    return std::string();
  }
  return MslPrelude(error) + VelocityStaticDefine() + VelocityStaticMsl(error) +
         "\n#define SUBJECT_WRITES_VELOCITY " + (options.WritesVelocity ? "1" : "0") +
         "\n#define SUBJECT_WRITES_SHADING_NORMAL " + (options.NormalIndex >= 0 ? "1" : "0") +
         "\n#define SUBJECT_NORMAL_COLOUR_INDEX " +
         std::to_string(options.NormalIndex < 0 ? 0 : options.NormalIndex) +
         "\n#define SUBJECT_WRITES_SURFACE_IDENTITY " + (options.IdentityIndex >= 0 ? "1" : "0") +
         "\n#define SUBJECT_IDENTITY_COLOUR_INDEX " +
         std::to_string(options.IdentityIndex < 0 ? 0 : options.IdentityIndex) + "\n" + bindings +
         body + brdf + sheen + iridescence + energy + lit + litTextured + normalMap + mapped;
}

bool SubjectDraw::Configure(const Gpu &gpu, std::string &error) {
  Device = gpu.Device;
  Bound().Device = gpu.Device;
  Bound().FiltersFloat32 = gpu.FiltersFloat32;

  Colours.clear();
  for (const Resource colour : gpu.SceneColours) { Colours.push_back(colour); }
  const auto attachmentIndex = [this](Resource which) -> long {
    const auto at = std::find(Colours.begin(), Colours.end(), which);
    return at == Colours.end() ? -1 : (long)(at - Colours.begin());
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
    SDL_GPUColorTargetDescription targets[kMaxColourAttachments] = {};
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

    for (const VertexLayoutRow &row : kVertexLayouts) {
      const VertexLayout layout = row.Layout;
      const VertexShape shape = ShapeOf(layout, WritesVelocity);
      const OwnedShader vertex(
          Device, MakeShader(Device, source, VertexEntry(layout), SDL_GPU_SHADERSTAGE_VERTEX));
      const OwnedShader fragment(
          Device,
          MakeShader(Device, source, FragmentEntry(kind, layout), SDL_GPU_SHADERSTAGE_FRAGMENT));
      if (!vertex || !fragment) {
        error = std::string("the subject's shader did not compile at ") + VertexEntry(layout) +
                "/" + FragmentEntry(kind, layout) + ": " + SDL_GetError();
        return false;
      }
      SDL_GPUGraphicsPipelineCreateInfo wanted{};
      wanted.vertex_shader = vertex.Get();
      wanted.fragment_shader = fragment.Get();
      wanted.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
      wanted.vertex_input_state.vertex_buffer_descriptions = shape.Buffers;
      wanted.vertex_input_state.num_vertex_buffers = shape.Count;
      wanted.vertex_input_state.vertex_attributes = shape.Attributes;
      wanted.vertex_input_state.num_vertex_attributes = shape.Count;
      wanted.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
      wanted.rasterizer_state.front_face = kGltfFrontFace;
      wanted.target_info.color_target_descriptions = targets;
      wanted.target_info.num_color_targets = (Uint32)Colours.size();
      wanted.target_info.has_depth_stencil_target = true;
      wanted.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
      wanted.depth_stencil_state.enable_depth_test = true;
      wanted.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_GREATER;

      wanted.depth_stencil_state.enable_depth_write = !blends;

      for (const bool cullsBack : {false, true}) {
        wanted.rasterizer_state.cull_mode =
            cullsBack ? SDL_GPU_CULLMODE_BACK : SDL_GPU_CULLMODE_NONE;
        SDL_GPUGraphicsPipeline *made = SDL_CreateGPUGraphicsPipeline(Device, &wanted);
        if (!made) {
          error = std::string("the subject's pipeline was refused at ") +
                  FragmentEntry(kind, layout) + ": " + SDL_GetError();
          return false;
        }
        Pipelines[PipelineAt(layout, kind, cullsBack)] = OwnedPipeline(Device, made);
        ++Built;
      }
    }
  }
  return true;
}

size_t SubjectDraw::PipelineAt(VertexLayout layout, SurfaceKind kind, bool cullsBack) {
  const size_t at = static_cast<size_t>(kind);
  return (static_cast<size_t>(layout) * 2u + (cullsBack ? 1u : 0u)) * kSurfaceKinds + at;
}

void SubjectDraw::BindSurface(const SubjectMaterial &material) {
  SurfaceSlot slot;
  slot.Kind = material.State().Kind();
  slot.CullsBack = CullsBackFaces(material.State(), kSubjectWinding);
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

  const float identity = (float)(Slots.size() + 1u);

  float f0[3];
  DielectricF0(row, f0);

  const float scalars[] = {material.Coverage(),
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
  static_assert(sizeof scalars / sizeof scalars[0] == (size_t)kSurfaceScalars,
                "the surface row and its declared length are one statement");
  std::copy(std::begin(scalars), std::end(scalars), slot.Row.begin());

  const SubjectTexture *const images[kSubjectMaterialImages] = {&material.Colour,
                                                                &material.Normal,
                                                                &material.MetalRough,
                                                                &material.Emissive,
                                                                &material.SpecularStrength,
                                                                &material.SpecularTint};
  size_t at = (size_t)kSurfaceScalars;
  for (const SubjectTexture *image : images) {
    for (const double element : image->Uv.M) { slot.Row[at++] = (float)element; }
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
    for (SDL_GPUTexture *const held : distinct) { seen = seen || held == colour; }
    if (!seen) { distinct.push_back(colour); }
  }
  return (uint32_t)distinct.size();
}

uint32_t SubjectDraw::Layouts() const {
  std::vector<VertexLayout> distinct;
  for (const DrawBatch &batch : Batches) {
    bool seen = false;
    for (const VertexLayout held : distinct) { seen = seen || held == batch.Layout; }
    if (!seen) { distinct.push_back(batch.Layout); }
  }
  return (uint32_t)distinct.size();
}

uint32_t SubjectDraw::DistinctPlacements() const {
  const size_t rows = Placed_.size() / 16u;
  if (rows == 0) { return 0; }
  uint32_t distinct = 0;
  for (size_t row = 0; row < rows; ++row) {
    bool seen = false;
    for (size_t over = 0; over < row && !seen; ++over) {
      seen = std::memcmp(Placed_.data() + row * 16u,
                         Placed_.data() + over * 16u,
                         16u * sizeof(double)) == 0;
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
  Bound().NIdx = 0;
  if (!Device) {
    error = "the subject unit has no device, so no surface can be bound";
    return false;
  }
  if (!Bound().FiltersFloat32) {
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

bool SubjectDraw::SetMesh(const SubjectMesh &mesh, std::string &error) {
  ++Reshaped_;
  Bound().DropStaged();
  Bound().NVerts = mesh.VertexCount;
  Bound().NIdx = mesh.IndexCount;
  Bound().HasUv = mesh.Uv.Stands();
  Bound().HasUv1 = mesh.Uv1.Stands();
  Bound().HasNormal = mesh.Normals.Stands();
  Bound().HasTangent = mesh.Tangents.Stands();
  Bound().HasColour = mesh.Colours.Stands();
  Batches.clear();
  BatchLayout.clear();
  for (int axis = 0; axis < 3; ++axis) { Anchor[axis] = mesh.Anchor[axis]; }
  if (ModelStamp_ != Frame_) {
    const double *const from = ModelStamp_ == 0u ? mesh.Model : Model;
    for (int part = 0; part < 16; part++) { ModelBefore_[part] = from[part]; }
    ModelStamp_ = Frame_;
  }
  for (int part = 0; part < 16; part++) { Model[part] = mesh.Model[part]; }
  RowsStale_ = true;
  if (Bound().NVerts == 0 || Bound().NIdx == 0) {
    Bound().NIdx = 0;
    return true;
  }
  if (!Device) {
    Bound().NIdx = 0;
    error = "the subject stage carries no device, so a mesh of " + std::to_string(Bound().NVerts) +
            " vertices has nowhere to become resident";
    return false;
  }
  {
    const char *missing = nullptr;
    if (!mesh.Verts.Stands()) {
      missing = "position run";
    } else if (!mesh.Indices) {
      missing = "index run";
    } else if (!mesh.Draws) {
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
      Bound().NIdx = 0;
      return false;
    }
  }

  if (mesh.PrevVerts.Stands() && !WritesVelocity) {
    Bound().NIdx = 0;
    error = "the mesh carries a previous pose and the pass attaches no velocity target, so the run "
            "would reach no shader";
    return false;
  }
  for (const DrawBatch &batch : mesh.Draws->Batches()) {
    if (batch.MaterialSlot < Slots.size() && Slots[batch.MaterialSlot].ReadsSecondUv &&
        !(CarriesUv1(batch.Layout) && Bound().HasUv1)) {
      Bound().NIdx = 0;
      error = "surface slot " + std::to_string(batch.MaterialSlot) +
              " reads an image from the second uv set and the draw wearing it " +
              (Bound().HasUv1 ? "takes a vertex layout that binds no second run"
                              : "has no second uv run at all") +
              ", and the first set is not a substitute for it";
      return false;
    }
    if (batch.MaterialSlot >= Slots.size()) {
      Bound().NIdx = 0;
      error = "a draw names surface slot " + std::to_string(batch.MaterialSlot) +
              " over a table of " + std::to_string(Slots.size()) + " surfaces";
      return false;
    }
    if (batch.FirstIndex + batch.IndexCount > Bound().NIdx) {
      Bound().NIdx = 0;
      error = "a draw covers indices " + std::to_string(batch.FirstIndex) + " to " +
              std::to_string(batch.FirstIndex + batch.IndexCount) + " over a run of " +
              std::to_string(mesh.IndexCount);
      return false;
    }
  }
  Batches = mesh.Draws->Batches();

  BatchLayout.reserve(Batches.size());
  for (const DrawBatch &batch : Batches) {
    VertexRunsCarried carried;
    carried.Uv = CarriesUv(batch.Layout) && Bound().HasUv;
    carried.Normal = CarriesNormal(batch.Layout) && Bound().HasNormal;
    carried.Tangent =
        CarriesTangent(batch.Layout) && carried.Normal && carried.Uv && Bound().HasTangent;
    carried.Uv1 = CarriesUv1(batch.Layout) && carried.Uv && Bound().HasUv1;
    carried.Colour = CarriesColour(batch.Layout) && Bound().HasColour;
    VertexLayout drawn = VertexLayout::Position;
    if (!LayoutOf(carried, drawn)) {
      Bound().NIdx = 0;
      Batches.clear();
      BatchLayout.clear();
      error = "a draw's runs name no vertex layout this engine builds, and the nearest one is not "
              "an answer -- the combination is what the enumeration exists to refuse";
      return false;
    }
    BatchLayout.push_back(drawn);
  }

  if (Borrows()) { return true; }

  if (!HandClusters(mesh, error)) { return false; }

  {
    const Heap::Tagged uploading("mesh-upload");
    SubjectResidency::Crossing run[] = {
        {&Bound().Idx,
         &Bound().Held[(size_t)SubjectResidency::Stream::Index],
         SDL_GPU_BUFFERUSAGE_INDEX | SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ,
         mesh.Indices,
         Bound().NIdx * (uint32_t)sizeof(uint32_t)}};
    if (!Bound().Cross(run, 1, false, error)) {
      Bound().NIdx = 0;
      return false;
    }
  }
  if (!Bound().Idx) {
    Bound().NIdx = 0;
    error = std::string("the subject's index run did not reach the device: ") + SDL_GetError();
    return false;
  }
  if (!HandStreams(mesh, false, error)) { return false; }

  return true;
}

bool SubjectDraw::HandStreams(const SubjectPose &pose, bool deferred, std::string &error) {
  const Heap::Tagged uploading("mesh-upload");
  const uint32_t positionBytes = Bound().NVerts * 3u * (uint32_t)sizeof(float);
  const uint32_t pairBytes = Bound().NVerts * 2u * (uint32_t)sizeof(float);
  const uint32_t quadBytes = Bound().NVerts * 4u * (uint32_t)sizeof(float);
  const SubjectStream &previousPose = pose.PrevVerts.Stands() ? pose.PrevVerts : pose.Verts;
  const auto crossing = [](OwnedBuffer *into,
                           uint32_t *held,
                           const SubjectStream &stream,
                           bool carried,
                           uint32_t bytes) {
    SubjectResidency::Crossing made;
    made.Into = into;
    made.Held = held;
    made.Usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    made.Bytes = carried ? bytes : 0u;
    if (carried) {
      made.From = stream.From;
      made.Writes = stream.Writes;
      made.Carrying = stream.Carrying;
    }
    return made;
  };

  SubjectResidency::Crossing streams[] = {
      crossing(&Bound().Vtx,
               &Bound().Held[(size_t)SubjectResidency::Stream::Vertex],
               pose.Verts,
               true,
               positionBytes),
      crossing(&Bound().Emit,
               &Bound().Held[(size_t)SubjectResidency::Stream::Emitted],
               pose.Emitted,
               true,
               positionBytes),
      crossing(&Bound().Nrm,
               &Bound().Held[(size_t)SubjectResidency::Stream::Normal],
               pose.Normals,
               Bound().HasNormal,
               positionBytes),
      crossing(&Bound().Tan,
               &Bound().Held[(size_t)SubjectResidency::Stream::Tangent],
               pose.Tangents,
               Bound().HasTangent,
               quadBytes),
      crossing(&Bound().Uv,
               &Bound().Held[(size_t)SubjectResidency::Stream::Uv],
               pose.Uv,
               Bound().HasUv,
               pairBytes),
      crossing(&Bound().Uv1,
               &Bound().Held[(size_t)SubjectResidency::Stream::Uv1],
               pose.Uv1,
               Bound().HasUv1,
               pairBytes),
      crossing(&Bound().Col,
               &Bound().Held[(size_t)SubjectResidency::Stream::Colour],
               pose.Colours,
               Bound().HasColour,
               quadBytes),
      crossing(&Bound().Prev,
               &Bound().Held[(size_t)SubjectResidency::Stream::Previous],
               previousPose,
               WritesVelocity,
               positionBytes),
  };
  if (!HandPlacements(deferred, error)) { return false; }
  if (!Bound().Cross(streams, sizeof streams / sizeof streams[0], deferred, error)) {
    Bound().NIdx = 0;
    return false;
  }
  if (!Bound().Vtx || !Bound().Emit || (WritesVelocity && !Bound().Prev) ||
      (Bound().HasColour && !Bound().Col)) {
    Bound().NIdx = 0;
    error = std::string("the subject's vertex streams did not reach the device: ") + SDL_GetError();
    return false;
  }
  return true;
}

bool SubjectDraw::HandClusters(const SubjectMesh &mesh, std::string &error) {
  const Heap::Tagged uploading("mesh-cull");
  Args_.clear();
  Jobs_ = 0;
  if (mesh.Draws == nullptr) { return true; }
  const std::vector<uint32_t> &jobs = mesh.Draws->ClusterJobs();
  if (jobs.empty() || mesh.ClusterSpheres.empty()) { return true; }

  Args_.assign(Batches.size() * 5u, 0u);
  std::vector<uint32_t> rows(Batches.size() * 2u, 0u);
  uint32_t base = 0;
  for (size_t at = 0; at < Batches.size(); ++at) {
    const DrawBatch &batch = Batches[at];
    Args_[at * 5u + 1u] = batch.Instances;
    Args_[at * 5u + 2u] = base;
    Args_[at * 5u + 4u] = batch.ModelSlot;
    rows[at * 2u] = batch.FirstJob;
    rows[at * 2u + 1u] = batch.JobCount;
    for (uint32_t one = 0; one < batch.JobCount; ++one) {
      base += jobs[((size_t)batch.FirstJob + one) * DrawList::kJobWords + 3u];
    }
  }
  Jobs_ = (uint32_t)(jobs.size() / DrawList::kJobWords);
  if (base == 0) {
    Args_.clear();
    Jobs_ = 0;
    return true;
  }

  const auto read = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ;
  SubjectResidency::Crossing cut[] = {
      {&Bound().ClusterSpheres,
       &Bound().Held[(size_t)SubjectResidency::Stream::ClusterSpheres],
       read,
       mesh.ClusterSpheres.data(),
       (uint32_t)(mesh.ClusterSpheres.size() * sizeof(float))},
      {&Bound().ClusterJobs,
       &Bound().Held[(size_t)SubjectResidency::Stream::ClusterJobs],
       read,
       jobs.data(),
       (uint32_t)(jobs.size() * sizeof(uint32_t))},
      {&Bound().ClusterBatches,
       &Bound().Held[(size_t)SubjectResidency::Stream::ClusterBatches],
       read,
       rows.data(),
       (uint32_t)(rows.size() * sizeof(uint32_t))},
  };
  if (!Bound().Cross(cut, sizeof cut / sizeof cut[0], false, error)) {
    Args_.clear();
    Jobs_ = 0;
    return false;
  }

  if (!Room(Bound().ClusterKept,
            SubjectResidency::Stream::ClusterKept,
            SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ | SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,
            Jobs_ * (uint32_t)sizeof(uint32_t)) ||
      !Room(Bound().ClusterSlot,
            SubjectResidency::Stream::ClusterSlot,
            SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ | SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,
            Jobs_ * (uint32_t)sizeof(uint32_t)) ||
      !Room(Bound().DrawIdx,
            SubjectResidency::Stream::DrawIndex,
            SDL_GPU_BUFFERUSAGE_INDEX | SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,
            base * (uint32_t)sizeof(uint32_t)) ||
      !Room(Bound().DrawArgs,
            SubjectResidency::Stream::DrawArguments,
            SDL_GPU_BUFFERUSAGE_INDIRECT | SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,
            (uint32_t)(Args_.size() * sizeof(uint32_t)))) {
    Args_.clear();
    Jobs_ = 0;
    error = std::string("the cut's compacted run found no room on the device: ") + SDL_GetError();
    return false;
  }
  return HandDrawArguments(false, error);
}

bool SubjectDraw::HandDrawArguments(bool deferred, std::string &error) {
  if (Args_.empty() || !Bound().DrawArgs) { return true; }
  for (size_t at = 0; at * 5u < Args_.size(); ++at) { Args_[at * 5u] = 0u; }
  const uint32_t bytes = (uint32_t)(Args_.size() * sizeof(uint32_t));
  SubjectResidency::Crossing table[] = {
      {&Bound().DrawArgs,
       &Bound().Held[(size_t)SubjectResidency::Stream::DrawArguments],
       SDL_GPU_BUFFERUSAGE_INDIRECT,
       Args_.data(),
       bytes}};
  return Bound().Cross(table, 1, deferred, error);
}

bool SubjectDraw::HandPlacements(bool deferred, std::string &error) {
  size_t needed = Placed_.size() / 16u;
  for (const DrawBatch &batch : Batches) {
    needed = std::max(needed, (size_t)batch.ModelSlot + (size_t)batch.Instances);
  }
  if (!RowsStale_ && Rows_.size() == needed * 32u) { return true; }
  RowsStale_ = false;
  if (needed == 0) { return true; }
  Rows_.assign(needed * 32u, 0.0f);
  for (size_t row = 0; row < needed; ++row) {
    const bool placed = row * 16u + 16u <= Placed_.size();
    const double *const now = placed ? Placed_.data() + row * 16u : Model;
    const bool carried = placed ? row < Stamped_.size() && Stamped_[row] != 0u : ModelStamp_ != 0u;
    const double *const was = !carried ? now : (placed ? Before_.data() + row * 16u : ModelBefore_);
    for (size_t at = 0; at < 16u; ++at) {
      Rows_[row * 32u + at] = (float)now[at];
      Rows_[row * 32u + 16u + at] = (float)was[at];
    }
  }
  SubjectResidency::Crossing rows[] = {
      {&Bound().Placed,
       &Bound().Held[(size_t)SubjectResidency::Stream::Placements],
       SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ | SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ,
       Rows_.data(),
       (uint32_t)(Rows_.size() * sizeof(float))}};
  return Bound().Cross(rows, 1, deferred, error);
}

bool SubjectDraw::SetPose(const SubjectPose &pose, std::string &error) {
  ++Reshaped_;
  if (Borrows()) { return true; }
  if (Bound().NIdx == 0) {
    error = "a pose arrived before any mesh, and there is no subject for it to be a pose of";
    return false;
  }
  if (pose.VertexCount != Bound().NVerts) {
    error = "the pose carries " + std::to_string(pose.VertexCount) +
            " vertices and the subject has " + std::to_string(Bound().NVerts) +
            ", so it is a different body rather than the same one moved";
    return false;
  }
  if (!pose.Verts.Stands() || !pose.Emitted.Stands()) {
    error = "a pose arrived without positions or emitted radiance, which every draw binds";
    return false;
  }
  for (int axis = 0; axis < 3; ++axis) { Anchor[axis] = pose.Anchor[axis]; }
  if (ModelStamp_ != Frame_) {
    const double *const from = ModelStamp_ == 0u ? pose.Model : Model;
    for (int part = 0; part < 16; part++) { ModelBefore_[part] = from[part]; }
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

std::array<float, SubjectDraw::kLightFloats>
SubjectDraw::PackedLights(const FrameContext &ctx) const {
  std::array<float, kLightFloats> packed{};
  packed[0] = (float)Placed.size();
  packed[1] = 0.0f;
  packed[2] = Shadowed_ ? 1.0f : 0.0f;
  packed[3] = 1.0f / (float)kShadowAtlasPx;
  for (int channel = 0; channel < 3; ++channel) {
    packed[4 + channel] = (float)IndirectLight.RadianceLinear[channel];
    packed[8 + channel] = (float)IndirectLight.GroundLinear[channel];
    packed[12 + channel] = (float)IndirectLight.UpUnit[channel];
  }
  for (size_t at = 0; at < Placed.size(); ++at) {
    const PunctualLight &light = Placed[at].Light;
    float *entry = packed.data() + 16 + at * 4u * (size_t)kLightVec4s;
    for (int channel = 0; channel < 3; ++channel) {
      entry[channel] = light.Colour[channel] * light.Intensity;
    }
    entry[3] = light.Kind == LightKind::Directional
                   ? 0.0f
                   : (light.Kind == LightKind::Point ? 1.0f : 2.0f);
    for (int axis = 0; axis < 3; ++axis) {
      entry[4 + axis] = (float)(Placed[at].PositionEcefM[axis] + ctx.PreViewTranslation[axis]);
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

namespace {}

uint32_t SubjectDraw::DrawCount() const {
  uint32_t drawn = 0;
  for (const DrawBatch &batch : Batches) { drawn += batch.Draws; }
  return drawn;
}

static_assert(sizeof(SDL_GPUIndexedIndirectDrawCommand) == 5u * sizeof(uint32_t),
              "the indirect argument table is written as five uints a batch");
inline constexpr size_t kIndirectStride = sizeof(SDL_GPUIndexedIndirectDrawCommand);

void SubjectDraw::Encode(const FrameContext &ctx, const PassRecording &into) {
  if (Bound().NIdx == 0 || Batches.empty() || !Bound().Vtx || !Bound().Idx || !Bound().Emit) {
    return;
  }
  float uniform[kUniFloats] = {};
  const auto place = [this, &ctx, &uniform, &into]() {
    for (int axis = 0; axis < 3; ++axis) {
      uniform[48 + axis] = (float)(Anchor[axis] + ctx.PreViewTranslation[axis]);
      uniform[52 + axis] = (float)(Anchor[axis] + ctx.PrevPreViewTranslation[axis]);
    }
    for (int i = 0; i < 16; i++) { uniform[i] = ctx.Mvp16[i]; }
    for (int i = 0; i < 16; i++) { uniform[16 + i] = ctx.PrevMvp16[i]; }
    for (int i = 0; i < 16; i++) { uniform[32 + i] = (float)LightFromWorld_[i]; }
    ++UniformPushes_;
    SDL_PushGPUVertexUniformData(into.Commands, 0, uniform, sizeof uniform);
  };
  place();
  const std::array<float, kLightFloats> lights = PackedLights(ctx);
  ShadowedFrames_ += lights[2] > 0.5f ? 1u : 0u;
  SDL_PushGPUFragmentUniformData(
      into.Commands, 1, lights.data(), (uint32_t)(lights.size() * sizeof(float)));

  bool boundCut = false;
  bool anyIndex = false;

  SDL_GPUBuffer *const rows[1] = {Bound().Placed.Get()};
  SDL_BindGPUVertexStorageBuffers(into.Pass, 0, rows, 1);

  size_t bound = kPipelines;
  size_t boundSlot = 0;
  bool slotBound = false;
  const bool cut = Bound().DrawIdx && Bound().DrawArgs && !Args_.empty();
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

    const size_t wantedPipeline = PipelineAt(wanted, surface.Kind, surface.CullsBack);
    if (wantedPipeline != bound) {
      SDL_BindGPUGraphicsPipeline(into.Pass, Pipelines[wantedPipeline].Get());

      SDL_GPUBufferBinding runs[VertexShape::kRuns] = {};
      uint32_t count = 0;
      runs[count++] = SDL_GPUBufferBinding{Bound().Vtx.Get(), 0};
      if (textured) { runs[count++] = SDL_GPUBufferBinding{Bound().Uv.Get(), 0}; }
      if (secondUv) { runs[count++] = SDL_GPUBufferBinding{Bound().Uv1.Get(), 0}; }
      runs[count++] = SDL_GPUBufferBinding{lit ? Bound().Nrm.Get() : Bound().Emit.Get(), 0};
      if (mapped) { runs[count++] = SDL_GPUBufferBinding{Bound().Tan.Get(), 0}; }
      if (tinted) { runs[count++] = SDL_GPUBufferBinding{Bound().Col.Get(), 0}; }

      if (WritesVelocity) { runs[count++] = SDL_GPUBufferBinding{Bound().Prev.Get(), 0}; }
      SDL_BindGPUVertexBuffers(into.Pass, 0, runs, count);
      bound = wantedPipeline;
    }
    if (!slotBound || boundSlot != batch.MaterialSlot) {
      const SDL_GPUTextureSamplerBinding images[kSubjectImages] = {
          {surface.Colour.Image.Get(), surface.Colour.Sample.Get()},
          {surface.Normal.Image.Get(), surface.Normal.Sample.Get()},
          {surface.MetalRough.Image.Get(), surface.MetalRough.Sample.Get()},
          {surface.Emissive.Image.Get(), surface.Emissive.Sample.Get()},
          {surface.SpecularStrength.Image.Get(), surface.SpecularStrength.Sample.Get()},
          {surface.SpecularTint.Image.Get(), surface.SpecularTint.Sample.Get()},

          {Behind != nullptr ? Behind : surface.Colour.Image.Get(),
           BehindSampler != nullptr ? BehindSampler : surface.Colour.Sample.Get()},

          {Atlas_ != nullptr ? Atlas_ : surface.Colour.Image.Get(),
           AtlasSampler_ != nullptr ? AtlasSampler_ : surface.Colour.Sample.Get()}};
      SDL_BindGPUFragmentSamplers(into.Pass, 0, images, kSubjectImages);
      if (SkyIrradiance_ != nullptr) {
        SDL_BindGPUFragmentStorageBuffers(into.Pass, 0, &SkyIrradiance_, 1u);
      }
      SDL_PushGPUFragmentUniformData(
          into.Commands, 0, surface.Row.data(), (uint32_t)(surface.Row.size() * sizeof(float)));
      boundSlot = batch.MaterialSlot;
      slotBound = true;
    }
    if (!anyIndex || boundCut != culled) {
      SDL_GPUBufferBinding indices{culled ? Bound().DrawIdx.Get() : Bound().Idx.Get(), 0};
      SDL_BindGPUIndexBuffer(into.Pass, &indices, SDL_GPU_INDEXELEMENTSIZE_32BIT);
      boundCut = culled;
      anyIndex = true;
    }
    if (culled) {
      SDL_DrawGPUIndexedPrimitivesIndirect(
          into.Pass, Bound().DrawArgs.Get(), (Uint32)(at * kIndirectStride), 1u);
      continue;
    }
    SDL_DrawGPUIndexedPrimitives(
        into.Pass, batch.IndexCount, batch.Instances, batch.FirstIndex, 0, batch.ModelSlot);
  }
}

} // namespace outshine::Render
