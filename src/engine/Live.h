#ifndef OUTSHINE_ENGINE_LIVE_H
#define OUTSHINE_ENGINE_LIVE_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

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
#include "Renderer.h"
#include "Style.h"
#include "Material.h"
#include "Subject.h"
#include "Surfaces.h"

namespace outshine::Core {

struct Declaration {
  AssetAnimation Animation = AssetAnimation::Play;

  int SurfaceWidthPx = 0, SurfaceHeightPx = 0;

  std::string Stands;

  const Gltf::Subject *Built = nullptr;
  std::vector<Material> Surfacing{Material{}};

  std::string Variant;

  double MetresPerUnit = 1.0;

  double Fps = 60.0;

  double Fill = 0.0;

  double OrbitDegPerFrame = 0.0;

  double PictureLeftFrac = 0.0, PictureTopFrac = 0.0;
  double PictureWidthFrac = 0.0, PictureHeightFrac = 0.0;

  double Environment[3] = {0.0, 0.0, 0.0};
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

  [[nodiscard]] static bool Open(Render::Renderer &renderer, Declaration declaration,
                                 const Ui::Font *font, std::unique_ptr<Live> &out,
                                 std::string &error);

  [[nodiscard]] bool Redeclare(std::vector<Shows> surfaces, std::string &error);
  [[nodiscard]] bool Restands(std::string stands, std::string variant, AssetAnimation animation,
                             std::string &error);
  [[nodiscard]] const std::string &ProgrammeOf(size_t surface) const;

  [[nodiscard]] bool Restand(const Gltf::Subject &built, size_t carried, std::string &error);
  [[nodiscard]] bool Restand(const Gltf::Subject &built, size_t carried, const Material &wearing,
                             std::string &error);

  void ScaledBy(double metresPerUnit) { Declared_.MetresPerUnit = metresPerUnit; }
  [[nodiscard]] double ShadowRadiusStanding() const { return ShadowRadiusStoodM_; }
  [[nodiscard]] const double *ShadowCentreStanding() const { return Renderer_->ShadowStoodAtM(); }
  [[nodiscard]] bool Carry(const double worldFromBodyM[16], const double built[16],
                           std::string &error);

  [[nodiscard]] bool Screenshot(const std::string &path, std::string &error);

  [[nodiscard]] bool ReadPixels(std::vector<uint8_t> &rgba, std::string &error);

  [[nodiscard]] bool PlacedBounds(double least[3], double most[3], std::string &error);

  void SkyEye(double aboveGroundM);

  [[nodiscard]] bool Advance(std::string &error);
  [[nodiscard]] bool Draw(std::string &error);

  void Eye(const Gltf::Placement &from);

  [[nodiscard]] const Gltf::Placement &Aimed() const { return Looking_.Eye; }

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

  static size_t TookPosing_, TookSubmitting_, TookAiming_, TookDrawing_;
  static size_t AssetReads_;
  static size_t PlanInits_;
  Render::PlanSpec PlanDeclared_;
  [[nodiscard]] static size_t TookPosing() { return TookPosing_; }
  [[nodiscard]] static size_t TookSubmitting() { return TookSubmitting_; }
  [[nodiscard]] static size_t TookAiming() { return TookAiming_; }
  [[nodiscard]] static size_t TookDrawing() { return TookDrawing_; }
  [[nodiscard]] static size_t AssetReads() { return AssetReads_; }
  [[nodiscard]] static size_t PlanInits() { return PlanInits_; }

  [[nodiscard]] const Gltf::Subject &Shown() const { return Held_.Geometry(); }
  [[nodiscard]] size_t CarriedParts() const { return Joined_; }
  [[nodiscard]] bool Stands() const { return Stoodup_; }

  [[nodiscard]] int At() const { return Held_.At(); }
  [[nodiscard]] int Frames() const { return Held_.Frames(); }

private:
  Live(Render::Renderer &renderer, Declaration declaration, const Ui::Font *font);
  [[nodiscard]] bool Build(std::string &error);
  [[nodiscard]] double Framing() const;
  [[nodiscard]] bool Pose(int frame, std::string &error);
  [[nodiscard]] bool Look(std::string &error);
  [[nodiscard]] bool Stand(std::string &error);
  [[nodiscard]] bool Submit(std::string &error);
  [[nodiscard]] bool Compose(std::string &error) {
    return Over_.Compose(*Renderer_, Declared_.Surfaces, (double)Declared_.SurfaceWidthPx,
                         (double)Declared_.SurfaceHeightPx, error);
  }

  Render::Renderer *Renderer_ = nullptr;
  Overlay Over_;
  Declaration Declared_;
  double ShadowRadiusStoodM_ = 0.0;
  std::shared_ptr<const Render::RenderPlan> Plan_;
  Gltf::Placement Eye_;
  bool HaveEye_ = false;
  bool Aimed_ = true;
  std::array<double, 16> SentBody_;
  std::array<double, 16> SentBuilt_;
  struct Volume {
    bool Empty = true;
    double LeastM[3] = {0.0, 0.0, 0.0};
    double MostM[3] = {0.0, 0.0, 0.0};
  };
  std::vector<Volume> PartBounds_;
  [[nodiscard]] bool PartVolumes(std::string &error);
  SurfaceTable Table_;
  Asset Held_;
  Render::SubjectProxy Stood_;
  Render::Eye Looking_;
  Render::SubjectScratch Scratch_;

  bool Stoodup_ = false;
  size_t Joined_ = 0;
  size_t Carrying_ = 0;

  double Around_ = 0.0;
};

}
#endif
