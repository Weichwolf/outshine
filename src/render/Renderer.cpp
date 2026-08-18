#include "Renderer.h"

#include <cmath>
#include <cstring>

#include <SDL3/SDL.h>

#include "Log.h"
#include "stages/SceneTargets.h"

namespace outshine::Render {
namespace {
/* Whether the device is created with the driver's validation enabled. the harness builds a second
 * arm with `-DOUTSHINE_GPU_VALIDATION=1`; every other build gets `false` and the flag costs a
 * constant fold (board:1123). */
#ifdef OUTSHINE_GPU_VALIDATION
constexpr bool kGpuValidation = true;
#else
constexpr bool kGpuValidation = false;
#endif
} // namespace

namespace {

/* Camera-relative: vertices arrive pre-translated by (origin-cam), so the eye is at the ORIGIN and
 * the view is pure rotation -- no absolute ECEF coordinate ever reaches float. */
void MvpCamRel(float *m, const double R[3], const double Uc[3], const double F[3], int w, int h,
               float fovDeg, float orthoM) {
  const float fov = fovDeg * 3.14159265f / 180.0f, asp = (float)w / (float)h;
  const float zn = Renderer::kNearM;
  const float f = 1.0f / std::tan(fov / 2.0f);
  const float v[16] = {(float)R[0], (float)Uc[0], -(float)F[0], 0,
                       (float)R[1], (float)Uc[1], -(float)F[1], 0,
                       (float)R[2], (float)Uc[2], -(float)F[2], 0,
                       0,           0,            0,            1};
  /* infinite reversed-Z projection ([0,1]): z_clip = zn, w = -z_eye -> depth = zn / -z_eye. */
  float p[16] = {f / asp, 0, 0, 0, 0, f, 0, 0, 0, 0, 0, -1, 0, 0, zn, 0};
  if (orthoM > 0.0f) {
    /* A comparison against a map is a comparison against a PARALLEL projection; under perspective
     * the same field is two different shapes at the centre and at the corner. */
    const float hw = 0.5f * orthoM * asp, hh = 0.5f * orthoM;
    const float zf = 60000.0f, rz = 1.0f / (zf - zn);
    const float q[16] = {1.0f / hw, 0, 0, 0, 0, 1.0f / hh, 0, 0, 0, 0, rz, 0, 0, 0, zf * rz, 1};
    for (int i = 0; i < 16; i++) { p[i] = q[i]; }
  }
  for (int c = 0; c < 4; c++) {
    for (int r = 0; r < 4; r++) {
      m[c * 4 + r] = 0;
      for (int k = 0; k < 4; k++) { m[c * 4 + r] += p[k * 4 + r] * v[c * 4 + k]; }
    }
  }
}

SDL_GPUTextureFormat FormatOf(TexelFormat declared) {
  switch (declared) {
    case TexelFormat::Handle: return SDL_GPU_TEXTUREFORMAT_INVALID;
    case TexelFormat::Rgba16Float: return SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
    case TexelFormat::Rgba32Float: return SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
    case TexelFormat::Rg16Float: return SDL_GPU_TEXTUREFORMAT_R16G16_FLOAT;
    case TexelFormat::R8Unorm: return SDL_GPU_TEXTUREFORMAT_R8_UNORM;
    case TexelFormat::Rgba8UnormSrgb: return SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB;
    case TexelFormat::Depth32Float: return SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
  }
  return SDL_GPU_TEXTUREFORMAT_INVALID;
}

/* binary16 -> f32, by the format's own rules and not by a library: a subnormal, an infinity and a
 * NaN all appear in a render target and a conversion that flushed one would be a second thing that
 * can be wrong inside a measurement. */
float HalfToFloat(uint16_t bits) {
  const uint32_t sign = (uint32_t)(bits & 0x8000u) << 16;
  uint32_t exponent = (bits >> 10) & 0x1Fu;
  uint32_t mantissa = bits & 0x3FFu;
  uint32_t assembled = 0;
  if (exponent == 0) {
    if (mantissa != 0) {
      int shift = 0;
      while ((mantissa & 0x400u) == 0) {
        mantissa <<= 1;
        ++shift;
      }
      mantissa &= 0x3FFu;
      assembled = ((uint32_t)(127 - 15 - shift + 1) << 23) | (mantissa << 13);
    }
  } else if (exponent == 0x1Fu) {
    assembled = 0x7F800000u | (mantissa << 13);
  } else {
    assembled = ((exponent + 127 - 15) << 23) | (mantissa << 13);
  }
  assembled |= sign;
  float value = 0;
  std::memcpy(&value, &assembled, sizeof value);
  return value;
}

} // namespace

void Renderer::SetCameraBasis(const double eye[3], const double fwd[3], const double right[3],
                              const double up[3]) {
  for (int axis = 0; axis < 3; axis++) {
    Eye[axis] = eye[axis];
    Fwd[axis] = fwd[axis];
    Right[axis] = right[axis];
    Up[axis] = up[axis];
  }
  CameraFull = true;
}

/* WHAT THIS DEVICE LAYER CAN EXECUTE. Exhaustive over the catalogue and with no `default:`, so a new
 * stage row does not compile until this answers for it -- and an unimplemented one is a refusal at
 * Init rather than a pass that encodes nothing. */
bool Renderer::Executable(Stage stage) {
  switch (stage) {
    case Stage::Subjects:
    case Stage::Tonemap:
      return true;
    case Stage::SubjectsTransmissive:
    case Stage::CompositeTransmission:
    case Stage::MediumTransmittance:
    case Stage::MediumMultiScatter:
    case Stage::MediumRadiance:
    case Stage::Irradiance:
    case Stage::AutoExposure:
    case Stage::LightVisibility:
    case Stage::Sky:
    case Stage::Sun:
    case Stage::Moon:
    case Stage::Stars:
    case Stage::Terrain:
    case Stage::Buildings:
    case Stage::Water:
    case Stage::Models:
    case Stage::AmbientOcclusion:
    case Stage::TemporalResolve:
    case Stage::Present:
    case Stage::kCount:
      return false;
  }
  return false;
}

void Renderer::Init(int width, int height, std::shared_ptr<const RenderPlan> plan) {
  Plan_ = std::move(plan);
  Width = width;
  Height = height;

  for (const Stage stage : Plan_->Order()) {
    if (Executable(stage)) { continue; }
    Log::Error("render", "stage_not_executed", {{"stage", Row(stage).Name}});
    return;
  }

  if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
    Log::Error("render", "no_video", {{"msg", SDL_GetError()}});
    return;
  }
  /* THE API-CONTRACT ARM, AND IT IS A BUILD FLAG RATHER THAN A CALL (board:1123). The driver's own
   * validation is the only instrument in this tree whose domain is the CONTRACT rather than the
   * picture: a pipeline whose output set disagrees with its pass renders correctly and is undefined,
   * which is how a pruned attachment passed 118 tests and aborted under one flag. It is compiled in
   * rather than read from the environment for the reason every other arm is -- the library declares
   * what it needs from a host and reads nothing behind the host's back -- and it is off in every
   * build that does not ask, so no shipping frame pays for it. */
  SDL_GPUDevice *device =
      SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_MSL, kGpuValidation, nullptr);
  if (!device) {
    Log::Error("render", "no_device", {{"msg", SDL_GetError()}});
    return;
  }

  Device_ = OwnedDevice(device);
  Handles.Device = device;
  Handles.HdrFormat = FormatOf(Plan_->Format(Resource::SceneHdr));
  Handles.SurfaceFormat = FormatOf(Plan_->Format(Resource::FrameTex));
  Handles.Width = Width;
  Handles.Height = Height;
  /* A base colour uploaded as exact linear floats needs the device to filter them; without it the
   * subject unit refuses rather than sampling a texture it cannot filter. */
  /* THE SCENE PASS'S OWN ATTACHMENT SET, HANDED TO THE STAGES THAT DRAW INTO IT (board:1121). Taken
   * from the compiled plan rather than from the catalogue rows, because the prune is exactly the
   * difference between the two: a row says what a stage draws into, the plan says what this picture
   * attaches. The pass is found by the depth target, which is what makes a pass a GEOMETRY pass. */
  for (const RenderPlan::Pass &pass : Plan_->Passes()) {
    if (pass.Kind == PassKind::Compute || pass.Depth == kNoEdge) { continue; }
    Handles.SceneColours = pass.Colours;
    break;
  }
  Handles.FiltersFloat32 =
      SDL_GPUTextureSupportsFormat(device, SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT,
                                   SDL_GPU_TEXTURETYPE_2D, SDL_GPU_TEXTUREUSAGE_SAMPLER);

  /* WHAT THE PLAN HOLDS AND NOTHING ELSE. Resources first, then stages in the plan's derived order
   * -- which is a topological order of the read/write graph, so a stage can never bind a resource
   * that has not been created. */
  for (size_t r = 0; r < kResourceCount; ++r) {
    const Resource id = static_cast<Resource>(r);
    if (Plan_->Holds(id)) { Create(id); }
  }
  for (const Stage stage : Plan_->Order()) {
    std::string why;
    if (Configure(stage, why)) { continue; }
    Log::Error("render", "stage_not_configured", {{"stage", Row(stage).Name}, {"msg", why}});
    return;
  }
  Ready = true;

  Log::Info("render", "device_ready",
            {{"width", Width},
             {"height", Height},
             {"driver", SDL_GetGPUDeviceDriver(device)},
             {"plan", Plan_->Digest()},
             {"passes", Plan_->PassCount()},
             {"stages", (int)Plan_->Order().size()},
             {"f32filter", Handles.FiltersFloat32}});
  for (const std::string &merge : Plan_->Merges()) {
    Log::Info("render", "plan_merge", {{"merge", merge}});
  }
  for (const std::string &alias : Plan_->Aliases()) {
    Log::Info("render", "plan_alias", {{"alias", alias}});
  }
}

/* WHAT A RESOURCE IS ON THIS DEVICE. Exhaustive over the catalogue and with no `default:`. The arms
 * that create nothing are the resources only an unexecuted stage wants: `Init` has already refused
 * every plan that could reach one, so they are unreachable rather than empty. */
void Renderer::Create(Resource resource) {
  const auto target = [&](Resource of, SDL_GPUTextureUsageFlags usage) {
    SDL_GPUTextureCreateInfo wanted{};
    wanted.type = SDL_GPU_TEXTURETYPE_2D;
    wanted.format = FormatOf(Plan_->Format(of));
    wanted.usage = usage;
    wanted.width = (uint32_t)Width;
    wanted.height = (uint32_t)Height;
    wanted.layer_count_or_depth = 1;
    wanted.num_levels = 1;
    wanted.sample_count = SDL_GPU_SAMPLECOUNT_1;
    return OwnedTexture(Device_.Get(), SDL_CreateGPUTexture(Device_.Get(), &wanted));
  };
  const SDL_GPUTextureUsageFlags colour =
      SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;

  switch (resource) {
    case Resource::LinearSampler: {
      SDL_GPUSamplerCreateInfo wanted{};
      wanted.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
      wanted.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
      wanted.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
      wanted.min_filter = SDL_GPU_FILTER_LINEAR;
      wanted.mag_filter = SDL_GPU_FILTER_LINEAR;
      wanted.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
      Samp = OwnedSampler(Device_.Get(), SDL_CreateGPUSampler(Device_.Get(), &wanted));
      return;
    }
    case Resource::SceneHdr: HdrTex = target(resource, colour); return;
    case Resource::SceneTransmissive: TransmissiveTex = target(resource, colour); return;
    case Resource::SceneComposited: CompositedTex = target(resource, colour); return;
    case Resource::SceneVelocity: VelTex = target(resource, colour); return;
    case Resource::SceneShadingNormal: ShadingNormalTex = target(resource, colour); return;
    case Resource::SceneSurfaceIdentity: SurfaceIdentityTex = target(resource, colour); return;
    case Resource::SceneDepth:
      /* SDL_GPU admits exactly two usages on a depth format, so the depth the display transfer reads
       * is a sampled texture and never a storage one. */
      DepthTex = target(resource, SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET |
                                      SDL_GPU_TEXTUREUSAGE_SAMPLER);
      return;
    case Resource::FrameTex: FrameTex = target(resource, colour); return;
    case Resource::Surface: OffscreenTex = target(resource, colour); return;
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
    case Resource::AoBuffer:
    case Resource::SceneLinear:
    case Resource::kCount:
      return;
  }
}

SDL_GPUTexture *Renderer::Target(Resource resource) const {
  switch (resource) {
    case Resource::SceneHdr: return HdrTex.Get();
    case Resource::SceneTransmissive: return TransmissiveTex.Get();
    case Resource::SceneComposited: return CompositedTex.Get();
    case Resource::SceneVelocity: return VelTex.Get();
    case Resource::SceneShadingNormal: return ShadingNormalTex.Get();
    case Resource::SceneSurfaceIdentity: return SurfaceIdentityTex.Get();
    case Resource::SceneDepth: return DepthTex.Get();
    case Resource::FrameTex: return FrameTex.Get();
    case Resource::Surface: return OffscreenTex.Get();
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
    case Resource::AoBuffer:
    case Resource::SceneLinear:
    case Resource::kCount:
      return nullptr;
  }
  return nullptr;
}

/* THE DISPLAY TRANSFER THE PLAN DECLARED, as the stage that emits it reads it. */
DisplayOptions Renderer::Display(void) const {
  DisplayOptions options;
  options.Exposure = Plan_->Exposure();
  options.Curve = Plan_->Display();
  return options;
}

SDL_GPUTexture *Renderer::LinearSource(void) const {
  return Target(Plan_->Bound(Resource::SceneLinear));
}

bool Renderer::Configure(Stage stage, std::string &error) {
  switch (stage) {
    case Stage::Subjects: return Subjects_.Configure(Handles, error);
    case Stage::Tonemap:
      return Tonemap_.Configure(Handles, LinearSource(), DepthTex.Get(), Samp.Get(), Display(),
                                error);
    case Stage::SubjectsTransmissive:
    case Stage::CompositeTransmission:
    case Stage::MediumTransmittance:
    case Stage::MediumMultiScatter:
    case Stage::MediumRadiance:
    case Stage::Irradiance:
    case Stage::AutoExposure:
    case Stage::LightVisibility:
    case Stage::Sky:
    case Stage::Sun:
    case Stage::Moon:
    case Stage::Stars:
    case Stage::Terrain:
    case Stage::Buildings:
    case Stage::Water:
    case Stage::Models:
    case Stage::AmbientOcclusion:
    case Stage::TemporalResolve:
    case Stage::Present:
    case Stage::kCount:
      error = "this device layer does not execute the stage";
      return false;
  }
  error = "this device layer does not execute the stage";
  return false;
}

void Renderer::EncodeStage(Stage stage, const PassRecording &into) {
  FrameContext ctx{};
  for (int axis = 0; axis < 3; axis++) { ctx.Eye[axis] = Eye[axis]; }
  MvpCamRel(ctx.Mvp16, Right, Up, Fwd, Width, Height, FovDeg, OrthoM);
  for (int axis = 0; axis < 3; axis++) {
    ctx.PrevEye[axis] = Submitted ? PrevEye[axis] : ctx.Eye[axis];
  }
  for (int at = 0; at < 16; at++) {
    ctx.PrevMvp16[at] = Submitted ? PrevMvp16[at] : ctx.Mvp16[at];
  }
  switch (stage) {
    case Stage::Subjects: Subjects_.Encode(ctx, into); return;
    case Stage::Tonemap: Tonemap_.Encode(ctx, into); return;
    case Stage::SubjectsTransmissive:
    case Stage::CompositeTransmission:
    case Stage::MediumTransmittance:
    case Stage::MediumMultiScatter:
    case Stage::MediumRadiance:
    case Stage::Irradiance:
    case Stage::AutoExposure:
    case Stage::LightVisibility:
    case Stage::Sky:
    case Stage::Sun:
    case Stage::Moon:
    case Stage::Stars:
    case Stage::Terrain:
    case Stage::Buildings:
    case Stage::Water:
    case Stage::Models:
    case Stage::AmbientOcclusion:
    case Stage::TemporalResolve:
    case Stage::Present:
    case Stage::kCount:
      return;
  }
}

/* THE ONE PLACE A PASS DESCRIPTOR IS BUILT, and every field of it comes from the plan: the target
 * set is the union of what the pass's stages write and contribute, in the catalogue's own order, so
 * a pipeline's attachment order and a pass's attachment order cannot disagree.
 *
 * WHAT AN ATTACHMENT IS CLEARED TO IS A STATEMENT: the scene target's clear is what a pixel nothing
 * drew carries, the velocity sentinel says "nothing dynamic wrote this pixel", and the reversed-Z
 * depth clears to the far plane at zero. */
void Renderer::EncodePass(SDL_GPUCommandBuffer *commands, size_t pass) {
  const RenderPlan::Pass &declared = Plan_->Passes()[pass];
  SDL_GPUColorTargetInfo colours[kMaxColourAttachments] = {};
  uint32_t colourCount = 0;
  for (const Resource wanted : declared.Colours) {
    SDL_GPUColorTargetInfo &attachment = colours[colourCount++];
    attachment.texture = Target(wanted);
    attachment.load_op = SDL_GPU_LOADOP_CLEAR;
    attachment.store_op = SDL_GPU_STOREOP_STORE;
    attachment.clear_color = wanted == Resource::SceneVelocity
                                 ? SDL_FColor{kVelocityStatic, kVelocityStatic, 0, 0}
                                 : SDL_FColor{0, 0, 0, 1};
  }
  SDL_GPUDepthStencilTargetInfo depth{};
  if (declared.Depth != kNoEdge) {
    depth.texture = Target(declared.Depth);
    depth.load_op = SDL_GPU_LOADOP_CLEAR;
    depth.store_op = SDL_GPU_STOREOP_STORE;
    depth.clear_depth = 0.0f;
    depth.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
    depth.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
  }
  PassRecording into{commands, SDL_BeginGPURenderPass(commands, colours, colourCount,
                                                      declared.Depth != kNoEdge ? &depth : nullptr)};
  for (size_t at = 0; at < declared.Count; ++at) {
    EncodeStage(Plan_->Order()[declared.First + at], into);
  }
  SDL_EndGPURenderPass(into.Pass);
}

void Renderer::RenderFrame(void) {
  if (!Ready || !CameraFull) { return; }
  SDL_GPUCommandBuffer *commands = SDL_AcquireGPUCommandBuffer(Device_.Get());
  /* THE PASSES ARE THE COMPILER'S. There is no tally here and no fixed enumeration to keep one
   * against: the count, the order and the attachment set of every pass are what Compile derived. */
  for (size_t pass = 0; pass < Plan_->Passes().size(); ++pass) { EncodePass(commands, pass); }
  SDL_SubmitGPUCommandBuffer(commands);
  for (int axis = 0; axis < 3; axis++) { PrevEye[axis] = Eye[axis]; }
  MvpCamRel(PrevMvp16, Right, Up, Fwd, Width, Height, FovDeg, OrthoM);
  Submitted = true;
}

void Renderer::WaitForGpu(void) {
  if (!Ready) { return; }
  SDL_WaitForGPUIdle(Device_.Get());
}

ReadState Renderer::ReadPixels(std::vector<uint8_t> &rgba) {
  if (!Ready || !FrameTex) { return ReadState::Failed; }
  Readback read;
  if (read.FromTexture(Device_.Get(), FrameTex.Get(), (uint32_t)Width, (uint32_t)Height, 4u) !=
      ReadState::Ready) {
    return ReadState::Failed;
  }
  rgba.assign(read.Rows(), read.Rows() + (size_t)Width * (size_t)Height * 4u);
  return ReadState::Ready;
}

ReadState Renderer::ReadDepth(std::vector<float> &depth) {
  if (!Ready || !DepthTex) { return ReadState::Failed; }
  Readback read;
  if (read.FromTexture(Device_.Get(), DepthTex.Get(), (uint32_t)Width, (uint32_t)Height, 4u) !=
      ReadState::Ready) {
    return ReadState::Failed;
  }
  depth.resize((size_t)Width * (size_t)Height);
  std::memcpy(depth.data(), read.Rows(), depth.size() * sizeof(float));
  return ReadState::Ready;
}

/* THE SCENE-REFERRED LINEAR TAP. Same cost model as the depth read: it copies a texture that already
 * exists, on the frames a caller asks for and never on a frame nobody asks. `SceneLinear` is the
 * scene target under the plan's alias, and the plan publishes which -- so a comparison knows which
 * image it got. */
ReadState Renderer::ReadSceneLinear(std::vector<float> &rgba) {
  SDL_GPUTexture *source = LinearSource();
  if (!Ready || !source) { return ReadState::Failed; }
  const bool wide = Plan_->Format(Resource::SceneLinear) == TexelFormat::Rgba32Float;
  Readback read;
  if (read.FromTexture(Device_.Get(), source, (uint32_t)Width, (uint32_t)Height, wide ? 16u : 8u) !=
      ReadState::Ready) {
    return ReadState::Failed;
  }
  const size_t components = (size_t)Width * (size_t)Height * 4u;
  rgba.resize(components);
  if (wide) {
    std::memcpy(rgba.data(), read.Rows(), components * sizeof(float));
    return ReadState::Ready;
  }
  /* The half path widens rather than reinterpreting: binary16 to f32 is exact, so one currency
   * leaves this call and the plan is what says which storage produced it. */
  for (size_t component = 0; component < components; ++component) {
    uint16_t bits = 0;
    std::memcpy(&bits, read.Rows() + component * sizeof(uint16_t), sizeof bits);
    rgba[component] = HalfToFloat(bits);
  }
  return ReadState::Ready;
}

/* THE THIRD LEG OF THE NORMAL COMPARISON (board:1122). Read at rgba16float and widened the way the
 * scene tap is: binary16 to f32 is exact, so one currency leaves this call. The fourth channel is
 * the shader's own marker -- 1 where a lobe was shaded and the vector is a direction, and the
 * emissive arms write a declared zero VECTOR there, so a reader tells "no shading normal" from "a
 * normal pointing away" by length rather than by a threshold. */
ReadState Renderer::ReadShadingNormal(std::vector<float> &xyz) {
  SDL_GPUTexture *source = ShadingNormalTex.Get();
  if (!Ready || !source) { return ReadState::Failed; }
  Readback read;
  if (read.FromTexture(Device_.Get(), source, (uint32_t)Width, (uint32_t)Height, 8u) !=
      ReadState::Ready) {
    return ReadState::Failed;
  }
  const size_t components = (size_t)Width * (size_t)Height * 4u;
  xyz.resize(components);
  for (size_t component = 0; component < components; ++component) {
    uint16_t bits = 0;
    std::memcpy(&bits, read.Rows() + component * sizeof(uint16_t), sizeof bits);
    xyz[component] = HalfToFloat(bits);
  }
  return ReadState::Ready;
}

/* WHICH SURFACE SLOT THE FRAGMENT WORE, one value per pixel (board:1138). It is read at the
 * attachment's own f32 and widened by nothing, so what leaves this call is the number the fragment
 * wrote rather than a decode of it.
 *
 * ZERO MEANS NO SUBJECT FRAGMENT REACHED THIS PIXEL, which is why the slot is written one higher
 * than it is: the target is cleared before the pass and a slot 0 written verbatim would be
 * indistinguishable from the clear. A reader therefore needs no second mask to tell the two apart,
 * and cannot mistake the sky for the subject's first material. */
ReadState Renderer::ReadSceneVelocity(std::vector<float> &xy) {
  SDL_GPUTexture *source = VelTex.Get();
  if (!Ready || !source) { return ReadState::Failed; }
  Readback read;
  if (read.FromTexture(Device_.Get(), source, (uint32_t)Width, (uint32_t)Height, 4u) !=
      ReadState::Ready) {
    return ReadState::Failed;
  }
  const size_t components = (size_t)Width * (size_t)Height * 2u;
  xy.resize(components);
  for (size_t component = 0; component < components; ++component) {
    uint16_t bits = 0;
    std::memcpy(&bits, read.Rows() + component * sizeof(uint16_t), sizeof bits);
    xy[component] = HalfToFloat(bits);
  }
  return ReadState::Ready;
}

ReadState Renderer::ReadSurfaceIdentity(std::vector<float> &slot) {
  SDL_GPUTexture *source = SurfaceIdentityTex.Get();
  if (!Ready || !source) { return ReadState::Failed; }
  Readback read;
  if (read.FromTexture(Device_.Get(), source, (uint32_t)Width, (uint32_t)Height, 16u) !=
      ReadState::Ready) {
    return ReadState::Failed;
  }
  const size_t components = (size_t)Width * (size_t)Height * 4u;
  slot.resize(components);
  std::memcpy(slot.data(), read.Rows(), components * sizeof(float));
  return ReadState::Ready;
}

} // namespace outshine::Render
