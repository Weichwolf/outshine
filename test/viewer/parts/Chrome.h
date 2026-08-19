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
[[nodiscard]] const char *Style(void);

/* HOW MANY ROWS FIT, which the scroll needs and the declaration already decides. One number in one
 * place, or the list scrolls past its own end on one side and not the other. */
[[nodiscard]] int RowsThatFit(int heightPx);

/* THE TRANSLATION, AND IT IS THE CLIENT'S BY DESIGN. The renderer takes its own quad because no
 * content noun has a spelling in it; this is the loop that costs. */
[[nodiscard]] std::vector<Render::OverlayQuad> AsOverlay(const std::vector<Ui::Quad> &quads);

} // namespace outshine::Viewer
#endif
