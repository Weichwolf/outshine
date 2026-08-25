#ifndef OUTSHINE_CLIENTS_LIVE_H
#define OUTSHINE_CLIENTS_LIVE_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <Scenario.h>

#include "Document.h"
#include "GltfStudio.h"
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

namespace outshine::Clients {

struct Shows {
  std::string Markup;

  std::string Style;
  std::string Programme;
  double LeftFrac = 0.0, TopFrac = 0.0, WidthFrac = 1.0, HeightFrac = 1.0;
};

struct Declaration {
  AssetAnimation Animation = AssetAnimation::Play;

  int SurfaceWidthPx = 0, SurfaceHeightPx = 0;

  std::string Stands;

  const Gltf::Subject *Built = nullptr;
  std::vector<Material> Surfacing{Material{}};

  std::string Variant;

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
  [[nodiscard]] bool Touched(double xPx, double yPx, size_t &surface, std::string &action) const;
  [[nodiscard]] const std::string &ProgrammeOf(size_t surface) const;

  [[nodiscard]] bool Restand(const Gltf::Subject &built, std::string &error);

  [[nodiscard]] bool Carry(const double body[16], const double built[16],
                           std::string &error);

  [[nodiscard]] bool Screenshot(const std::string &path, std::string &error);

  [[nodiscard]] bool ReadPixels(std::vector<uint8_t> &rgba, std::string &error);

  [[nodiscard]] bool PlacedBounds(double least[3], double most[3], std::string &error);

  void SkyEye(double aboveGroundM);

  [[nodiscard]] bool Advance(std::string &error);

  void Eye(const Gltf::Placement &from);

  [[nodiscard]] const Gltf::Placement &Aimed() const { return Stood_.Eye; }

  void FrameItself() {
    HaveEye_ = false;
    Aimed_ = false;
  }

  [[nodiscard]] Ui::Touched Under(double xPx, double yPx) const;

  static size_t TookPosing_, TookSubmitting_, TookAiming_, TookDrawing_;
  static size_t AssetReads_;
  [[nodiscard]] static size_t TookPosing() { return TookPosing_; }
  [[nodiscard]] static size_t TookSubmitting() { return TookSubmitting_; }
  [[nodiscard]] static size_t TookAiming() { return TookAiming_; }
  [[nodiscard]] static size_t TookDrawing() { return TookDrawing_; }
  [[nodiscard]] static size_t AssetReads() { return AssetReads_; }

  [[nodiscard]] const Gltf::Subject &Shown() const { return Geometry_; }
  [[nodiscard]] size_t CarriedParts() const { return Joined_; }

  [[nodiscard]] int At() const { return At_; }
  [[nodiscard]] int Frames() const { return Frames_; }

private:
  Live(Render::Renderer &renderer, Declaration declaration, const Ui::Font *font);
  [[nodiscard]] bool Build(std::string &error);
  [[nodiscard]] double Framing() const;
  [[nodiscard]] bool Pose(int frame, std::string &error);
  [[nodiscard]] bool Look(std::string &error);
  [[nodiscard]] bool Stand(std::string &error);
  [[nodiscard]] bool Submit(std::string &error);
  [[nodiscard]] bool Compose(std::string &error);

  struct Laid {
    Ui::Markup Tree;
    Ui::Stylesheet Sheet;
    Ui::Layout Placed;
    Ui::Painting Painted;
    double LeftPx = 0.0, TopPx = 0.0;
  };

  Render::Renderer *Renderer_ = nullptr;
  const Ui::Font *Font_ = nullptr;
  uint64_t Cut_ = 0;
  Declaration Declared_;
  std::shared_ptr<const Render::RenderPlan> Plan_;
  Gltf::Document File_;
  Gltf::Subject Geometry_;
  std::vector<double> PreviousPositionsM_;
  Gltf::Placement Eye_;
  bool HaveEye_ = false;
  bool Aimed_ = true;
  bool BoundsPlaced_ = false;
  double PlacedLeast_[3] = {0, 0, 0}, PlacedMost_[3] = {0, 0, 0};
  float SkyToSun_[3] = {0, 0, 0};
  float SkyUp_[3] = {0, 0, 0};
  bool SkyStands_ = false;
  Gltf::Pose Motion_;
  Gltf::VariantSelection Variant_;
  std::vector<Gltf::Transform> Locals_;
  std::vector<double> Weights_;
  SurfaceTable Table_;
  Studio Stood_;
  StudioScratch Scratch_;
  std::vector<Laid> Laid_;
  std::vector<Render::OverlayQuad> Quads_;
  bool Moves_ = false;
  bool FileStands_ = false;

  bool Stoodup_ = false;
  size_t Joined_ = 0;
  int Frames_ = 1;
  int At_ = 0;

  double Around_ = 0.0;
};

}
#endif
