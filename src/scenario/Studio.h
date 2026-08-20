#ifndef STUDIO_H
#define STUDIO_H

#include <string>
#include <variant>

namespace outshine::Scenario {

constexpr double kAnchorLatDeg = 0.0, kAnchorLonDeg = 0.0;

struct Substrate {
  std::string MaterialClass;
  double GroundAslM = 0.0;
};

struct KeyLight {
  double ElevationDeg = 0.0;
};

enum class Backdrop { None, Card };

enum class Foliage { Bare, Leaves };

struct TreeSubject {
  std::string Species;
  double HeightM = 0.0;
  int LeafMult = 1;
  Foliage Leaf = Foliage::Leaves;
};

struct SwardSubject {
  std::string Template;
  double HeightM = 0.0;
};

using SubjectParams = std::variant<TreeSubject, SwardSubject>;

struct Subject {
  SubjectParams What;
};

struct StudioStage {
  Substrate Ground;
  KeyLight Key;
  Backdrop Behind = Backdrop::Card;
  Subject Stands;
};

}
#endif
