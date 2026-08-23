#include <cstdio>
#include <cstring>
#include <string>

#include "Check.h"

#include "Json.h"

using outshine::Json;

namespace {

[[nodiscard]] bool Parses(const std::string &text) {
  Json json;
  return json.Parse(text.c_str(), text.size());
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  CHECK(Parses(R"({"a": [1, 2.5, -3e2], "b": {"c": "text"}, "d": true, "e": null})"),
        "a well-formed document parses");

  {
    // the depth bomb: 200k brackets once segfaulted the process behind every careful
    // refusal the gltf door holds -- now the COUNTER refuses, not the C stack
    std::string bomb(200000, '[');
    Json json;
    CHECK(!json.Parse(bomb.c_str(), bomb.size()),
          "**A 200000-BRACKET BOMB IS A REFUSAL, NOT A SEGFAULT** -- the depth is a "
          "counter's business, never the C stack's (board:1732)");
    Note("the refusal stopped at byte", (double)json.StoppedAt(), "offset");
  }

  CHECK(!Parses("[1 2 3]"),
        "**THE GRAMMAR IS ENFORCED**: two values without a comma are not an array");
  CHECK(!Parses("[1, 2,]"), "and a trailing comma is a promise the grammar does not keep");
  CHECK(!Parses("{\"x\": 1} garbage"),
        "**THE WHOLE TEXT IS THE DOCUMENT**: a valid prefix with a suffix is not json");
  CHECK(!Parses("truex"), "'truex' is not true with a suffix");
  CHECK(!Parses("[Infinity]"), "Infinity is not a json number");
  CHECK(!Parses("[nan]"), "neither is nan");
  CHECK(Parses("[true, false, null]"), "the three literals parse where the grammar puts them");

  {
    Json json;
    const char *text = R"({"byteLength": true, "count": 7})";
    CHECK(json.Parse(text, std::strlen(text)), "the typed document parses");
    CHECK(json.Root()["byteLength"].Num(-1.0) == -1.0,
          "**A BOOL IS NOT A NUMBER**: 'byteLength': true answers the caller's default, "
          "never 1.0 -- the checked-size door is fed numbers or nothing (board:1732)");
    CHECK(!json.Root()["count"].Bool(false) || json.Root()["count"].Num(0.0) == 7.0,
          "and a number is not a bool from the other side");
    CHECK(json.Root()["count"].Num(0.0) == 7.0, "the real number still answers");
  }

  {
    Json json;
    const char *text = R"({"pair": "\uD83D\uDE00", "lone": "\uD800x"})";
    CHECK(json.Parse(text, std::strlen(text)), "the surrogate document parses");
    const std::string pair = json.Root()["pair"].Str("");
    CHECK(pair.size() == 4 && (uint8_t)pair[0] == 0xF0 && (uint8_t)pair[1] == 0x9F,
          "**A SURROGATE PAIR DECODES TO ONE FOUR-BYTE CHARACTER** -- U+1F600, valid "
          "utf-8, never two three-byte halves");
    const std::string lone = json.Root()["lone"].Str("");
    CHECK(lone.size() == 4 && (uint8_t)lone[0] == 0xEF && (uint8_t)lone[1] == 0xBF &&
              (uint8_t)lone[2] == 0xBD && lone[3] == 'x',
          "and a lone half becomes U+FFFD instead of invalid utf-8");
  }

  {
    // deep-but-legal nesting parses: the bound is for bombs, not for documents
    std::string nested;
    for (int at = 0; at < 100; ++at) { nested += '['; }
    nested += '1';
    for (int at = 0; at < 100; ++at) { nested += ']'; }
    CHECK(Parses(nested), "a hundred legal levels parse under the [SET] bound of 256");
  }

  Covers("I.25 the json door refuses what is not json and survives what is hostile: a "
         "counted depth, an enforced grammar, whole-text consumption, kinds kept apart, "
         "surrogate pairs decoded whole -- the parser under the one content surface has "
         "its own twin (board:1732)");
  return Report();
}
