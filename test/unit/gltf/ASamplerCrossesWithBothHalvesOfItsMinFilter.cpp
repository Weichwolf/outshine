#include <cstdint>
#include <string>
#include <vector>

#include "Check.h"
#include "Document.h"
#include "Span.h"
#include "Types.h"

namespace {

using outshine::Gltf::Document;
using outshine::Gltf::Filter;
using outshine::Gltf::MipFilter;

bool ReadsASampler(const std::string &fields, outshine::Gltf::Document &into) {
  const std::string text = "{\"asset\":{\"version\":\"2.0\"},\"samplers\":[{" + fields + "}]}";
  return into.Read(outshine::Span<const uint8_t>((const uint8_t *)text.data(), text.size()), "");
}

struct Row {
  int Raw;
  Filter Base;
  MipFilter Mip;
  const char *Spelling;
};

}

int main() {
  using namespace outshine::Test;

  const Row rows[] = {
      {9728, Filter::Nearest, MipFilter::None, "NEAREST"},
      {9729, Filter::Linear, MipFilter::None, "LINEAR"},
      {9984, Filter::Nearest, MipFilter::Nearest, "NEAREST_MIPMAP_NEAREST"},
      {9985, Filter::Linear, MipFilter::Nearest, "LINEAR_MIPMAP_NEAREST"},
      {9986, Filter::Nearest, MipFilter::Linear, "NEAREST_MIPMAP_LINEAR"},
      {9987, Filter::Linear, MipFilter::Linear, "LINEAR_MIPMAP_LINEAR"},
  };
  for (const Row &row : rows) {
    Document document;
    const bool read = ReadsASampler("\"minFilter\":" + std::to_string(row.Raw), document);
    CHECK(read, (std::string("a sampler declaring minFilter ") + row.Spelling + " is read").c_str());
    if (!read || document.Samplers().empty()) { continue; }
    const outshine::Gltf::Sampler &sampler = document.Samplers()[0];
    CHECK(sampler.Min == row.Base,
          (std::string("minFilter ") + row.Spelling +
           " has the base filter its NAME says, and the two values that are a nearest base WITH "
           "mipmapping are the ones the old rule collapsed").c_str());
    CHECK(sampler.Mip == row.Mip,
          (std::string("minFilter ") + row.Spelling +
           " carries its level filter too, so the half the sampler used to decide by a constant now "
           "comes from the file").c_str());
  }

  for (const int raw : {9728, 9729}) {
    Document document;
    CHECK(ReadsASampler("\"magFilter\":" + std::to_string(raw), document),
          "the two magFilter values glTF defines are read");
  }
  for (const int raw : {9984, 9985, 9986, 9987, 1, 0}) {
    Document document;
    const bool read = ReadsASampler("\"magFilter\":" + std::to_string(raw), document);
    CHECK(!read, "a magFilter outside glTF's two legal values is REFUSED BY NAME rather than mapped to "
                 "a plausible one -- a mip mode has no meaning at magnification, and a file asking for "
                 "one is asking for something this format does not define");
  }

  for (const int raw : {9730, 9983, 9988, 0}) {
    Document document;
    CHECK(!ReadsASampler("\"minFilter\":" + std::to_string(raw), document),
          "a minFilter glTF 2.0 does not define is refused by name, like a wrap mode");
  }

  {
    Document document;
    CHECK(ReadsASampler("", document), "a sampler declaring no filter at all is read");
    if (!document.Samplers().empty()) {
      const outshine::Gltf::Sampler &sampler = document.Samplers()[0];
      CHECK(sampler.Mag == Filter::Linear && sampler.Min == Filter::Linear &&
                sampler.Mip == MipFilter::Linear,
            "an undeclared filter reads as linear throughout, which is what the sampler did before this "
            "field existed, so absence changes no picture");
    }
  }

  Covers("glTF's minFilter names the filter within a level AND the filter between levels, "
         "and both halves cross the reader: 9984 and 9986 are a NEAREST base, which the previous rule "
         "collapsed to LINEAR for two corpus subjects");
  return Report();
}
