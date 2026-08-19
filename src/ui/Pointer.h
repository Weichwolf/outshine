/* WHAT A POINTER FOUND, AND NOTHING ABOUT WHAT IT MEANS (board:1442).
 *
 * **A HIT IS AN ANSWER, NEVER A DECISION.** The client asks *what is under this point*; the library
 * replies with the element and the action that element DECLARED, and stops there. Whether that action
 * opens a door, turns a page or fires a weapon is the consumer's, and the consumer's alone -- which is
 * why this header holds no registry, no dispatcher and no callback the library owns.
 *
 * **THERE IS NO CALLBACK REGISTERED WITH THE ENGINE, AND THAT IS THE DESIGN RATHER THAN A GAP.** A
 * stage signals readiness; it never asks a question. A library that invoked a consumer's function
 * would be the library asking -- it would decide WHEN the consumer runs, on which thread, and inside
 * which frame -- so the direction is inverted: the consumer polls a value it owns the timing of. The
 * client's own callback is the client's business and lives on its side of this line.
 *
 * **THE ACTION IS AN ATTRIBUTE AND THE ENGINE NEVER READS IT.** `data-action` is HTML's own spelling
 * for author data, so it cannot collide with anything this engine consults, and its VALUE is opaque
 * here: a string handed back exactly as declared. An engine that understood one action would have a
 * vocabulary of meanings, and that is the noun-in-the-mechanism defect the whole decomposition exists
 * to prevent. */
#ifndef UI_POINTER_H
#define UI_POINTER_H

#include <string>

#include "Layout.h"
#include "Markup.h"

namespace outshine::Ui {

/* THE ELEMENT UNDER A POINT AND WHAT IT DECLARED. `Node` is -1 and `Held` false when the point falls
 * on nothing, which a client checks before it does anything -- a hit that reported the root for every
 * stray click would make a miss unspellable. */
struct Touched {
  int Node = -1;
  /* The nearest `data-action` at or above the hit element, empty when nobody declared one. */
  std::string Action;
  /* Which element declared it, which is not always the one that was hit. */
  int DeclaredBy = -1;
  /* Where inside the hit element's border box the point landed, in the same pixels everything else
   * here is in -- so a client can place a menu, start a drag or seek a bar without a second lookup. */
  double LocalX = 0, LocalY = 0;
  [[nodiscard]] bool Held(void) const { return Node >= 0; }
};

/* **THE ACTION IS INHERITED UP THE TREE AND THAT IS DELIBERATE.** A pointer lands on whatever box is
 * deepest -- the text inside a button, not the button -- so an action declared on the button would be
 * missed by every click that hit its label. Walking up is what makes a declaration cover what it
 * contains, and it is the same rule a browser's own event path states. */
[[nodiscard]] Touched Under(const Layout &layout, const Markup &markup, double x, double y);

} // namespace outshine::Ui
#endif
