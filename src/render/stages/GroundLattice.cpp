#include "GroundLattice.h"

#include <array>
#include <cstdint>
#include <utility>
#include <cstring>
#include <format>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace outshine::Render {

namespace {

namespace Says {
constexpr std::string_view kNoDevice = "the ground lattice has no device to stand on";
constexpr std::string_view kShaderRefused = "the ground lattice's shader was refused at {}: {}";
constexpr std::string_view kPipelineRefused = "the ground lattice's pipeline was refused: {}";
constexpr std::string_view kBufferRefused = "the ground lattice found no room for its {}: {}";
constexpr std::string_view kPageWrongSize =
    "a height page is {} nodes a side and this one brought {} floats";
constexpr std::string_view kPagesFull = "every one of the {} height pages is placed";
constexpr std::string_view kStagingDidNotMap = "the height page's staging did not map: {}";
} // namespace Says

struct LatticeInput {
  std::array<SDL_GPUVertexBufferDescription, 2> Buffers{};
  std::array<SDL_GPUVertexAttribute, 8> Attributes{};
};

LatticeInput InputOf() {
  LatticeInput in;
  in.Buffers[0].slot = 0;
  in.Buffers[0].pitch = GroundLattice::kGridFloats * static_cast<uint32_t>(sizeof(float));
  in.Buffers[0].input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
  in.Buffers[1].slot = 1;
  in.Buffers[1].pitch = kGroundInstanceFloats * static_cast<uint32_t>(sizeof(float));
  in.Buffers[1].input_rate = SDL_GPU_VERTEXINPUTRATE_INSTANCE;
  in.Buffers[1].instance_step_rate = 1;
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
  SDL_GPUCommandBuffer *const commands = SDL_AcquireGPUCommandBuffer(device);
  SDL_GPUCopyPass *const copy = SDL_BeginGPUCopyPass(commands);
  const SDL_GPUTransferBufferLocation source{.transfer_buffer = staging.Get(), .offset = 0};
  const SDL_GPUBufferRegion region{.buffer = into, .offset = 0, .size = bytes};
  SDL_UploadToGPUBuffer(copy, &source, &region, false);
  SDL_EndGPUCopyPass(copy);
  SDL_SubmitGPUCommandBuffer(commands);
  return true;
}

} // namespace

bool GroundLattice::BuildGrid(std::span<const float> fractions, std::string &error) {
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
          {at(i, j), at(i, j + 1), at(i + 1, j), at(i + 1, j), at(i, j + 1), at(i + 1, j + 1)});
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
  Grid_ = OwnedBuffer(Device_, SDL_CreateGPUBuffer(Device_, &wanted));
  wanted.usage = SDL_GPU_BUFFERUSAGE_INDEX;
  wanted.size = static_cast<uint32_t>(index.size() * sizeof(uint32_t));
  Index_ = OwnedBuffer(Device_, SDL_CreateGPUBuffer(Device_, &wanted));
  if (!Grid_ || !Index_) {
    error = std::format(Says::kBufferRefused, "grid", SDL_GetError());
    return false;
  }
  return UploadBuffer(Device_,
                      Grid_.Get(),
                      grid.data(),
                      static_cast<uint32_t>(grid.size() * sizeof(float)),
                      error) &&
         UploadBuffer(Device_,
                      Index_.Get(),
                      index.data(),
                      static_cast<uint32_t>(index.size() * sizeof(uint32_t)),
                      error);
}

bool GroundLattice::BuildPages(std::string &error) {
  SDL_GPUTextureCreateInfo wanted{};
  wanted.type = SDL_GPU_TEXTURETYPE_2D_ARRAY;
  wanted.format = SDL_GPU_TEXTUREFORMAT_R32_FLOAT;
  wanted.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
  wanted.width = static_cast<uint32_t>(kSide);
  wanted.height = static_cast<uint32_t>(kSide);
  wanted.layer_count_or_depth = kPages;
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

bool GroundLattice::Configure(SDL_GPUDevice *device,
                              std::string_view source,
                              std::span<const SDL_GPUColorTargetDescription> targets,
                              std::string &error) {
  if (device == nullptr) {
    error = std::string(Says::kNoDevice);
    return false;
  }
  if (Device_ != device || !Pages_ || !Grid_) {
    Device_ = device;
    Instances_.Reset();
    InstanceRoom_ = 0;
    InstanceCount_ = 0;
    Spare_.clear();
    PagesMade_ = 0;
    PagesLive_ = 0;
    if (!BuildGrid({}, error) || !BuildPages(error)) { return false; }
  }
  const OwnedShader vertex(
      Device_,
      ShaderFrom(Device_, source, "vsGroundLattice", SDL_GPU_SHADERSTAGE_VERTEX, LitShape));
  const OwnedShader fragment(
      Device_, ShaderFrom(Device_, source, "fsGroundLit", SDL_GPU_SHADERSTAGE_FRAGMENT, LitShape));
  if (!vertex || !fragment) {
    error = std::format(Says::kShaderRefused, "vsGroundLattice/fsGroundLit", SDL_GetError());
    return false;
  }
  const LatticeInput in = InputOf();
  SDL_GPUGraphicsPipelineCreateInfo wanted{};
  wanted.vertex_shader = vertex.Get();
  wanted.fragment_shader = fragment.Get();
  wanted.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
  wanted.vertex_input_state.vertex_buffer_descriptions = in.Buffers.data();
  wanted.vertex_input_state.num_vertex_buffers = static_cast<uint32_t>(in.Buffers.size());
  wanted.vertex_input_state.vertex_attributes = in.Attributes.data();
  wanted.vertex_input_state.num_vertex_attributes = static_cast<uint32_t>(in.Attributes.size());
  wanted.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
  wanted.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
  wanted.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
  wanted.target_info.color_target_descriptions = targets.data();
  wanted.target_info.num_color_targets = static_cast<Uint32>(targets.size());
  wanted.target_info.has_depth_stencil_target = true;
  wanted.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
  wanted.depth_stencil_state.enable_depth_test = true;
  wanted.depth_stencil_state.enable_depth_write = true;
  wanted.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_GREATER;
  SDL_GPUGraphicsPipeline *const made = SDL_CreateGPUGraphicsPipeline(Device_, &wanted);
  if (made == nullptr) {
    error = std::format(Says::kPipelineRefused, SDL_GetError());
    return false;
  }
  Lit_ = OwnedPipeline(Device_, made);
  return true;
}

bool GroundLattice::ConfigureDepth(SDL_GPUDevice *device,
                                   std::string_view depthSource,
                                   std::string &error) {
  if (device == nullptr) {
    error = std::string(Says::kNoDevice);
    return false;
  }
  const OwnedShader vertex(
      device,
      ShaderFrom(
          device, depthSource, "vsGroundLatticeDepth", SDL_GPU_SHADERSTAGE_VERTEX, DepthShape));
  const OwnedShader fragment(
      device, ShaderFrom(device, depthSource, "fsDepth", SDL_GPU_SHADERSTAGE_FRAGMENT, DepthShape));
  if (!vertex || !fragment) {
    error = std::format(Says::kShaderRefused, "vsGroundLatticeDepth/fsDepth", SDL_GetError());
    return false;
  }
  const LatticeInput in = InputOf();
  SDL_GPUGraphicsPipelineCreateInfo wanted{};
  wanted.vertex_shader = vertex.Get();
  wanted.fragment_shader = fragment.Get();
  wanted.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
  wanted.vertex_input_state.vertex_buffer_descriptions = in.Buffers.data();
  wanted.vertex_input_state.num_vertex_buffers = static_cast<uint32_t>(in.Buffers.size());
  wanted.vertex_input_state.vertex_attributes = in.Attributes.data();
  wanted.vertex_input_state.num_vertex_attributes = static_cast<uint32_t>(in.Attributes.size());
  wanted.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
  wanted.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
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

bool GroundLattice::SetGrid(std::span<const float> fractions, std::string &error) {
  if (Device_ == nullptr) {
    error = std::string(Says::kNoDevice);
    return false;
  }
  if (fractions.size() != static_cast<size_t>(kSide)) {
    error = std::format(Says::kPageWrongSize, kSide, fractions.size());
    return false;
  }
  return BuildGrid(fractions, error);
}

PageId GroundLattice::PlacePage(std::span<const float> nodes, std::string &error) {
  if (!Pages_) {
    error = std::string(Says::kNoDevice);
    return kNoPage;
  }
  if (nodes.size() != kNodes) {
    error = std::format(Says::kPageWrongSize, kSide, nodes.size());
    return kNoPage;
  }
  PageId page = kNoPage;
  if (!Spare_.empty()) {
    page = Spare_.back();
    Spare_.pop_back();
  } else if (PagesMade_ < kPages) {
    page = PagesMade_++;
  } else {
    error = std::format(Says::kPagesFull, kPages);
    return kNoPage;
  }
  SDL_GPUTransferBufferCreateInfo room{};
  room.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
  room.size = static_cast<uint32_t>(nodes.size_bytes());
  const OwnedTransfer staging(Device_, SDL_CreateGPUTransferBuffer(Device_, &room));
  void *const mapped = staging ? SDL_MapGPUTransferBuffer(Device_, staging.Get(), false) : nullptr;
  if (mapped == nullptr) {
    Spare_.push_back(page);
    error = std::format(Says::kStagingDidNotMap, SDL_GetError());
    return kNoPage;
  }
  std::memcpy(mapped, nodes.data(), nodes.size_bytes());
  SDL_UnmapGPUTransferBuffer(Device_, staging.Get());
  SDL_GPUCommandBuffer *const commands = SDL_AcquireGPUCommandBuffer(Device_);
  SDL_GPUCopyPass *const copy = SDL_BeginGPUCopyPass(commands);
  SDL_GPUTextureTransferInfo source{};
  source.transfer_buffer = staging.Get();
  SDL_GPUTextureRegion into{};
  into.texture = Pages_.Get();
  into.layer = page;
  into.w = static_cast<uint32_t>(kSide);
  into.h = static_cast<uint32_t>(kSide);
  into.d = 1;
  SDL_UploadToGPUTexture(copy, &source, &into, false);
  SDL_EndGPUCopyPass(copy);
  SDL_SubmitGPUCommandBuffer(commands);
  ++PagesLive_;
  return page;
}

void GroundLattice::ReleasePage(PageId which) {
  if (which == kNoPage || which >= PagesMade_) { return; }
  Spare_.push_back(which);
  --PagesLive_;
}

bool GroundLattice::SetInstances(std::span<const GroundInstance> instances, std::string &error) {
  InstanceCount_ = 0;
  if (instances.empty()) { return true; }
  if (Device_ == nullptr) {
    error = std::string(Says::kNoDevice);
    return false;
  }
  const auto count = static_cast<uint32_t>(instances.size());
  if (!Instances_ || InstanceRoom_ < count) {
    uint32_t room = InstanceRoom_ > 0 ? InstanceRoom_ : 64u;
    while (room < count) { room *= 2u; }
    SDL_GPUBufferCreateInfo wanted{};
    wanted.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    wanted.size = room * kGroundInstanceFloats * static_cast<uint32_t>(sizeof(float));
    Instances_ = OwnedBuffer(Device_, SDL_CreateGPUBuffer(Device_, &wanted));
    if (!Instances_) {
      InstanceRoom_ = 0;
      error = std::format(Says::kBufferRefused, "instances", SDL_GetError());
      return false;
    }
    InstanceRoom_ = room;
  }
  if (!UploadBuffer(Device_,
                    Instances_.Get(),
                    instances.data(),
                    static_cast<uint32_t>(instances.size_bytes()),
                    error)) {
    return false;
  }
  InstanceCount_ = count;
  return true;
}

void GroundLattice::Draw(const PassRecording &into, SDL_GPUGraphicsPipeline *pipeline) const {
  if (pipeline == nullptr || InstanceCount_ == 0 || into.Pass == nullptr || !Grid_ || !Index_ ||
      !Instances_ || !Pages_) {
    return;
  }
  SDL_BindGPUGraphicsPipeline(into.Pass, pipeline);
  const std::array<SDL_GPUBufferBinding, 2> runs = {
      {{.buffer = Grid_.Get(), .offset = 0}, {.buffer = Instances_.Get(), .offset = 0}}};
  SDL_BindGPUVertexBuffers(into.Pass, 0, runs.data(), static_cast<uint32_t>(runs.size()));
  const SDL_GPUBufferBinding index{.buffer = Index_.Get(), .offset = 0};
  SDL_BindGPUIndexBuffer(into.Pass, &index, SDL_GPU_INDEXELEMENTSIZE_32BIT);
  const SDL_GPUTextureSamplerBinding pages{.texture = Pages_.Get(), .sampler = Nearest_.Get()};
  SDL_BindGPUVertexSamplers(into.Pass, 0, &pages, 1);
  SDL_DrawGPUIndexedPrimitives(into.Pass, kIndices, InstanceCount_, 0, 0, 0);
}

void GroundLattice::Encode(const PassRecording &into) const {
  Draw(into, Lit_.Get());
}

void GroundLattice::Cast(const PassRecording &into) const {
  Draw(into, Depth_.Get());
}

} // namespace outshine::Render
