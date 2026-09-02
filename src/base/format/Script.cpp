#include "Script.h"

#include <array>
#include <charconv>

#include "DecimalEdge.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string_view>
#include <string>
#include <vector>
#include <utility>
#include <system_error>

namespace outshine::Script {

constexpr double kWholeMost = 1e15;

bool Value::Truth() const {
  switch (What) {
    case Kind::Nothing: return false;
    case Kind::Number: return Number != 0.0;
    case Kind::Text: return !Text.empty();
    case Kind::Ref: return true;
  }
  return false;
}

std::string Value::AsText() const {
  switch (What) {
    case Kind::Nothing: return "";
    case Kind::Text: return Text;
    case Kind::Ref: return "";
    case Kind::Number: break;
  }

  std::array<char, 32> held{};
  if (Number == std::floor(Number) && std::fabs(Number) < kWholeMost) {
    std::snprintf(held.data(), held.size(), "%lld", static_cast<long long>(Number));
  } else {
    std::snprintf(held.data(), held.size(), "%g", Number);
  }
  return held.data();
}

namespace {

enum class Word : uint8_t { End, Number, Text, Name, Mark };

struct Token {
  Word What = Word::End;
  std::string Spelling;
  double Number = 0.0;
  size_t At = 0;
};

bool Space(char c) {
  return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f';
}

size_t SpaceRun(std::string_view text, size_t at) {
  if (Space(text[at])) { return 1; }
  static const std::array<const char *const, 11> kWide = {{"\xC2\xA0",
                                                           "\xE2\x80\xA8",
                                                           "\xE2\x80\xA9",
                                                           "\xEF\xBB\xBF",
                                                           "\xE2\x80\x80",
                                                           "\xE2\x80\x81",
                                                           "\xE2\x80\x82",
                                                           "\xE2\x80\x83",
                                                           "\xE2\x80\x89",
                                                           "\xE2\x80\xAF",
                                                           "\xE3\x80\x80"}};
  for (const char *wide : kWide) {
    const size_t length = std::char_traits<char>::length(wide);
    if (text.compare(at, length, wide) == 0) { return length; }
  }
  return 0;
}

bool Starts(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '$';
}

bool Continues(char c) {
  return Starts(c) || (c >= '0' && c <= '9');
}

constexpr std::array<const char *, 27> kReserved = {
    {"new",     "typeof", "void",  "delete", "instanceof", "in",    "function",
     "class",   "await",  "yield", "async",  "throw",      "try",   "catch",
     "finally", "switch", "case",  "for",    "do",         "break", "continue",
     "return",  "this",   "super", "export", "import",     "with"}};

constexpr std::array<const char *, 26> kMarks = {
    {"++", "--", "==", "!=", "<=", ">=", "&&", "||", "(", ")", "{", "}", "[",
     "]",  ";",  ",",  ".",  "+",  "-",  "*",  "/",  "%", "<", ">", "=", "!"}};

std::string Where(std::string_view text, size_t at) {
  size_t line = 1;
  size_t column = 1;
  for (size_t i = 0; i < at && i < text.size(); ++i) {
    if (text[i] == '\n') {
      ++line;
      column = 1;
    } else {
      ++column;
    }
  }
  return "line " + std::to_string(line) + " column " + std::to_string(column);
}

bool Tokenise(std::string_view text, std::vector<Token> &out, std::string &error) {
  size_t at = 0;
  while (at < text.size()) {
    if (const size_t run = SpaceRun(text, at)) {
      at += run;
      continue;
    }
    if (text.compare(at, 2, "//") == 0) {
      const size_t end = text.find('\n', at);
      at = end == std::string::npos ? text.size() : end + 1;
      continue;
    }
    if (text.compare(at, 2, "/*") == 0) {
      const size_t end = text.find("*/", at + 2);
      at = end == std::string::npos ? text.size() : end + 2;
      continue;
    }
    if (out.size() >= kMaxTokens) {
      error = "the script reaches the token bound of " + std::to_string(kMaxTokens) + " at " +
              Where(text, at) + ", and a program cut in half is a program that did something else";
      return false;
    }
    Token token;
    token.At = at;
    if (text[at] == '"' || text[at] == '\'') {
      const char quote = text[at];
      ++at;
      token.What = Word::Text;
      while (at < text.size() && text[at] != quote) {
        if (text[at] == '\\' && at + 1 < text.size()) {
          ++at;
          const char escaped = text[at];
          token.Spelling.push_back(escaped == 'n' ? '\n' : escaped == 't' ? '\t' : escaped);
        } else {
          token.Spelling.push_back(text[at]);
        }
        ++at;
      }
      if (at >= text.size()) {
        error = "the script opens a string at " + Where(text, token.At) + " and never closes it";
        return false;
      }
      ++at;
      out.push_back(std::move(token));
      continue;
    }
    if ((text[at] >= '0' && text[at] <= '9') ||
        (text[at] == '.' && at + 1 < text.size() && text[at + 1] >= '0' && text[at + 1] <= '9')) {
      token.What = Word::Number;
      if (text[at] == '0' && at + 1 < text.size() && (text[at + 1] == 'x' || text[at + 1] == 'X')) {
        uint64_t wide = 0;
        const auto hex = std::from_chars(text.data() + at + 2, text.data() + text.size(), wide, 16);
        if (hex.ec == std::errc::result_out_of_range) {
          size_t digitEnd = at + 2;
          while (digitEnd < text.size() && ((text[digitEnd] >= '0' && text[digitEnd] <= '9') ||
                                            (text[digitEnd] >= 'a' && text[digitEnd] <= 'f') ||
                                            (text[digitEnd] >= 'A' && text[digitEnd] <= 'F'))) {
            ++digitEnd;
          }
          const auto whole = std::from_chars(
              text.data() + at + 2, text.data() + digitEnd, token.Number, std::chars_format::hex);
          if (whole.ec == std::errc::result_out_of_range) { token.Number = HUGE_VAL; }
          at = digitEnd;
          out.push_back(std::move(token));
          continue;
        }
        if (hex.ptr != text.data() + at + 2) {
          token.Number = static_cast<double>(wide);
          at = static_cast<size_t>(hex.ptr - text.data());
          out.push_back(std::move(token));
          continue;
        }
      }
      const auto scanned =
          std::from_chars(text.data() + at, text.data() + text.size(), token.Number);
      if (scanned.ec == std::errc::result_out_of_range) { token.Number = HUGE_VAL; }
      at = static_cast<size_t>(scanned.ptr - text.data());
      out.push_back(std::move(token));
      continue;
    }
    if (Starts(text[at])) {
      token.What = Word::Name;
      while (at < text.size() && Continues(text[at])) { token.Spelling.push_back(text[at++]); }
      for (const char *word : kReserved) {
        if (token.Spelling == word) { token.What = Word::Mark; }
      }
      out.push_back(std::move(token));
      continue;
    }
    bool marked = false;
    for (const char *mark : kMarks) {
      const size_t length = std::char_traits<char>::length(mark);
      if (text.compare(at, length, mark) != 0) { continue; }
      token.What = Word::Mark;
      token.Spelling = mark;
      at += length;
      out.push_back(std::move(token));
      marked = true;
      break;
    }
    if (!marked) {
      error = std::string("the script carries '") + text[at] + "' at " + Where(text, at) +
              ", which is outside the subset this interpreter declares";
      return false;
    }
  }
  Token end;
  end.At = text.size();
  out.push_back(std::move(end));
  return true;
}

} // namespace

struct Program::Node {
  enum class Shape : uint8_t {
    Number,
    Text,
    Nothing,
    Name,
    Member,
    Call,
    Unary,
    Binary,
    Assign,
    AssignMember,
    Step,
    If,
    While,
    Block
  };
  Shape What = Shape::Number;
  double Number = 0.0;
  std::string Spelling;
  size_t A = 0, B = 0, C = 0;
  std::vector<size_t> Parts;
};

namespace {

struct Reading {
  std::string_view Text;
  const std::vector<Token> &Tokens;
  std::vector<Program::Node> &Nodes;
  std::string &Error;
  size_t At = 0;
  size_t Depth = 0;

  [[nodiscard]] const Token &Now() const { return Tokens[At]; }

  [[nodiscard]] bool Is(const char *mark) const {
    return Now().What == Word::Mark && Now().Spelling == mark;
  }

  [[nodiscard]] bool IsWord(const char *word) const {
    return Now().What == Word::Name && Now().Spelling == word;
  }

  bool Take(const char *mark) {
    if (!Is(mark)) { return false; }
    ++At;
    return true;
  }

  [[nodiscard]] bool Want(const char *mark) {
    if (Take(mark)) { return true; }
    Error = std::string("the script expected '") + mark + "' at " + Where(Text, Now().At) +
            " and found " +
            (Now().What == Word::End ? std::string("the end of the script")
                                     : "'" + Now().Spelling + "'");
    return false;
  }

  [[nodiscard]] bool Room() {
    if (Nodes.size() < kMaxNodes) { return true; }
    Error = "the script reaches the node bound of " + std::to_string(kMaxNodes);
    return false;
  }

  [[nodiscard]] bool Deeper() {
    if (++Depth <= kMaxDepth) { return true; }
    Error = "the script nests past the depth bound of " + std::to_string(kMaxDepth) + " at " +
            Where(Text, Now().At);
    return false;
  }

  void Shallower() { --Depth; }

  size_t Make(Program::Node node) {
    Nodes.push_back(std::move(node));
    return Nodes.size() - 1;
  }
};

using Shape = Program::Node::Shape;

bool ReadExpression(Reading &in, size_t &out);

bool ReadSequence(Reading &in, size_t &out);
bool ReadStatement(Reading &in, size_t &out);

bool ReadPrimary(Reading &in, size_t &out) {
  if (!in.Room()) { return false; }
  const Token &token = in.Now();
  if (token.What == Word::Number) {
    ++in.At;
    Program::Node node;
    node.What = Shape::Number;
    node.Number = token.Number;
    out = in.Make(std::move(node));
  } else if (token.What == Word::Text) {
    ++in.At;
    Program::Node node;
    node.What = Shape::Text;
    node.Spelling = token.Spelling;
    out = in.Make(std::move(node));
  } else if (token.What == Word::Name) {
    ++in.At;
    Program::Node node;

    if (token.Spelling == "true" || token.Spelling == "false") {
      node.What = Shape::Number;
      node.Number = token.Spelling == "true" ? 1.0 : 0.0;
    } else if (token.Spelling == "null" || token.Spelling == "undefined") {
      node.What = Shape::Nothing;
    } else {
      node.What = Shape::Name;
      node.Spelling = token.Spelling;
    }
    out = in.Make(std::move(node));
  } else if (in.Take("(")) {
    if (!ReadSequence(in, out)) { return false; }
    if (!in.Want(")")) { return false; }
  } else if (in.Is("++") || in.Is("--")) {
    const std::string mark = in.Now().Spelling;
    ++in.At;
    size_t target = 0;
    if (!ReadPrimary(in, target)) { return false; }
    if (in.Nodes[target].What != Shape::Name) {
      in.Error =
          "the script increments something that is not a name, at " + Where(in.Text, in.Now().At);
      return false;
    }
    Program::Node node;
    node.What = Shape::Step;
    node.Spelling = in.Nodes[target].Spelling;
    node.Number = mark == "++" ? 1.0 : -1.0;
    node.A = 0;
    if (!in.Room()) { return false; }
    out = in.Make(std::move(node));
    return true;
  } else if (in.Is("-") || in.Is("+") || in.Is("!")) {
    const std::string mark = token.Spelling;
    ++in.At;
    size_t inner = 0;
    if (!ReadPrimary(in, inner)) { return false; }
    Program::Node node;
    node.What = Shape::Unary;
    node.Spelling = mark;
    node.A = inner;
    out = in.Make(std::move(node));
  } else {
    in.Error = "the script expected a value at " + Where(in.Text, token.At) + " and found " +
               (token.What == Word::End ? std::string("the end of the script")
                                        : "'" + token.Spelling + "'");
    return false;
  }

  for (;;) {
    if (in.Take(".")) {
      if (in.Now().What != Word::Name) {
        in.Error = "the script expected a member name at " + Where(in.Text, in.Now().At);
        return false;
      }
      Program::Node node;
      node.What = Shape::Member;
      node.Spelling = in.Now().Spelling;
      node.A = out;
      ++in.At;
      if (!in.Room()) { return false; }
      out = in.Make(std::move(node));
      continue;
    }
    if (in.Is("++") || in.Is("--")) {
      if (in.Nodes[out].What != Shape::Name) { break; }
      Program::Node node;
      node.What = Shape::Step;
      node.Spelling = in.Nodes[out].Spelling;
      node.Number = in.Now().Spelling == "++" ? 1.0 : -1.0;
      node.A = 1;
      ++in.At;
      if (!in.Room()) { return false; }
      out = in.Make(std::move(node));
      continue;
    }
    if (in.Take("(")) {
      Program::Node node;
      node.What = Shape::Call;
      node.A = out;
      if (!in.Is(")")) {
        for (;;) {
          size_t argument = 0;
          if (!ReadExpression(in, argument)) { return false; }
          if (node.Parts.size() >= kMaxArgs) {
            in.Error = "the script passes past the argument bound of " + std::to_string(kMaxArgs) +
                       " at " + Where(in.Text, in.Now().At);
            return false;
          }
          node.Parts.push_back(argument);
          if (!in.Take(",")) { break; }
        }
      }
      if (!in.Want(")")) { return false; }
      if (!in.Room()) { return false; }
      out = in.Make(std::move(node));
      continue;
    }
    break;
  }
  return true;
}

struct Level {
  const std::array<const char *, 4> Marks;
};

constexpr std::array<Level, 6> kLevels = {{
    {{"||", nullptr, nullptr, nullptr}},
    {{"&&", nullptr, nullptr, nullptr}},
    {{"==", "!=", nullptr, nullptr}},
    {{"<", "<=", ">", ">="}},
    {{"+", "-", nullptr, nullptr}},
    {{"*", "/", "%", nullptr}},
}};
constexpr size_t kLevelCount = sizeof(kLevels) / sizeof(kLevels[0]);

bool ReadBinary(Reading &in, size_t level, size_t &out) {
  if (level >= kLevelCount) { return ReadPrimary(in, out); }
  if (!in.Deeper()) { return false; }
  bool held = ReadBinary(in, level + 1, out);
  while (held) {
    const char *found = nullptr;
    for (const char *mark : kLevels[level].Marks) {
      if (mark != nullptr && in.Is(mark)) { found = mark; }
    }
    if (found == nullptr) { break; }
    ++in.At;
    size_t right = 0;
    held = ReadBinary(in, level + 1, right);
    if (!held) { break; }
    if (!in.Room()) {
      held = false;
      break;
    }
    Program::Node node;
    node.What = Shape::Binary;
    node.Spelling = found;
    node.A = out;
    node.B = right;
    out = in.Make(std::move(node));
  }
  in.Shallower();
  return held;
}

bool ReadExpression(Reading &in, size_t &out) {
  return ReadBinary(in, 0, out);
}

bool ReadSequence(Reading &in, size_t &out) {
  if (!ReadExpression(in, out)) { return false; }
  while (in.Is(",")) {
    ++in.At;
    size_t right = 0;
    if (!ReadExpression(in, right)) { return false; }
    if (!in.Room()) { return false; }
    Program::Node node;
    node.What = Shape::Binary;
    node.Spelling = ",";
    node.A = out;
    node.B = right;
    out = in.Make(std::move(node));
  }
  return true;
}

bool ReadBlock(Reading &in, size_t &out) {
  Program::Node node;
  node.What = Shape::Block;
  if (!in.Want("{")) { return false; }
  while (!in.Is("}") && in.Now().What != Word::End) {
    size_t statement = 0;
    if (!ReadStatement(in, statement)) { return false; }
    node.Parts.push_back(statement);
  }
  if (!in.Want("}")) { return false; }
  if (!in.Room()) { return false; }
  out = in.Make(std::move(node));
  return true;
}

bool ReadStatement(Reading &in, size_t &out) {
  if (!in.Deeper()) { return false; }
  bool held = true;
  if (in.Take(";")) {
    Program::Node node;
    node.What = Shape::Block;
    if (in.Room()) {
      out = in.Make(std::move(node));
    } else {
      held = false;
    }
  } else if (in.Is("{")) {
    held = ReadBlock(in, out);
  } else if (in.IsWord("if")) {
    ++in.At;
    Program::Node node;
    node.What = Shape::If;
    node.C = 0;
    held = in.Want("(") && ReadSequence(in, node.A) && in.Want(")");
    if (held) { held = ReadStatement(in, node.B); }
    if (held && in.IsWord("else")) {
      ++in.At;
      size_t otherwise = 0;
      held = ReadStatement(in, otherwise);

      node.C = held ? otherwise + 1 : 0;
    }
    if (held && in.Room()) {
      out = in.Make(std::move(node));
    } else {
      held = false;
    }
  } else if (in.IsWord("while")) {
    ++in.At;
    Program::Node node;
    node.What = Shape::While;
    held = in.Want("(") && ReadSequence(in, node.A) && in.Want(")") && ReadStatement(in, node.B);
    if (held && in.Room()) {
      out = in.Make(std::move(node));
    } else {
      held = false;
    }
  } else {
    const bool declaring = in.IsWord("var") || in.IsWord("let") || in.IsWord("const");
    if (declaring) { ++in.At; }
    if (declaring) {
      Program::Node list;
      list.What = Shape::Block;
      for (;;) {
        if (in.Now().What != Word::Name) {
          in.Error =
              "the script declares something that is not a name, at " + Where(in.Text, in.Now().At);
          held = false;
          break;
        }
        Program::Node node;
        node.What = Shape::Assign;
        node.Spelling = in.Now().Spelling;
        ++in.At;
        if (in.Take("=")) {
          held = ReadExpression(in, node.A);
        } else {
          Program::Node nothing;
          nothing.What = Shape::Nothing;
          if (!in.Room()) {
            held = false;
            break;
          }
          node.A = in.Make(std::move(nothing));
        }
        if (!held || !in.Room()) {
          held = false;
          break;
        }
        list.Parts.push_back(in.Make(std::move(node)));
        if (!in.Take(",")) { break; }
      }
      if (held && in.Room()) {
        out = in.Make(std::move(list));
      } else {
        held = false;
      }
    } else {
      size_t left = 0;
      held = ReadExpression(in, left);
      if (held && in.Take("=")) {
        size_t right = 0;
        held = ReadExpression(in, right);
        if (held) {
          const Program::Node &target = in.Nodes[left];
          Program::Node node;
          if (target.What == Shape::Name) {
            node.What = Shape::Assign;
            node.Spelling = target.Spelling;
            node.A = right;
          } else if (target.What == Shape::Member) {
            node.What = Shape::AssignMember;
            node.Spelling = target.Spelling;
            node.A = target.A;
            node.B = right;
          } else {
            in.Error = "the script assigns to something that is neither a name nor a member, at " +
                       Where(in.Text, in.Now().At);
            held = false;
          }
          if (held && in.Room()) {
            out = in.Make(std::move(node));
          } else {
            held = false;
          }
        }
      } else if (held) {
        while (held && in.Take(",")) {
          size_t right = 0;
          held = ReadExpression(in, right);
          if (!held || !in.Room()) {
            held = false;
            break;
          }
          Program::Node node;
          node.What = Shape::Binary;
          node.Spelling = ",";
          node.A = left;
          node.B = right;
          left = in.Make(std::move(node));
        }
        out = left;
      }
    }
    if (held) { (void)in.Take(";"); }
  }
  in.Shallower();
  return held;
}

} // namespace

bool Program::Read(std::string_view text, std::string &error) {
  Nodes_.clear();
  Names_.clear();
  Held_.clear();
  Stopped_.clear();
  std::vector<Token> tokens;
  if (!Tokenise(text, tokens, error)) {
    const size_t quoted = error.find('\'');
    if (quoted != std::string::npos && quoted + 1 < error.size()) {
      Stopped_ = error.substr(quoted + 1, 1);
    }
    return false;
  }

  Reading in{.Text = text, .Tokens = tokens, .Nodes = Nodes_, .Error = error, .At = 0, .Depth = 0};
  Node top;
  top.What = Node::Shape::Block;
  while (in.Now().What != Word::End) {
    size_t statement = 0;
    if (!ReadStatement(in, statement)) {
      Stopped_ = in.Now().What == Word::Number ? "a number"
                 : in.Now().What == Word::Text ? "a string"
                 : in.Now().What == Word::End  ? "the end"
                                               : in.Now().Spelling;
      Nodes_.clear();
      return false;
    }
    top.Parts.push_back(statement);
  }
  Nodes_.push_back(std::move(top));
  Root_ = Nodes_.size() - 1;
  return true;
}

Program::Program() = default;
Program::~Program() = default;
Program::Program(Program &&) noexcept = default;
Program &Program::operator=(Program &&) noexcept = default;

namespace {

struct Boundary {
  const char *Name;
  const char *Why;
};

const std::array<Boundary, 85> kBoundaries = {{

    {.Name = "token:function",
     .Why = "a script here is a handler; one that defines callables has a lifetime to bound"},
    {.Name = "token:=>", .Why = "the same, written shorter"},
    {.Name = "token:class",
     .Why = "a type system in a declaration is a program the consumer cannot see the shape of"},
    {.Name = "token:new", .Why = "there is nothing to construct where a script defines no type"},
    {.Name = "token:return", .Why = "a handler runs to its end; there is no frame to return from"},
    {.Name = "token:yield", .Why = "the same, suspended"},
    {.Name = "token:await",
     .Why = "nothing here is asynchronous, and a handler that waited would hold a frame"},
    {.Name = "token:async", .Why = "the same, declared"},

    {.Name = "token:[",
     .Why = "an array is memory a script owns, and the host owns the values here"},
    {.Name = "token:{", .Why = "an object literal is the same, keyed"},
    {.Name = "token::",
     .Why = "an object literal, a label or a conditional -- three grammars behind one mark"},
    {.Name = "token:?", .Why = "a conditional expression is an `if` written where a value goes"},
    {.Name = "token:...", .Why = "spreading is an aggregate under another name"},

    {.Name = "token:for",
     .Why = "a `while` says it, and one loop with one bound is easier to reason about"},
    {.Name = "token:do", .Why = "the same, tested at the end"},
    {.Name = "token:switch", .Why = "an `if` chain says it"},
    {.Name = "token:try",
     .Why = "there are no exceptions here; a refusal ends the run and names itself"},
    {.Name = "token:throw", .Why = "the same, thrown"},
    {.Name = "token:break",
     .Why = "a loop here ends by its condition, which is what makes its bound readable"},
    {.Name = "token:continue", .Why = "the same"},
    {.Name = "token:`",
     .Why = "a template literal is a second string grammar with expressions inside it"},
    {.Name = "token:\\", .Why = "an escaped identifier is a second spelling of a name"},
    {.Name = "token:/",
     .Why = "a regular expression is a second language, and this one has no strings to match"},
    {.Name = "token:typeof",
     .Why = "a type query needs a type system, and values here are four kinds"},
    {.Name = "token:void", .Why = "an operator whose whole job is to discard"},
    {.Name = "token:delete", .Why = "there is nothing to delete where a script owns no aggregate"},
    {.Name = "token:in", .Why = "a membership test over an aggregate"},
    {.Name = "token:instanceof", .Why = "a type test"},
    {.Name = "token:n", .Why = "a BigInt literal is a second number type"},
    {.Name = "token:=",
     .Why = "an assignment where a value was expected -- a destructuring or a default"},

    {.Name = "name:Object", .Why = "the standard library is the host's; this language brings none"},
    {.Name = "name:Array", .Why = "the same"},
    {.Name = "name:Function", .Why = "the same"},
    {.Name = "name:String", .Why = "the same"},
    {.Name = "name:Number", .Why = "the same"},
    {.Name = "name:Boolean", .Why = "the same"},
    {.Name = "name:Symbol", .Why = "the same"},
    {.Name = "name:BigInt", .Why = "the same"},
    {.Name = "name:Math", .Why = "the same"},
    {.Name = "name:JSON", .Why = "the same"},
    {.Name = "name:Date", .Why = "the same"},
    {.Name = "name:RegExp", .Why = "the same"},
    {.Name = "name:Error", .Why = "the same"},
    {.Name = "name:TypeError", .Why = "the same"},
    {.Name = "name:RangeError", .Why = "the same"},
    {.Name = "name:SyntaxError", .Why = "the same"},
    {.Name = "name:ReferenceError", .Why = "the same"},
    {.Name = "name:EvalError", .Why = "the same"},
    {.Name = "name:Reflect", .Why = "the same"},
    {.Name = "name:Proxy", .Why = "the same"},
    {.Name = "name:Promise", .Why = "the same"},
    {.Name = "name:Map", .Why = "the same"},
    {.Name = "name:Set", .Why = "the same"},
    {.Name = "name:WeakMap", .Why = "the same"},
    {.Name = "name:WeakSet", .Why = "the same"},
    {.Name = "name:ArrayBuffer", .Why = "the same"},
    {.Name = "name:DataView", .Why = "the same"},
    {.Name = "name:Int8Array", .Why = "the same"},
    {.Name = "name:Uint8Array", .Why = "the same"},
    {.Name = "name:Float64Array", .Why = "the same"},
    {.Name = "name:globalThis", .Why = "the same"},
    {.Name = "name:Infinity", .Why = "the same"},
    {.Name = "name:NaN", .Why = "the same"},
    {.Name = "name:parseInt", .Why = "the same"},
    {.Name = "name:parseFloat", .Why = "the same"},
    {.Name = "name:isNaN", .Why = "the same"},
    {.Name = "name:this", .Why = "there is no receiver where a script defines no callable"},
    {.Name = "name:arguments", .Why = "the same, and no frame to read them from"},
    {.Name = "name:eval", .Why = "a program that makes a program cannot be bounded by reading it"},
    {.Name = "name:Test262Error",
     .Why = "a corpus's own error type, which the runner provides as a refusal instead"},
    {.Name = "name:$262",
     .Why = "the corpus's own host object, which is a browser's job and not an engine's"},
    {.Name = "name:$DONE", .Why = "the same, for an asynchronous case"},
    {.Name = "name:compareArray",
     .Why = "a harness helper written in the part of the language named outside"},
    {.Name = "name:verifyProperty", .Why = "the same"},
    {.Name = "name:testWithTypedArrayConstructors", .Why = "the same"},

    {.Name = "negative-parse",
     .Why = "this parser refuses a valid program past its subset with the same voice it refuses an "
            "invalid "
            "one, so a case that passed by refusing would be a green light about something else"},
    {.Name = "negative-resolution", .Why = "module resolution, and there are no modules"},
    {.Name = "flags:module", .Why = "a module is a second program shape with its own resolution"},
    {.Name = "flags:async", .Why = "nothing here is asynchronous"},
    {.Name = "flags:onlyStrict", .Why = "one mode, and it is the only one this interpreter has"},
    {.Name = "flags:noStrict", .Why = "the same, from the other side"},
    {.Name = "includes:",
     .Why = "a harness helper written in the part of the language named outside"},
    {.Name = "the script reaches the token bound",
     .Why = "a bound somebody chose, and a case that reaches it is larger than a handler or a "
            "fixture"},
    {.Name = "the script nests past the depth bound", .Why = "the same, in depth"},
    {.Name = "the script reaches the node bound", .Why = "the same, in size"},
}};

} // namespace

[[nodiscard]] static bool EcmaDecimalShaped(std::string_view held) {
  size_t at = 0;
  size_t whole = 0;
  while (at < held.size() && held[at] >= '0' && held[at] <= '9') {
    ++at;
    ++whole;
  }
  size_t fraction = 0;
  if (at < held.size() && held[at] == '.') {
    ++at;
    while (at < held.size() && held[at] >= '0' && held[at] <= '9') {
      ++at;
      ++fraction;
    }
  }
  if (whole == 0 && fraction == 0) { return false; }
  if (at < held.size() && (held[at] == 'e' || held[at] == 'E')) {
    ++at;
    if (at < held.size() && (held[at] == '+' || held[at] == '-')) { ++at; }
    size_t exponent = 0;
    while (at < held.size() && held[at] >= '0' && held[at] <= '9') {
      ++at;
      ++exponent;
    }
    if (exponent == 0) { return false; }
  }
  return at == held.size();
}

[[nodiscard]] static double TextToNumber(const std::string &text) {
  std::string_view held(text);
  while (!held.empty() && (held.front() == ' ' || held.front() == '\t' || held.front() == '\n' ||
                           held.front() == '\r')) {
    held.remove_prefix(1);
  }
  while (!held.empty() && (held.back() == ' ' || held.back() == '\t' || held.back() == '\n' ||
                           held.back() == '\r')) {
    held.remove_suffix(1);
  }
  if (held.empty()) { return 0.0; }
  double sign = 1.0;
  bool wroteSign = false;
  if (held.front() == '+' || held.front() == '-') {
    sign = held.front() == '-' ? -1.0 : 1.0;
    wroteSign = true;
    held.remove_prefix(1);
  }
  if (held == "Infinity") { return sign * HUGE_VAL; }
  if (held.size() > 2 && held[0] == '0' && (held[1] == 'x' || held[1] == 'X')) {
    if (wroteSign) { return std::nan(""); }
    for (size_t at = 2; at < held.size(); ++at) {
      const char c = held[at];
      const bool digit = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
      if (!digit) { return std::nan(""); }
    }
    double value = 0.0;
    const auto hex =
        std::from_chars(held.data() + 2, held.data() + held.size(), value, std::chars_format::hex);
    if (hex.ec == std::errc::result_out_of_range) { return HUGE_VAL; }
    return hex.ec == std::errc() ? value : std::nan("");
  }
  if (!EcmaDecimalShaped(held)) { return std::nan(""); }
  double value = 0.0;
  const auto scanned = std::from_chars(held.data(), held.data() + held.size(), value);
  if (scanned.ec == std::errc::result_out_of_range) {
    return sign * (DecimalEdge(held) == Edge::Zero ? 0.0 : HUGE_VAL);
  }
  return scanned.ec == std::errc() ? sign * value : std::nan("");
}

const char *WhyOutside(std::string_view name) {
  for (const Boundary &boundary : kBoundaries) {
    if (name.starts_with(boundary.Name)) { return boundary.Why; }
  }
  return nullptr;
}

void Program::Reset() {
  Names_.clear();
  Held_.clear();
}

bool Program::Held() const {
  return !Nodes_.empty();
}

size_t Program::NodeCount() const {
  return Nodes_.size();
}

const Value *Program::Named(std::string_view name) const {
  for (size_t at = 0; at < Names_.size(); ++at) {
    if (Names_[at] == name) { return &Held_[at]; }
  }
  return nullptr;
}

bool Program::Evaluate(size_t at, Host &host, Value &out, std::string &error) {
  if (++Steps_ > kMaxSteps) {
    error = "the script reaches the step bound of " + std::to_string(kMaxSteps);
    return false;
  }
  const Node &node = Nodes_[at];
  switch (node.What) {
    case Node::Shape::Number: out = Value::OfNumber(node.Number); return true;
    case Node::Shape::Text: out = Value::OfText(node.Spelling); return true;
    case Node::Shape::Nothing: out = Value(); return true;
    case Node::Shape::Name: {
      const Value *held = Named(node.Spelling);

      out = held != nullptr ? *held : host.Global(node.Spelling);
      return true;
    }
    case Node::Shape::Member: {
      Value object;
      if (!Evaluate(node.A, host, object, error)) { return false; }
      out = host.Member(object, node.Spelling);
      return true;
    }
    case Node::Shape::Call: {
      Value callee;
      if (!Evaluate(node.A, host, callee, error)) { return false; }
      std::array<Value, kMaxArgs> args{};
      for (size_t i = 0; i < node.Parts.size() && i < kMaxArgs; ++i) {
        if (!Evaluate(node.Parts[i], host, args[i], error)) { return false; }
      }
      if (!host.Call(callee, std::span(args).first(std::min(node.Parts.size(), kMaxArgs)), out)) {
        error = "the host does not answer this call, and a call that quietly did nothing is the "
                "defect a refusal here prevents";
        return false;
      }
      return true;
    }
    case Node::Shape::Step: {
      const Value *held = Named(node.Spelling);
      const double was = held != nullptr ? held->Number : host.Global(node.Spelling).Number;
      const Value now = Value::OfNumber(was + node.Number);
      bool set = false;
      for (size_t i = 0; i < Names_.size(); ++i) {
        if (Names_[i] == node.Spelling) {
          Held_[i] = now;
          set = true;
        }
      }
      if (!set) {
        if (Names_.size() >= kMaxNames) {
          error = "the script reaches the name bound of " + std::to_string(kMaxNames);
          return false;
        }
        Names_.push_back(node.Spelling);
        Held_.push_back(now);
      }
      out = node.A == 0 ? now : Value::OfNumber(was);
      return true;
    }
    case Node::Shape::Unary: {
      Value inner;
      if (!Evaluate(node.A, host, inner, error)) { return false; }

      out =
          node.Spelling == "!" ? Value::OfNumber(inner.Truth() ? 0.0 : 1.0)
          : node.Spelling == "+"
              ? Value::OfNumber(inner.What == Kind::Text ? TextToNumber(inner.Text) : inner.Number)
              : Value::OfNumber(-inner.Number);
      return true;
    }
    case Node::Shape::Binary: {
      Value left;
      if (!Evaluate(node.A, host, left, error)) { return false; }

      if (node.Spelling == ",") { return Evaluate(node.B, host, out, error); }
      if (node.Spelling == "&&") {
        if (!left.Truth()) {
          out = left;
          return true;
        }
        return Evaluate(node.B, host, out, error);
      }
      if (node.Spelling == "||") {
        if (left.Truth()) {
          out = left;
          return true;
        }
        return Evaluate(node.B, host, out, error);
      }
      Value right;
      if (!Evaluate(node.B, host, right, error)) { return false; }

      if (node.Spelling == "+" && (left.What == Kind::Text || right.What == Kind::Text)) {
        out = Value::OfText(left.AsText() + right.AsText());
        return true;
      }
      if (node.Spelling == "==" || node.Spelling == "!=") {
        const bool same = left.What == Kind::Text || right.What == Kind::Text
                              ? left.AsText() == right.AsText()
                              : (left.What == Kind::Ref || right.What == Kind::Ref
                                     ? left.What == right.What && left.Ref == right.Ref
                                     : left.Number == right.Number);
        out = Value::OfNumber((node.Spelling == "==") == same ? 1.0 : 0.0);
        return true;
      }
      const double a = left.Number;
      const double b = right.Number;
      double answer = 0.0;
      if (node.Spelling == "+") {
        answer = a + b;
      } else if (node.Spelling == "-") {
        answer = a - b;
      } else if (node.Spelling == "*") {
        answer = a * b;
      } else if (node.Spelling == "/") {
        answer = b == 0.0 ? 0.0 : a / b;
      } else if (node.Spelling == "%") {
        answer = b == 0.0 ? 0.0 : std::fmod(a, b);
      } else if (node.Spelling == "<") {
        answer = a < b ? 1.0 : 0.0;
      } else if (node.Spelling == "<=") {
        answer = a <= b ? 1.0 : 0.0;
      } else if (node.Spelling == ">") {
        answer = a > b ? 1.0 : 0.0;
      } else {
        answer = a >= b ? 1.0 : 0.0;
      }
      out = Value::OfNumber(answer);
      return true;
    }
    default: break;
  }
  return Perform(at, host, error);
}

bool Program::Perform(size_t at, Host &host, std::string &error) {
  if (++Steps_ > kMaxSteps) {
    error = "the script reaches the step bound of " + std::to_string(kMaxSteps);
    return false;
  }
  const Node &node = Nodes_[at];
  switch (node.What) {
    case Node::Shape::Block: {
      for (unsigned long Part : Nodes_[at].Parts) {
        if (!Perform(Part, host, error)) { return false; }
      }
      return true;
    }
    case Node::Shape::Assign: {
      Value held;
      if (!Evaluate(node.A, host, held, error)) { return false; }
      for (size_t i = 0; i < Names_.size(); ++i) {
        if (Names_[i] == node.Spelling) {
          Held_[i] = std::move(held);
          return true;
        }
      }
      if (Names_.size() >= kMaxNames) {
        error = "the script reaches the name bound of " + std::to_string(kMaxNames);
        return false;
      }
      Names_.push_back(node.Spelling);
      Held_.push_back(std::move(held));
      return true;
    }
    case Node::Shape::AssignMember: {
      Value object;
      Value held;
      if (!Evaluate(node.A, host, object, error)) { return false; }
      if (!Evaluate(node.B, host, held, error)) { return false; }
      if (!host.SetMember(object, node.Spelling, held)) {
        error = "the host does not take '" + node.Spelling +
                "', so the script wrote where nothing "
                "is listening";
        return false;
      }
      return true;
    }
    case Node::Shape::If: {
      Value condition;
      if (!Evaluate(node.A, host, condition, error)) { return false; }
      if (condition.Truth()) { return Perform(node.B, host, error); }
      return node.C == 0 ? true : Perform(node.C - 1, host, error);
    }
    case Node::Shape::While: {
      const size_t condition = node.A;
      const size_t body = node.B;
      for (;;) {
        Value held;
        if (!Evaluate(condition, host, held, error)) { return false; }
        if (!held.Truth()) { return true; }
        if (!Perform(body, host, error)) { return false; }
      }
    }
    default: break;
  }
  Value discarded;
  return Evaluate(at, host, discarded, error);
}

bool Program::Run(Host &host, std::string &error) {
  if (!Held()) {
    error = "there is no script to run, which is not the same as a script that did nothing";
    return false;
  }
  Steps_ = 0;
  return Perform(Root_, host, error);
}

} // namespace outshine::Script
