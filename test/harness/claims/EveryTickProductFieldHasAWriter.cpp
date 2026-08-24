#include <cctype>
#include <cstdio>
#include <string>
#include <vector>

#include "Check.h"

namespace {

bool Slurp(const char *path, std::string &into) {
  std::FILE *file = std::fopen(path, "rb");
  if (file == nullptr) { return false; }
  char block[1 << 16];
  for (size_t read = std::fread(block, 1, sizeof block, file); read > 0;
       read = std::fread(block, 1, sizeof block, file)) {
    into.append(block, read);
  }
  std::fclose(file);
  return true;
}

// the fields of one struct block: the identifier before '=', '[' or ';' on each member line
std::vector<std::string> FieldsOf(const std::string &header, const std::string &name,
                                  std::string &error) {
  std::vector<std::string> fields;
  const size_t open = header.find("struct " + name + " {");
  if (open == std::string::npos) {
    error = "struct " + name + " is not in the header";
    return fields;
  }
  const size_t close = header.find("\n};", open);
  const std::string body = header.substr(open, close - open);
  for (size_t at = body.find('\n'); at != std::string::npos; at = body.find('\n', at + 1)) {
    size_t end = body.find('\n', at + 1);
    if (end == std::string::npos) { end = body.size(); }
    std::string line = body.substr(at + 1, end - at - 1);
    const size_t stop = line.find_first_of("=[;");
    if (stop == std::string::npos || line.find("(") != std::string::npos) { continue; }
    // a `static constexpr` inside a product is a CONSTANT, not a field a tick fills: it has no
    // writer by construction and it cannot be a silent zero wearing a meaning, because the
    // meaning is the value. Skipping it is not a relaxation -- it is the difference between a
    // product's fields and the numbers the product is measured in.
    if (line.find("static ") != std::string::npos) { continue; }
    size_t last = line.find_last_not_of(" \t", stop == 0 ? 0 : stop - 1);
    if (last == std::string::npos) { continue; }
    size_t first = last;
    while (first > 0 && (std::isalnum((unsigned char)line[first - 1]) || line[first - 1] == '_')) {
      --first;
    }
    if (!std::isalpha((unsigned char)line[first])) { continue; }
    fields.push_back(line.substr(first, last - first + 1));
  }
  return fields;
}

bool Written(const std::string &source, const std::string &product, const std::string &field) {
  const std::string touch = product + "." + field;
  for (size_t at = source.find(touch); at != std::string::npos;
       at = source.find(touch, at + 1)) {
    if (at >= 2 && source.compare(at - 2, 2, "++") == 0) { return true; }
    size_t after = at + touch.size();
    if (after < source.size() &&
        (std::isalnum((unsigned char)source[after]) || source[after] == '_')) {
      continue;
    }
    while (after < source.size() && (source[after] == ' ' || source[after] == '[')) {
      if (source[after] == '[') { after = source.find(']', after) + 1; continue; }
      ++after;
    }
    if (after + 1 < source.size() && source.compare(after, 2, "++") == 0) { return true; }
    if (after < source.size() && source[after] == '=' && source[after + 1] != '=') {
      return true;
    }
    if (after + 1 < source.size() && source[after + 1] == '=' &&
        (source[after] == '+' || source[after] == '-' || source[after] == '|')) {
      return true;
    }
  }
  return false;
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  // 1613 closed this class once and 1703 saw it return: a product struct whose field has no
  // writer hands every consumer a silent zero wearing a meaning. The tick's public products
  // therefore prove their writers HERE, so the third return goes red the day it lands.
  struct Product {
    const char *Header;
    const char *Source;
    const char *Name;
    const char *Held; // the variable the source writes it through
  };
  const Product products[] = {
      {"src/sim/DriveTick.h", "src/sim/DriveTick.cpp", "Ridden", "out"},
  };

  for (const Product &product : products) {
    std::string header, source, error;
    CHECK(Slurp(product.Header, header) && Slurp(product.Source, source),
          "both sides of the seam read");
    const std::vector<std::string> fields = FieldsOf(header, product.Name, error);
    CHECK(!fields.empty(), ("the struct parses: " + error).c_str());
    for (const std::string &field : fields) {
      CHECK(Written(source, product.Held, field),
            (std::string(product.Name) + "::" + field +
             " has a writer in " + product.Source +
             " -- **A PRODUCT FIELD NOBODY WRITES IS A SILENT ZERO WEARING A MEANING** "
             "(board:1613, 1703)")
                .c_str());
    }
  }

  Covers("IV.7 every field of the tick's public product has a writer in the tick -- the "
         "writerless-mirror-field class (1613, 1703) is a claims gate now, not a review "
         "catch");
  return Report();
}
