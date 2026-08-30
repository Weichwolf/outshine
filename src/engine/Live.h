#ifndef OUTSHINE_ENGINE_LIVE_H
#define OUTSHINE_ENGINE_LIVE_H

#include "Shape.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <Outshine.h>
#include <Scenario.h>

#include "Asset.h"
#include "Document.h"
#include "SubjectProxy.h"
#include "Overlay.h"
#include "Layout.h"
#include "Markup.h"
#include "Paint.h"
#include "Pointer.h"
#include "Pose.h"
#include "SceneRenderer.h"
#include "Style.h"
#include "Material.h"
#include "Subject.h"
#include "Surfacing.h"

namespace outshine::Core {

struct Declaration {
  AssetAnimation Animation = AssetAnimation::Play;
  int Clip = 0;

  int SurfaceWidthPx = 0, SurfaceHeightPx = 0;

  std::string Stands;
  std::vector<std::string> Joins;

  std::vector<std::string> Stages;

  // THE PICTURES A CLIENT ASKS THE FRAME TO KEEP, beyond the one it displays. A conformance case
  // states a claim about the DEPTH or the SHADING NORMAL and has to be able to ask for them; a
  // client that asks for nothing gets what the plan needs and no more, which is the fast path.
  std::vector<std::string> Outputs;

  // HOW THE FRAME IS TRANSFERRED TO THE DISPLAY, and how much precision the scene carries. Both
  // stood in the door as strings and neither was read, so a client asking for a LINEAR frame got
  // the filmic curve and a picture 177 code values away from the one it declared.
  std::string Transfer;
  std::string Precision;

  const Gltf::Subject *Built = nullptr;
  std::vector<Material> Surfacing{Material{}};

  // WHAT A CLIENT SAID THE FILE'S OWN SURFACES ARE, matched by the name the file states. Empty is
  // the ordinary case and means the file's materials stand as they were written.
  std::vector<SurfaceOverride> Overriding;

  std::string Variant;

  double MetresPerUnit = 1.0;

  double Fps = 60.0;

  double Fill = 0.0;

  double OrbitDegPerFrame = 0.0;

  double PictureLeftFrac = 0.0, PictureTopFrac = 0.0;
  double PictureWidthFrac = 0.0, PictureHeightFrac = 0.0;

  double IndirectLight[3] = {0.0, 0.0, 0.0};
  double KeyLux = 0.0;

  double Exposure = 0.0;

  bool DrawsSky = false;
  double ShadowRadiusM = 0.0;
  double KeyElevationDeg = 0.0, KeyBearingDeg = 0.0;

  std::vector<Shows> Surfaces;
};

class Live {
public:
  ~Live();
  Live(const Live &) = delete;
  Live &operator=(const Live &) = delete;

  [[nodiscard]] static bool Open(Render::SceneRenderer &renderer,
                                 Declaration declaration,
                                 const Ui::Font *font,
                                 std::unique_ptr<Live> &out,
                                 std::string &error);

  [[nodiscard]] bool Carries(size_t bodies, std::string &error);
  [[nodiscard]] bool Redeclare(std::vector<Shows> surfaces, std::string &error);
  [[nodiscard]] bool Restands(std::string stands,
                              std::string variant,
                              AssetAnimation animation,
                              int clip,
                              std::string &error);
  [[nodiscard]] const std::string &ProgrammeOf(size_t surface) const;

  [[nodiscard]] double BuildMs() const { return BuildMs_; }

  [[nodiscard]] double CarryMs() const { return CarryMs_; }

  [[nodiscard]] double ResolveMs() const { return ResolveMs_; }

  [[nodiscard]] double BoundsMs() const { return BoundsMs_; }

  [[nodiscard]] double InsideMs() const { return InsideMs_; }

  [[nodiscard]] double SurfaceMs() const { return SurfaceMs_; }

  [[nodiscard]] double StandMs() const { return StandMs_; }

  [[nodiscard]] double SubmitMs() const { return SubmitMs_; }

  [[nodiscard]] bool Restand(const Gltf::Subject &built, size_t carried, std::string &error);
  void Reshape();
  [[nodiscard]] bool
  Restand(outshine::Geometry &&built, size_t carried, const Material &wearing, std::string &error);
  [[nodiscard]] bool
  Restand(const Gltf::Subject &built, size_t carried, const Material &wearing, std::string &error);

  void ScaledBy(double metresPerUnit) { Declared_.MetresPerUnit = metresPerUnit; }

  [[nodiscard]] double ShadowRadiusStanding() const { return ShadowRadiusStoodM_; }

  [[nodiscard]] const double *ShadowCentreStanding() const { return Renderer_->ShadowStoodAtM(); }

  [[nodiscard]] bool
  Carry(size_t body, const double worldFromBodyM[16], const double built[16], std::string &error);
  [[nodiscard]] bool
  Carry(const double worldFromBodyM[16], const double built[16], std::string &error);

  [[nodiscard]] bool Present(std::string &error);
  [[nodiscard]] bool Settle(std::string &error);

  [[nodiscard]] bool Screenshot(const std::string &path, std::string &error);

  [[nodiscard]] bool ReadPixels(std::vector<uint8_t> &rgba, std::string &error);

  [[nodiscard]] bool
  ReadBuffer(outshine::Buffer which, std::vector<float> &out, std::string &error);

  [[nodiscard]] bool PlacedBounds(double least[3], double most[3], std::string &error);

  void SkyEye(double aboveGroundM);

  [[nodiscard]] bool Advance(std::string &error);
  [[nodiscard]] bool Draw(std::string &error);

  void Eye(const Render::Viewpoint &from);

  [[nodiscard]] const Render::Viewpoint &Aimed() const { return Looking_.Eye; }

  [[nodiscard]] const Render::Viewpoint &Watching() const { return Eye_; }

  [[nodiscard]] const Declaration &Standing() const { return Declared_; }

  void Grounding(const double albedo[3]) {
    for (int channel = 0; channel < 3; ++channel) { GroundAlbedo_[channel] = albedo[channel]; }
  }

  [[nodiscard]] const Render::SubjectEnvironment &AmbientStanding() const {
    return Stood_.IndirectLight();
  }

  [[nodiscard]] size_t PartsStanding() const { return Stood_.Parts(); }

  [[nodiscard]] size_t InstancesStanding() const { return Stood_.Instances(); }

  [[nodiscard]] double NearStanding() const { return (double)Renderer_->NearMetres(); }

  // THE NEAREST A DECLARED CAMERA MAY STAND. A framing near plane is derived from the scene's
  // radius, which is right for fitting one object in a frame and ruinous for a world: with a ring
  // reaching 388 km it came out at 1 904 878 m. Reverse-Z writes `near / distance`, so every
  // surface closer than 1 905 km clamps to the same depth and the depth test stops discriminating
  // -- which is why distant towers drew and the buildings beside the camera did not.
  [[nodiscard]] static double NearestStandable() { return (double)Render::SceneRenderer::kNearM; }

  [[nodiscard]] const double *PlacementStanding(size_t part) const {
    return Stood_.Placement(part).data();
  }

  [[nodiscard]] bool Watched() const { return HaveEye_; }

  void FrameItself() {
    HaveEye_ = false;
    Aimed_ = false;
  }

  [[nodiscard]] Ui::Touched Under(double xPx, double yPx, size_t &surface) const {
    return Over_.Under(xPx, yPx, surface);
  }

  [[nodiscard]] bool Wheeled(double xPx, double yPx, double byPx, std::string &error) {
    bool again = false;
    Over_.Wheeled(xPx, yPx, byPx, again);
    return !again || Compose(error);
  }

  [[nodiscard]] const std::vector<std::vector<Ui::Layout::Scrolled>> &Scrolled() const {
    return Over_.Scrolled();
  }

  [[nodiscard]] bool Scrolled(std::vector<std::vector<Ui::Layout::Scrolled>> kept,
                              std::string &error) {
    Over_.Scrolled(std::move(kept));
    return Compose(error);
  }

  [[nodiscard]] static size_t TookPosing() { return TookPosing_; }

  [[nodiscard]] static size_t TookSubmitting() { return TookSubmitting_; }

  [[nodiscard]] static size_t TookAiming() { return TookAiming_; }

  [[nodiscard]] static size_t TookDrawing() { return TookDrawing_; }

  [[nodiscard]] static size_t AssetReads() { return AssetReads_; }

  [[nodiscard]] static size_t PlanInits() { return PlanInits_; }

  [[nodiscard]] size_t PlanStages() const { return Plan_ ? Plan_->Order().size() : 0u; }

  [[nodiscard]] size_t PlanPasses() const { return Plan_ ? Plan_->Passes().size() : 0u; }

  [[nodiscard]] const Render::Shape &Shown() const { return Shaped_; }

  [[nodiscard]] size_t CarriedParts() const { return Joined_; }

  [[nodiscard]] bool Stands() const { return Stoodup_; }

  static constexpr int kSweepSamples = 16;

  [[nodiscard]] double AtS() const { return Held_.AtS(); }

  [[nodiscard]] int Frames() const { return Held_.Frames(); }

private:
  static size_t TookPosing_, TookSubmitting_, TookAiming_, TookDrawing_;
  static size_t AssetReads_;
  static size_t PlanInits_;
  Render::PlanSpec PlanDeclared_;

  Live(Render::SceneRenderer &renderer, Declaration declaration, const Ui::Font *font);
  [[nodiscard]] bool Build(std::string &error);
  [[nodiscard]] double Framing() const;
  [[nodiscard]] bool Pose(double seconds, std::string &error);
  [[nodiscard]] bool Measure(double seconds, std::string &error);

  // WHERE A SWEEP SAMPLES, and it is the animation's OWN span rather than a frame grid. A framing
  // rule wants the EXTENT a subject reaches over its motion; tying that to a declared frame rate
  // made it depend on a number the framing has nothing to do with, and a scenario that declared
  // none divided by zero and bounded the rest pose against the end pose alone.
  [[nodiscard]] int Sweeps() const {
    const int frames = Held_.Frames();
    return frames < 1 ? 1 : (frames < kSweepSamples ? frames : kSweepSamples);
  }

  [[nodiscard]] double Seconds(int sample) const {
    const int over = Sweeps();
    return over > 1 ? (double)sample * Held_.DurationS() / (double)(over - 1) : 0.0;
  }

  [[nodiscard]] bool Look(std::string &error);
  [[nodiscard]] bool Stand(std::string &error);
  [[nodiscard]] bool Submit(std::string &error);

  [[nodiscard]] bool Compose(std::string &error) {
    return Over_.Compose(*Renderer_,
                         Declared_.Surfaces,
                         (double)Declared_.SurfaceWidthPx,
                         (double)Declared_.SurfaceHeightPx,
                         error);
  }

  Render::SceneRenderer *Renderer_ = nullptr;
  Overlay Over_;
  Declaration Declared_;
  double GroundAlbedo_[3] = {0.10, 0.13, 0.07};
  double ShadowRadiusStoodM_ = 0.0;
  std::shared_ptr<const Render::Compiled> Plan_;
  Render::Viewpoint Eye_;
  bool HaveEye_ = false;
  bool Aimed_ = true;
  std::vector<std::array<double, 16>> SentBody_;
  std::array<double, 16> SentBuilt_;

  struct Volume {
    bool Empty = true;
    double LeastM[3] = {0.0, 0.0, 0.0};
    double MostM[3] = {0.0, 0.0, 0.0};
  };

  std::vector<Volume> PartBounds_;
  [[nodiscard]] bool PartVolumes(std::string &error);
  Render::SurfaceTable Table_;
  Posed Held_;
  Render::SubjectProxy Stood_;
  Render::Eye Looking_;
  Render::SubjectScratch Scratch_;

  Render::ShapeStore ShapeParts_;
  Render::Shape Shaped_;
  double BuildMs_ = 0.0, StandMs_ = 0.0, SubmitMs_ = 0.0;
  double CarryMs_ = 0.0, ResolveMs_ = 0.0, BoundsMs_ = 0.0, InsideMs_ = 0.0, SurfaceMs_ = 0.0;

  bool Stoodup_ = false;
  size_t Joined_ = 0;
  size_t Carrying_ = 0;

  double Around_ = 0.0;
};

} // namespace outshine::Core
#endif
