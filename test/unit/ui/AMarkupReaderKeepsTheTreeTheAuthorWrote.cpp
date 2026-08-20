#include <string>

#include "Check.h"

#include "Markup.h"

using outshine::Ui::Markup;
using outshine::Ui::NodeKind;

namespace {

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

}

int main(void) {
  using namespace outshine::Test;

  Markup markup;
  std::string why;
  CHECK(markup.Read(kFixture, why), "a declaration of the shape this corpus has reads");
  if (!why.empty()) { std::printf("       %s\n", why.c_str()); }

  CHECK(Named(markup, "html") == 1, "the document's own element is one node and not two");
  CHECK(Named(markup, "div") == 2, "both divisions are elements");
  CHECK(Named(markup, "link") == 1, "a self-closing link is an element");

  const int link = FirstNamed(markup, "link");
  const int style = FirstNamed(markup, "style");
  CHECK(link >= 0 && style >= 0, "the fixture carries both");
  if (link >= 0 && style >= 0) {
    CHECK(markup.Nodes()[(size_t)link].Parent == markup.Nodes()[(size_t)style].Parent,
          "a void element is a sibling of what follows it and not its parent");
  }

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

  CHECK(markup.StyleText().find("display: flex") != std::string::npos,
        "every style block of the document is kept, joined and in order");
  CHECK(Named(markup, "flexbox") == 0, "a style block's text is text and never markup");

  int inner = -1;
  for (size_t at = 0; at < markup.Nodes().size(); ++at) {
    if (markup.AttributeOf((int)at, "data-expected-width") != nullptr) { inner = (int)at; }
  }
  const std::string *width = markup.AttributeOf(inner, "data-expected-width");
  const std::string *offset = markup.AttributeOf(inner, "data-offset-x");
  CHECK(width != nullptr && *width == "20", "a declared expected width is read back as it was written");
  CHECK(offset != nullptr && *offset == "8", "and so is a declared offset");

  const std::string *xmlns = markup.AttributeOf(FirstNamed(markup, "html"), "xmlns");
  CHECK(xmlns != nullptr && *xmlns == "http://www.w3.org/1999/xhtml",
        "an attribute's value survives its reading exactly");

  Markup refuses;
  CHECK(!refuses.Read("<div><br></br></div>", why),
        "an end tag for a void element is a refusal rather than a recovery");
  CHECK(!refuses.Read("<style>div { }", why),
        "a raw-text element that never closes is a refusal, because where its text ends would be a "
        "guess");

  Covers("a consumer declares an interface as markup and this reader is where it becomes a "
         "tree, with every recovery rule written down and the two refusals named");
  return Report();
}
