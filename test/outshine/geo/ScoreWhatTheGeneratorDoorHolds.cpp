#include <cstdio>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "Check.h"
#include "Document.h"
#include "Generate.h"
#include "Geometry.h"
#include "Material.h"
#include "Structures.h"

// THE GENERATOR LIBRARY HAS ITS OWN DOOR, AND THIS PROGRAM STANDS BEHIND IT WITHOUT THE ENGINE.
//
// Unreal's PCG is a plugin with its own registry, outside the engine module; RAGE has no
// equivalent. Taking Unreal, because a generator library that links without the engine is the
// only shape that lets another project take it -- and part 3 of board:1948's target is exactly
// that: another project uses the generators alone.
//
// MEASURED BEFORE THIS: the registry was a `std::vector<const Generates *>` inside
// `Engine::State`, reached only through `Engine::Offers`. So a foreign program could implement
// the `Generates` interface and had nowhere to register it -- the door published the interface
// and kept the thing that resolves it. `Makers` is that registry, declared in `include/Generate.h`
// beside the interface it serves and implemented in the generator tier; the engine holds one and
// delegates to it rather than keeping a second.
//
// THIS SUITE LINKS `src/generators` AND NOTHING OF `src/engine`, `src/render`, `src/scenario` OR
// `src/sim` -- the group list in `test/run.sh` says so. That this case compiles and links here is
// half the claim; the numbers are the other half.

namespace {

class Slab final : public outshine::Generates {
public:
  [[nodiscard]] std::string_view Kind() const override { return "slab"; }

  [[nodiscard]] bool Make(const outshine::Ask &ask, outshine::Geometry &into) const override {
    const int surface = into.Surface("slab", outshine::Material{});
    if (surface < 0) { return false; }
    const int part = into.Part("slab", surface);
    if (part < 0) { return false; }
    const float half = (float)(ask.ExtentM > 0.0 ? 0.5 * ask.ExtentM : 0.5);
    const float places[12] = {-half, 0.0f, -half, half, 0.0f, -half,
                              half,  0.0f, half,  -half, 0.0f, half};
    const uint32_t triangles[6] = {0, 1, 2, 0, 2, 3};
    return into.Positions(part, std::span<const float>(places, 12)) &&
           into.Triangles(part, std::span<const uint32_t>(triangles, 6));
  }
};

}

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  outshine::Makers offering;
  const outshine::Generators::Structures shipped;
  const Slab mine;

  const bool tookShipped = offering.Offers(shipped);
  const bool tookMine = offering.Offers(mine);
  const bool refusedTwice = !offering.Offers(shipped);

  std::printf("THE DOOR'S REGISTRY HOLDS  %zu maker(s): shipped %s, mine %s, a repeat %s\n",
              offering.Count(), tookShipped ? "in" : "REFUSED", tookMine ? "in" : "REFUSED",
              refusedTwice ? "refused" : "TAKEN AGAIN");

  CHECK(tookShipped && tookMine && refusedTwice && offering.Count() == 2,
        "**A CLIENT'S GENERATOR ENTERS THE SAME REGISTRY AS A SHIPPED ONE, AND A KIND IS "
        "ISSUED ONCE**: Unreal enumerates its built-in factories and registers plugin factories "
        "into the same table. A registry that took a kind twice would resolve a declaration by "
        "whichever entry it reached first, which is a coin toss wearing a lookup");

  size_t shippedFound = 0;
  for (size_t at = 0; at < (size_t)outshine::Ships::kCount; ++at) {
    if (offering.Named(outshine::kShipped[at]) != nullptr) { ++shippedFound; }
  }
  std::printf("AND THE SHIPPED CATALOGUE NAMES %zu kind(s), all %zu of them registered\n",
              (size_t)outshine::Ships::kCount, shippedFound);

  // THE SHIPPED CATALOGUE IS CLOSED AGAINST A TYPO, and the compiler is what closes it.
  // `Ships` enumerates what outshine ships and `kShipped` spells each one; a `static_assert` pairs
  // the two counts and a second refuses a blank or a repeated name. So a shipped kind is not a
  // string a caller can mistype -- `Structures::Kind()` returns `NameOf(Ships::Structures)` and
  // nothing else may. A CLIENT's generator is the other half and is deliberately unlike it: it
  // enters as a VALUE, and its kind is whatever it says, because the catalogue cannot enumerate
  // what it has never seen. That is Unreal's own split -- built-in factories enumerated, plugin
  // factories registered.
  CHECK(shippedFound == (size_t)outshine::Ships::kCount,
        "**EVERY KIND THE CATALOGUE ENUMERATES IS ACTUALLY REGISTERED**: a catalogue that names a "
        "generator the registry does not hold is a declaration surface with nothing behind it, "
        "which is this tree's commonest defect and the one this whole item is about");

  const outshine::Generates *const found = offering.Named("slab");
  const outshine::Generates *const missing = offering.Named("nothing-offers-this");
  std::printf("IT RESOLVES 'slab' %s and an unknown kind %s\n",
              found ? "to a maker" : "to NOTHING", missing ? "TO A MAKER" : "to nothing");

  CHECK(found != nullptr && missing == nullptr,
        "and it resolves BY KIND, so a declaration naming a generator is answered by the door "
        "rather than by the engine -- which is what lets a foreign program use this tier at all");

  outshine::Ask ask;
  ask.ExtentM = 4.0;
  outshine::Geometry made;
  const bool stood = found->Make(ask, made);
  std::printf("AND THE MAKER FILLS THE DOOR'S VALUE: %s, %d part(s)\n", stood ? "yes" : "no",
              stood ? made.Parts() : -1);

  CHECK(stood && made.Parts() == 1,
        "**AND WHAT COMES BACK IS THE DOOR'S OWN VALUE**: a generator hands back a `Geometry` and "
        "never a file, which is CLAUDE.md's *universal interface for 3D with outshine* -- a "
        "reader fills one, a generator fills one, a foreign program fills one with no file "
        "anywhere");

  std::vector<uint8_t> glb;
  std::string why;
  const bool wrote = outshine::WriteGlb(made, glb, why);
  std::printf("AND IT SERIALISES TO %zu GLB byte(s)%s\n", glb.size(),
              wrote ? "" : (" -- refused: " + why).c_str());

  // A SERIALISER SHIPS BESIDE THE LIBRARY, and it was already written. `Gltf::Emit` is a complete
  // GLB writer and had ZERO callers; `Gltf::Subject::Assemble(const Geometry &)` is the bridge
  // from the door's value to what it emits, and it had none either. So both halves stood in the
  // tree with no wire between them -- the sixth capability of that shape this session found.
  // Nothing on the streaming path calls this: the compositor takes the representation, and the
  // serialiser is for a caller who wants a FILE.
  outshine::Gltf::Document read;
  const bool accepted =
      wrote && read.Read(outshine::Span<const uint8_t>(glb.data(), glb.size()), "generated.glb");
  std::printf("AND THE TREE'S OWN READER %s it: %d mesh(es)\n",
              accepted ? "ACCEPTS" : "refuses", accepted ? (int)read.Meshes().size() : -1);

  CHECK(wrote && accepted && !read.Meshes().empty(),
        "**A CALLER WHO WANTS A FILE GETS ONE, AND IT IS A DOCUMENT THIS TREE READS**: Unreal's "
        "PCG can bake its output to an asset and RAGE's tool chain writes its own; a generator "
        "library whose output can only be handed to one engine is a library that project cannot "
        "take. Written by `Gltf::Emit` and read back by `Gltf::Document`, so the two halves of "
        "the format agree with each other rather than only with themselves");

  Covers("the generator tier's door: its registry is declared beside its interface, a client's "
         "maker and a shipped one enter the same table, a kind is issued once, and the value that "
         "comes back is the public `Geometry` -- all of it without linking the engine");
  return Report();
}
