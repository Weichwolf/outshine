#include "GroundLattice.h"
#include "SurfaceOutputs.h"
#include "ShaderFile.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <utility>

#include <cstring>
#include <format>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace outshine::Render {

namespace {

namespace Says {
constexpr std::string_view kNoDevice = "the ground lattice has no device to stand on";
constexpr std::string_view kPipelineRefused = "the ground lattice's pipeline was refused: {}";
constexpr std::string_view kBufferRefused = "the ground lattice found no room for its {}: {}";
constexpr std::string_view kPageWrongSize =
    "a height page is {} nodes a side and this one brought {} floats";
constexpr std::string_view kPagesFull = "every one of the {} height pages is placed";
constexpr std::string_view kStagingDidNotMap = "the height page's staging did not map: {}";
constexpr std::string_view kCopyAcquireFailed =
    "ground upload could not acquire a copy command buffer: {}";
constexpr std::string_view kCopyPassFailed = "ground upload could not begin a copy pass: {}";
constexpr std::string_view kCopySubmitFailed = "ground upload could not submit its copy: {}";
constexpr std::string_view kVisibleUploadFailed = "ground visibility upload failed: {}";
constexpr std::string_view kTooManyInstances = "the ground lattice has too many instances";
}

using SidePlanes = std::array<std::array<float, 4>, 4>;

[[nodiscard]] SidePlanes SidePlanesOf(const Mat4f &mvp) {
  const auto row = [&mvp](size_t i, size_t k) { return mvp[k * 4u + i]; };
  SidePlanes planes{};
  for (size_t k = 0; k < 4; ++k) {
    planes[0][k] = row(3, k) + row(0, k);
    planes[1][k] = row(3, k) - row(0, k);
    planes[2][k] = row(3, k) + row(1, k);
    planes[3][k] = row(3, k) - row(1, k);
  }
  for (std::array<float, 4> &plane : planes) {
    const float length = std::sqrt(plane[0] * plane[0] + plane[1] * plane[1] + plane[2] * plane[2]);
    if (length > 0.0f) {
      for (float &c : plane) { c /= length; }
    }
  }
  return planes;
}

struct LatticeVertexInput {
  std::array<SDL_GPUVertexBufferDescription, 2> Buffers{};
  std::array<SDL_GPUVertexAttribute, 8> Attributes{};
};

[[nodiscard]] constexpr LatticeVertexInput MakeLatticeVertexInput() {
  LatticeVertexInput in;
  in.Buffers[0].slot = 0;
  in.Buffers[0].pitch = GroundLattice::kGridFloats * static_cast<uint32_t>(sizeof(float));
  in.Buffers[0].input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
  in.Buffers[1].slot = 1;
  in.Buffers[1].pitch = kGroundInstanceFloats * static_cast<uint32_t>(sizeof(float));
  in.Buffers[1].input_rate = SDL_GPU_VERTEXINPUTRATE_INSTANCE;
  in.Buffers[1].instance_step_rate = 0;
  in.Attributes[0].location = 0;
  in.Attributes[0].buffer_slot = 0;
  in.Attributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
  in.Attributes[0].offset = 0;
  for (uint32_t at = 1; at < 8; ++at) {
    in.Attributes[at].location = at;
    in.Attributes[at].buffer_slot = 1;
    in.Attributes[at].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
    in.Attributes[at].offset = (at - 1u) * 4u * static_cast<uint32_t>(sizeof(float));
  }
  return in;
}

static_assert(MakeLatticeVertexInput().Buffers[0].instance_step_rate == 0 &&
              MakeLatticeVertexInput().Buffers[1].instance_step_rate == 0);

struct CopyCommands {
  SDL_GPUCommandBuffer *Commands;
  SDL_GPUCopyPass *Pass;
};

[[nodiscard]] std::optional<CopyCommands> BeginCopy(SDL_GPUDevice *device, std::string &error) {
  SDL_GPUCommandBuffer *const commands = SDL_AcquireGPUCommandBuffer(device);
  if (commands == nullptr) {
    error = std::format(Says::kCopyAcquireFailed, SDL_GetError());
    return std::nullopt;
  }
  SDL_GPUCopyPass *const pass = SDL_BeginGPUCopyPass(commands);
  if (pass == nullptr) {
    error = std::format(Says::kCopyPassFailed, SDL_GetError());
    SDL_CancelGPUCommandBuffer(commands);
    return std::nullopt;
  }
  return CopyCommands{.Commands = commands, .Pass = pass};
}

[[nodiscard]] bool SubmitCopy(CopyCommands copy, std::string &error) {
  SDL_EndGPUCopyPass(copy.Pass);
  if (!SDL_SubmitGPUCommandBuffer(copy.Commands)) {
    error = std::format(Says::kCopySubmitFailed, SDL_GetError());
    return false;
  }
  return true;
}

bool UploadBuffer(SDL_GPUDevice *device,
                  SDL_GPUBuffer *into,
                  const void *from,
                  uint32_t bytes,
                  std::string &error) {
  SDL_GPUTransferBufferCreateInfo room{};
  room.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
  room.size = bytes;
  const OwnedTransfer staging(device, SDL_CreateGPUTransferBuffer(device, &room));
  if (!staging) {
    error = std::format(Says::kBufferRefused, "staging", SDL_GetError());
    return false;
  }
  void *const mapped = SDL_MapGPUTransferBuffer(device, staging.Get(), false);
  if (mapped == nullptr) {
    error = std::format(Says::kStagingDidNotMap, SDL_GetError());
    return false;
  }
  std::memcpy(mapped, from, bytes);
  SDL_UnmapGPUTransferBuffer(device, staging.Get());
  const std::optional<CopyCommands> copy = BeginCopy(device, error);
  if (!copy) { return false; }
  const SDL_GPUTransferBufferLocation source{.transfer_buffer = staging.Get(), .offset = 0};
  const SDL_GPUBufferRegion region{.buffer = into, .offset = 0, .size = bytes};
  SDL_UploadToGPUBuffer(copy->Pass, &source, &region, false);
  return SubmitCopy(*copy, error);
}

}

bool GroundLattice::BuildGrid(std::span<const float> fractions,
                              OwnedBuffer &into,
                              std::string &error) {
  std::vector<float> grid;
  grid.reserve(static_cast<size_t>(kVertices) * kGridFloats);
  const auto step = [fractions](int k) {
    return std::cmp_less(k, fractions.size())
               ? fractions[static_cast<size_t>(k)]
               : static_cast<float>(k) / static_cast<float>(kSide - 1);
  };
  for (int j = 0; j < kSide; ++j) {
    for (int i = 0; i < kSide; ++i) { grid.insert(grid.end(), {step(i), step(j), 0.0f}); }
  }
  for (int k = 0; k < kSide; ++k) { grid.insert(grid.end(), {step(k), 0.0f, 1.0f}); }
  for (int k = 0; k < kSide; ++k) { grid.insert(grid.end(), {step(k), 1.0f, 1.0f}); }
  for (int k = 0; k < kSide; ++k) { grid.insert(grid.end(), {0.0f, step(k), 1.0f}); }
  for (int k = 0; k < kSide; ++k) { grid.insert(grid.end(), {1.0f, step(k), 1.0f}); }

  std::vector<uint32_t> index;
  index.reserve(kIndices);
  const auto at = [](int i, int j) { return static_cast<uint32_t>(j * kSide + i); };
  for (int j = 0; j + 1 < kSide; ++j) {
    for (int i = 0; i + 1 < kSide; ++i) {
      index.insert(
          index.end(),
          {at(i, j), at(i, j + 1), at(i + 1, j + 1), at(i, j), at(i + 1, j + 1), at(i + 1, j)});
    }
  }
  const auto skirt = [&index](uint32_t edgeA, uint32_t edgeB, uint32_t dropA, uint32_t dropB) {
    index.insert(index.end(), {edgeA, dropA, edgeB, edgeB, dropA, dropB});
  };
  const uint32_t north = kNodes;
  const uint32_t south = kNodes + static_cast<uint32_t>(kSide);
  const uint32_t west = kNodes + 2u * static_cast<uint32_t>(kSide);
  const uint32_t east = kNodes + 3u * static_cast<uint32_t>(kSide);
  for (uint32_t k = 0; k + 1 < static_cast<uint32_t>(kSide); ++k) {
    skirt(at(static_cast<int>(k), 0), at(static_cast<int>(k + 1), 0), north + k, north + k + 1);
    skirt(at(static_cast<int>(k + 1), kSide - 1),
          at(static_cast<int>(k), kSide - 1),
          south + k + 1,
          south + k);
    skirt(at(0, static_cast<int>(k + 1)), at(0, static_cast<int>(k)), west + k + 1, west + k);
    skirt(at(kSide - 1, static_cast<int>(k)),
          at(kSide - 1, static_cast<int>(k + 1)),
          east + k,
          east + k + 1);
  }

  SDL_GPUBufferCreateInfo wanted{};
  wanted.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
  wanted.size = static_cast<uint32_t>(grid.size() * sizeof(float));
  OwnedBuffer madeGrid(Device_, SDL_CreateGPUBuffer(Device_, &wanted));
  if (!madeGrid) {
    error = std::format(Says::kBufferRefused, "grid", SDL_GetError());
    return false;
  }
  OwnedBuffer madeIndex;
  SDL_GPUBuffer *indexBuffer = Index_.Get();
  if (!Index_) {
    wanted.usage = SDL_GPU_BUFFERUSAGE_INDEX;
    wanted.size = static_cast<uint32_t>(index.size() * sizeof(uint32_t));
    madeIndex = OwnedBuffer(Device_, SDL_CreateGPUBuffer(Device_, &wanted));
    if (!madeIndex) {
      error = std::format(Says::kBufferRefused, "grid", SDL_GetError());
      return false;
    }
    indexBuffer = madeIndex.Get();
    if (!UploadBuffer(Device_,
                      indexBuffer,
                      index.data(),
                      static_cast<uint32_t>(index.size() * sizeof(uint32_t)),
                      error)) {
      return false;
    }
  }
  if (!UploadBuffer(Device_,
                    madeGrid.Get(),
                    grid.data(),
                    static_cast<uint32_t>(grid.size() * sizeof(float)),
                    error)) {
    return false;
  }
  if (madeIndex) { Index_ = std::move(madeIndex); }
  into = std::move(madeGrid);
  return true;
}

bool GroundLattice::BuildPages(std::string &error) {
  SDL_GPUTextureCreateInfo wanted{};
  wanted.type = SDL_GPU_TEXTURETYPE_2D_ARRAY;
  wanted.format = SDL_GPU_TEXTUREFORMAT_R32_FLOAT;
  wanted.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
  wanted.width = static_cast<uint32_t>(kPageSide) * kPageColumns;
  wanted.height = static_cast<uint32_t>(kPageSide) * kPageColumns;
  wanted.layer_count_or_depth = kPageLayers;
  wanted.num_levels = 1;
  wanted.sample_count = SDL_GPU_SAMPLECOUNT_1;
  if (!SDL_GPUTextureSupportsFormat(Device_,
                                    SDL_GPU_TEXTUREFORMAT_R32_FLOAT,
                                    SDL_GPU_TEXTURETYPE_2D_ARRAY,
                                    SDL_GPU_TEXTUREUSAGE_SAMPLER)) {
    error = "this device does not sample an R32 float array, and the height pages are one";
    return false;
  }
  Pages_ = OwnedTexture(Device_, SDL_CreateGPUTexture(Device_, &wanted));
  SDL_GPUSamplerCreateInfo nearest{};
  nearest.min_filter = SDL_GPU_FILTER_NEAREST;
  nearest.mag_filter = SDL_GPU_FILTER_NEAREST;
  nearest.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
  nearest.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
  nearest.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
  nearest.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
  Nearest_ = OwnedSampler(Device_, SDL_CreateGPUSampler(Device_, &nearest));
  if (!Pages_ || !Nearest_) {
    error = std::format(Says::kBufferRefused, "pages", SDL_GetError());
    return false;
  }
  return true;
}

bool GroundPipelineBinding::ConfigureLit(SDL_GPUDevice *device,
                                         const SurfaceOutputs &outputs,
                                         std::span<const SDL_GPUColorTargetDescription> targets,
                                         SDL_GPUVertexInputState input,
                                         std::string &error) {
  const OwnedShader vertex(device,
                           ShaderFrom(device,
                                      outputs.VertexPath("groundLattice"),
                                      SDL_GPU_SHADERSTAGE_VERTEX,
                                      GroundLattice::LitShape,
                                      error));
  const OwnedShader fragment(device,
                             ShaderFrom(device,
                                        outputs.FragmentPath("groundLit"),
                                        SDL_GPU_SHADERSTAGE_FRAGMENT,
                                        GroundLattice::LitShape,
                                        error));
  if (!vertex || !fragment) { return false; }
  SDL_GPUGraphicsPipelineCreateInfo wanted{};
  wanted.vertex_shader = vertex.Get();
  wanted.fragment_shader = fragment.Get();
  wanted.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
  wanted.vertex_input_state = input;
  wanted.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
  wanted.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_BACK;
  wanted.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
  wanted.target_info.color_target_descriptions = targets.data();
  wanted.target_info.num_color_targets = static_cast<Uint32>(targets.size());
  wanted.target_info.has_depth_stencil_target = true;
  wanted.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
  wanted.depth_stencil_state.enable_depth_test = true;
  wanted.depth_stencil_state.enable_depth_write = true;
  wanted.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_GREATER;
  SDL_GPUGraphicsPipeline *const made = SDL_CreateGPUGraphicsPipeline(device, &wanted);
  if (made == nullptr) {
    error = std::format(Says::kPipelineRefused, SDL_GetError());
    return false;
  }
  Lit_ = OwnedPipeline(device, made);
  return true;
}

bool GroundPipelineBinding::ConfigureDepth(SDL_GPUDevice *device,
                                           SDL_GPUVertexInputState input,
                                           std::string &error) {
  const OwnedShader vertex(device,
                           ShaderFrom(device,
                                      "build/shaders/groundLatticeDepth.vert.spv",
                                      SDL_GPU_SHADERSTAGE_VERTEX,
                                      GroundLattice::DepthShape,
                                      error));
  const OwnedShader fragment(device,
                             ShaderFrom(device,
                                        "build/shaders/depth.frag.spv",
                                        SDL_GPU_SHADERSTAGE_FRAGMENT,
                                        GroundLattice::DepthShape,
                                        error));
  if (!vertex || !fragment) { return false; }
  SDL_GPUGraphicsPipelineCreateInfo wanted{};
  wanted.vertex_shader = vertex.Get();
  wanted.fragment_shader = fragment.Get();
  wanted.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
  wanted.vertex_input_state = input;
  wanted.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
  wanted.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_BACK;
  wanted.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
  wanted.depth_stencil_state.enable_depth_test = true;
  wanted.depth_stencil_state.enable_depth_write = true;
  wanted.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_GREATER;
  wanted.target_info.num_color_targets = 0;
  wanted.target_info.has_depth_stencil_target = true;
  wanted.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
  SDL_GPUGraphicsPipeline *const made = SDL_CreateGPUGraphicsPipeline(device, &wanted);
  if (made == nullptr) {
    error = std::format(Says::kPipelineRefused, SDL_GetError());
    return false;
  }
  Depth_ = OwnedPipeline(device, made);
  return true;
}

bool GroundLattice::Configure(GroundPipelineBinding &pipelines,
                              SDL_GPUDevice *device,
                              const SurfaceOutputs &outputs,
                              std::span<const SDL_GPUColorTargetDescription> targets,
                              std::string &error) {
  if (!AttachPipelines(pipelines, device, error)) { return false; }
  const LatticeVertexInput in = MakeLatticeVertexInput();
  const SDL_GPUVertexInputState input{
      .vertex_buffer_descriptions = in.Buffers.data(),
      .num_vertex_buffers = static_cast<uint32_t>(in.Buffers.size()),
      .vertex_attributes = in.Attributes.data(),
      .num_vertex_attributes = static_cast<uint32_t>(in.Attributes.size())};
  return pipelines.ConfigureLit(Device_, outputs, targets, input, error);
}

bool GroundLattice::AttachPipelines(GroundPipelineBinding &pipelines,
                                    SDL_GPUDevice *device,
                                    std::string &error) {
  if (device == nullptr) {
    error = std::string(Says::kNoDevice);
    return false;
  }
  if (Device_ != device || !Pages_ || !Grid_) {
    Device_ = device;
    Instances_.Reset();
    Index_.Reset();
    InstanceRoom_ = 0;
    RealCount_ = 0;
    VirtualCount_ = 0;
    Spare_.clear();
    PagesMade_ = 0;
    PagesLive_ = 0;
    if (!BuildGrid({}, Grid_, error) || !BuildGrid({}, UniformGrid_, error) || !BuildPages(error)) {
      return false;
    }
  }
  Pipelines_ = &pipelines;
  return true;
}

bool GroundLattice::ConfigureDepth(GroundPipelineBinding &pipelines,
                                   SDL_GPUDevice *device,
                                   std::string &error) {
  if (device == nullptr) {
    error = std::string(Says::kNoDevice);
    return false;
  }
  const LatticeVertexInput in = MakeLatticeVertexInput();
  const SDL_GPUVertexInputState input{
      .vertex_buffer_descriptions = in.Buffers.data(),
      .num_vertex_buffers = static_cast<uint32_t>(in.Buffers.size()),
      .vertex_attributes = in.Attributes.data(),
      .num_vertex_attributes = static_cast<uint32_t>(in.Attributes.size())};
  return pipelines.ConfigureDepth(device, input, error);
}

bool GroundLattice::Configure(SDL_GPUDevice *device,
                              const SurfaceOutputs &outputs,
                              std::span<const SDL_GPUColorTargetDescription> targets,
                              std::string &error) {
  GroundPipelineBinding candidate;
  if (!Configure(candidate, device, outputs, targets, error)) { return false; }
  OwnedPipelines_ = std::move(candidate);
  Pipelines_ = &OwnedPipelines_;
  return true;
}

bool GroundLattice::ConfigureDepth(SDL_GPUDevice *device, std::string &error) {
  GroundPipelineBinding candidate;
  if (!ConfigureDepth(candidate, device, error)) { return false; }
  OwnedPipelines_ = std::move(candidate);
  Pipelines_ = &OwnedPipelines_;
  return true;
}

bool GroundLattice::SetGrid(std::span<const float> fractions, std::string &error) {
  if (Device_ == nullptr) {
    error = std::string(Says::kNoDevice);
    return false;
  }
  if (fractions.size() != static_cast<size_t>(kSide)) {
    error = std::format(Says::kPageWrongSize, kSide, fractions.size());
    return false;
  }
  return BuildGrid(fractions, Grid_, error);
}

PageId GroundLattice::PlacePage(std::span<const float> nodes, std::string &error) {
  if (!Pages_) {
    error = std::string(Says::kNoDevice);
    return kNoPage;
  }
  if (nodes.size() != kPageNodes) {
    error = std::format(Says::kPageWrongSize, kPageSide, nodes.size());
    return kNoPage;
  }
  PageId page = kNoPage;
  bool borrowed = false;
  if (!Spare_.empty()) {
    page = Spare_.back();
    Spare_.pop_back();
    borrowed = true;
  } else if (PagesMade_ < kPages) {
    page = PagesMade_++;
  } else {
    error = std::format(Says::kPagesFull, kPages);
    return kNoPage;
  }
  const auto restore = [this, page, borrowed] {
    if (borrowed) {
      Spare_.push_back(page);
    } else {
      --PagesMade_;
    }
  };
  SDL_GPUTransferBufferCreateInfo room{};
  room.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
  room.size = static_cast<uint32_t>(nodes.size_bytes());
  const OwnedTransfer staging(Device_, SDL_CreateGPUTransferBuffer(Device_, &room));
  void *const mapped = staging ? SDL_MapGPUTransferBuffer(Device_, staging.Get(), false) : nullptr;
  if (mapped == nullptr) {
    restore();
    error = std::format(Says::kStagingDidNotMap, SDL_GetError());
    return kNoPage;
  }
  std::memcpy(mapped, nodes.data(), nodes.size_bytes());
  SDL_UnmapGPUTransferBuffer(Device_, staging.Get());
  const std::optional<CopyCommands> copy = BeginCopy(Device_, error);
  if (!copy) {
    restore();
    return kNoPage;
  }
  SDL_GPUTextureTransferInfo source{};
  source.transfer_buffer = staging.Get();
  SDL_GPUTextureRegion into{};
  into.texture = Pages_.Get();
  into.layer = page / kPagesPerLayer;
  into.x = (page % kPageColumns) * static_cast<uint32_t>(kPageSide);
  into.y = ((page % kPagesPerLayer) / kPageColumns) * static_cast<uint32_t>(kPageSide);
  into.w = static_cast<uint32_t>(kPageSide);
  into.h = static_cast<uint32_t>(kPageSide);
  into.d = 1;
  SDL_UploadToGPUTexture(copy->Pass, &source, &into, false);
  if (!SubmitCopy(*copy, error)) {
    restore();
    return kNoPage;
  }
  ++PagesLive_;
  return page;
}

void GroundLattice::ReleasePage(PageId which) {
  if (which == kNoPage || which >= PagesMade_) { return; }
  Spare_.push_back(which);
  --PagesLive_;
}

bool GroundLattice::SetInstances(std::span<const GroundTile> real,
                                 std::span<const GroundTile> virtual_,
                                 std::string &error) {
  const size_t maximum = std::numeric_limits<uint32_t>::max() / sizeof(GroundInstance);
  if (real.size() > maximum || virtual_.size() > maximum - real.size()) {
    error = std::string(Says::kTooManyInstances);
    return false;
  }
  const size_t total = real.size() + virtual_.size();
  if (total == 0) {
    Held_.clear();
    Bounds_.clear();
    RealCount_ = 0;
    VirtualCount_ = 0;
    VisibleReal_ = 0;
    VisibleVirtual_ = 0;
    return true;
  }
  if (Device_ == nullptr) {
    error = std::string(Says::kNoDevice);
    return false;
  }
  std::vector<GroundInstance> instances;
  std::vector<std::array<float, 4>> bounds;
  instances.reserve(total);
  bounds.reserve(total);
  for (const std::span<const GroundTile> tiles : {real, virtual_}) {
    for (const GroundTile &tile : tiles) {
      const GroundInstance &one = tile.Instance;
      instances.push_back(one);
      float reach = 0.0f;
      for (size_t corner = 0; corner < 4; ++corner) {
        const float e = one.Corners[corner * 2u];
        const float n = one.Corners[corner * 2u + 1u];
        reach = std::max(reach, e * e + n * n);
      }
      const float mid = 0.5f * (tile.LowM + tile.HighM);
      const float half = 0.5f * (tile.HighM - tile.LowM);
      bounds.push_back({{one.Row[12] + one.Row[8] * mid,
                         one.Row[13] + one.Row[9] * mid,
                         one.Row[14] + one.Row[10] * mid,
                         std::sqrt(reach + half * half)}});
    }
  }
  const auto count = static_cast<uint32_t>(instances.size());
  OwnedBuffer replacement;
  SDL_GPUBuffer *target = Instances_.Get();
  uint32_t room = InstanceRoom_;
  if (!Instances_ || InstanceRoom_ < count) {
    room = InstanceRoom_ > 0 ? InstanceRoom_ : 64u;
    while (room < count) {
      if (room > static_cast<uint32_t>(maximum / 2u)) {
        room = count;
        break;
      }
      room *= 2u;
    }
    SDL_GPUBufferCreateInfo wanted{};
    wanted.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    wanted.size = room * kGroundInstanceFloats * static_cast<uint32_t>(sizeof(float));
    replacement = OwnedBuffer(Device_, SDL_CreateGPUBuffer(Device_, &wanted));
    if (!replacement) {
      error = std::format(Says::kBufferRefused, "instances", SDL_GetError());
      return false;
    }
    target = replacement.Get();
  }
  if (!UploadBuffer(Device_,
                    target,
                    instances.data(),
                    static_cast<uint32_t>(instances.size() * sizeof(GroundInstance)),
                    error)) {
    return false;
  }
  if (replacement) { Instances_ = std::move(replacement); }
  Held_ = std::move(instances);
  Bounds_ = std::move(bounds);
  InstanceRoom_ = room;
  RealCount_ = static_cast<uint32_t>(real.size());
  VirtualCount_ = static_cast<uint32_t>(virtual_.size());
  VisibleReal_ = RealCount_;
  VisibleVirtual_ = VirtualCount_;
  return true;
}

bool GroundLattice::Cull(const FrameContext &ctx,
                         const Vec3 &anchorM,
                         SDL_GPUCommandBuffer *commands,
                         std::string &error) {
  VisibleReal_ = 0;
  VisibleVirtual_ = 0;
  if (Held_.empty()) { return true; }
  if (Device_ == nullptr || commands == nullptr) {
    error = std::format(Says::kVisibleUploadFailed, "no device or command buffer");
    return false;
  }
  std::array<float, 3> shift{};
  for (size_t axis = 0; axis < 3; ++axis) {
    shift[axis] = static_cast<float>(anchorM[static_cast<int>(axis)] +
                                     ctx.PreViewTranslation[static_cast<int>(axis)]);
  }
  const SidePlanes planes = SidePlanesOf(ctx.Mvp);
  Seen_.clear();
  for (size_t at = 0; at < Held_.size(); ++at) {
    const std::array<float, 4> &sphere = Bounds_[at];
    const float x = sphere[0] + shift[0];
    const float y = sphere[1] + shift[1];
    const float z = sphere[2] + shift[2];
    bool inside = true;
    for (const std::array<float, 4> &plane : planes) {
      if (plane[0] * x + plane[1] * y + plane[2] * z + plane[3] < -sphere[3]) {
        inside = false;
        break;
      }
    }
    if (!inside) { continue; }
    Seen_.push_back(Held_[at]);
    if (at < RealCount_) {
      ++VisibleReal_;
    } else {
      ++VisibleVirtual_;
    }
  }
  if (!Seen_.empty() && !HandsVisible(commands, error)) {
    VisibleReal_ = 0;
    VisibleVirtual_ = 0;
    return false;
  }
  return true;
}

bool GroundLattice::HandsVisible(SDL_GPUCommandBuffer *commands, std::string &error) {
  const auto count = static_cast<uint32_t>(Seen_.size());
  const uint32_t bytes = count * kGroundInstanceFloats * static_cast<uint32_t>(sizeof(float));
  if (!Visible_ || VisibleRoom_ < count) {
    uint32_t room = VisibleRoom_ > 0 ? VisibleRoom_ : 64u;
    while (room < count) { room *= 2u; }
    SDL_GPUBufferCreateInfo wanted{};
    wanted.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    wanted.size = room * kGroundInstanceFloats * static_cast<uint32_t>(sizeof(float));
    OwnedBuffer visible(Device_, SDL_CreateGPUBuffer(Device_, &wanted));
    SDL_GPUTransferBufferCreateInfo staging{};
    staging.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    staging.size = wanted.size;
    OwnedTransfer visibleStaging(Device_, SDL_CreateGPUTransferBuffer(Device_, &staging));
    if (!visible || !visibleStaging) {
      error = std::format(Says::kVisibleUploadFailed, SDL_GetError());
      return false;
    }
    Visible_ = std::move(visible);
    VisibleStaging_ = std::move(visibleStaging);
    VisibleRoom_ = room;
  }
  void *const mapped = SDL_MapGPUTransferBuffer(Device_, VisibleStaging_.Get(), true);
  if (mapped == nullptr) {
    error = std::format(Says::kVisibleUploadFailed, SDL_GetError());
    return false;
  }
  std::memcpy(mapped, Seen_.data(), bytes);
  SDL_UnmapGPUTransferBuffer(Device_, VisibleStaging_.Get());
  SDL_GPUCopyPass *const copy = SDL_BeginGPUCopyPass(commands);
  if (copy == nullptr) {
    error = std::format(Says::kVisibleUploadFailed, SDL_GetError());
    return false;
  }
  const SDL_GPUTransferBufferLocation source{.transfer_buffer = VisibleStaging_.Get(), .offset = 0};
  const SDL_GPUBufferRegion region{.buffer = Visible_.Get(), .offset = 0, .size = bytes};
  SDL_UploadToGPUBuffer(copy, &source, &region, true);
  SDL_EndGPUCopyPass(copy);
  return true;
}

void GroundLattice::Draw(const PassRecording &into,
                         SDL_GPUGraphicsPipeline *pipeline,
                         const OwnedBuffer &instances,
                         uint32_t real,
                         uint32_t virtual_) const {
  if (pipeline == nullptr || real + virtual_ == 0 || into.Pass == nullptr || !Grid_ ||
      !UniformGrid_ || !Index_ || !instances || !Pages_) {
    return;
  }
  SDL_BindGPUGraphicsPipeline(into.Pass, pipeline);
  const SDL_GPUBufferBinding index{.buffer = Index_.Get(), .offset = 0};
  SDL_BindGPUIndexBuffer(into.Pass, &index, SDL_GPU_INDEXELEMENTSIZE_32BIT);
  const SDL_GPUTextureSamplerBinding pages{.texture = Pages_.Get(), .sampler = Nearest_.Get()};
  SDL_BindGPUVertexSamplers(into.Pass, 0, &pages, 1);
  const auto draw = [&into, &instances](const OwnedBuffer &grid, uint32_t first, uint32_t count) {
    if (count == 0) { return; }
    const std::array<SDL_GPUBufferBinding, 2> runs = {
        {{.buffer = grid.Get(), .offset = 0}, {.buffer = instances.Get(), .offset = 0}}};
    SDL_BindGPUVertexBuffers(into.Pass, 0, runs.data(), static_cast<uint32_t>(runs.size()));
    SDL_DrawGPUIndexedPrimitives(into.Pass, kIndices, count, 0, 0, first);
  };
  draw(Grid_, 0, real);
  draw(UniformGrid_, real, virtual_);
}

void GroundLattice::Encode(const PassRecording &into) const {
  Draw(into, Pipelines_->Lit(), Visible_, VisibleReal_, VisibleVirtual_);
}

void GroundLattice::Cast(const PassRecording &into) const {
  Draw(into, Pipelines_->Depth(), Instances_, RealCount_, VirtualCount_);
}

}
