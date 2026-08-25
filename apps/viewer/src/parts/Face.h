#ifndef VIEWER_FACE_H
#define VIEWER_FACE_H

#include <cstdint>
#include <string>
#include <vector>

#include "Layout.h"
#include "OverlayDraw.h"
#include "Paint.h"

namespace outshine::Viewer {

struct Listed {
  std::string Suite;
  std::string Name;
  std::string Prepared;
  bool Ready = false;
  bool Document = false;

  bool Script = false;
};

struct Showing {
  int Selected = -1;
  int ScrolledRows = 0;
  std::string Suite;
  std::string Note;
};

[[nodiscard]] std::vector<Listed> Cases(void);

[[nodiscard]] std::vector<std::string> Suites(const std::vector<Listed> &cases);

[[nodiscard]] std::vector<int> Filtered(const std::vector<Listed> &cases, const Showing &showing);

[[nodiscard]] std::string Declaration(const std::vector<Listed> &cases, const Showing &showing,
                                      int widthPx, int heightPx);

[[nodiscard]] std::string Style(void);

[[nodiscard]] double RootEmPx(int heightPx);

struct Region {
  double X = 0, Y = 0, Width = 0, Height = 0;
  [[nodiscard]] bool Held(void) const { return Width > 0 && Height > 0; }
};

[[nodiscard]] Region StageRegion(int widthPx, int heightPx);

[[nodiscard]] double ColumnsWidth(int widthPx);

[[nodiscard]] int RowsThatFit(int heightPx);

[[nodiscard]] std::string EntryPath(const std::string &prepared);

[[nodiscard]] std::string EntryOf(const std::string &prepared, bool &found);

[[nodiscard]] std::string LinkedSheets(const std::string &prepared);

[[nodiscard]] std::vector<Render::OverlayQuad> AsOverlay(const std::vector<Ui::Quad> &quads,
                                                        double offsetX = 0, double offsetY = 0);

[[nodiscard]] std::string Console(const std::string &title, const std::string &source,
                                  const std::string &verdict, const char *why, int widthPx,
                                  int heightPx);

}
#endif
