/* WHAT A DECLARATION SAYS, READ BACK AS A TREE (board:1442).
 *
 * The fixture is not a wish: it is the shape the corpus actually has -- a doctype, comments, an
 * `xmlns` attribute, self-closing `<link/>`, a `<style>` block, prose in `<p>` without an end tag, and
 * the attributes a layout assertion is carried in. Every rule this reader recovers from is exercised
 * here, and the two it refuses are asked for by name. */
#include <string>

#include "Check.h"

#include "Markup.h"

using outshine::Ui::Markup;
using outshine::Ui::NodeKind;

namespace {

/* THE CORPUS'S OWN SHAPE, shortened but not simplified: every construct below appears in
 * `css/css-flexbox/align-content-horiz-001a.html` at the pin `board:1443` names. */
const char *kFixture = R"MARKUP(<!doctype html>
<!-- a comment that reaches no node -->
<html xmlns="http://www.w3.org/1999/xhtml">
  <head>
    <title>CSS Test: reading a declaration</title>
    <link rel="author" title="Someone" href="mailto:someone@example.com"/>
    <style>
      div.flexbox { width: 20px; display: flex; }
    </style>
  </head>
  <body onload="checkLayout('.flexbox div')">
    <p>Prose the author never closed
    <div class="flexbox">
      <div class="a" data-expected-width="20" data-offset-x="8"></div>
    </div>
  </body>
</html>)MARKUP";

int Named(const Markup &markup, const char *name) {
  int found = 0;
  for (const outshine::Ui::Node &node : markup.Nodes()) {
    if (node.Kind == NodeKind::Element && node.Name == name) { ++found; }
  }
  return found;
}

int FirstNamed(const Markup &markup, const char *name) {
  for (size_t at = 0; at < markup.Nodes().size(); ++at) {
    if (markup.Nodes()[at].Kind == NodeKind::Element && markup.Nodes()[at].Name == name) {
      return (int)at;
    }
  }
  return -1;
}

}  // namespace

int main(void) {
  using namespace outshine::Test;

  Markup markup;
  std::string why;
  CHECK(markup.Read(kFixture, why), "a declaration of the shape this corpus has reads");
  if (!why.empty()) { std::printf("       %s\n", why.c_str()); }

  CHECK(Named(markup, "html") == 1, "the document's own element is one node and not two");
  CHECK(Named(markup, "div") == 2, "both divisions are elements");
  CHECK(Named(markup, "link") == 1, "a self-closing link is an element");

  /* THE VOID ELEMENT DID NOT SWALLOW WHAT FOLLOWED IT, which is the whole reason the set is
   * published: a reader that pushed `<link>` would nest the style, the body and every box inside it. */
  const int link = FirstNamed(markup, "link");
  const int style = FirstNamed(markup, "style");
  CHECK(link >= 0 && style >= 0, "the fixture carries both");
  if (link >= 0 && style >= 0) {
    CHECK(markup.Nodes()[(size_t)link].Parent == markup.Nodes()[(size_t)style].Parent,
          "a void element is a sibling of what follows it and not its parent");
  }

  /* A `<p>` THE AUTHOR NEVER CLOSED HOLDS ITS PROSE AND NOT THE REST OF THE DOCUMENT. */
  const int paragraph = FirstNamed(markup, "p");
  CHECK(paragraph >= 0, "the prose is an element");
  if (paragraph >= 0) {
    const outshine::Ui::Node &node = markup.Nodes()[(size_t)paragraph];
    CHECK(node.Children.size() == 1, "the unclosed paragraph holds its own text and nothing after it");
    for (const int child : node.Children) {
      CHECK(markup.Nodes()[(size_t)child].Kind == NodeKind::Text,
            "what it holds is the run the author typed");
    }
  }

  /* THE STYLE IS TEXT AND NOT MARKUP, so a selector containing `<` would not have become an element
   * -- and the block is kept for the cascade rather than dropped. */
  CHECK(markup.StyleText().find("display: flex") != std::string::npos,
        "every style block of the document is kept, joined and in order");
  CHECK(Named(markup, "flexbox") == 0, "a style block's text is text and never markup");

  /* THE ASSERTIONS A LAYOUT CASE CARRIES ARE ATTRIBUTES, which is what makes that family readable at
   * all -- `board:1443` measures that upstream states them this way. */
  /* ASKED FOR BY WHAT IT CARRIES AND NOT BY ITS POSITION: the first child of the outer division is the
   * whitespace between the two tags, which is a text run the author typed and this reader keeps. */
  int inner = -1;
  for (size_t at = 0; at < markup.Nodes().size(); ++at) {
    if (markup.AttributeOf((int)at, "data-expected-width") != nullptr) { inner = (int)at; }
  }
  const std::string *width = markup.AttributeOf(inner, "data-expected-width");
  const std::string *offset = markup.AttributeOf(inner, "data-offset-x");
  CHECK(width != nullptr && *width == "20", "a declared expected width is read back as it was written");
  CHECK(offset != nullptr && *offset == "8", "and so is a declared offset");

  /* AN ATTRIBUTE'S NAME IS LOWERED AND ITS VALUE IS NOT, because a tag name is a spelling and a value
   * is content. */
  const std::string *xmlns = markup.AttributeOf(FirstNamed(markup, "html"), "xmlns");
  CHECK(xmlns != nullptr && *xmlns == "http://www.w3.org/1999/xhtml",
        "an attribute's value survives its reading exactly");

  /* THE TWO REFUSALS, ASKED FOR BY NAME. Both mean the document says something its author did not,
   * and a guess would put content in a box nobody declared. */
  Markup refuses;
  CHECK(!refuses.Read("<div><br></br></div>", why),
        "an end tag for a void element is a refusal rather than a recovery");
  CHECK(!refuses.Read("<style>div { }", why),
        "a raw-text element that never closes is a refusal, because where its text ends would be a "
        "guess");

  Covers("board:1442 a consumer declares an interface as markup and this reader is where it becomes a "
         "tree, with every recovery rule written down and the two refusals named");
  return Report();
}
