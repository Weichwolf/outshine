/* WHERE COLLECTION HAPPENS: the instances a generator produced for its own region, gathered into
 * the one frame the renderer draws them in. Everything view-dependent lives HERE and nowhere in
 * placement — how far the picture reaches, and the stand whose crown holds the lens.
 *
 * NO LENS SITS INSIDE A CROWN. A pedestrian at 1.7 m is below any crown base and loses nothing; a
 * camera on a 14 m mast stands in the middle of one, and in reality there is no tree there. That is
 * a statement about the eye, so it cannot be made where the trees are placed: the same stand exists
 * or does not exist depending on where one stands, and placement must not know. */
#ifndef STANDFIELD_H
#define STANDFIELD_H

#include <vector>

#include "DrawSink.h"
#include "Region.h"
#include "TangentFrame.h"
#include "TreePrototype.h"

namespace outshine::Clients {

class StandField : public Generators::DrawSink {
public:
  struct Lens {
    TangentFrame Frame;      /* the collected instances' own metres, anchored at the eye's place */
    double AslM = 0.0;
    double ReachM = 0.0;
  };

  /* THE CEILING, taken once at bring-up and never grown: the collection is the one buffer on this
   * path that a fixed heap has to be told the size of, and a `Full()` that can never be true makes
   * `Add`'s refusal a dead branch that looks like handling. */
  void Reserve(uint32_t stands);
  /* Clears what was collected: a collection is a whole answer, never an append to an older one. */
  void Aim(const Lens &lens, const Generators::TreePrototype::Crown &crown);
  /* The region the instances that follow are measured in. */
  void From(const Generators::Region &region) { Region_ = region; }

  [[nodiscard]] bool Add(Generators::BodyId body, Generators::ClusterId cluster,
                         const Generators::Instance &instance) noexcept override;
  [[nodiscard]] bool Full() const noexcept override { return Stands_.size() >= (size_t)Cap_ * kFloats; }

  /* Render::ModelDraw's instance layout: east, north, foot over the frame's anchor, yaw, and
   * the factor on the model's own height. */
  const std::vector<float> &Stands() const { return Stands_; }
  uint32_t Count() const { return (uint32_t)(Stands_.size() / kFloats); }
  uint32_t BeyondReach() const { return Beyond_; }
  uint32_t InCrown() const { return InCrown_; }
  /* Instances the ceiling refused, this collection. Non-zero means the picture is missing stands
   * that the world placed, which is a budget statement and belongs in a line. */
  uint32_t Refused() const { return Refused_; }
  uint32_t Capacity() const { return Cap_; }
  size_t HeapBytes() const { return Stands_.capacity() * sizeof(float); }
  /* The field's own extent in the frame it was collected in: nearest and farthest stand, metres. */
  double NearestM() const { return Nearest_; }
  double FarthestM() const { return Farthest_; }
  const TangentFrame &Frame() const { return Lens_.Frame; }

  static constexpr size_t kFloats = 5;

private:
  Lens Lens_;
  Generators::TreePrototype::Crown Crown_;
  Generators::Region Region_{14, 0, 0};
  std::vector<float> Stands_;
  uint32_t Cap_ = 0;
  uint32_t Beyond_ = 0, InCrown_ = 0, Refused_ = 0;
  double Nearest_ = 0.0, Farthest_ = 0.0;
};

} // namespace outshine::Clients
#endif
