#include "Pointer.h"

namespace outshine::Ui {

Touched Under(const Layout &layout, const Markup &markup, double x, double y) {
  Touched found;
  found.Node = layout.Hit(x, y);
  if (!found.Held()) { return found; }

  for (const Box &box : layout.Boxes()) {
    if (box.Node != found.Node) { continue; }
    if (x >= box.X && x < box.X + box.Width && y >= box.Y && y < box.Y + box.Height) {
      found.LocalX = x - box.X;
      found.LocalY = y - box.Y;
      break;
    }
  }

  for (int at = found.Node; at >= 0; at = markup.Nodes()[(size_t)at].Parent) {
    const std::string *declared = markup.AttributeOf(at, "data-action");
    if (declared != nullptr) {
      found.Action = *declared;
      found.DeclaredBy = at;
      break;
    }
  }
  return found;
}

}
