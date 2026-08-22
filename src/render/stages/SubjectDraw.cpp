#include "SubjectDraw.h"

#include <span>
#include <new>

#include "Heap.h"

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
#include "ShadowRay.h"
#include "SurfaceState.h"
#include "TriangleBvh.h"
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

}


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

VertexShape ShapeOf(VertexLayout layout, bool writesVelocity) {
  const bool textured = CarriesUv(layout);
  const bool lit = CarriesNormal(layout);
  const bool mapped = CarriesTangent(layout);
  VertexShape shape;
  shape.Buffers[shape.Count] = Run(shape.Count, 3);
  shape.Attributes[shape.Count] = At(0, shape.Count, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3);
  ++shape.Count;
  if (textured) {
    shape.Buffers[shape.Count] = Run(shape.Count, 2);
    shape.Attributes[shape.Count] = At(1, shape.Count, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2);
    ++shape.Count;
  }

  if (CarriesUv1(layout)) {
    shape.Buffers[shape.Count] = Run(shape.Count, 2);
    shape.Attributes[shape.Count] = At(6, shape.Count, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2);
    ++shape.Count;
  }

  shape.Buffers[shape.Count] = Run(shape.Count, 3);
  shape.Attributes[shape.Count] = At(lit ? 3 : 2, shape.Count, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3);
  ++shape.Count;
  if (mapped) {
    shape.Buffers[shape.Count] = Run(shape.Count, 4);
    shape.Attributes[shape.Count] = At(4, shape.Count, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4);
    ++shape.Count;
  }

  if (CarriesColour(layout)) {
    shape.Buffers[shape.Count] = Run(shape.Count, 4);
    shape.Attributes[shape.Count] = At(7, shape.Count, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4);
    ++shape.Count;
  }

  if (writesVelocity) {
    shape.Buffers[shape.Count] = Run(shape.Count, 3);
    shape.Attributes[shape.Count] = At(5, shape.Count, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3);
    ++shape.Count;
  }
  return shape;
}

SDL_GPUShader *MakeShader(SDL_GPUDevice *device, const std::string &source, const char *entry,
                          SDL_GPUShaderStage stage) {
  const bool fragment = stage == SDL_GPU_SHADERSTAGE_FRAGMENT;
  SDL_GPUShaderCreateInfo wanted{};
  wanted.code = reinterpret_cast<const Uint8 *>(source.c_str());
  wanted.code_size = source.size();
  wanted.entrypoint = entry;
  wanted.format = SDL_GPU_SHADERFORMAT_MSL;
  wanted.stage = stage;
  const DrawShape &shape = SubjectDraw::ShaderShape;
  wanted.num_samplers = fragment ? shape.FragmentSamplers : shape.VertexSamplers;
  wanted.num_storage_buffers = fragment ? shape.FragmentStorageBuffers : shape.VertexStorageBuffers;
  wanted.num_uniform_buffers = fragment ? shape.FragmentUniformBuffers : shape.VertexUniformBuffers;
  return SDL_CreateGPUShader(device, &wanted);
}

}

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
      case SurfaceKind::Refractive: return "fsLitTransmissive";
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
  const std::string shadowRay = ShadowRayMsl(error);
  const std::string brdf = MetalRoughBrdfMsl(error);
  const std::string sheen = SheenLobeMsl(error);
  const std::string iridescence = IridescenceLobeMsl(error);
  const std::string energy = MicrofacetEnergyMsl(error);
  const std::string normalMap = NormalFromMapMsl(error);
  if (shadowRay.empty() || brdf.empty() || sheen.empty() || iridescence.empty() ||
      energy.empty() || normalMap.empty()) {
    return std::string();
  }
  return MslPrelude() + VelocityStaticDefine() + kVelocityMsl + shadowRay +
         "\n#define SUBJECT_WRITES_VELOCITY " + (options.WritesVelocity ? "1" : "0") +
         "\n#define SUBJECT_WRITES_SHADING_NORMAL " + (options.NormalIndex >= 0 ? "1" : "0") +
         "\n#define SUBJECT_NORMAL_COLOUR_INDEX " +
         std::to_string(options.NormalIndex < 0 ? 0 : options.NormalIndex) +
         "\n#define SUBJECT_WRITES_SURFACE_IDENTITY " + (options.IdentityIndex >= 0 ? "1" : "0") +
         "\n#define SUBJECT_IDENTITY_COLOUR_INDEX " +
         std::to_string(options.IdentityIndex < 0 ? 0 : options.IdentityIndex) + "\n" + bindings +
         body + brdf + sheen + iridescence + energy + lit + litTextured + normalMap + mapped;
}

std::string SubjectDraw::DepthOnlySource(void) {
  std::string ignored;
  return DepthOnlySource(ignored);
}

std::string SubjectDraw::DepthOnlySource(std::string &error) {
  std::string body;
  if (!LoadShaderText("src/render/shaders/subjectDepthOnly.msl", body, error)) {
    return std::string();
  }
  return MslPrelude() + body;
}

bool SubjectDraw::Configure(const Gpu &gpu, std::string &error) {
  Device = gpu.Device;
  Resident_.Device = gpu.Device;
  Resident_.FiltersFloat32 = gpu.FiltersFloat32;

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
  for (const SurfaceKind kind : {SurfaceKind::Opaque, SurfaceKind::Masked, SurfaceKind::Blended,
                                 SurfaceKind::ThinTransmissive, SurfaceKind::Refractive}) {
    if (!glass && (kind == SurfaceKind::ThinTransmissive || kind == SurfaceKind::Refractive)) {
      continue;
    }

    const bool blends = kind == SurfaceKind::Blended;
    SDL_GPUColorTargetDescription targets[kMaxColourAttachments] = {};
    targets[0].format = gpu.HdrFormat;
    if (blends) { targets[0].blend_state = OverBlend(); }

    if (writesVelocity) { targets[attachmentIndex(Resource::SceneVelocity)] = VelocityTarget(!blends); }

    if (normalIndex >= 0) { targets[normalIndex].format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT; }

    if (identityIndex >= 0) {
      targets[identityIndex].format = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
    }

    for (const VertexLayoutRow &row : kVertexLayouts) {
      const VertexLayout layout = row.Layout;
      const VertexShape shape = ShapeOf(layout, WritesVelocity);
      const OwnedShader vertex(Device, MakeShader(Device, source, VertexEntry(layout),
                                                  SDL_GPU_SHADERSTAGE_VERTEX));
      const OwnedShader fragment(Device, MakeShader(Device, source, FragmentEntry(kind, layout),
                                                   SDL_GPU_SHADERSTAGE_FRAGMENT));
      if (!vertex || !fragment) {
        error = std::string("the subject's shader did not compile at ") +
                VertexEntry(layout) + "/" + FragmentEntry(kind, layout) + ": " +
                SDL_GetError();
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
  slot.Colour = Resident_.Upload(material.Colour, SubjectResidency::Transfer::Srgb, TexelKind::Value);
  slot.Normal = Resident_.Upload(material.Normal, SubjectResidency::Transfer::Linear, TexelKind::Direction);
  slot.MetalRough = Resident_.Upload(material.MetalRough, SubjectResidency::Transfer::Linear, TexelKind::Value);
  slot.Emissive = Resident_.Upload(material.Emissive, SubjectResidency::Transfer::Srgb, TexelKind::Value);

  slot.SpecularStrength = Resident_.Upload(material.SpecularStrength, SubjectResidency::Transfer::Linear, TexelKind::Value);
  slot.SpecularTint = Resident_.Upload(material.SpecularTint, SubjectResidency::Transfer::Srgb, TexelKind::Value);

  const Material &row = material.Row;

  const float identity = (float)(Slots.size() + 1u);

  float f0[3];
  DielectricF0(row, f0);

  const float scalars[] = {
              material.Coverage(), material.State().CoverageCut(),
              row.Metalness,       row.Roughness,
              row.BaseColour[0],   row.BaseColour[1], row.BaseColour[2], row.BaseColour[3],
              row.Emission[0],     row.Emission[1],   row.Emission[2],   material.NormalScale,
              identity,            f0[0],             f0[1],             f0[2],
              DielectricF90(row),
              row.Transmission,    row.Thickness,     row.AttenuationDistance,
              row.AttenuationColour[0], row.AttenuationColour[1], row.AttenuationColour[2],
              row.SheenColour[0], row.SheenColour[1], row.SheenColour[2], row.SheenRoughness,
              row.Clearcoat,      row.ClearcoatRoughness,
              row.Anisotropy,     row.AnisotropyRotationRad,
              row.Iridescence,    row.IridescenceIor,
              row.IridescenceThicknessMinNm, row.IridescenceThicknessMaxNm};
  static_assert(sizeof scalars / sizeof scalars[0] == (size_t)kSurfaceScalars,
                "the surface row and its declared length are one statement");
  std::copy(std::begin(scalars), std::end(scalars), slot.Row.begin());

  const SubjectTexture *const images[kSubjectMaterialImages] = {&material.Colour, &material.Normal,
                                                        &material.MetalRough, &material.Emissive,
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

bool SubjectDraw::SetMaterials(std::span<const SubjectMaterial> materials, std::string &error) {

  Slots.clear();
  Batches.clear();
  BatchLayout.clear();
  Resident_.NIdx = 0;
  if (!Device) {
    error = "the subject unit has no device, so no surface can be bound";
    return false;
  }
  if (!Resident_.FiltersFloat32) {
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

  Resident_.DropStaged();
  Resident_.NVerts = mesh.VertexCount;
  Resident_.NIdx = mesh.IndexCount;
  Resident_.HasUv = mesh.Uv != nullptr;
  Resident_.HasUv1 = mesh.Uv1 != nullptr;
  Resident_.HasNormal = mesh.Normals != nullptr;
  Resident_.HasTangent = mesh.Tangents != nullptr;
  Resident_.HasColour = mesh.Colours != nullptr;
  Batches.clear();
  BatchLayout.clear();
  for (int axis = 0; axis < 3; ++axis) {
    Anchor[axis] = mesh.Anchor[axis];
    PrevAnchor[axis] = mesh.PrevAnchor[axis];
  }
  for (int part = 0; part < 16; part++) { Model[part] = mesh.Model[part]; }
  if (Resident_.NVerts == 0 || Resident_.NIdx == 0 || !Device || !mesh.Emitted || !mesh.Verts || !mesh.Indices ||
      !mesh.Draws) {
    Resident_.NIdx = 0;
    return true;
  }

  if (mesh.PrevVerts != nullptr && !WritesVelocity) {
    Resident_.NIdx = 0;
    error = "the mesh carries a previous pose and the pass attaches no velocity target, so the run "
            "would reach no shader";
    return false;
  }
  for (const DrawBatch &batch : mesh.Draws->Batches()) {

    if (batch.MaterialSlot < Slots.size() && Slots[batch.MaterialSlot].ReadsSecondUv &&
        !(CarriesUv1(batch.Layout) && Resident_.HasUv1)) {
      Resident_.NIdx = 0;
      error = "surface slot " + std::to_string(batch.MaterialSlot) +
              " reads an image from the second uv set and the draw wearing it " +
              (Resident_.HasUv1 ? "takes a vertex layout that binds no second run"
                      : "has no second uv run at all") +
              ", and the first set is not a substitute for it";
      return false;
    }
    if (batch.MaterialSlot >= Slots.size()) {
      Resident_.NIdx = 0;
      error = "a draw names surface slot " + std::to_string(batch.MaterialSlot) +
              " over a table of " + std::to_string(Slots.size()) + " surfaces";
      return false;
    }
    if (batch.FirstIndex + batch.IndexCount > Resident_.NIdx) {
      Resident_.NIdx = 0;
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
    carried.Uv = CarriesUv(batch.Layout) && Resident_.HasUv;
    carried.Normal = CarriesNormal(batch.Layout) && Resident_.HasNormal;
    carried.Tangent = CarriesTangent(batch.Layout) && carried.Normal && carried.Uv && Resident_.HasTangent;
    carried.Uv1 = CarriesUv1(batch.Layout) && carried.Uv && Resident_.HasUv1;
    carried.Colour = CarriesColour(batch.Layout) && Resident_.HasColour;
    VertexLayout drawn = VertexLayout::Position;
    if (!LayoutOf(carried, drawn)) {
      Resident_.NIdx = 0;
      Batches.clear();
      BatchLayout.clear();
      error = "a draw's runs name no vertex layout this engine builds, and the nearest one is not "
              "an answer -- the combination is what the enumeration exists to refuse";
      return false;
    }
    BatchLayout.push_back(drawn);
  }

  {
    const Heap::Tagged uploading("mesh-upload");
    Resident_.Idx = Resident_.Fill(SDL_GPU_BUFFERUSAGE_INDEX, mesh.Indices, Resident_.NIdx * (uint32_t)sizeof(uint32_t));
  }
  if (!Resident_.Idx) {
    Resident_.NIdx = 0;
    error = std::string("the subject's index run did not reach the device: ") + SDL_GetError();
    return false;
  }
  if (!HandStreams(mesh, false, error)) { return false; }

  {
    const Heap::Tagged building("mesh-bvh");

    Visibility_ = TriangleBvh::Over(Span<const float>(mesh.Verts, (size_t)Resident_.NVerts * 3u),
                                    Span<const uint32_t>(mesh.Indices, (size_t)Resident_.NIdx));
  }
  if (Visibility_.Empty()) {
    Resident_.NIdx = 0;
    error = "the subject's " + std::to_string(Resident_.NIdx / 3u) +
            " triangles built no visibility structure, so no light could be occluded by them";
    return false;
  }
  return HandVisibility(false, error);
}

bool SubjectDraw::HandStreams(const SubjectPose &pose, bool deferred, std::string &error) {
  const Heap::Tagged uploading("mesh-upload");
  const uint32_t positionBytes = Resident_.NVerts * 3u * (uint32_t)sizeof(float);
  const uint32_t pairBytes = Resident_.NVerts * 2u * (uint32_t)sizeof(float);
  const uint32_t quadBytes = Resident_.NVerts * 4u * (uint32_t)sizeof(float);
  const float *const previousPose = pose.PrevVerts != nullptr ? pose.PrevVerts : pose.Verts;
  const auto vertex = SDL_GPU_BUFFERUSAGE_VERTEX;

  SubjectResidency::Crossing streams[] = {
      {&Resident_.Vtx, &Resident_.Held[(size_t)SubjectResidency::Stream::Vertex], vertex, pose.Verts, positionBytes},
      {&Resident_.Emit, &Resident_.Held[(size_t)SubjectResidency::Stream::Emitted], vertex, pose.Emitted, positionBytes},
      {&Resident_.Nrm, &Resident_.Held[(size_t)SubjectResidency::Stream::Normal], vertex, Resident_.HasNormal ? pose.Normals : nullptr,
       Resident_.HasNormal ? positionBytes : 0u},
      {&Resident_.Tan, &Resident_.Held[(size_t)SubjectResidency::Stream::Tangent], vertex, Resident_.HasTangent ? pose.Tangents : nullptr,
       Resident_.HasTangent ? quadBytes : 0u},
      {&Resident_.Uv, &Resident_.Held[(size_t)SubjectResidency::Stream::Uv], vertex, Resident_.HasUv ? pose.Uv : nullptr, Resident_.HasUv ? pairBytes : 0u},
      {&Resident_.Uv1, &Resident_.Held[(size_t)SubjectResidency::Stream::Uv1], vertex, Resident_.HasUv1 ? pose.Uv1 : nullptr,
       Resident_.HasUv1 ? pairBytes : 0u},
      {&Resident_.Col, &Resident_.Held[(size_t)SubjectResidency::Stream::Colour], vertex, Resident_.HasColour ? pose.Colours : nullptr,
       Resident_.HasColour ? quadBytes : 0u},
      {&Resident_.Prev, &Resident_.Held[(size_t)SubjectResidency::Stream::Previous], vertex, WritesVelocity ? previousPose : nullptr,
       WritesVelocity ? positionBytes : 0u},
  };
  if (!Resident_.Cross(streams, sizeof streams / sizeof streams[0], deferred, error)) {
    Resident_.NIdx = 0;
    return false;
  }
  if (!Resident_.Vtx || !Resident_.Emit || (WritesVelocity && !Resident_.Prev) || (Resident_.HasColour && !Resident_.Col)) {
    Resident_.NIdx = 0;
    error = std::string("the subject's vertex streams did not reach the device: ") + SDL_GetError();
    return false;
  }
  return true;
}

bool SubjectDraw::HandVisibility(bool deferred, std::string &error) {
  const Heap::Tagged uploading("mesh-upload");
  const auto storage = SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ;

  SubjectResidency::Crossing structure[] = {
      {&Resident_.BvhNodes, &Resident_.Held[(size_t)SubjectResidency::Stream::BvhNodes], storage, Visibility_.Nodes().Data(),
       (uint32_t)Visibility_.Nodes().Bytes()},
      {&Resident_.BvhTris, &Resident_.Held[(size_t)SubjectResidency::Stream::BvhTriangles], storage, Visibility_.Triangles().Data(),
       (uint32_t)Visibility_.Triangles().Bytes()},
  };
  if (!Resident_.Cross(structure, sizeof structure / sizeof structure[0], deferred, error)) {
    Resident_.NIdx = 0;
    return false;
  }
  if (!Resident_.BvhNodes || !Resident_.BvhTris) {
    Resident_.NIdx = 0;
    error = std::string("the subject's visibility structure did not reach the device: ") +
            SDL_GetError();
    return false;
  }

  const BvhNode &root = Visibility_.Nodes()[0];
  float diagonal = 0.0f;
  for (int axis = 0; axis < 3; ++axis) {
    const float span = root.MaxM[axis] - root.MinM[axis];
    diagonal += span * span;
  }
  ShadowNearM_ = std::sqrt(diagonal) * kShadowRayNearFraction;
  return true;
}

bool SubjectDraw::SetPose(const SubjectPose &pose, std::string &error) {
  if (Resident_.NIdx == 0 || Visibility_.Empty()) {
    error = "a pose arrived before any mesh, and there is no subject for it to be a pose of";
    return false;
  }
  if (pose.VertexCount != Resident_.NVerts) {
    error = "the pose carries " + std::to_string(pose.VertexCount) + " vertices and the subject has " +
            std::to_string(Resident_.NVerts) + ", so it is a different body rather than the same one moved";
    return false;
  }
  if (!pose.Verts || !pose.Emitted) {
    error = "a pose arrived without positions or emitted radiance, which every draw binds";
    return false;
  }
  for (int axis = 0; axis < 3; ++axis) {
    Anchor[axis] = pose.Anchor[axis];
    PrevAnchor[axis] = pose.PrevAnchor[axis];
  }
  for (int part = 0; part < 16; part++) { Model[part] = pose.Model[part]; }
  if (!HandStreams(pose, true, error)) { return false; }
  {
    const Heap::Tagged refitting("mesh-bvh");
    if (!Visibility_.Refit(Span<const float>(pose.Verts, (size_t)Resident_.NVerts * 3u))) {
      error = "the subject's visibility structure did not refit to this pose";
      return false;
    }
  }
  return HandVisibility(true, error);
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

std::array<float, SubjectDraw::kLightFloats> SubjectDraw::PackedLights(
    const FrameContext &ctx) const {
  std::array<float, kLightFloats> packed{};
  packed[0] = (float)Placed.size();
  packed[1] = ShadowNearM_;
  for (int channel = 0; channel < 3; ++channel) {
    packed[4 + channel] = (float)Environment.RadianceLinear[channel];
  }
  for (size_t at = 0; at < Placed.size(); ++at) {
    const PunctualLight &light = Placed[at].Light;
    float *entry = packed.data() + 8 + at * 4u * (size_t)kLightVec4s;
    for (int channel = 0; channel < 3; ++channel) {
      entry[channel] = light.Colour[channel] * light.Intensity;
    }
    entry[3] = light.Kind == LightKind::Directional ? 0.0f
                                                    : (light.Kind == LightKind::Point ? 1.0f : 2.0f);
    for (int axis = 0; axis < 3; ++axis) {
      entry[4 + axis] = (float)(Placed[at].PositionEcefM[axis] - ctx.Eye[axis]);
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

namespace {


}

bool SubjectDraw::ConfigureDepthOnly(const Gpu &gpu, std::string &error) {
  if (DepthOnly_) { return true; }
  SDL_GPUDevice *const device = gpu.Device;
  if (device == nullptr) {
    error = "the subject unit has no device, so no depth-only pipeline can be built";
    return false;
  }
  const std::string source = DepthOnlySource(error);
  if (source.empty()) { return false; }
  SDL_GPUShaderCreateInfo wanted{};
  wanted.code = reinterpret_cast<const Uint8 *>(source.c_str());
  wanted.code_size = source.size();
  wanted.format = SDL_GPU_SHADERFORMAT_MSL;
  wanted.entrypoint = "vsDepth";
  wanted.stage = SDL_GPU_SHADERSTAGE_VERTEX;
  wanted.num_samplers = DepthOnlyShape.VertexSamplers;
  wanted.num_uniform_buffers = DepthOnlyShape.VertexUniformBuffers;
  const OwnedShader vertex(device, SDL_CreateGPUShader(device, &wanted));
  wanted.entrypoint = "fsDepth";
  wanted.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
  wanted.num_samplers = DepthOnlyShape.FragmentSamplers;
  wanted.num_uniform_buffers = DepthOnlyShape.FragmentUniformBuffers;
  const OwnedShader fragment(device, SDL_CreateGPUShader(device, &wanted));
  if (!vertex || !fragment) {
    error = std::string("the depth-only shaders were refused: ") + SDL_GetError();
    return false;
  }

  SDL_GPUVertexBufferDescription buffer{};
  buffer.slot = 0;
  buffer.pitch = 3 * sizeof(float);
  buffer.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
  SDL_GPUVertexAttribute attribute{};
  attribute.location = 0;
  attribute.buffer_slot = 0;
  attribute.format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
  attribute.offset = 0;

  SDL_GPUGraphicsPipelineCreateInfo pipeline{};
  pipeline.vertex_shader = vertex.Get();
  pipeline.fragment_shader = fragment.Get();
  pipeline.vertex_input_state.vertex_buffer_descriptions = &buffer;
  pipeline.vertex_input_state.num_vertex_buffers = 1;
  pipeline.vertex_input_state.vertex_attributes = &attribute;
  pipeline.vertex_input_state.num_vertex_attributes = 1;
  pipeline.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
  pipeline.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;

  pipeline.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_FRONT;
  pipeline.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
  pipeline.depth_stencil_state.enable_depth_test = true;
  pipeline.depth_stencil_state.enable_depth_write = true;
  pipeline.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_GREATER;
  pipeline.target_info.num_color_targets = 0;
  pipeline.target_info.has_depth_stencil_target = true;
  pipeline.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
  SDL_GPUGraphicsPipeline *made = SDL_CreateGPUGraphicsPipeline(device, &pipeline);
  if (made == nullptr) {
    error = std::string("the depth-only pipeline was refused: ") + SDL_GetError();
    return false;
  }
  DepthOnly_ = OwnedPipeline(device, made);
  return true;
}

void SubjectDraw::EncodeDepthOnly(const double lightFromWorld16[16], const double eye[3],
                                  int atlasPx, const PassRecording &into) {
  if (!DepthOnly_ || Resident_.NIdx == 0 || Batches.empty() || !Resident_.Vtx || !Resident_.Idx || into.Pass == nullptr) {
    return;
  }
  SDL_GPUViewport square{};
  square.w = (float)atlasPx;
  square.h = (float)atlasPx;
  square.min_depth = 0.0f;
  square.max_depth = 1.0f;
  SDL_SetGPUViewport(into.Pass, &square);
  SDL_BindGPUGraphicsPipeline(into.Pass, DepthOnly_.Get());
  SDL_GPUBufferBinding vertices{Resident_.Vtx.Get(), 0};
  SDL_BindGPUVertexBuffers(into.Pass, 0, &vertices, 1);
  SDL_GPUBufferBinding indices{Resident_.Idx.Get(), 0};
  SDL_BindGPUIndexBuffer(into.Pass, &indices, SDL_GPU_INDEXELEMENTSIZE_32BIT);

  float uniform[16];
  uint32_t standing = ~0u;
  const auto place = [&](uint32_t slot) {
    const double *const model =
        Placed_.empty() ? Model : Placed_.data() + (size_t)slot * 16u;
    double carried[16];
    for (int i = 0; i < 16; i++) { carried[i] = model[i]; }
    for (int axis = 0; axis < 3; ++axis) { carried[12 + axis] += Anchor[axis] - eye[axis]; }
    double placed[16];
    for (int row = 0; row < 4; ++row) {
      for (int column = 0; column < 4; ++column) {
        double sum = 0.0;
        for (int over = 0; over < 4; ++over) {
          sum += lightFromWorld16[over * 4 + row] * carried[column * 4 + over];
        }
        placed[column * 4 + row] = sum;
      }
    }
    for (int i = 0; i < 16; i++) { uniform[i] = (float)placed[i]; }
    SDL_PushGPUVertexUniformData(into.Commands, 0, uniform, sizeof uniform);
  };
  for (const DrawBatch &batch : Batches) {
    if (batch.ModelSlot != standing) {
      place(batch.ModelSlot);
      standing = batch.ModelSlot;
    }
    SDL_DrawGPUIndexedPrimitives(into.Pass, batch.IndexCount, 1, batch.FirstIndex, 0, 0);
  }
}

uint32_t SubjectDraw::DrawCount() const {
  uint32_t drawn = 0;
  for (const DrawBatch &batch : Batches) { drawn += batch.Draws; }
  return drawn;
}

void SubjectDraw::Encode(const FrameContext &ctx, const PassRecording &into) {
  if (Resident_.NIdx == 0 || Batches.empty() || !Resident_.Vtx || !Resident_.Idx || !Resident_.Emit || !Resident_.BvhNodes || !Resident_.BvhTris) { return; }
  float uniform[kUniFloats] = {};
  const auto place = [this, &ctx, &uniform, &into](uint32_t slot) {
    const double *const model =
        Placed_.empty() ? Model : Placed_.data() + (size_t)slot * 16u;

    double carried[16];
    for (int i = 0; i < 16; i++) { carried[i] = model[i]; }
    if (!Placed_.empty()) {
      for (int axis = 0; axis < 3; ++axis) { carried[12 + axis] += Anchor[axis] - ctx.Eye[axis]; }
    }
    double placed[16];
    for (int row = 0; row < 4; ++row) {
      for (int column = 0; column < 4; ++column) {
        double sum = 0.0;
        for (int over = 0; over < 4; ++over) {
          sum += (double)ctx.Mvp16[over * 4 + row] * carried[column * 4 + over];
        }
        placed[column * 4 + row] = sum;
      }
    }
    for (int i = 0; i < 16; i++) { uniform[i] = (float)placed[i]; }
    for (int i = 0; i < 3; i++) {
      uniform[16 + i] = Placed_.empty() ? (float)(Anchor[i] - ctx.Eye[i]) : 0.0f;
    }
    for (int i = 0; i < 16; i++) { uniform[20 + i] = ctx.PrevMvp16[i]; }
    for (int i = 0; i < 3; i++) { uniform[36 + i] = (float)(PrevAnchor[i] - ctx.PrevEye[i]); }

    if (Placed_.empty()) {
      for (int axis = 0; axis < 3; ++axis) { carried[12 + axis] += Anchor[axis] - ctx.Eye[axis]; }
    }
    for (int i = 0; i < 16; i++) { uniform[40 + i] = (float)carried[i]; }
    SDL_PushGPUVertexUniformData(into.Commands, 0, uniform, sizeof uniform);
  };
  place(Batches.empty() ? 0u : Batches.front().ModelSlot);
  uint32_t standing = Batches.empty() ? 0u : Batches.front().ModelSlot;
  const std::array<float, kLightFloats> lights = PackedLights(ctx);
  SDL_PushGPUFragmentUniformData(into.Commands, 1, lights.data(),
                                 (uint32_t)(lights.size() * sizeof(float)));

  SDL_GPUBufferBinding indices{Resident_.Idx.Get(), 0};
  SDL_BindGPUIndexBuffer(into.Pass, &indices, SDL_GPU_INDEXELEMENTSIZE_32BIT);

  SDL_GPUBuffer *const occluders[kSubjectStorageBuffers] = {Resident_.BvhNodes.Get(), Resident_.BvhTris.Get()};
  SDL_BindGPUFragmentStorageBuffers(into.Pass, 0, occluders, kSubjectStorageBuffers);

  size_t bound = kPipelines;
  size_t boundSlot = 0;
  bool slotBound = false;
  for (size_t at = 0; at < Batches.size(); ++at) {
    const DrawBatch &batch = Batches[at];
    const SurfaceSlot &surface = Slots[batch.MaterialSlot];

    const bool glassSlot = surface.Kind == SurfaceKind::ThinTransmissive ||
                           surface.Kind == SurfaceKind::Refractive;
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
      runs[count++] = SDL_GPUBufferBinding{Resident_.Vtx.Get(), 0};
      if (textured) { runs[count++] = SDL_GPUBufferBinding{Resident_.Uv.Get(), 0}; }
      if (secondUv) { runs[count++] = SDL_GPUBufferBinding{Resident_.Uv1.Get(), 0}; }
      runs[count++] = SDL_GPUBufferBinding{lit ? Resident_.Nrm.Get() : Resident_.Emit.Get(), 0};
      if (mapped) { runs[count++] = SDL_GPUBufferBinding{Resident_.Tan.Get(), 0}; }
      if (tinted) { runs[count++] = SDL_GPUBufferBinding{Resident_.Col.Get(), 0}; }

      if (WritesVelocity) { runs[count++] = SDL_GPUBufferBinding{Resident_.Prev.Get(), 0}; }
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
           BehindSampler != nullptr ? BehindSampler : surface.Colour.Sample.Get()}};
      SDL_BindGPUFragmentSamplers(into.Pass, 0, images, kSubjectImages);
      SDL_PushGPUFragmentUniformData(into.Commands, 0, surface.Row.data(),
                                     (uint32_t)(surface.Row.size() * sizeof(float)));
      boundSlot = batch.MaterialSlot;
      slotBound = true;
    }
    if (batch.ModelSlot != standing) {
      place(batch.ModelSlot);
      standing = batch.ModelSlot;
    }
    SDL_DrawGPUIndexedPrimitives(into.Pass, batch.IndexCount, 1, batch.FirstIndex, 0, 0);
  }
}

}
