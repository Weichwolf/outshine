/* WHAT A LAID-OUT DECLARATION LOOKS LIKE, AS RECTANGLES.
 *
 * **This is the paint half of the engine and it still knows no device** (board:1442): boxes and text
 * runs go in, quads come out, and nothing here names a pipeline, a texture or a draw call. That is
 * what makes a painting judgeable with no GPU in scope, and it is the same reason the layout is.
 *
 * **ONE VERB AND NO TAXONOMY**: *draw this rectangle, with that patch of that image, at that colour.*
 * A button, a name, a health bar and a page of a book are the same four numbers here, which is what
 * keeps a content vocabulary out of the renderer — and it is why the target is free: the consumer
 * decides whether this list lands on the frame or in a texture that a wall wears next frame.
 *
 * **THE ORDER IS THE PAINTER'S AND IT IS THE WHOLE OF THE Z QUESTION.** Background, then border, then
 * whatever the box contains, in tree order. This subset has no `z-index` and no stacking context, so
 * the list is the depth and there is nothing else to consult.
 *
 * **A CLIP TRAVELS WITH THE QUAD RATHER THAN AS A COMMAND.** `overflow: hidden` on an ancestor is an
 * intersection this layer performs once, so a consumer draws the list in one pass and never has to
 * replay a stack of scissors it did not build. */
#ifndef UI_PAINT_H
#define UI_PAINT_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "Layout.h"

namespace outshine::Ui {

/* ONE RECTANGLE IN THE TARGET'S OWN PIXELS, and one patch of the consumer's atlas in it. A patch with
 * no area draws from `Colour` alone. `Opacity` is already the product down the tree, so a consumer
 * multiplies nothing and a nested declaration cannot come out brighter than its parent. */
struct Quad {
  double X = 0, Y = 0, Width = 0, Height = 0;
  double U0 = 0, V0 = 0, U1 = 0, V1 = 0;
  uint32_t Colour = 0;
  double Radius = 0;
  double Opacity = 1.0;
  /* THE RECTANGLE THIS QUAD IS ALLOWED TO TOUCH, already intersected with every clipping ancestor. */
  double ClipX = 0, ClipY = 0, ClipWidth = 0, ClipHeight = 0;
  /* Which node asked for it, so a consumer can answer *what did I just draw* without a second walk. */
  int Node = -1;
};

/* [SET] THE BOUND, AND IT IS A NUMBER SOMEBODY CHOSE. Sixteen thousand rectangles is a 720p screen
 * filled twice over with eight-pixel glyphs; a declaration that needs more is describing a texture
 * rather than an interface. **The overage is published and never truncated silently** — a list cut
 * without a word draws a picture nobody declared, and `CLAUDE.md` calls that the failure a bound
 * exists to make visible. */
inline constexpr size_t kQuadBound = 16384;

class Painting {
public:
  /* The font must be the one the layout was measured with, or the glyphs land in columns nothing
   * measured. The markup is not needed and not taken: every box already carries the node that asked
   * for it, and a painter that could reach the tree would be a painter that could consult meaning. */
  [[nodiscard]] bool Build(const Layout &layout, const Font &font, std::string &error);

  [[nodiscard]] const std::vector<Quad> &Quads(void) const { return Quads_; }
  /* HOW MANY THE DECLARATION ASKED FOR PAST THE BOUND. Zero is the normal answer and a consumer that
   * wants a frame it can trust checks it — the same shape a generator's capability has, and for the
   * same reason: what was achieved is reported in both directions. */
  [[nodiscard]] size_t QuadsBeyondTheBound(void) const { return Beyond_; }

private:
  std::vector<Quad> Quads_;
  size_t Beyond_ = 0;
};

} // namespace outshine::Ui
#endif
