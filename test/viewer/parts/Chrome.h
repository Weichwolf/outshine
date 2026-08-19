/* THE BROWSER'S OWN SURFACE, AS A DECLARATION (board:1447).
 *
 * **EVERYTHING THE BROWSER SHOWS OF ITSELF IS SPELLED HERE, IN MARKUP AND STYLE.** There is no
 * rectangle in this program that is not a `<div>`, no colour that is not a property, and no position
 * that is not a consequence of the layout -- which is what the claim *the renderer is the library*
 * costs to be true rather than said.
 *
 * **THE STATE IS THE INPUT AND THE DECLARATION IS THE OUTPUT.** Which suite, which case, where the
 * list is scrolled: a handful of numbers in, one document out. That is what makes the browser's whole
 * appearance a pure function of its state -- reproducible, diffable, and testable without a window.
 *
 * **NO WIDGET HAS A NAME HERE EITHER.** A row, a bar and a pane are classes on boxes; the engine sees
 * boxes and this file sees a case list, and neither knows what the other means. */
#ifndef VIEWER_CHROME_H
#define VIEWER_CHROME_H

#include <cstdint>
#include <string>
#include <vector>

#include "Layout.h"
#include "OverlayDraw.h"
#include "Paint.h"

namespace outshine::Viewer {

/* ONE CASE AS THE BROWSER KNOWS IT: what the tree declares about it, and whether it is on disk. */
struct Listed {
  std::string Suite;
  std::string Name;
  std::string Prepared;
  bool Ready = false;
  bool Document = false;
  /* A CASE WHOSE SUBJECT IS A PROGRAM AND NOT A PAGE. The browser shows it as its own text and its own
   * verdict; counting it among the documents would report 813 pages that were never pages. */
  bool Script = false;
};

/* WHAT THE BROWSER IS SHOWING, AND IT IS THE WHOLE OF ITS STATE. A field added here is a thing the
 * appearance can depend on; nothing else is. */
struct Showing {
  int Selected = -1;       /* index into the filtered list, or -1 */
  int ScrolledRows = 0;    /* how many rows the list has been moved up by */
  std::string Suite;       /* which suite is filtered to, empty for all */
  std::string Note;        /* the status line's own sentence */
};

/* EVERY CASE THE TREE DECLARES, in a stable order. A case whose preparation has not run is LISTED and
 * marked, never dropped: a browser that showed only what happened to be on disk would answer a
 * different question than *what does this tree declare*. */
[[nodiscard]] std::vector<Listed> Cases(void);

/* The suites present in a listing, in first-seen order, so the toolbar is derived and not written. */
[[nodiscard]] std::vector<std::string> Suites(const std::vector<Listed> &cases);

/* Which of `cases` the current filter admits, as indices into it. */
[[nodiscard]] std::vector<int> Filtered(const std::vector<Listed> &cases, const Showing &showing);

/* THE DOCUMENT THE BROWSER IS, for this state and this surface. */
[[nodiscard]] std::string Declaration(const std::vector<Listed> &cases, const Showing &showing,
                                      int widthPx, int heightPx);

/* The stylesheet, kept apart from the markup so a reader can see the whole appearance in one place. */
[[nodiscard]] std::string Style(void);

/* **THE ONE PLACE A RATIO BECOMES A DEVICE PIXEL**: the root's text size, as a share of the surface's
 * height. Everything else in this browser is a percentage, a multiple of the text or a share of what
 * is left -- so a window twice as large shows the same interface twice as big. */
[[nodiscard]] double RootEmPx(int heightPx);

/* WHERE THE PICTURE OF A CASE GOES, **IN FRACTIONS OF THE SURFACE**: the room left of the two columns,
 * below the plate. The library keeps the case's own shape inside it and resolves the pixels, so this
 * browser never names one -- and a window that is resized costs it nothing.
 *
 * The two column widths are pixels because they are a stylesheet's, and turning them into a fraction
 * is the one division that has to happen somewhere. */
struct Region {
  double X = 0, Y = 0, Width = 0, Height = 0;
  [[nodiscard]] bool Held(void) const { return Width > 0 && Height > 0; }
};

[[nodiscard]] Region StageRegion(int widthPx, int heightPx);
/* How much of the surface the two columns take, so a caller placing something in the pane needs no
 * second copy of the two shares. */
[[nodiscard]] double ColumnsWidth(int widthPx);

/* HOW MANY ROWS FIT, which the scroll needs and the declaration already decides. One number in one
 * place, or the list scrolls past its own end on one side and not the other. */
[[nodiscard]] int RowsThatFit(int heightPx);

/* A PREPARED DOCUMENT CASE'S OWN TEXT, and the stylesheets its manifest says it links. They are two
 * calls because a document that does not read is a different answer from one whose sheet is missing. */
/* WHERE THE CASE'S OWN SUBJECT FILE IS, named by its manifest. A consumer setting a scenario up hands
 * over a path; reading it is the engine's. Empty where the case is not prepared. */
[[nodiscard]] std::string EntryPath(const std::string &prepared);

[[nodiscard]] std::string EntryOf(const std::string &prepared, bool &found);
/* THE SHEETS A CASE'S DIRECTORY CARRIES BESIDE ITS PAGE, as text, because a scenario is declared
 * with style and not with a parsed object. */
[[nodiscard]] std::string LinkedSheets(const std::string &prepared);

/* THE TRANSLATION, AND IT IS THE CLIENT'S BY DESIGN. The renderer takes its own quad because no
 * content noun has a spelling in it; this is the loop that costs. */
[[nodiscard]] std::vector<Render::OverlayQuad> AsOverlay(const std::vector<Ui::Quad> &quads,
                                                        double offsetX = 0, double offsetY = 0);

/* **A CONSOLE FOR A CASE WHOSE SUBJECT IS A PROGRAM.** A script has no picture, so showing it as a
 * blank pane says nothing; showing the program and what this engine made of it says everything a
 * reader wants. It is a declaration like any other -- the browser has one way to draw. */
[[nodiscard]] std::string Console(const std::string &title, const std::string &source,
                                  const std::string &verdict, const char *why, int widthPx,
                                  int heightPx);

} // namespace outshine::Viewer
#endif
