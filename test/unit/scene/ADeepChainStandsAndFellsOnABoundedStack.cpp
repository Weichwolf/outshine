#include <cstdio>
#include <pthread.h>

#include "Check.h"

#include "Store.h"

using outshine::Entity;
using outshine::Relation;
using outshine::Role;
using outshine::Store;

namespace {

// [SET] 512 KiB, an eighth of the main thread's stack -- the same rig 1712 proved Fit on;
// a per-link recursion pays ~one frame per ChildOf link and dies thousands of links in
constexpr size_t kThreadStack = 512 * 1024;
constexpr size_t kLinks = 16384;

struct Rig {
  Store Scene;
  bool Stood = false;
  bool Felled = false;
  bool Raised = false;
};

void *Run(void *held) {
  Rig *rig = (Rig *)held;
  Store &scene = rig->Scene;
  if (!scene.Open(2 * kLinks + 8)) { return nullptr; }

  // a train: every carriage is ChildOf the one before it, and ChildOf is owned by its
  // target, so felling the engine fells the whole chain
  const Entity engine = scene.Add(Role::Body);
  Entity behind = engine;
  for (size_t link = 1; link < kLinks; ++link) {
    const Entity carriage = scene.Add(Role::Body);
    if (!scene.Link(carriage, Relation::ChildOf, behind)) { return nullptr; }
    behind = carriage;
  }
  rig->Stood = true;

  scene.Remove(engine);
  rig->Felled = !scene.Alive(engine) && !scene.Alive(behind);

  // and the same depth stands UP: a prefab chain instantiates without one frame per link
  const Entity prefab = scene.Add(Role::Body);
  Entity under = prefab;
  for (size_t link = 1; link < kLinks / 2; ++link) {
    const Entity part = scene.Add(Role::Body);
    if (!scene.Link(part, Relation::ChildOf, under)) { return nullptr; }
    under = part;
  }
  const Entity instance = scene.Instantiate(prefab);
  rig->Raised = scene.Alive(instance);
  return nullptr;
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  static Rig rig;
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setstacksize(&attr, kThreadStack);
  pthread_t worker;
  CHECK(pthread_create(&worker, &attr, Run, &rig) == 0, "the bounded-stack thread starts");
  pthread_join(worker, nullptr);
  pthread_attr_destroy(&attr);

  CHECK(rig.Stood, "a sixteen-thousand-link owned chain assembles");
  CHECK(rig.Felled,
        "**THE TEARDOWN WALKS WITH AN EXPLICIT STACK**: felling the engine fells every "
        "carriage on a stack the per-link recursion could not survive (board:1721)");
  CHECK(rig.Raised,
        "**AND SO DOES THE STAND-UP**: an eight-thousand-part prefab chain instantiates on "
        "the same bounded stack -- depth is the pool's business, never the call stack's "
        "(board:1721)");

  Covers("IV.8 the store's teardown and stand-up are bounded terms: owned chains fell and "
         "prefab trees raise with explicit work stacks, proven on a thread stack the "
         "recursive forms overflow (board:1721)");
  return Report();
}
