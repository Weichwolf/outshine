/* The stand: ONE species grows, and it stands a thousand times. Grows the rank ladder of meshes,
 * measures the crown and scatters the stands over the class field — the world logic every client
 * gets, because none of them writes it itself.
 *
 * NOTHING HERE DRAWS. Growing yields a prototype and scattering yields stands; whoever wants a
 * picture out of them collects them. */
#ifndef FOREST_H
#define FOREST_H

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "TreeField.h"
#include "Capacity.h"
#include "TreeLook.h"

namespace outshine::World {

class ClassField;
class TreeSpecies;
class VegetationTemplates;

class Forest {
public:
  /* THE GROWN TREE, in model space and metres, one entry per rank of the mesh ladder. It is a
   * yield and not a member: the collector uploads it once and the world keeps nothing of it. */
  struct Prototype {
    struct Rank {
      std::vector<float> BarkVerts;      /* core/ChunkVtx.h layout */
      uint32_t BarkVertCount = 0;
      std::vector<uint32_t> BarkIdx;
      std::vector<float> Cards;          /* TreeFoliage::kFloats per card */
      uint32_t CardCount = 0;
      float CardLeafM = 0.0f;            /* the leaf a card draws, edge length in metres */
    };
    std::vector<Rank> Ranks;
    TreeLook Look;
    double HeightM = 0.0;
    /* [SET] The fan a shoot's leaves stand in about their stalk, full angle. A card is one shoot
     * tip, and a rosette of four laminae over less than a right angle stacks into a line from every
     * direction but one. */
    float CardFanDeg = 110.0f;
    TreeField::Crown Crown;
  };

  /* Grows the declared species into the rank ladder and measures its crown. Empty = the declaration
   * could not be read. */
  std::optional<Prototype> Grow(const char *speciesPath);
  bool Grown() const { return Grown_; }

  /* Once, when the tiles stand: the scattering asks the terrain height per stand, and 166 823 queries
   * every 32 passes are a hundred million over a warm-up — measured as a hang. */
  void Scatter(const ClassField &cls, const VegetationTemplates &veg, double camLat, double camLon,
               double eyeAglM);
  bool Scattered() const { return Scattered_; }

  const TreeField::Crown &Crown() const { return Crown_; }
  long StandCount() const { return (long)(Stands_.size() / (size_t)TreeField::kStandFloats); }
  /* TreeField::kStandFloats per stand, and one distance in metres per stand beside it. */
  const std::vector<float> &Stands() const { return Stands_; }
  const std::vector<float> &StandDistM() const { return Dist_; }
  const TreeField &Field() const { return Field_; }

  /* The stands scattered once at bring-up, so a rise here after the first frame is a defect and not
   * a load. The prototype is not counted: it left with Grow(). */
  size_t HeapBytes() const { return CapacityBytes(Stands_) + CapacityBytes(Dist_); }

  static bool LoadSpecies(const char *path, TreeSpecies *out);
  /* A declared colour is sRGB and a reflectance is linear; the leaf tint multiplies the table's own
   * fresh-blade green. Both conversions are world knowledge, so render/ never sees a declaration. */
  static TreeLook LookOf(const TreeSpecies &sp);

  /* 900 m: beyond it a 25 m tree is under two pixels wide at 720p and belongs to the distance the
   * ground material carries. */
  static constexpr double kScatterRadiusM = 900.0;

private:
  TreeField Field_;
  TreeField::Crown Crown_;
  std::vector<float> Stands_, Dist_;
  float HeightSigma_ = 0.0f;
  bool Grown_ = false;
  bool Scattered_ = false;
};

} // namespace outshine::World
#endif
