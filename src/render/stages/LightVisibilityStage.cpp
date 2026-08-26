#include "LightVisibilityStage.h"

#include <cmath>

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
    ToSun_[axis] = (double)toSun[axis];
    Up_[axis] = (double)up[axis];
  }
  RadiusM_ = radiusM;
  double sunLength = 0.0, crossLength = 0.0;
  double cross[3] = {Up_[1] * ToSun_[2] - Up_[2] * ToSun_[1],
                     Up_[2] * ToSun_[0] - Up_[0] * ToSun_[2],
                     Up_[0] * ToSun_[1] - Up_[1] * ToSun_[0]};
  for (int axis = 0; axis < 3; ++axis) {
    sunLength += ToSun_[axis] * ToSun_[axis];
    crossLength += cross[axis] * cross[axis];
  }
  Declared_ = radiusM > 0.0 && sunLength > 0.0 && crossLength > 0.0;
}

void LightVisibilityStage::Frame(const double centreM[3]) {
  for (int axis = 0; axis < 3; ++axis) { CentreM_[axis] = centreM[axis]; }
}

void LightVisibilityStage::Build(const double eye[3]) {
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

  const double texelM = 2.0 * RadiusM_ / (double)kShadowAtlasPx;
  const double *const anchor = Subjects_ != nullptr ? Subjects_->AnchorM() : nullptr;
  double centre[3];
  for (int axis = 0; axis < 3; ++axis) {
    centre[axis] = CentreM_[axis] + (anchor != nullptr ? anchor[axis] : 0.0);
  }
  double eyeLight[3] = {0.0, 0.0, 0.0};
  for (int axis = 0; axis < 3; ++axis) {
    eyeLight[0] += right[axis] * centre[axis];
    eyeLight[1] += upward[axis] * centre[axis];
    eyeLight[2] += forward[axis] * centre[axis];
  }

  eyeLight[0] = std::floor(eyeLight[0] / texelM) * texelM;
  eyeLight[1] = std::floor(eyeLight[1] / texelM) * texelM;

  const double depthM = 2.0 * RadiusM_;
  const double nearAlong = eyeLight[2] - depthM;
  const double farAlong = eyeLight[2] + depthM;
  for (int i = 0; i < 16; ++i) { LightFromWorld_[i] = 0.0; }
  for (int axis = 0; axis < 3; ++axis) {
    LightFromWorld_[axis * 4 + 0] = right[axis] / RadiusM_;
    LightFromWorld_[axis * 4 + 1] = upward[axis] / RadiusM_;

    LightFromWorld_[axis * 4 + 2] = -forward[axis] / (farAlong - nearAlong);
  }
  LightFromWorld_[12] = -eyeLight[0] / RadiusM_;
  LightFromWorld_[13] = -eyeLight[1] / RadiusM_;
  LightFromWorld_[14] = farAlong / (farAlong - nearAlong);
  LightFromWorld_[15] = 1.0;

  for (int row = 0; row < 3; ++row) {
    double carried = 0.0;
    for (int axis = 0; axis < 3; ++axis) {
      carried += LightFromWorld_[axis * 4 + row] * eye[axis];
    }
    LightFromWorld_[12 + row] += carried;
  }
}

void LightVisibilityStage::Encode(const FrameContext &ctx, const PassRecording &into) {
  if (!Declared_ || Subjects_ == nullptr) { return; }
  Build(ctx.Eye);
  Cast(LightFromWorld_, ctx.Eye, kShadowAtlasPx, into);
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
  SDL_GPUShaderCreateInfo wanted{};
  wanted.code = reinterpret_cast<const Uint8 *>(source.data());
  wanted.code_size = source.size();
  wanted.format = SDL_GPU_SHADERFORMAT_MSL;
  wanted.entrypoint = "vsDepth";
  wanted.stage = SDL_GPU_SHADERSTAGE_VERTEX;
  wanted.num_samplers = SubjectDraw::DepthOnlyShape.VertexSamplers;
  wanted.num_uniform_buffers = SubjectDraw::DepthOnlyShape.VertexUniformBuffers;
  const OwnedShader vertex(device, SDL_CreateGPUShader(device, &wanted));
  wanted.entrypoint = "fsDepth";
  wanted.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
  wanted.num_samplers = SubjectDraw::DepthOnlyShape.FragmentSamplers;
  wanted.num_uniform_buffers = SubjectDraw::DepthOnlyShape.FragmentUniformBuffers;
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
void LightVisibilityStage::Cast(const double lightFromWorld16[16], const double eye[3],
                                int atlasPx, const PassRecording &into) {
  if (Subjects_ == nullptr) { return; }
  const SubjectResidency &Resident_ = Subjects_->Resident();
  const std::vector<double> &Placed_ = Subjects_->Placements();
  const double *const Anchor = Subjects_->AnchorM();
  const double *const Model = Subjects_->ModelM();
  const std::vector<DrawBatch> &Batches = Subjects_->Drawn();
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
  CastBatches_ = 0;
  for (const DrawBatch &batch : Batches) {
    if (batch.ModelSlot >= CastsBelow_) { continue; }
    ++CastBatches_;
    if (batch.ModelSlot != standing) {
      place(batch.ModelSlot);
      standing = batch.ModelSlot;
    }
    SDL_DrawGPUIndexedPrimitives(into.Pass, batch.IndexCount, 1, batch.FirstIndex, 0, 0);
  }
}

}
