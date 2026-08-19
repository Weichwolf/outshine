/* THE DEVICE HALF OF A COMPILED PLAN. Renderer owns the device, the resources the plan holds and
 * every render-pass boundary; drawing lives in the stages that record into the pass it opened. A
 * stage never begins or ends a pass.
 *
 * NOTHING HERE DECIDES WHAT IS RENDERED. The plan says which resources exist, which stages are
 * configured, in what order and in how many passes; this file executes that and holds no composition
 * of its own. There is no pass tally to keep against a fixed enumeration, because the pass count is
 * an output of the compiler.
 *
 * THE CATALOGUE IS WHAT A PLAN CAN SAY AND THIS IS WHAT THIS DEVICE LAYER CAN DO, and the two are
 * not the same set. `Init` refuses a plan holding a stage this layer does not execute, naming it: a
 * stage silently encoding nothing is a black picture with nothing to attribute it to, which is the
 * one failure a renderer must never be able to report as success. */
#ifndef RENDERER_H
#define RENDERER_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <SDL3/SDL_gpu.h>

#include "FrameContext.h"
#include "Gpu.h"
#include "GpuOwned.h"
#include "Readback.h"
#include "RenderPlan.h"
#include "stages/OverlayDraw.h"
#include "stages/PresentStage.h"
#include "stages/Resolve.h"
#include "stages/SubjectDraw.h"
#include "stages/CompositeTransmissionStage.h"
#include "stages/TonemapStage.h"

namespace outshine::Render {

class Renderer {
public:
  /* Bring-up into the render target. The plan is the whole of what will be created and encoded, and
   * it cannot be unvalidated -- it has no public constructor and only RenderPlan::Compile makes one.
   * A refusal leaves `DeviceUsable()` false and says why in the log. */
  void Init(int width, int height, std::shared_ptr<const RenderPlan> plan);
  [[nodiscard]] const RenderPlan &Plan(void) const { return *Plan_; }
  [[nodiscard]] bool DeviceUsable(void) const { return Ready; }
  /* THE DEVICE THIS LIBRARY CHOSE, SO A HOST CAN CLAIM ITS OWN WINDOW FOR IT (board:1443). The engine
   * creates the device because it is the one that knows which shader format and which validation it
   * needs; a host that wants a window on the same device needs the handle and nothing else. */
  [[nodiscard]] SDL_GPUDevice *Device(void) const { return Device_.Get(); }
  /* WHAT FORMAT A SURFACE THE HOST DECLARES MUST BE (board:1443). The plan says which, because the
   * catalogue does; the host has to create a texture the final pass can attach and this is the one
   * number it needs to do that. Invalid before `Init`, which is the same statement as "there is no
   * plan yet". */
  [[nodiscard]] SDL_GPUTextureFormat SurfaceFormat(void) const;
  /* WHERE THE PICTURE GOES, AND THE HOST DECLARES IT (board:1443). A swapchain image is acquired per
   * frame and belongs to whoever owns the window, so this is set per frame; a headless host hands in
   * a texture of its own and sets it once. **This library allocates no surface at all** -- it is given
   * one, which is what every renderer library does and what a plan asking for `Resource::Surface`
   * means.
   *
   * IT IS THE HOST'S DECLARATION AND NOT A MODE: the engine does not learn whether it is windowed, it
   * learns which texture the final pass attaches. */
  void PresentInto(SDL_GPUTexture *surface) { HostSurface_ = surface; }

  /* **WHICH PART OF THE SURFACE THE PICTURE OCCUPIES, IN FRACTIONS OF IT** (board:1447). A browser
   * showing a case beside its lists, a split screen, a mirror in a corridor -- all three are this
   * rectangle, and none of them is a second renderer.
   *
   * **THE HOST SPEAKS RATIOS AND THE LIBRARY KNOWS PIXELS.** That the renderer has a resolution is
   * plain; that a consumer should have to say one is not. Where the picture goes is a ratio, how much
   * of the frame the subject spans is a ratio, and what shape it has is a ratio -- so a window that is
   * resized costs the host nothing at all, and the one place a pixel is resolved is the one place that
   * knows how many there are.
   *
   * `aspect` is the shape to keep INSIDE that rectangle: the largest box of that shape is centred in
   * it, which is what makes a camera framed for 16:9 stay 16:9 wherever it lands. Zero means *take the
   * rectangle as it is*, which is what a consumer filling a whole surface wants. A width or height of
   * zero means the whole surface. */
  void SetPictureRegion(double x, double y, double width, double height, double aspect = 0.0) {
    RegionX_ = x;
    RegionY_ = y;
    RegionW_ = width;
    RegionH_ = height;
    RegionAspect_ = aspect;
  }

  /* Run the passes and submit. It does NOT wait: the device runs behind the caller, which is what a
   * host that wants to prepare the next frame while this one draws depends on. */
  void RenderFrame(void);

  /* Blocks until every frame submitted so far has finished on the device. It is what makes a frame
   * a MEASURABLE quantity -- a submit returns before any of the work happens, so a clock around
   * `RenderFrame` alone times the encoder and not the frame. Nothing on a shipping frame path calls
   * it; the reads already fence for themselves. */
  void WaitForGpu(void);

  /* HOW MANY FRAMES BEFORE THE PICTURE IS THE PICTURE -- a property the compiled plan states, so no
   * caller carries a settle constant of its own. */
  [[nodiscard]] int SettleFrames(void) const { return Plan_ ? Plan_->SettleFrames() : 1; }

  /* Tightly packed W*H*4 RGBA8, already sRGB-encoded -- ready for a PNG writer. */
  [[nodiscard]] ReadState ReadPixels(std::vector<uint8_t> &rgba);

  /* Reversed-Z scene depth, W*H floats, row-major. Range along the view ray follows as
   * kNearM / depth / cos(angle off boresight). */
  [[nodiscard]] ReadState ReadDepth(std::vector<float> &depth);
  static constexpr float kNearM = 0.05f;   /* MvpCamRel's zn -- the numerator of that division */

  /* THE SCENE-REFERRED LINEAR TAP: W*H RGBA floats, row-major -- resolved linear radiance BEFORE the
   * display transfer. It is the zero point a radiance comparison needs, and a plan that holds no
   * such resource refuses.
   *
   * ONE CURRENCY OUT, AND THE PLAN SAYS WHICH STORAGE PRODUCED IT. Under `ScenePrecision::Half` the
   * values are widened from binary16, which is exact; under `Float` they are the target's own bits.
   * A caller whose verdict is the value reads `Plan::Format(Resource::SceneLinear)` to know which
   * floor it is measuring against. */
  [[nodiscard]] ReadState ReadSceneLinear(std::vector<float> &rgba);
  /* THE NORMAL THE BRDF RECEIVED, one xyz per pixel, world-space in the subject's own frame
   * (board:1122). Empty unless the plan holds the target, which it does only where something asked
   * for it -- a readback of an attachment nobody requested would be reading a texture that was
   * never allocated. */
  [[nodiscard]] ReadState ReadShadingNormal(std::vector<float> &xyz);
  /* WHICH SURFACE SLOT THE FRAGMENT WORE, one value per pixel in `x`, ONE HIGHER THAN THE SLOT so
   * that 0 is "no subject fragment here" (board:1138). Empty unless the plan holds the target. A
   * slot is not a material: which material a slot carries is the consumer's own table, and this
   * layer has no spelling for one. */
  [[nodiscard]] ReadState ReadSurfaceIdentity(std::vector<float> &slot);
  /* THE SCREEN-SPACE MOTION OF WHAT WROTE THE DEPTH, two floats per pixel in NDC units per frame
   * (board:1169). Empty unless the plan holds the target. A pixel no subject fragment reached
   * carries the pass's clear value, `kVelocityStatic` in both channels, which is unreachable for a
   * real motion -- NDC displacement is bounded by 2 per axis -- so "static" and "did not move" are
   * two answers here and not one. */
  [[nodiscard]] ReadState ReadSceneVelocity(std::vector<float> &xy);

  /* THE DECLARED SUBJECT OF A STUDIO (stages/SubjectDraw.h): one indexed mesh and the DRAW LIST over
   * it -- many primitives, each with its own surface slot and vertex layout. */
  /* THE INTERFACE THE CONSUMER DECLARED, AS RECTANGLES IN THE TARGET'S OWN PIXELS (board:1442).
   *
   * **THE RENDERER TAKES `OverlayQuad` AND NOT THE UI LIBRARY'S OWN TYPE, and that is the layering
   * rather than an inconvenience.** No content noun has a spelling in the renderer; a box, a glyph and
   * a page are the UI's vocabulary, and a renderer that included them would have learned what it is
   * drawing. The consumer translates -- which costs a loop it already owns and keeps the two ends
   * independent enough that either can be replaced.
   *
   * **IT IS SET OUTSIDE THE FRAME AND THE UPLOAD HAPPENS HERE**, so the frame path binds and draws
   * and touches no allocator. */
  [[nodiscard]] bool SetOverlay(const OverlayQuad *quads, size_t count, std::string &error) {
    return Overlay_.SetQuads(Handles, quads, count, error);
  }
  /* The consumer's atlas -- a glyph sheet, an icon page. RGBA8, tightly packed, held until replaced. */
  [[nodiscard]] bool SetOverlayAtlas(const uint8_t *rgba, int width, int height, std::string &error) {
    return Overlay_.SetAtlas(Handles, rgba, width, height, error);
  }

  [[nodiscard]] bool SetSubjectMesh(const SubjectMesh &mesh, std::string &error) {
    return Subjects_.SetMesh(mesh, error) && (!DrawsGlass_ || Glass_.SetMesh(mesh, error));
  }
  /* THE SUBJECT'S SURFACES, one per slot a draw key can name: the file says which image and how it
   * is addressed, the consumer decodes it, and this holds it. */
  [[nodiscard]] bool SetSubjectMaterials(const std::vector<SubjectMaterial> &materials,
                                         std::string &error) {
    return Subjects_.SetMaterials(materials, error) && (!DrawsGlass_ || Glass_.SetMaterials(materials, error));
  }
  /* THE LIGHTS THE SUBJECT IS LIT BY, as a list. */
  [[nodiscard]] bool SetSubjectLights(const std::vector<SubjectLight> &lights, std::string &error) {
    return Subjects_.SetLights(lights, error) && (!DrawsGlass_ || Glass_.SetLights(lights, error));
  }
  /* THE ENVIRONMENT THE SUBJECT SITS IN (board:1206), declared and zero where none was declared. */
  void SetSubjectEnvironment(const SubjectEnvironment &environment) {
    Subjects_.SetEnvironment(environment);
    if (DrawsGlass_) { Glass_.SetEnvironment(environment); }
  }
  [[nodiscard]] uint32_t SubjectBatchCount(void) const { return Subjects_.BatchCount(); }
  [[nodiscard]] uint32_t SubjectDrawCount(void) const { return Subjects_.DrawCount(); }
  /* HOW MANY SUBJECT PIPELINES `Init` BUILT (board:1187). It is readable because a frame instrument
   * asked whether a pipeline count cost anything must read the count in the same run as the
   * duration; a number quoted from a source diff beside a measurement is the pairing this tree has
   * already been wrong about once. */
  [[nodiscard]] uint32_t SubjectPipelineCount(void) const { return Subjects_.PipelineCount(); }
  /* WHERE A SHADOW RAY STARTS, in the subject's own metres (`stages/ShadowRay.h`). It is readable
   * because it is the ONLY quantity in the visibility estimator a comparison can be displaced by:
   * I.26.15 bounds a shadow-edge disagreement in screen pixels against this bias projected, and a
   * bound over a number nobody can read would be a bound nobody can recompute. */
  [[nodiscard]] float ShadowRayNearM(void) const { return Subjects_.ShadowNearM(); }

  /* Carries ROLL, so the horizon tilts at bank. */
  void SetCameraBasis(const double eye[3], const double fwd[3], const double right[3],
                      const double up[3]);

  /* THE SCENE'S vertical field of view over the full frame height, as the scene file declares it.
   * It enters the projection and nothing else, so there is never a second copy to drift from. */
  void SetFovDeg(double deg) { FovDeg = deg > 0.0 ? (float)deg : FovDeg; }
  void SetOrthoM(double m) { OrthoM = (float)m; }
  /* HOW NEAR THIS CONSUMER DRAWS, DECLARED RATHER THAN FIXED (board:1420).
   *
   * A CONSTANT HERE IS A SIZE OF WORLD THIS ENGINE CANNOT SHOW. `kNearM` was 0.05 m and nothing could
   * override it, so a subject smaller than a matchbox lay entirely in front of the near plane and
   * nothing was drawn at all -- [MEASURED] `MetalRoughSpheresNoTextures` has a radius of 5.6 mm and is
   * framed at 40 mm, so *vertex 0 sits 0.042035 m along the view axis, inside the engine's fixed near
   * plane of 0.050000 m* and the whole picture was a refusal. **A game engine draws a coin in a hand,
   * an inventory item and a scope reticle**, and every one of those is inside 5 cm.
   *
   * IT COSTS NOTHING TO MOVE, AND THAT IS THE PROJECTION'S DOING. The matrix is a reversed-Z infinite
   * one, where depth precision follows 1/z and is nearly uniform in floating point -- which is the
   * whole reason that projection was chosen, and it is why the near plane can be declared by whoever
   * knows how near their world is instead of being a compromise nobody can move.
   *
   * `kNearM` REMAINS THE DEFAULT AND NOT A LIMIT: a consumer that declares nothing renders exactly
   * what it rendered before. */
  void SetNearM(double m) { NearM = m > 0.0 ? (float)m : NearM; }
  [[nodiscard]] float NearMetres(void) const { return NearM; }

  /* WHERE A TEMPORAL SEQUENCE BEGINS, DECLARED BY THE CONSUMER AND NEVER GUESSED (board:1413).
   * Accumulation across frames is only correct while the frames belong to ONE continuous view: a
   * camera cut, a teleport, a new scenario or a benchmark repeat all end the sequence, and history
   * carried across one is the previous run's picture bleeding into this one's.
   *
   * IT IS NOT DETECTED FROM THE CAMERA. A heuristic on how far the eye moved would be a threshold
   * nobody could state, and it would be wrong in both directions -- a fast pan is continuous and a
   * one-metre cut is not. The consumer knows which it did.
   *
   * THIS IS WHAT MAKES THE PICTURE A FUNCTION OF THE DECLARATION AND NOT OF THE PACE: [MEASURED] the
   * frame suite's five repeats of one arm stopped drawing the same picture the moment a resolve was
   * declared, because the second repeat began with the first one's history. */
  void BeginTemporalRun(void);

  /* THE SURFACE'S SIZE, which is the host's, and the PICTURE'S, which is the region the host declared
   * inside it. They are the same number until a host asks for a region, and telling them apart is what
   * lets a browser hold a case beside its lists without every camera in the tree being wrong by the
   * difference. */
  [[nodiscard]] int SceneW(void) const { return Width; }
  [[nodiscard]] int SceneH(void) const { return Height; }
  [[nodiscard]] double PictureW(void) const;
  [[nodiscard]] double PictureH(void) const;
  /* **THE PICTURE'S SHAPE AND NOT THE SURFACE'S.** A parallel projection needs it and a perspective one
   * does not: the vertical field of view fixes the horizontal only once the aspect is known -- and the
   * aspect a camera was framed for is the picture's. One place answers *what shape is the picture*, so
   * the projection and every check against it cannot disagree. */
  [[nodiscard]] double SceneAspect(void) const {
    return PictureH() > 0 ? (double)PictureW() / (double)PictureH() : 0.0;
  }

private:
  /* THE THREE EXHAUSTIVE SWITCHES OVER THE CATALOGUE, and there are no others: whether this layer
   * can execute a stage at all, what a resource IS, and what configures a stage. Each has no
   * `default:`, so a new catalogue row is a compile error until all three answer for it. */
  [[nodiscard]] static bool Executable(Stage stage);
  void Create(Resource resource);
  [[nodiscard]] bool Configure(Stage stage, std::string &error);
  void EncodeStage(Stage stage, const PassRecording &into);
  void EncodePass(SDL_GPUCommandBuffer *commands, size_t pass);
  [[nodiscard]] SDL_GPUTexture *Target(Resource resource) const;
  [[nodiscard]] DisplayOptions Display(void) const;
  /* The texture the linear tap copies, which is whatever the plan bound in `sceneLinear`'s place. */
  [[nodiscard]] SDL_GPUTexture *LinearSource(void) const;

  /* FIRST, SO IT IS DESTROYED LAST (`C.13`): every handle below is taken from it. */
  OwnedDevice Device_;
  /* Null unless a host declared one for this frame. */
  SDL_GPUTexture *HostSurface_ = nullptr;
  std::shared_ptr<const RenderPlan> Plan_;
  Gpu Handles;
  OwnedTexture HdrTex, VelTex, DepthTex, FrameTex;
  /* `KHR_materials_transmission` (board:1386): where a transmissive draw puts its radiance, and the
   * two put together. Kept apart from `HdrTex` because the pass that reads what stands behind it
   * must not also be writing that -- the plan's own topological invariant refuses the shape where it
   * would be, and that refusal is what chose this layout. Both stay unbound on a plan that declares
   * no glass, where `SceneComposited` aliases straight to `SceneHdr`. */
  OwnedTexture TransmissiveTex, CompositedTex;
  /* The normal the BRDF received, allocated only where a plan reads it (board:1122). */
  OwnedTexture ShadingNormalTex;
  /* Which surface slot the fragment wore, allocated only where a plan reads it (board:1138). */
  OwnedTexture SurfaceIdentityTex;
  OwnedSampler Samp;
  SubjectDraw Subjects_;
  /* THE SAME SUBJECT DRAWN AGAIN FOR ITS TRANSMISSIVE SLOTS (board:1386), with the opaque scene
   * bound as what stands behind it. It is the same unit over the same draw list -- the split is
   * stated once, inside the encode loop -- so a subject with no glass encodes nothing here. */
  SubjectDraw Glass_;
  /* WHETHER THE COMPILED PLAN ASKED FOR THE TRANSMISSIVE PASS AT ALL (board:1386). A subject is
   * declared before any stage is configured, so mirroring it into a unit the plan never pulled
   * reaches a unit with no device -- and its refusal, *the subject unit has no device*, then travels
   * out as the SUBJECT's refusal. [MEASURED] that mistake failed every one of the corpus's 444 arms
   * at once, which is the shape of a guard put on the wrong side of a question. */
  bool DrawsGlass_ = false;
  CompositeTransmissionStage CompositeTransmission_;
  TonemapStage Tonemap_;
  OverlayDraw Overlay_;
  PresentStage Present_;

  bool Ready = false;
  int Width = 0, Height = 0;

  /* THE TEMPORAL PAIR AND WHICH OF THEM THIS FRAME WRITES (board:1413). `SceneLinear` is an alias
   * whenever no temporal stage is held, so these exist only on the plans that pull one -- and then
   * there are TWO, because the resolve reads what the previous frame wrote and a texture cannot be
   * both. They are swapped rather than copied: a blit here would be 884 MB/s at 720p60 to move data
   * that never had to move. */
  OwnedTexture LinearTex_[2];
  int LinearAt_ = 0;
  bool HistoryHeld_ = false;
  /* Whether THIS temporal run has already written a history, which is not the same question as
   * whether the renderer has ever submitted a frame -- and conflating the two is what let one arm's
   * accumulation reach the next. */
  bool HistoryStarted_ = false;

  /* THE SUB-PIXEL OFFSET THE PROJECTION CARRIES, AND IT IS WHAT MAKES THE RESOLVE ANTI-ALIASING
   * rather than a smoother of the same samples. Halton(2, 3) over `kJitterPeriod` frames, in pixels
   * and centred on zero.
   *
   * IT IS APPLIED ONLY WHERE THE RESOLVE EXISTS TO UNDO IT. A plan without a temporal stage renders
   * on the pixel centres it always did, so no case that declines this moves by a sub-pixel -- which
   * is the difference between a feature and a change to every picture in the corpus. */
  static constexpr int kJitterPeriod = 8;
  int JitterAt_ = 0;
  float Jitter_[2] = {0.0f, 0.0f};
  float PrevJitter_[2] = {0.0f, 0.0f};
  bool CameraFull = false;                /* SetCameraBasis used */
  /* Fractions of the surface, and the shape to keep inside them. */
  double RegionX_ = 0, RegionY_ = 0, RegionW_ = 0, RegionH_ = 0, RegionAspect_ = 0;
  /* THE RESOLVED RECTANGLE IN THE SURFACE'S OWN PIXELS, derived where the pixels are known and stored
   * so that the projection, the viewport and the aspect all read one answer. */
  struct Placed {
    double LeftPx = 0, TopPx = 0, WidthPx = 0, HeightPx = 0;
  };
  [[nodiscard]] Placed PictureRect(void) const;
  double Eye[3] = {0, 0, 0};
  double Fwd[3] = {0, 0, 0}, Right[3] = {0, 0, 0}, Up[3] = {0, 0, 0};
  float FovDeg = 60.0f;                   /* [SET] until a scene declares one */
  float OrthoM = 0.0f;                    /* > 0: parallel projection covering this many metres */
  float NearM = kNearM;                   /* the declared near plane, defaulting to the constant */
  /* WHERE THE EYE WAS WHEN THE LAST FRAME WAS SUBMITTED, which is the other half of a screen-space
   * motion vector (board:1169). It is written at the END of RenderFrame, so every stage of one
   * frame sees the same previous camera whatever order the passes were compiled into. Before the
   * first submit it is this frame's, which makes the first frame's motion zero rather than the
   * displacement from an undefined pose. */
  bool Submitted = false;
  double PrevEye[3] = {0, 0, 0};
  float PrevMvp16[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
};

} // namespace outshine::Render
#endif
