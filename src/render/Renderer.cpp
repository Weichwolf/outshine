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
/* `jitterX` and `jitterY` are a sub-pixel offset in PIXELS, zero on every plan that declares no
 * temporal resolve (board:1413).
 *
 * IT GOES IN THE z COLUMN AND NOT IN THE TRANSLATION, and that is the whole of what makes it a
 * SUB-PIXEL offset rather than a depth-dependent skew. Under perspective `w = -z_eye`, so a constant
 * added to `x_clip` moves a near vertex by many pixels and a far one by none; what has to be constant
 * is `x_ndc = x_clip / w`, so the term added to `x_clip` must itself carry `w` -- which is exactly
 * what an entry multiplying `z_eye` does. A parallel projection has `w = 1` and the same offset
 * belongs in the translation instead, which is why the two branches spell it differently. */
void MvpCamRel(float *m, const double R[3], const double Uc[3], const double F[3], double w, double h,
               float fovDeg, float orthoM, float jitterX, float jitterY, float nearM) {
  const float fov = fovDeg * 3.14159265f / 180.0f, asp = (float)w / (float)h;
  const float zn = nearM;
  const float f = 1.0f / std::tan(fov / 2.0f);
  const float v[16] = {(float)R[0], (float)Uc[0], -(float)F[0], 0,
                       (float)R[1], (float)Uc[1], -(float)F[1], 0,
                       (float)R[2], (float)Uc[2], -(float)F[2], 0,
                       0,           0,            0,            1};
  /* infinite reversed-Z projection ([0,1]): z_clip = zn, w = -z_eye -> depth = zn / -z_eye. */
  float p[16] = {f / asp, 0, 0, 0, 0, f, 0, 0, 0, 0, 0, -1, 0, 0, zn, 0};
  /* Two NDC units span the frame, so a pixel is `2 / w` of it. */
  const float ndcX = w > 0 ? 2.0f * jitterX / (float)w : 0.0f;
  const float ndcY = h > 0 ? 2.0f * jitterY / (float)h : 0.0f;
  p[8] = -ndcX;
  p[9] = -ndcY;
  if (orthoM > 0.0f) {
    /* A comparison against a map is a comparison against a PARALLEL projection; under perspective
     * the same field is two different shapes at the centre and at the corner. */
    const float hw = 0.5f * orthoM * asp, hh = 0.5f * orthoM;
    const float zf = 60000.0f, rz = 1.0f / (zf - zn);
    float q[16] = {1.0f / hw, 0, 0, 0, 0, 1.0f / hh, 0, 0, 0, 0, rz, 0, 0, 0, zf * rz, 1};
    q[12] = ndcX;
    q[13] = ndcY;
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
    case Stage::Overlay:
    case Stage::Present:
      return true;
    case Stage::TemporalResolve:
      return true;
    case Stage::SubjectsTransmissive:
    case Stage::CompositeTransmission:
      return true;
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

  Ready = false;
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
  /* **THE DEVICE OUTLIVES THE PLAN, AND A SECOND `Init` KEEPS IT.** A host claims its window for the
   * device this library chose (`Device()`), so replacing the device on a later `Init` would pull the
   * ground out from under the host's swapchain -- and every resource still held from the old device
   * would be released against a destroyed one. [MEASURED] a browser showing a second case segfaulted
   * on exactly that, which is the shape a windowed host takes every time somebody selects a different
   * case.
   *
   * **WHAT THE PLAN OWNS IS REBUILT AND WHAT THE HOST PAIRED WITH IS NOT.** That is the line: a plan
   * is a picture and a device is an agreement with a host. */
  if (!Device_) {
    SDL_GPUDevice *device =
        SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_MSL, kGpuValidation, nullptr);
    if (!device) {
      Log::Error("render", "no_device", {{"msg", SDL_GetError()}});
      return;
    }
    Device_ = OwnedDevice(device);
  }
  /* EVERY FRAME SUBMITTED AGAINST THE OLD PLAN HAS TO RETIRE BEFORE ITS TARGETS GO, or a texture is
   * released while the device is still reading it. */
  SDL_WaitForGPUIdle(Device_.Get());
  SDL_GPUDevice *const device = Device_.Get();
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
  /* AND IT IS THE STAGE'S OWN PASS, NOT THE FIRST GEOMETRY ONE (board:1386). With a transmissive
   * pass in the plan there are TWO geometry passes and they attach different sets -- the opaque one
   * carries the shading normal and the surface identity, the transmissive one carries neither -- so
   * handing every stage the first pass's set builds pipelines that declare a colour attachment the
   * pass does not set. [MEASURED] Metal's own validation says exactly that: *for color attachment 2,
   * the renderPipelineState pixelFormat must be MTLPixelFormatInvalid, as no texture is set*, and it
   * aborted 30 arms. It is `board:1121`'s defect in a new place, which is why its rule -- the set
   * comes from the compiled plan and never from the catalogue row -- is applied per stage here. */
  for (const RenderPlan::Pass &pass : Plan_->Passes()) {
    if (pass.Kind == PassKind::Compute || pass.Depth == kNoEdge) { continue; }
    Handles.SceneColours = pass.Colours;
    break;
  }
  const auto coloursOfPassWith = [this](Stage wanted) {
    for (const RenderPlan::Pass &pass : Plan_->Passes()) {
      for (size_t at = pass.First; at < pass.First + pass.Count; ++at) {
        if (Plan_->Order()[at] == wanted) { return pass.Colours; }
      }
    }
    return Handles.SceneColours;
  };
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
  /* WHETHER THE PLAN PULLED THE TRANSMISSIVE PASS, ANSWERED BEFORE ANY SUBJECT IS SET (board:1386):
   * a subject arrives before a stage is configured, so a unit the plan never asked for must not be
   * handed one. */
  for (const Stage stage : Plan_->Order()) {
    if (stage == Stage::SubjectsTransmissive) { DrawsGlass_ = true; }
  }
  for (const Stage stage : Plan_->Order()) {
    std::string why;
    Handles.SceneColours = coloursOfPassWith(stage);
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
    /* THE CONSUMER'S ATLAS IS NOT THE PLAN'S TO MAKE. It is `Given` like a sampler -- the client
     * hands it over through `SetOverlayAtlas` -- and creating one here would be the engine deciding
     * what an interface is made of. The stage carries one white texel until it is given something. */
    case Resource::OverlayAtlas: return;
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
    /* THE SURFACE IS THE HOST'S AND THIS LIBRARY ALLOCATES NONE (board:1443). A window's swapchain
     * image is acquired per frame and owned by whoever owns the window; a headless host hands in a
     * texture of its own. Either way the engine is given one, which is what every renderer library
     * does and what makes this one usable by a host at all. */
    case Resource::Surface: return;
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
      return;
    /* ITS OWN ARM AND NOT A LABEL ON THE ONE ABOVE, which is how this first went wrong: sharing the
     * body with `Meter`, `ShadowAtlas` and `AoBuffer` made a HELD one of those allocate a texture at
     * its own format, and those formats are `Handle`. [MEASURED] Metal aborted with *MTLTextureDescriptor
     * has invalid pixelFormat (0)* -- the tonemap reads `AoBuffer`, so it is held on every plan. */
    case Resource::SceneLinear:
      /* TWO, AND ONLY WHERE THE PLAN PULLED THE RESOLVE. `SceneLinear` is an alias otherwise and
       * `Allocate` is never reached for it; where it is real, the resolve reads what the previous
       * frame wrote, so the pair is what makes that possible without a copy. */
      LinearTex_[0] = target(resource, colour);
      LinearTex_[1] = target(resource, colour);
      LinearAt_ = 0;
      HistoryHeld_ = false;
      return;
    case Resource::kCount:
      return;
  }
}

SDL_GPUTexture *Renderer::Target(Resource resource) const {
  switch (resource) {
    /* THE ATLAS IS NOT A TARGET AND NOTHING DRAWS INTO IT. It lives inside the stage that samples it,
     * which is where a `Given` resource belongs. */
    case Resource::OverlayAtlas: return nullptr;
    case Resource::SceneHdr: return HdrTex.Get();
    case Resource::SceneTransmissive: return TransmissiveTex.Get();
    case Resource::SceneComposited: return CompositedTex.Get();
    case Resource::SceneVelocity: return VelTex.Get();
    case Resource::SceneShadingNormal: return ShadingNormalTex.Get();
    case Resource::SceneSurfaceIdentity: return SurfaceIdentityTex.Get();
    case Resource::SceneDepth: return DepthTex.Get();
    case Resource::FrameTex: return FrameTex.Get();
    /* THE HOST'S SURFACE WHERE IT DECLARED ONE, AND THIS ENGINE'S OWN WHERE IT DID NOT (board:1443).
     * One expression rather than a branch on a mode, so a windowed host and a headless one travel the
     * same path and the picture is a function of the declaration either way. */
    case Resource::Surface: return HostSurface_;
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
      return nullptr;
    /* Its own arm, for the reason `Create` records one line up. */
    case Resource::SceneLinear:
      return LinearTex_[LinearAt_].Get();
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
  /* THE THIRD FIELD, AND IT WAS THE ONE THAT DECIDED WHICH FRAGMENT EXISTS (board:1413). The two
   * display fragments carry one entry point between them -- `fragment float4 fs` writes the display
   * alone, `fragment Resolved fs` writes the resolved scene at `color(0)` and the display at
   * `color(1)` -- and `Configure` picks by this flag. It was never assigned, so a plan that HELD the
   * resolve compiled the fragment that does not perform it: the pass attached `SceneLinear` and
   * `FrameTex`, the single output landed on `SceneLinear`, `FrameTex` was written by nothing, and the
   * shader declared two images against the four the pass bound. [MEASURED] 924 488 covered pixels of
   * one arm's probes came back non-finite, and a constant written into the fused fragment never
   * reached the readback -- because that fragment was never compiled. */
  options.Temporal = Plan_->Holds(Stage::TemporalResolve);
  return options;
}

/* **THE ONE PLACE A RATIO BECOMES A PIXEL.** The host declared a rectangle in fractions and a shape to
 * keep inside it; here is where the surface's own size is known, so here is where the two meet -- and
 * the projection, the viewport and the aspect all read this one answer rather than three arithmetics
 * that can disagree. It is not rounded: rounding a rectangle changes its shape, and a camera framed for
 * one shape may not be projected into another. */
Renderer::Placed Renderer::PictureRect(void) const {
  Placed out;
  out.LeftPx = 0;
  out.TopPx = 0;
  out.WidthPx = (double)Width;
  out.HeightPx = (double)Height;
  if (RegionW_ > 0 && RegionH_ > 0) {
    out.LeftPx = RegionX_ * (double)Width;
    out.TopPx = RegionY_ * (double)Height;
    out.WidthPx = RegionW_ * (double)Width;
    out.HeightPx = RegionH_ * (double)Height;
  }
  if (RegionAspect_ > 0 && out.WidthPx > 0 && out.HeightPx > 0) {
    const double fitted = out.WidthPx / out.HeightPx > RegionAspect_ ? out.HeightPx * RegionAspect_
                                                                    : out.WidthPx;
    const double tall = fitted / RegionAspect_;
    out.LeftPx += (out.WidthPx - fitted) / 2.0;
    out.TopPx += (out.HeightPx - tall) / 2.0;
    out.WidthPx = fitted;
    out.HeightPx = tall;
  }
  return out;
}

double Renderer::PictureW(void) const { return PictureRect().WidthPx; }
double Renderer::PictureH(void) const { return PictureRect().HeightPx; }

SDL_GPUTextureFormat Renderer::SurfaceFormat(void) const {
  return Plan_ ? FormatOf(Plan_->Format(Resource::Surface)) : SDL_GPU_TEXTUREFORMAT_INVALID;
}

SDL_GPUTexture *Renderer::LinearSource(void) const {
  return Target(Plan_->Bound(Resource::SceneLinear));
}

bool Renderer::Configure(Stage stage, std::string &error) {
  switch (stage) {
    case Stage::Subjects:
      if (DrawsGlass_) { Subjects_.GlassIsDrawnElsewhere(); }
      return Subjects_.Configure(Handles, error);
    case Stage::SubjectsTransmissive:
      /* THE BACKGROUND IS GIVEN BEFORE `Configure` BECAUSE IT DECIDES WHICH PIPELINES ARE BUILT
       * (board:1386): without it the unit builds no transmissive pipeline and refuses a
       * transmissive slot by name. */
      Glass_.SeeThroughTo(HdrTex.Get(), Samp.Get());
      return Glass_.Configure(Handles, error);
    case Stage::CompositeTransmission:
      return CompositeTransmission_.Configure(Handles, HdrTex.Get(), TransmissiveTex.Get(),
                                              Samp.Get(),
                                              FormatOf(Plan_->Format(Resource::SceneComposited)),
                                              error);
    case Stage::TemporalResolve:
      /* FUSED INTO THE TRANSFER AND SO CONFIGURED BY IT (board:1413). The catalogue makes the two one
       * fragment, so there is nothing of its own to build -- and it is `Executable` rather than
       * refused, because `Init` refuses any stage this layer cannot run and the picture IS made. */
      return true;
    case Stage::Overlay:
      return Overlay_.Configure(Handles, Samp.Get(), FormatOf(Plan_->Format(Resource::FrameTex)),
                                error);
    case Stage::Present:
      return Present_.Configure(Handles, FrameTex.Get(), Samp.Get(), error);
    case Stage::Tonemap:
      return Tonemap_.Configure(Handles, Target(Plan_->Bound(Resource::SceneComposited)),
                                DepthTex.Get(), Samp.Get(),
                                FormatOf(Plan_->Format(Resource::SceneLinear)), Display(), error);
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
  /* THE PROJECTION TAKES THE PICTURE'S OWN SHAPE. Where a host gave no region that is the surface, and
   * where it gave one it is the rectangle the picture will occupy -- so a case framed for 16:9 is
   * projected as 16:9 wherever on the surface it lands. */
  MvpCamRel(ctx.Mvp16, Right, Up, Fwd, PictureW(), PictureH(), FovDeg, OrthoM, Jitter_[0],
            Jitter_[1], NearM);
  for (int axis = 0; axis < 3; axis++) {
    ctx.PrevEye[axis] = Submitted ? PrevEye[axis] : ctx.Eye[axis];
  }
  for (int at = 0; at < 16; at++) {
    ctx.PrevMvp16[at] = Submitted ? PrevMvp16[at] : ctx.Mvp16[at];
  }
  /* THE PICTURE'S STAGES ARE CONFINED TO THE REGION AND THE INTERFACE'S ARE NOT. A viewport is
   * per-draw state inside a pass, so one pass can carry both -- which is what lets the tonemap end at
   * the pane's edge while the overlay reaches the whole surface. */
  const auto within = [&](bool picture) {
    /* **THE VIEWPORT IS FLOATING POINT, SO THE RECTANGLE NEED NOT BE WHOLE PIXELS.** Rounding it
     * changes its ASPECT -- 840 by 472 is 1.779661 where a case declares 1.777778 -- and a camera
     * checked to a part in a trillion refuses that difference. The picture is placed exactly and the
     * driver resolves the fraction, which is what a float viewport is for. */
    SDL_GPUViewport where{};
    const Placed rect = PictureRect();
    where.x = picture ? (float)rect.LeftPx : 0.0f;
    where.y = picture ? (float)rect.TopPx : 0.0f;
    where.w = picture ? (float)rect.WidthPx : (float)Width;
    where.h = picture ? (float)rect.HeightPx : (float)Height;
    where.min_depth = 0.0f;
    where.max_depth = 1.0f;
    if (into.Pass != nullptr) { SDL_SetGPUViewport(into.Pass, &where); }
  };

  switch (stage) {
    case Stage::Subjects:
      within(true);
      Subjects_.Encode(ctx, into);
      return;
    case Stage::SubjectsTransmissive:
      within(true);
      Glass_.Encode(ctx, into);
      return;
    case Stage::CompositeTransmission:
      within(true);
      CompositeTransmission_.Encode(ctx, into);
      return;
    /* NOTHING OF ITS OWN TO ENCODE: the transfer below draws the one fragment that is both. */
    case Stage::TemporalResolve: return;
    case Stage::Tonemap: {
      /* WHAT THE FUSED FRAGMENT READS IS WHAT EVERY CONTRIBUTOR LEFT, through the plan's own alias
       * rather than named here: with no glass in the plan `SceneComposited` IS `SceneHdr`, and a
       * second spelling of that would be a second answer. */
      Tonemap_.Bind(Target(Plan_->Bound(Resource::SceneComposited)));
      const float delta[2] = {Jitter_[0] - PrevJitter_[0], Jitter_[1] - PrevJitter_[1]};
      Tonemap_.BindTemporal(LinearTex_[1 - LinearAt_].Get(), VelTex.Get(), Width, Height, delta,
                            HistoryHeld_);
      within(true);
      Tonemap_.Encode(ctx, into);
      return;
    }
    /* THE INTERFACE IS DRAWN OVER THE FRAME THE TRANSFER JUST PRODUCED, in the target's own pixels.
     * What it draws was handed in by the consumer through `SetOverlay`; nothing here decides what a
     * rectangle means, and the stage is given no camera because an interface has none. */
    case Stage::Overlay:
      within(false);
      Overlay_.Bind(Width, Height);
      Overlay_.Encode(ctx, into);
      return;
    /* THE HOST'S SURFACE ARRIVED WITH THE FRAME AND ITS FORMAT CAME WITH IT, so the pipeline is built
     * on the first frame that names one and reused on every frame after. A host that hands over a
     * different format -- a window moved to another display -- gets a rebuild and not a refusal. */
    case Stage::Present:
      /* THE HOST'S SURFACE ARRIVED WITH THE FRAME AND SO DID ITS FORMAT. The plan states which format
       * `Resource::Surface` carries, which is the same number the host used to create or claim it --
       * so the pipeline is built the first time a frame is presented and reused after. */
      {
        std::string why;
        if (!Present_.For(Handles, SurfaceFormat(), why)) {
          Log::Error("render", "present_not_built", {{"msg", why}});
          return;
        }
      }
      within(false);
      Present_.Encode(ctx, into);
      return;
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
/* THE RADICAL INVERSE OF `index` IN `base`, which is Halton's own definition: write the index in the
 * base and reflect its digits about the point. Two coprime bases give a pair that fills the pixel
 * evenly at every prefix length, which is the property a temporal sequence needs -- an accumulation
 * cut short after three frames should already be well spread.
 *
 * INDEXED FROM ONE, because the radical inverse of zero is zero and a frame that sampled the exact
 * pixel centre would contribute nothing the un-jittered picture did not already have. */
static float RadicalInverse(int index, int base) {
  float result = 0.0f;
  float weight = 1.0f / (float)base;
  int at = index + 1;
  while (at > 0) {
    result += weight * (float)(at % base);
    at /= base;
    weight /= (float)base;
  }
  return result;
}

void Renderer::EncodePass(SDL_GPUCommandBuffer *commands, size_t pass) {
  const RenderPlan::Pass &declared = Plan_->Passes()[pass];
  SDL_GPUColorTargetInfo colours[kMaxColourAttachments] = {};
  uint32_t colourCount = 0;
  for (const Resource wanted : declared.Colours) {
    SDL_GPUColorTargetInfo &attachment = colours[colourCount++];
    attachment.texture = Target(wanted);
    attachment.load_op = SDL_GPU_LOADOP_CLEAR;
    attachment.store_op = SDL_GPU_STOREOP_STORE;
    /* THE SCENE'S ALPHA CLEARS TO ZERO BECAUSE ALPHA IS COVERAGE AND NOTHING HAS COVERED IT YET
     * (board:1423). It cleared to ONE, and the `over` blend a transparent surface composites with --
     * `src ONE, dst ONE_MINUS_SRC_ALPHA` -- then yields `a_src + 1 * (1 - a_src)` = 1 at every pixel,
     * so a blended fragment's own coverage was destroyed before anything could read it.
     *
     * AN OPAQUE ARM STILL WRITES ONE AND REPLACES, so a BLACK opaque subject stays covered -- which is
     * the property the display transfer took from the depth buffer and must keep.
     *
     * **`SceneTransmissive` CARRIES COVERAGE FOR EXACTLY THE SAME REASON AND CLEARED TO ONE**
     * (board:1459) -- and
     * that reading it as *premultiplied, so its one is harmless* was backwards, because the stage that
     * composites it computes `behind * (1 - front.a) + front`. An UNCOVERED pixel at alpha one makes
     * that `behind * 0`, so **every pixel no transmissive fragment touched erased the opaque scene
     * underneath it**: a subject carrying one sheet of glass drew the glass and nothing else.
     *
     * [MEASURED] `ABeautifulGame` went from **258 of 102 480 sampled pixels carrying ink to 9213** --
     * a chess set that was sixteen pawn tops in the dark became a board, thirty-two pieces and their
     * shadows; `GlassVaseFlowers` from 4327 to 9340, one vase to two vases and their flowers.
     *
     * NOTHING IN THE CORPUS COULD SEE IT. Every case there declares `transmission` bounces of zero, so
     * `DeclarePlan` never asks for the transmissive pass and the composite never runs -- the defect
     * lived in the one arm no oracle comparison reaches, and a scenario run found it in its first hour.
     *
     * THE OTHER TARGETS KEEP THEIR ONE: the velocity sentinel says *nothing dynamic wrote this pixel*,
     * and a target whose alpha nobody reads is unchanged either way. */
    const bool carriesCoverage = wanted == Resource::SceneHdr || wanted == Resource::SceneComposited ||
                                 wanted == Resource::SceneTransmissive ||
                                 wanted == Resource::SceneLinear;
    attachment.clear_color = wanted == Resource::SceneVelocity
                                 ? SDL_FColor{kVelocityStatic, kVelocityStatic, 0, 0}
                                 : SDL_FColor{0, 0, 0, carriesCoverage ? 0.0f : 1.0f};
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

void Renderer::BeginTemporalRun(void) {
  HistoryStarted_ = false;
  JitterAt_ = 0;
  Jitter_[0] = 0.0f;
  Jitter_[1] = 0.0f;
  PrevJitter_[0] = 0.0f;
  PrevJitter_[1] = 0.0f;
  LinearAt_ = 0;
  HistoryHeld_ = false;
}

void Renderer::RenderFrame(void) {
  if (!Ready || !CameraFull) { return; }
  /* THE SUB-PIXEL OFFSET AND THE PAIR ADVANCE ONCE PER FRAME AND BEFORE ANY PASS, so every stage in
   * this frame sees one projection and one history -- a jitter advanced inside the pass loop would
   * make the geometry and the resolve disagree about where the frame was sampled.
   *
   * IT MOVES ONLY WHERE THE RESOLVE EXISTS TO UNDO IT (board:1413): a plan that declares no temporal
   * stage keeps the pixel centres it always had, so nothing that declines this feature moves. */
  if (Plan_->Holds(Stage::TemporalResolve)) {
    PrevJitter_[0] = Jitter_[0];
    PrevJitter_[1] = Jitter_[1];
    JitterAt_ = (JitterAt_ + 1) % kJitterPeriod;
    Jitter_[0] = RadicalInverse(JitterAt_, 2) - 0.5f;
    Jitter_[1] = RadicalInverse(JitterAt_, 3) - 0.5f;
    /* The frame that writes index 1 can read what index 0 left; the very first cannot, and says so
     * rather than blending towards a cleared texture. */
    HistoryHeld_ = HistoryStarted_;
    HistoryStarted_ = true;
    LinearAt_ = 1 - LinearAt_;
  }
  SDL_GPUCommandBuffer *commands = SDL_AcquireGPUCommandBuffer(Device_.Get());
  /* THE PASSES ARE THE COMPILER'S. There is no tally here and no fixed enumeration to keep one
   * against: the count, the order and the attachment set of every pass are what Compile derived. */
  for (size_t pass = 0; pass < Plan_->Passes().size(); ++pass) { EncodePass(commands, pass); }
  SDL_SubmitGPUCommandBuffer(commands);
  for (int axis = 0; axis < 3; axis++) { PrevEye[axis] = Eye[axis]; }
  /* THE SAME OFFSET THIS FRAME RASTERISED WITH, because this is what the NEXT frame's velocity is
   * measured against -- and the resolve subtracts the difference of the two. */
  MvpCamRel(PrevMvp16, Right, Up, Fwd, PictureW(), PictureH(), FovDeg, OrthoM, Jitter_[0],
            Jitter_[1], NearM);
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
