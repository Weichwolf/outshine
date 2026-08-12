/* THE STUDIO STAGE, RECORDED: the one subject a scenario declares, alone on its declared substrate
 * beside a neutral card, in the declared key light (scenario/Studio.h). It is a layer OVER the same
 * binary, the same Renderer and the same stages — never a mode inside one of them. What the studio
 * replaces is the WORLD (no tiles, no OSM, no DEM, no wire) and the LIGHT (declared, never computed
 * from an ephemeris); what it may never replace is anything that draws the subject.
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
#include "Stage.h"

namespace outshine::Clients {

class SubjectBench {
public:
  /* WHAT THE STAGE AND THE RUN SETTLED, in one parameter object rather than seven setters (`I.23`):
   * a bench that could be half-configured had a state in which it would photograph a subject of
   * height zero. Everything here is resolved — the studio's own declarations plus the two numbers
   * the runner had to look up, the subject's height and its substrate's reflectance.
   *
   * PRECONDITION (`I.5`): `HeightM > 0`. A subject whose declaration names no height is refused by
   * the runner that resolved it, where the name that failed is still known. */
  struct Setup {
    /* WHICH FAMILY OF FRAMINGS. It follows from the subject and is not a taste: the herb views are a
     * square metre of sward and a 0.4 m tuft, and neither has a meaning at 30 m. */
    enum class Habit { Herb, Tree };

    Habit Frames = Habit::Tree;
    double HeightM = 0.0;
    /* The air column over the floor, metres above sea level, so the atmosphere over the bench is the
     * atmosphere the studio declared and not one nobody chose. */
    double GroundAslM = 0.0;
    /* The key light's elevation, degrees. A low raking light is where a silhouette and a
     * transmission term are decided; its BEARING is an axis of the matrix below, not a declaration. */
    double KeyElDeg = 0.0;
    /* The declared lens, degrees vertical. 30 is a 45 mm normal lens on 35 mm format
     * (f = 12/tan 15 deg): anything wider puts perspective distortion into a judgement about form. */
    double FovDeg = 30.0;
    Scenario::Backdrop Behind = Scenario::Backdrop::Card;
    /* What the files are named after, and the ground-material class the floor takes its linear
     * reflectance from. An undeclared substrate leaves the floor at the 18 % neutral. */
    std::string Name, SubstrateName = "neutral", Dir = "bench";
    float SubstrateRgb[3] = {Render::BenchGroundStage::kCardAlbedo,
                             Render::BenchGroundStage::kCardAlbedo,
                             Render::BenchGroundStage::kCardAlbedo};
    int TurnSteps = 8;
  };

  /* THE SINK IS THE RUN'S, not the process's working directory (Artifacts.h): a bench that wrote
   * with a bare file write delivered nothing in the browser at all and failed natively for every
   * declared directory that did not already exist in the tree. */
  SubjectBench(Render::Renderer &renderer, Artifacts &out, Setup setup)
      : R_(renderer), Out_(out), Setup_(std::move(setup)) {}

  enum class Progress { Running, Done, Failed };
  [[nodiscard]] Progress Step();

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

  [[nodiscard]] bool Begin();
  void Place(const Order &o);
  [[nodiscard]] Progress Meter(const Order &o);
  [[nodiscard]] Progress Write(const Order &o);
  Fill MeasureFill();

  /* THE TURNS ONE PICTURE TAKES. Each name is the answer being waited for. */
  enum class Take { Place, Meter, MeterIrradiance, Pixels, DepthAll, DepthNoCard, DepthSubject,
                    Write };

  Render::Renderer &R_;
  Artifacts &Out_;
  const Setup Setup_;

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
