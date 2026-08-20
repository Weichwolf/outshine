#include <string>

#include "Check.h"

#include "Markup.h"
#include "Style.h"

using outshine::Ui::Keyword;
using outshine::Ui::Markup;
using outshine::Ui::Property;
using outshine::Ui::Rule;
using outshine::Ui::Selects;
using outshine::Ui::Stylesheet;
using outshine::Ui::Unit;

namespace {

const Rule *Ranked(const Stylesheet &sheet, const Markup &markup, int node, Property wanted,
                   outshine::Ui::Value &out) {
  const Rule *best = nullptr;
  for (const Rule &rule : sheet.Rules()) {
    if (!Selects(rule, markup, node)) { continue; }
    for (const outshine::Ui::Declaration &one : rule.Declares) {
      if (one.What != wanted) { continue; }
      if (best == nullptr || rule.Specificity > best->Specificity ||
          (rule.Specificity == best->Specificity && rule.Order > best->Order)) {
        best = &rule;
        out = one.How;
      }
    }
  }
  return best;
}

int FirstWithId(const Markup &markup, const char *id) {
  for (size_t at = 0; at < markup.Nodes().size(); ++at) {
    const std::string *found = markup.AttributeOf((int)at, "id");
    if (found != nullptr && *found == id) { return (int)at; }
  }
  return -1;
}

}

int main(void) {
  using namespace outshine::Test;

  Markup markup;
  std::string why;
  CHECK(markup.Read(R"MARKUP(<html><body>
      <div class="flexbox"><div class="a" id="inner">x</div></div>
      <div id="circles"><div id="ring"></div></div>
    </body></html>)MARKUP",
                    why),
        "the fixture reads");

  Stylesheet sheet;
  sheet.Read(R"CSS(
      /* a comment reaches no rule */
      div { width: 1px; }
      .a { width: 2px; }
      div.flexbox div { width: 3px; }
      #inner { width: 4px; }
      #circles, #circles div { border-width: 1em; margin: auto; }
      div.flexbox { display: flex; background-color: lightgray; padding: 1px 2px 3px; }
      .a { color: #0f8; opacity: 0.5; }
  )CSS");

  const int inner = FirstWithId(markup, "inner");
  const int ring = FirstWithId(markup, "ring");
  CHECK(inner >= 0 && ring >= 0, "the fixture carries both elements");

  outshine::Ui::Value width;
  const Rule *won = Ranked(sheet, markup, inner, Property::Width, width);
  CHECK(won != nullptr, "a rule selects the inner element");
  CHECK(width.How == Unit::Pixels && width.Number == 4.0,
        "an id outranks a class, a descendant chain and a type");

  outshine::Ui::Value margin;
  const Rule *descends = Ranked(sheet, markup, ring, Property::MarginTop, margin);
  CHECK(descends != nullptr, "`#circles div` selects a division inside `#circles`");
  CHECK(margin.How == Unit::Auto, "and the value it carries is the keyword the author wrote");

  const int flexbox = markup.Nodes()[(size_t)inner].Parent;
  outshine::Ui::Value top, right, bottom, left;
  CHECK(Ranked(sheet, markup, flexbox, Property::PaddingTop, top) != nullptr, "the shorthand expands");
  CHECK(Ranked(sheet, markup, flexbox, Property::PaddingRight, right) != nullptr, "on every side");
  CHECK(Ranked(sheet, markup, flexbox, Property::PaddingBottom, bottom) != nullptr, "of the box");
  CHECK(Ranked(sheet, markup, flexbox, Property::PaddingLeft, left) != nullptr, "including the last");
  CHECK(top.Number == 1.0 && right.Number == 2.0 && bottom.Number == 3.0 && left.Number == 2.0,
        "three values are top, then right and left, then bottom -- CSS's own clock");

  outshine::Ui::Value colour;
  CHECK(Ranked(sheet, markup, inner, Property::Colour, colour) != nullptr, "the colour is declared");
  CHECK(colour.How == Unit::Colour && colour.Word == 0x00FF88FFu,
        "a three-digit hex doubles each digit and carries opaque alpha");

  outshine::Ui::Value display;
  CHECK(Ranked(sheet, markup, flexbox, Property::Display, display) != nullptr, "display is declared");
  CHECK(display.How == Unit::Keyword && display.Word == Keyword("flex"),
        "a keyword is compared as an integer and reads as its own spelling");

  {
    Markup nested;
    std::string why;
    CHECK(nested.Read("<div class=outer><p><span id=deep></span></p></div>", why),
          "the nested document reads");
    Stylesheet reaching;
    reaching.Read(".outer > span { width: 1px }\n.outer span { height: 2px }");
    CHECK(reaching.SelectorsOutsideTheSubset() == 0,
          "neither combinator is outside the subset");
    int deep = -1;
    for (int at = 0; at < (int)nested.Nodes().size(); ++at) {
      const std::string *id = nested.AttributeOf(at, "id");
      if (id != nullptr && *id == "deep") { deep = at; }
    }
    CHECK(deep >= 0, "the grandchild is found");
    if (deep >= 0) {
      outshine::Ui::Value width, height;
      CHECK(Ranked(reaching, nested, deep, Property::Width, width) == nullptr,
            "the child combinator does not reach a grandchild");
      CHECK(Ranked(reaching, nested, deep, Property::Height, height) != nullptr,
            "and the descendant combinator does, which is the whole of what separates them");
    }
  }

  Stylesheet outside;
  outside.Read("@media screen { div { width: 1px } }\n"
               "div:hover { width: 2px }\n"
               "div + p { width: 3px }\n"
               "div { transform: rotate(3deg); box-shadow: 0 0 2px black; width: calc(100% - 4em) }");
  CHECK(outside.SelectorsOutsideTheSubset() >= 3,
        "an at-rule, a pseudo-class and a sibling combinator are each counted as outside");
  CHECK(!outside.NamesOutsideTheSubset().empty(),
        "and each is NAMED and not only counted -- a count with nothing beside it is a rule quietly "
        "dropped, which is what the corpus's second number exists to make visible");
  CHECK(outside.PropertiesOutsideTheSubset() >= 2,
        "a property this engine does not hold is counted and never dropped in silence");
  CHECK(outside.Rules().empty() ||
            outside.Rules().size() < 4,
        "and no rule this reader could not express reached the sheet");

  Covers("the cascade is ranked by CSS specificity over the selector subset the corpus "
         "writes, and everything outside the subset is counted so the corpus selection is derived");
  return Report();
}
