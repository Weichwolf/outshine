#ifndef UI_POINTER_H
#define UI_POINTER_H

#include <string>

#include "Layout.h"
#include "Markup.h"

namespace outshine::Ui {

struct Touched {
  int Node = -1;

  std::string Action;

  int DeclaredBy = -1;

  double LocalX = 0, LocalY = 0;
  [[nodiscard]] bool Held() const { return Node >= 0; }
};

[[nodiscard]] Touched Under(const Layout &layout, const Markup &markup, double x, double y);

}
#endif
