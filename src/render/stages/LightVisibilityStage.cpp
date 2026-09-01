#include "LightVisibilityStage.h"

#include <cmath>
#include <cstring>

#include "ShaderFile.h"
#include "ShaderPrelude.h"
#include "SubjectDraw.h"

namespace outshine::Render {

bool LightVisibilityStage::Configure(SubjectDraw &subjects, const Gpu &gpu, std::string &error) {
  Subjects_ = &subjects;
  return ConfigureDepthOnly(gpu, error);
}

void LightVisibilityStage::Declare(const float toSun[3], const float up[3], double radiusM) {
  for (int axis = 0; axis < 3; ++axis) {
    ToSun_[axis] = static_cast<double>(toSun[axis]);
    Up_[axis] = static_cast<double>(up[axis]);
  }
  RadiusM_ = radiusM;
  double sunLength = 0.0;
  double crossLength = 0.0;
  double cross[3] = {Up_[1] * ToSun_[2] - Up_[2] * ToSun_[1],
                     Up_[2] * ToSun_[0] - Up_[0] * ToSun_[2],
                     Up_[0] * ToSun_[1] - Up_[1] * ToSun_[0]};
  for (int axis = 0; axis < 3; ++axis) {
    sunLength += ToSun_[axis] * ToSun_[axis];
    crossLength += cross[axis] * cross[axis];
  }
  Declared_ = radiusM > 0.0 && sunLength > 0.0 && crossLength > 0.0;
}

void LightVisibilityStage::Build(const double preView[3]) {
  double forward[3] = {-ToSun_[0], -ToSun_[1], -ToSun_[2]};
  double length = 0.0;
  for (int axis = 0; axis < 3; ++axis) { length += forward[axis] * forward[axis]; }
  length = std::sqrt(length);
  for (int axis = 0; axis < 3; ++axis) { forward[axis] /= length; }

  double right[3] = {Up_[1] * forward[2] - Up_[2] * forward[1],
                     Up_[2] * forward[0] - Up_[0] * forward[2],
                     Up_[0] * forward[1] - Up_[1] * forward[0]};
  double rLength = std::sqrt(right[0] * right[0] + right[1] * right[1] + right[2] * right[2]);
  for (int axis = 0; axis < 3; ++axis) { right[axis] /= rLength; }
  const double upward[3] = {forward[1] * right[2] - forward[2] * right[1],
                            forward[2] * right[0] - forward[0] * right[2],
                            forward[0] * right[1] - forward[1] * right[0]};

  const double texelM = 2.0 * RadiusM_ / static_cast<double>(kShadowAtlasPx);
  double centre[3] = {0.0, 0.0, 0.0};
  const auto reported = [this, &centre]() {
    for (int axis = 0; axis < 3; ++axis) { StoodAtM_[axis] = centre[axis]; }
  };
  {
    const double *const anchor = Subjects_ != nullptr ? Subjects_->AnchorM() : nullptr;
    double least[3] = {1.0e30, 1.0e30, 1.0e30};
    double most[3] = {-1.0e30, -1.0e30, -1.0e30};
    size_t counted = 0;
    if (Subjects_ != nullptr) {
      const std::vector<double> &placed = Subjects_->Placements();
      const size_t slots = placed.size() / 16u;
      for (size_t slot = 0; slot < slots && slot < CastsBelow_; ++slot) {
        const double *const model = placed.data() + slot * 16u;
        for (int axis = 0; axis < 3; ++axis) {
          const double at = model[12 + axis];
          least[axis] = at < least[axis] ? at : least[axis];
          most[axis] = at > most[axis] ? at : most[axis];
        }
        ++counted;
      }
    }
    if (counted > 0) {
      for (int axis = 0; axis < 3; ++axis) {
        centre[axis] = 0.5 * (least[axis] + most[axis]) + (anchor != nullptr ? anchor[axis] : 0.0);
      }
    }
  }
  double centreLight[3] = {0.0, 0.0, 0.0};
  for (int axis = 0; axis < 3; ++axis) {
    centreLight[0] += right[axis] * centre[axis];
    centreLight[1] += upward[axis] * centre[axis];
    centreLight[2] += forward[axis] * centre[axis];
  }

  centreLight[0] = std::floor(centreLight[0] / texelM) * texelM;
  centreLight[1] = std::floor(centreLight[1] / texelM) * texelM;

  const double depthM = 2.0 * RadiusM_;
  reported();
  const double nearAlong = centreLight[2] - depthM;
  const double farAlong = centreLight[2] + depthM;
  for (int i = 0; i < 16; ++i) { LightFromWorld_[i] = 0.0; }
  for (int axis = 0; axis < 3; ++axis) {
    LightFromWorld_[axis * 4 + 0] = right[axis] / RadiusM_;
    LightFromWorld_[axis * 4 + 1] = upward[axis] / RadiusM_;

    LightFromWorld_[axis * 4 + 2] = -forward[axis] / (farAlong - nearAlong);
  }
  LightFromWorld_[12] = -centreLight[0] / RadiusM_;
  LightFromWorld_[13] = -centreLight[1] / RadiusM_;
  LightFromWorld_[14] = farAlong / (farAlong - nearAlong);
  LightFromWorld_[15] = 1.0;

  for (int at = 0; at < 16; ++at) { Static_[at] = LightFromWorld_[at]; }
  for (int row = 0; row < 3; ++row) {
    double carried = 0.0;
    for (int axis = 0; axis < 3; ++axis) {
      carried -= LightFromWorld_[axis * 4 + row] * preView[axis];
    }
    LightFromWorld_[12 + row] += carried;
  }
}

void LightVisibilityStage::Prepare(const FrameContext &ctx) {
  Casting_ = false;
  if (!Declared_ || Subjects_ == nullptr) { return; }
  Build(ctx.PreViewTranslation);
  const uint64_t stands = Subjects_->Generation();
  if (Held_ && stands == CastAt_ && std::memcmp(Static_, CastFrom_, sizeof Static_) == 0) {
    return;
  }
  Casting_ = true;
  CastAt_ = stands;
  for (int at = 0; at < 16; ++at) { CastFrom_[at] = Static_[at]; }
}

void LightVisibilityStage::Encode(const FrameContext &ctx, const PassRecording &into) {
  if (!Casting_) { return; }
  Cast(LightFromWorld_, ctx.PreViewTranslation, kShadowAtlasPx, into);
  Held_ = true;
}

std::string LightVisibilityStage::DepthOnlySource() {
  std::string ignored;
  return DepthOnlySource(ignored);
}

std::string LightVisibilityStage::DepthOnlySource(std::string &error) {
  std::string body;
  if (!LoadShaderText("src/render/shaders/subjectDepthOnly.msl", body, error)) {
    return std::string();
  }
  return MslPrelude(error) + body;
}

bool LightVisibilityStage::ConfigureDepthOnly(const Gpu &gpu, std::string &error) {
  if (DepthOnly_) { return true; }
  SDL_GPUDevice *const device = gpu.Device;
  if (device == nullptr) {
    error = "the subject unit has no device, so no depth-only pipeline can be built";
    return false;
  }
  const std::string source = DepthOnlySource(error);
  if (source.empty()) { return false; }
  const OwnedShader vertex(
      device,
      ShaderFrom(
          device, source, "vsDepth", SDL_GPU_SHADERSTAGE_VERTEX, SubjectDraw::DepthOnlyShape));
  const OwnedShader fragment(
      device,
      ShaderFrom(
          device, source, "fsDepth", SDL_GPU_SHADERSTAGE_FRAGMENT, SubjectDraw::DepthOnlyShape));
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

void LightVisibilityStage::Cast(const double lightFromWorld16[16],
                                const double preView[3],
                                int atlasPx,
                                const PassRecording &into) {
  if (Subjects_ == nullptr) { return; }
  const SubjectResidency &Resident_ = Subjects_->Resident();
  const double *const Anchor = Subjects_->AnchorM();
  const std::vector<DrawBatch> &Batches = Subjects_->Drawn();
  if (!DepthOnly_ || Resident_.NIdx == 0 || Batches.empty() || !Resident_.Vtx || !Resident_.Idx ||
      into.Pass == nullptr) {
    return;
  }
  SDL_GPUViewport square{};
  square.w = static_cast<float>(atlasPx);
  square.h = static_cast<float>(atlasPx);
  square.min_depth = 0.0f;
  square.max_depth = 1.0f;
  SDL_SetGPUViewport(into.Pass, &square);
  SDL_BindGPUGraphicsPipeline(into.Pass, DepthOnly_.Get());
  SDL_GPUBufferBinding vertices{.buffer = Resident_.Vtx.Get(), .offset = 0};
  SDL_BindGPUVertexBuffers(into.Pass, 0, &vertices, 1);
  SDL_GPUBufferBinding indices{.buffer = Resident_.Idx.Get(), .offset = 0};
  SDL_BindGPUIndexBuffer(into.Pass, &indices, SDL_GPU_INDEXELEMENTSIZE_32BIT);

  SDL_GPUBuffer *const rows[1] = {Resident_.Placed.Get()};
  SDL_BindGPUVertexStorageBuffers(into.Pass, 0, rows, 1);

  float uniform[20] = {};
  for (int i = 0; i < 16; i++) { uniform[i] = static_cast<float>(lightFromWorld16[i]); }
  for (int axis = 0; axis < 3; ++axis) {
    uniform[16 + axis] = static_cast<float>(Anchor[axis] + preView[axis]);
  }
  SDL_PushGPUVertexUniformData(into.Commands, 0, uniform, sizeof uniform);

  CastBatches_ = 0;
  for (const DrawBatch &batch : Batches) {
    if (batch.ModelSlot >= CastsBelow_) { continue; }
    ++CastBatches_;
    SDL_DrawGPUIndexedPrimitives(
        into.Pass, batch.IndexCount, 1, batch.FirstIndex, 0, batch.ModelSlot);
  }
}

} // namespace outshine::Render
