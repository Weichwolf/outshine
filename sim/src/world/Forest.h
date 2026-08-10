/* Der Bestand: EINE Art waechst, und sie steht tausendfach. Waechst die Rangleiter der Meshes, backt
 * den Impostor und streut die Staende ueber das Klassenfeld — die Weltlogik, die jeder Client
 * bekommt, weil keiner sie selbst schreibt. */
#ifndef FOREST_H
#define FOREST_H

#include <string>
#include <vector>

#include "TreeField.h"
#include "Capacity.h"
#include "TreeLook.h"
#include "TreeMesh.h"

namespace outshine::Render { class Renderer; }

namespace outshine::World {

class ClassField;
class TreeSpecies;
class VegetationTemplates;

class Forest {
public:
  /* Grows the declared species into the renderer's rank ladder and bakes its impostor. Needs a live
   * device: TreeStage::Upload returns nothing without one and the stand is silently dropped. */
  bool Grow(Render::Renderer &r, const char *speciesPath);
  bool Grown() const { return !Mesh_.BarkIdx.empty(); }

  /* Once, when the tiles stand: the scattering asks the terrain height per stand, and 166 823 queries
   * every 32 passes are a hundred million over a warm-up — measured as a hang. */
  void Scatter(Render::Renderer &r, const ClassField &cls, const VegetationTemplates &veg,
               double camLat, double camLon, double eyeAglM);
  bool Scattered() const { return Scattered_; }

  const TreeField::Crown &Crown() const { return Crown_; }
  long StandCount() const { return (long)(Stands_.size() / (size_t)TreeField::kStandFloats); }
  const TreeField &Field() const { return Field_; }

  /* The grown prototype and the stands scattered from it. Grown once at bring-up and never again, so
   * a rise here after the first frame is a defect and not a load. */
  size_t HeapBytes() const {
    return Mesh_.Bytes() + CapacityBytes(Stands_) + CapacityBytes(Dist_);
  }

  static bool LoadSpecies(const char *path, TreeSpecies *out);
  /* A declared colour is sRGB and a reflectance is linear; the leaf tint multiplies the table's own
   * fresh-blade green. Both conversions are world knowledge, so render/ never sees a declaration. */
  static TreeLook LookOf(const TreeSpecies &sp);

  /* [SET] The fan a shoot's leaves stand in about their stalk, full angle. A card is one shoot tip,
   * and a rosette of four laminae over less than a right angle stacks into a line from every
   * direction but one. */
  static constexpr float kCardFanDeg = 110.0f;
  /* 900 m: beyond it a 25 m tree is under two pixels wide at 720p and belongs to the distance the
   * ground material carries. */
  static constexpr double kScatterRadiusM = 900.0;

private:
  TreeMesh Mesh_;
  TreeField Field_;
  TreeField::Crown Crown_;
  std::vector<float> Stands_, Dist_;
  float HeightSigma_ = 0.0f;
  bool Scattered_ = false;
};

} // namespace outshine::World
#endif
