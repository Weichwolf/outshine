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

  /* Clears what was collected: a collection is a whole answer, never an append to an older one. */
  void Aim(const Lens &lens, const Generators::TreePrototype::Crown &crown);
  /* The region the instances that follow are measured in. */
  void From(const Generators::Region &region) { Region_ = region; }

  [[nodiscard]] bool Add(Generators::BodyId body, Generators::ClusterId cluster,
                         const Generators::Instance &instance) noexcept override;
  bool Full() const noexcept override { return false; }

  /* Render::ModelDraw's instance layout: east, north, foot over the frame's anchor, yaw, and
   * the factor on the model's own height. */
  const std::vector<float> &Stands() const { return Stands_; }
  uint32_t Count() const { return (uint32_t)(Stands_.size() / kFloats); }
  uint32_t BeyondReach() const { return Beyond_; }
  uint32_t InCrown() const { return InCrown_; }
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
  uint32_t Beyond_ = 0, InCrown_ = 0;
  double Nearest_ = 0.0, Farthest_ = 0.0;
};

} // namespace outshine::Clients
#endif
