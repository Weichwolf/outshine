/* A STAGE WITH NO WORLD: one subject at the origin, on a declared substrate, under a declared key
 * light. It is what lets a generator be judged with no tile, no network and no place — and it is not
 * a second kind of scene but the other arm of the same one (Stage.h), so choosing it REMOVES the
 * observer's world fields rather than adding a dimension.
 *
 * Four things are declared here because the bench had to invent all four in C++: substrate, key
 * light, backdrop, subject. Nothing else is, and that is deliberate — a field this engine does not
 * yet read is the defect this whole shape was written against. */
#ifndef STUDIO_H
#define STUDIO_H

#include <string>
#include <variant>

namespace outshine::Scenario {

/* WHERE A STUDIO SITS ON THE GLOBE, and the answer is that the question is not the scenario's. The
 * renderer's frame is ECEF, so a camera basis has to be built at SOME geodetic point. The null
 * island is the one choice that is a datum rather than a preference.
 *
 * MEASURED, AND IT DOES NOT COME OUT FREE: moving this anchor moves every bench picture
 * (the bug board). The studio's floor ruler is drawn in world metres and its phase follows the anchor
 * by design; the horizon and the crown moving with it are not explained and are recorded there. */
constexpr double kAnchorLatDeg = 0.0, kAnchorLonDeg = 0.0;

/* WHAT THE SUBJECT STANDS ON. A ground-material class by name, whose declared albedo and roughness
 * are the floor's; absent leaves the bench's 18 % neutral, which is what a subject that declares no
 * substrate deserves. `GroundAslM` is the air column above the floor and nothing else: the
 * atmosphere is evaluated at an altitude, and a studio at sea level is not a studio in the Alps. */
struct Substrate {
  std::string MaterialClass;
  double GroundAslM = 0.0;
};

/* THE KEY LIGHT, declared rather than computed from an ephemeris, because a still life judges form
 * and a civil time there is a number nobody chose. Its ELEVATION is declared and its BEARING is not:
 * a bench frames every view under front, back and sky light, so the bearing is an axis of the
 * measurement and belongs to the run, while the elevation is one number for the whole studio. */
struct KeyLight {
  double ElevationDeg = 0.0;
};

/* WHAT STANDS BEHIND AND BESIDE. `Card` is the neutral reference card at the frame edge — the only
 * place a reference can be without becoming the subject's background. */
enum class Backdrop { None, Card };

enum class Foliage { Bare, Leaves };

/* ONE GENERATOR INVOCATION. Each generator reads its own parameter object rather than a shared field
 * soup (`I.23`), so a parameter that belongs to another generator has no place to be written. */
struct TreeSubject {
  std::string Species;
  double HeightM = 0.0;   /* 0 takes the species' own declared height */
  int LeafMult = 1;
  Foliage Leaf = Foliage::Leaves;
};

struct SwardSubject {
  std::string Template;
  double HeightM = 0.0;   /* 0 takes the template's own declared plant height */
};

using SubjectParams = std::variant<TreeSubject, SwardSubject>;

/* EXACTLY ONE SUBJECT, AT THE ORIGIN. One, because a bench that frames two things is measuring
 * composition rather than form — so there is no list here to hold a second. */
struct Subject {
  SubjectParams What;
};

struct StudioStage {
  Substrate Ground;
  KeyLight Key;
  Backdrop Behind = Backdrop::Card;
  Subject Stands;
};

} // namespace outshine::Scenario
#endif
