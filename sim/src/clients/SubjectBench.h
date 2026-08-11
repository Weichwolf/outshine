/* THE SUBJECT BENCH: one plant, alone, on
 * its own declared substrate beside a neutral card, in declared light. It is a MODE of the frame
 * oracle and not a client — same binary, same Renderer, same stages. What it replaces is the WORLD
 * (no tiles, no OSM, no DEM) and the LIGHT (declared, never inherited); what it may never replace is
 * anything that draws the subject.
 *
 * The image list is a FULL VIEW x LIGHT matrix — every view under every light, no exceptions — because
 * the two critics that asked for it asked along two orthogonal axes: the framings are the botanist's,
 * the lights are the art-director's, and a hole in the matrix is a question neither can answer. The
 * matrix is laid out as a LIST of orders at Begin(), so that the run through it is one index and the
 * declaration stays a table.
 *
 * EVERY SHOT CARRIES ITS OWN FILL, measured off the depth buffer against the same frame with the
 * studio furniture retired. It is the one number that tells an empty view from a black one, and both
 * critics lost a whole view to not having it.
 *
 * IT ADVANCES BY TURNS, for the reason SceneRunner.h states: the four readbacks a shot needs
 * complete on the thread a wait would be standing on. */
#ifndef SUBJECTBENCH_H
#define SUBJECTBENCH_H

#include <cstdint>
#include <string>
#include <vector>

#include "Artifacts.h"
#include "Renderer.h"
#include "VegetationTemplates.h"

namespace outshine::Clients {

class SubjectBench {
public:
  /* THE SINK IS THE RUN'S, not the process's working directory (Artifacts.h): a bench that wrote
   * with stbi_write_png delivered nothing in the browser at all and failed natively for every
   * declared directory that did not already exist in the tree. */
  SubjectBench(Render::Renderer &renderer, const World::VegetationTemplates &veg, Artifacts &out)
      : R_(renderer), Out_(out), Veg_(veg) {}

  /* `heightOverrideM` <= 0 takes the template's own declared plant height, which is what makes this a
   * VEGETATION bench: nothing below names a species or a layer. */
  bool Select(const char *templateName, double heightOverrideM);
  /* A TREE SUBJECT. It is a second Select and not a flag on the first because it changes what the
   * bench frames: the herb views are a square metre of sward and a 0.4 m tuft, and neither has a
   * meaning at 30 m. `heightM` is the species' own declared height. */
  bool SelectTree(const char *speciesName, double heightM);
  void Stand(double latDeg, double lonDeg, double groundAslM) {
    Lat_ = latDeg; Lon_ = lonDeg; AslM_ = groundAslM;
  }
  void SetOutDir(const std::string &dir) { OutDir_ = dir; }
  void SetTurntableSteps(int n) { TurnSteps_ = n > 0 ? n : 1; }
  /* THE SUBSTRATE IS THE TEMPLATE'S OWN, not the bench's taste: `name` and the linear reflectance come
   * from the ground-material class the vegetation template references. Undeclared leaves the floor at
   * the 18 % neutral, which is what a subject without a declared substrate deserves. */
  void SetSubstrate(const std::string &name, const float linearRgb[3]) {
    Substrate_ = name;
    for (int i = 0; i < 3; i++) SubstrateRgb_[i] = linearRgb[i];
  }

  /* THE declared lens. 30 deg vertical is a 45 mm normal lens on 35 mm format (f = 12/tan 15 deg):
   * anything wider puts perspective distortion into a judgement about form. */
  static constexpr double kFovDeg = 30.0;
  /* THE declared sun elevation, both critics' — a low raking light is where a silhouette and a
   * transmission term are decided. */
  static constexpr double kSunElDeg = 11.0;

  enum class Progress { Running, Done, Failed };
  Progress Step();

private:
  /* A framing, declared the way a critic states one: how wide the picture is AT THE SUBJECT, where the
   * eye is, where it points. Lengths are multiples of the subject's height unless they carry an
   * external unit — 1 m^2 is what coverage is defined in, 1.70 m is a human. */
  struct View {
    const char *Name;
    double FrameHPerH;   /* frame HEIGHT at the target plane, in subject heights (0 = unused) */
    double WidthPerH;    /* frame WIDTH at the target plane, in subject heights (0 = unused) */
    double WidthM;       /* absolute frame WIDTH; for a Square view this is the CROP width */
    double DistPerH;     /* declared camera distance in subject heights (0 = derive from the width) */
    double PitchDeg;
    double EyeAglM;      /* >= 0 declares the eye and derives the target; < 0 is the other way round */
    double TargetPerH;   /* target height / subject height; < 0 = half the frame height */
    double AzOffDeg;
    bool   Square;       /* write only the centred H x H crop, so the picture is a known area */
    bool   Turntable;
    bool   Wind;         /* the 0/6/12 m/s row: the framings on which a lean is actually readable */
  };

  /* ONE PICTURE THE MATRIX ASKED FOR. `windMs` is the DECLARED met wind at 10 m for this shot. The
   * named lights are all shot at zero: a still-life judges form, and the plant's form is what the
   * botanist pinned. The wind row is the one place it is non-zero, which is also what keeps those 45
   * files comparable across rounds. */
  struct Order {
    const View *Frame;
    std::string Light;
    double CamAzDeg, SunAzDeg, SunElDeg, CloudCover, WindMs;
    bool Metering;
  };

  /* WHAT THE PICTURE ACTUALLY HOLDS, taken off the DEPTH buffer and never off a hue: a colour
   * difference counts the shadow a subject throws on bare floor beside it as if it were the subject
   * (measured: 64.7 % against 13.0 % of actual cover). Three renders of one camera separate the three
   * things a bench frame can contain — the full frame, the frame with the card retired, and the frame
   * with the floor retired as well (which takes the card with it) — so a pixel is the card where the
   * first two disagree and the subject where the third still has depth. `Pct` is a share of the OPEN picture, the card's own area removed. */
  struct Fill {
    double Pct = -1.0, CardPct = -1.0;
    int Median = -1, CardMedian = -1;
  };

  /* WHAT THE PLACEMENT DERIVED, carried from the turn that placed the camera to the turn that writes
   * the file. Nothing here is declared; every field follows from the View and the subject's height. */
  struct Framing {
    double Aspect = 0.0, Fov = 0.0, HalfV = 0.0;
    double FrameW = 0.0, FrameH = 0.0, Dist = 0.0, Pitch = 0.0;
    double EyeAgl = 0.0, TargetH = 0.0, FloorR = 0.0, GridM = 0.0, MPerPx = 0.0;
    int OutW = 0, OutH = 0, Ox = 0, Oy = 0;
    Render::BenchCard Card;
  };

  bool Begin();
  void Place(const Order &o);
  Progress Meter(const Order &o);
  Progress Write(const Order &o);
  Fill MeasureFill();

  enum class Subject { None, Herb, Tree };
  /* THE TURNS ONE PICTURE TAKES. Each name is the answer being waited for. */
  enum class Take { Place, Meter, MeterIrradiance, Pixels, DepthAll, DepthNoCard, DepthSubject,
                    Write };

  Render::Renderer &R_;
  Artifacts &Out_;
  const World::VegetationTemplates &Veg_;

  Subject Kind_ = Subject::None;
  int Bucket_ = -1;
  double HeightM_ = 0.0;
  std::string Template_, OutDir_ = "bench", Substrate_ = "neutral";
  float SubstrateRgb_[3] = {Render::BenchGroundStage::kCardAlbedo,
                            Render::BenchGroundStage::kCardAlbedo,
                            Render::BenchGroundStage::kCardAlbedo};
  double Lat_ = 0.0, Lon_ = 0.0, AslM_ = 0.0;
  int TurnSteps_ = 8;
  float KeyEv_ = 0.0f;
  bool Metered_ = false;

  std::vector<Order> Orders_;
  size_t OrderIx_ = 0;
  int Written_ = 0;
  bool Begun_ = false;
  Take Take_ = Take::Place;
  Framing Shot_;
  float Meter_[Render::ExposureStage::kMeterFloats] = {};

  std::vector<float> DepthAll_, DepthNoCard_, DepthSubject_;
  std::vector<uint8_t> Rgba_, Img_;
};

} // namespace outshine::Clients
#endif
