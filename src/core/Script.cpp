#include "Script.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace outshine::Script {

bool Value::Truth(void) const {
  switch (What) {
    case Kind::Nothing: return false;
    case Kind::Number: return Number != 0.0;
    case Kind::Text: return !Text.empty();
    case Kind::Ref: return true;
  }
  return false;
}

std::string Value::AsText(void) const {
  switch (What) {
    case Kind::Nothing: return "";
    case Kind::Text: return Text;
    case Kind::Ref: return "";
    case Kind::Number: break;
  }
  /* AN INTEGRAL NUMBER IS WRITTEN WITHOUT A POINT, because `element.style.height = 100 + "px"` must
   * produce `100px` and not `100.000000px` -- and a host receiving the second would set a length
   * nobody declared. */
  char held[32];
  if (Number == std::floor(Number) && std::fabs(Number) < 1e15) {
    std::snprintf(held, sizeof held, "%lld", (long long)Number);
  } else {
    std::snprintf(held, sizeof held, "%g", Number);
  }
  return held;
}

namespace {

enum class Word : uint8_t { End, Number, Text, Name, Mark };

struct Token {
  Word What = Word::End;
  std::string Spelling;
  double Number = 0.0;
  size_t At = 0;
};

bool Space(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
bool Starts(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '$'; }
bool Continues(char c) { return Starts(c) || (c >= '0' && c <= '9'); }

/* THE MARKS, LONGEST FIRST, so `==` is never read as two `=`. */
constexpr const char *kMarks[] = {"==", "!=", "<=", ">=", "&&", "||", "(", ")", "{", "}",
                                  "[",  "]",  ";",  ",",  ".",  "+",  "-", "*", "/", "%",
                                  "<",  ">",  "=",  "!"};

std::string Where(const std::string &text, size_t at) {
  size_t line = 1, column = 1;
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

bool Tokenise(const std::string &text, std::vector<Token> &out, std::string &error) {
  size_t at = 0;
  while (at < text.size()) {
    if (Space(text[at])) {
      ++at;
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
          token.Spelling.push_back(escaped == 'n'    ? '\n'
                                   : escaped == 't'  ? '\t'
                                                     : escaped);
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
      char *stopped = nullptr;
      token.What = Word::Number;
      token.Number = std::strtod(text.c_str() + at, &stopped);
      at = (size_t)(stopped - text.c_str());
      out.push_back(std::move(token));
      continue;
    }
    if (Starts(text[at])) {
      token.What = Word::Name;
      while (at < text.size() && Continues(text[at])) { token.Spelling.push_back(text[at++]); }
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

/* ONE NODE OF THE TREE, and every edge is an index rather than a pointer: the vector may grow while a
 * parse is in flight, and a pointer into it would dangle exactly then. */
struct Program::Node {
  enum class Shape : uint8_t {
    Number, Text, Name, Member, Call, Unary, Binary, Assign, AssignMember, If, While, Block
  };
  Shape What = Shape::Number;
  double Number = 0.0;
  std::string Spelling;   /* a literal's text, a name, a member's name, or an operator */
  size_t A = 0, B = 0, C = 0;
  std::vector<size_t> Parts;
};

namespace {

/* THE PARSER'S OWN STATE, so the recursion carries one argument and the bounds live in one place. */
struct Reading {
  const std::string &Text;
  const std::vector<Token> &Tokens;
  std::vector<Program::Node> &Nodes;
  std::string &Error;
  size_t At = 0;
  size_t Depth = 0;

  [[nodiscard]] const Token &Now(void) const { return Tokens[At]; }
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
  [[nodiscard]] bool Room(void) {
    if (Nodes.size() < kMaxNodes) { return true; }
    Error = "the script reaches the node bound of " + std::to_string(kMaxNodes);
    return false;
  }
  [[nodiscard]] bool Deeper(void) {
    if (++Depth <= kMaxDepth) { return true; }
    Error = "the script nests past the depth bound of " + std::to_string(kMaxDepth) + " at " +
            Where(Text, Now().At);
    return false;
  }
  void Shallower(void) { --Depth; }
  size_t Make(Program::Node node) {
    Nodes.push_back(std::move(node));
    return Nodes.size() - 1;
  }
};

using Shape = Program::Node::Shape;

bool ReadExpression(Reading &in, size_t &out);
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
    node.What = Shape::Name;
    node.Spelling = token.Spelling;
    out = in.Make(std::move(node));
  } else if (in.Take("(")) {
    if (!ReadExpression(in, out)) { return false; }
    if (!in.Want(")")) { return false; }
  } else if (in.Is("-") || in.Is("!")) {
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

  /* MEMBERS AND CALLS CHAIN, so `document.getElementById("a").style` is one expression and not three
   * statements a consumer has to write. */
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

/* PRECEDENCE AS A TABLE AND NOT AS A LADDER OF FUNCTIONS: one row per level, read left to right, so
 * adding an operator is adding a row rather than a function nobody notices is missing. */
struct Level {
  const char *Marks[4];
};
constexpr Level kLevels[] = {
    {{"||", nullptr, nullptr, nullptr}},
    {{"&&", nullptr, nullptr, nullptr}},
    {{"==", "!=", nullptr, nullptr}},
    {{"<", "<=", ">", ">="}},
    {{"+", "-", nullptr, nullptr}},
    {{"*", "/", "%", nullptr}},
};
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

bool ReadExpression(Reading &in, size_t &out) { return ReadBinary(in, 0, out); }

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
  if (in.Is("{")) {
    held = ReadBlock(in, out);
  } else if (in.IsWord("if")) {
    ++in.At;
    Program::Node node;
    node.What = Shape::If;
    node.C = 0;
    held = in.Want("(") && ReadExpression(in, node.A) && in.Want(")");
    if (held) { held = ReadStatement(in, node.B); }
    if (held && in.IsWord("else")) {
      ++in.At;
      size_t otherwise = 0;
      held = ReadStatement(in, otherwise);
      /* ZERO IS A LEGAL NODE INDEX, so the `else` is carried as its index PLUS ONE and zero means
       * *there was none*. A sentinel that collides with a real value is the defect this avoids. */
      node.C = held ? otherwise + 1 : 0;
    }
    if (held && in.Room()) { out = in.Make(std::move(node)); } else { held = false; }
  } else if (in.IsWord("while")) {
    ++in.At;
    Program::Node node;
    node.What = Shape::While;
    held = in.Want("(") && ReadExpression(in, node.A) && in.Want(")") && ReadStatement(in, node.B);
    if (held && in.Room()) { out = in.Make(std::move(node)); } else { held = false; }
  } else {
    /* `var`, `let` AND `const` ARE READ AND DROPPED. A script here assigns a name and the name
     * exists; the three keywords carry a scoping rule this subset does not have, so accepting them
     * with their meaning would be a lie and refusing them would reject half the scripts anyone
     * writes. They are noise, and saying that here is the honest middle. */
    if (in.IsWord("var") || in.IsWord("let") || in.IsWord("const")) { ++in.At; }
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
        if (held && in.Room()) { out = in.Make(std::move(node)); } else { held = false; }
      }
    } else if (held) {
      out = left;
    }
    if (held) { (void)in.Take(";"); }
  }
  in.Shallower();
  return held;
}

} // namespace

bool Program::Read(const std::string &text, std::string &error) {
  Nodes_.clear();
  Names_.clear();
  Held_.clear();
  std::vector<Token> tokens;
  if (!Tokenise(text, tokens, error)) { return false; }

  Reading in{text, tokens, Nodes_, error, 0, 0};
  Node top;
  top.What = Node::Shape::Block;
  while (in.Now().What != Word::End) {
    size_t statement = 0;
    if (!ReadStatement(in, statement)) {
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

bool Program::Held(void) const { return !Nodes_.empty(); }
size_t Program::NodeCount(void) const { return Nodes_.size(); }

const Value *Program::Named(const std::string &name) const {
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
    case Node::Shape::Number:
      out = Value::OfNumber(node.Number);
      return true;
    case Node::Shape::Text:
      out = Value::OfText(node.Spelling);
      return true;
    case Node::Shape::Name: {
      const Value *held = Named(node.Spelling);
      /* A NAME THE SCRIPT ASSIGNED IS THE SCRIPT'S; ANY OTHER IS THE HOST'S. That order is what lets a
       * consumer expose `document` and a script still name a variable `document` without the two
       * fighting -- the script's own assignment wins, which is what every language does. */
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
      Value args[kMaxArgs];
      for (size_t i = 0; i < node.Parts.size() && i < kMaxArgs; ++i) {
        if (!Evaluate(node.Parts[i], host, args[i], error)) { return false; }
      }
      if (!host.Call(callee, args, node.Parts.size(), out)) {
        error = "the host does not answer this call, and a call that quietly did nothing is the "
                "defect a refusal here prevents";
        return false;
      }
      return true;
    }
    case Node::Shape::Unary: {
      Value inner;
      if (!Evaluate(node.A, host, inner, error)) { return false; }
      out = node.Spelling == "!" ? Value::OfNumber(inner.Truth() ? 0.0 : 1.0)
                                 : Value::OfNumber(-inner.Number);
      return true;
    }
    case Node::Shape::Binary: {
      Value left;
      if (!Evaluate(node.A, host, left, error)) { return false; }
      /* `&&` AND `||` DO NOT EVALUATE THEIR RIGHT SIDE UNLESS THEY HAVE TO, which is not an
       * optimisation: `a && a.b` is how a script guards a member read, and evaluating both would make
       * the guard the thing that fails. */
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
      /* `+` IS THE ONE OPERATOR THAT IS TWO OPERATIONS, and text wins when either side is text --
       * which is what makes `100 + "px"` the string a length is written as. */
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
      const double a = left.Number, b = right.Number;
      double answer = 0.0;
      if (node.Spelling == "+") { answer = a + b; }
      else if (node.Spelling == "-") { answer = a - b; }
      else if (node.Spelling == "*") { answer = a * b; }
      else if (node.Spelling == "/") { answer = b == 0.0 ? 0.0 : a / b; }
      else if (node.Spelling == "%") { answer = b == 0.0 ? 0.0 : std::fmod(a, b); }
      else if (node.Spelling == "<") { answer = a < b ? 1.0 : 0.0; }
      else if (node.Spelling == "<=") { answer = a <= b ? 1.0 : 0.0; }
      else if (node.Spelling == ">") { answer = a > b ? 1.0 : 0.0; }
      else { answer = a >= b ? 1.0 : 0.0; }
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
      /* THE PARTS ARE COPIED BEFORE THE WALK because a nested evaluation may grow nothing here, but a
       * reference into the node vector is the kind of thing that outlives its guarantee. */
      const std::vector<size_t> parts = node.Parts;
      for (const size_t part : parts) {
        if (!Perform(part, host, error)) { return false; }
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
      Value object, held;
      if (!Evaluate(node.A, host, object, error)) { return false; }
      if (!Evaluate(node.B, host, held, error)) { return false; }
      if (!host.SetMember(object, node.Spelling, held)) {
        error = "the host does not take '" + node.Spelling + "', so the script wrote where nothing "
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
      const size_t condition = node.A, body = node.B;
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
  Names_.clear();
  Held_.clear();
  return Perform(Root_, host, error);
}

} // namespace outshine::Script
