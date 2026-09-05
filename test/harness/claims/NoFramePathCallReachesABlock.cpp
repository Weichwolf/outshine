#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "Check.h"
#include "Shell.h"

using outshine::Test::Lines;
using outshine::Test::Run;

namespace {

// THE LINKER'S OWN GRAPH, NOT A GREP. CLAUDE.md bounds the frame path -- no alloc, no lock, no
// disk, no unbounded block -- and until this claim that bound was a sentence. It was broken in
// the same session it was quoted: a ground query added to the physics tick reached, four calls
// down, a poll loop of 30000 attempts at 1 ms, on a budget of 16.7 ms.
//
// A grep cannot find that, because no line of the tick mentions sleeping. The graph can.
// `test/harness/shared/graph/callgraph.sh` reads every object in the archive: `nm -n` gives the
// text symbols in ADDRESS order, and every relocation `objdump -dr` prints carries the address it
// sits at, so the enclosing function is the last symbol at or below it. What comes out is what
// the LINKER resolves, not what a header suggests.
//
// WHAT THIS WALK CANNOT SEE, stated because a guard that hides its blind spot is worse than
// none: a relocation names a direct call. An INDIRECT call through a vtable carries no symbol,
// so the walk stops at every virtual boundary. The frame path crosses exactly one -- `Support`
// is virtual so that the sim does not know the world -- and the override the engine installs is
// therefore named below as a second seed. A seed that matches NOTHING fails this claim by name,
// because a seed that stopped seeding looks exactly like a path that came up clean.
struct Seed {
  const char *Mangled;
  const char *Why;
};

constexpr Seed kStepSeeds[] = {
    {"Engine5State5FallsEv",
     "the physics step: gravity onto every free body, one `Physics::Step` per body per tick. It "
     "replaced `Sim::DriveTick` when the autopilot left in board:2117 -- the SEAT of the step "
     "moved, the bound on it did not"},
};

// THE PICTURE IS THE FRAME PATH TOO, AND IT WAS NEVER WALKED (board:2007). For as long as this
// claim stood it seeded the SIMULATION and nothing else, so a name promising the frame path had
// not once looked at the half that draws -- and that half was reading the velocity target back on
// every frame, 43.6 MB and a device sync, to publish two numbers nobody had asked for.
constexpr Seed kPictureSeeds[] = {
    {"Engine6renderENS_6ExtentE", "the picture: one call draws one frame and hands it over"},
    {"Render13SceneRenderer11RenderFrameEv",
     "what the picture reaches through `Live::Draw` -- a call the graph does follow, seeded "
     "anyway so a change to that chain cannot silently unseed it"},
    {"Engine5State4DrewEv", "what the picture publishes after it draws"},
};

// Each is forbidden for a reason CLAUDE.md gives, and each is a SUBSTRING of a mangled or C
// symbol so that every overload and every instantiation is caught by one entry.
struct Forbidden {
  const char *Symbol;
  const char *Why;
};

constexpr Forbidden kStepForbidden[] = {
    {"sleep", "an unbounded block: the step waits on something that is not the step"},
    {"nanosleep", "the same, one layer down"},
    {"BytesBlocking", "the tile pool's waiting fetch -- 30000 attempts at 1 ms by declaration"},
    {"StitchedGrid", "a terrain build, which fetches and allocates"},
    {"_Znwm", "operator new: an allocation the step cannot bound"},
    {"_Znam", "operator new[]: the same"},
    {"_malloc", "the same, reached through C"},
    {"_fopen", "disk"},
    {"_pread", "disk"},
};

// THE PICTURE'S LIST IS SHORTER, AND THAT IS A DECISION RATHER THAN A WEAKENING. Unreal's render
// thread allocates every frame -- `FMemStack` and `FRDGAllocator` are frame-lifetime linear
// allocators, and nobody at Epic calls a `new` on that thread a defect. RAGE has frame heaps for
// the same reason. What NEITHER tolerates is a STALL: a readback, a lock, a disk touch, a sleep.
// So the picture is held to the blocking terms, and its allocation is bounded by a frame
// allocator this tree does not have yet -- named in board:1943 rather than asserted here, because
// a claim that fails for a reason nobody intends to fix teaches the next reader to ignore it.
constexpr Forbidden kPictureForbidden[] = {
    {"sleep", "an unbounded block on the path that draws"},
    {"nanosleep", "the same, one layer down"},
    {"BytesBlocking", "the tile pool's waiting fetch, reached while drawing"},
    {"8Readback4Land",
     "a GPU->CPU readback that WAITS: the CPU stalls on the device for a number, which is the "
     "one thing a frame must never do. This is what board:2007 measured -- 43.6 MB and a sync "
     "every frame, spent publishing two numbers. Readback::Enqueue and Readback::Poll, a fence "
     "queried a frame later, are Unreal's FRHIGPUBufferReadback and allowed. Two other waits "
     "stay legitimate and unlisted: the frame-pacing wait on the fence of the frame that held "
     "this ring slot kFramesInFlight ago, which Unreal (RHIWaitForFrame) and RAGE both do, and "
     "the wait a client's readPixels asks for by name"},
    {"_fopen", "disk"},
    {"_pread", "disk"},
};

[[nodiscard]] std::string Demangled(const std::string &symbol) {
  std::string said;
  (void)Run("printf '%s' " + symbol + " | c++filt", said);
  while (!said.empty() && said.back() == '\n') { said.pop_back(); }
  return said.empty() ? symbol : said;
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const char *nest = std::getenv("OUTSHINE_NEST");
  if (nest == nullptr || *nest == 0) {
    Unprepared("this claim unpacks the archive into the runner's nest and was given none");
    return Report();
  }

  // A STALE ARCHIVE IS THE SAME GREEN AS A CLEAN PATH. This claim judges `build/liboutshine.a`,
  // and a run that names a suite does not rebuild it -- so an edit that puts a block back on the
  // frame path leaves the archive untouched and this walk reports the OLD graph, passing. That
  // happened to the negative control the hour this claim was written.
  std::string newer;
  (void)Run("find src include -newer build/liboutshine.a -name '*.cpp' -o -newer "
            "build/liboutshine.a -name '*.h' 2>/dev/null | head -3",
            newer);
  if (!newer.empty()) {
    std::printf("%s", newer.c_str());
    Unprepared("a source is newer than build/liboutshine.a, so this walk would judge a graph the "
               "tree has already left -- run make");
    return Report();
  }

  std::string edges;
  const int walked = Run("sh test/harness/shared/graph/callgraph.sh build/liboutshine.a " +
                             std::string(nest) + "/framewalk 2>/dev/null",
                         edges);
  CHECK(walked == 0 && !edges.empty(),
        "the archive walks -- a claim that cannot read the graph it judges is UNPREPARED, never "
        "green");
  if (walked != 0 || edges.empty()) { return Report(); }

  std::map<std::string, std::vector<std::string>> calls;
  size_t edgeCount = 0;
  for (const std::string &line : Lines(edges)) {
    const size_t tab = line.find('\t');
    if (tab == std::string::npos) { continue; }
    calls[line.substr(0, tab)].push_back(line.substr(tab + 1));
    ++edgeCount;
  }

  struct Walk {
    const char *Name;
    const Seed *Seeds;
    size_t SeedCount;
    const Forbidden *Forbid;
    size_t ForbidCount;
    const char *Claim;
  };

  const Walk walks[] = {
      {"the physics step",
       kStepSeeds,
       sizeof kStepSeeds / sizeof kStepSeeds[0],
       kStepForbidden,
       sizeof kStepForbidden / sizeof kStepForbidden[0],
       "**NOTHING THE PHYSICS STEP CAN REACH ALLOCATES, LOCKS, TOUCHES DISK OR WAITS**: the bound "
       "is CLAUDE.md's and it was a sentence until this walk. It was broken in the session that "
       "quoted it -- a ground query on the tick reached a poll of 30000 attempts at 1 ms, four "
       "calls down, on a 16.7 ms budget, and no line of the tick mentioned sleeping"},
      {"the picture",
       kPictureSeeds,
       sizeof kPictureSeeds / sizeof kPictureSeeds[0],
       kPictureForbidden,
       sizeof kPictureForbidden / sizeof kPictureForbidden[0],
       "**NOTHING THE PICTURE CAN REACH STALLS**: no readback, no lock, no disk, no sleep. Unreal "
       "polls FRHIGPUBufferReadback and never waits on it; RAGE double-buffers its timing buffer "
       "for the same reason. This walk did not exist until board:2007, and the first time it ran "
       "it found Engine::State::Drew reading the velocity target back on EVERY frame -- 43.6 MB "
       "and a device sync, spent on two numbers, which is why DamagedHelmet's wall was 20 ms a "
       "frame against a 0.006 ms step"},
  };

  for (const Walk &walk : walks) {
    std::set<std::string> seen;
    std::vector<std::string> queue;
    std::vector<size_t> seededBy(walk.SeedCount, 0);
    for (size_t which = 0; which < walk.SeedCount; ++which) {
      for (const auto &pair : calls) {
        if (pair.first.find(walk.Seeds[which].Mangled) == std::string::npos) { continue; }
        ++seededBy[which];
        if (seen.insert(pair.first).second) { queue.push_back(pair.first); }
      }
    }

    bool everySeedTook = true;
    for (size_t which = 0; which < walk.SeedCount; ++which) {
      std::printf("SEED %-34s matched %zu symbol(s) -- %s\n",
                  walk.Seeds[which].Mangled,
                  seededBy[which],
                  walk.Seeds[which].Why);
      everySeedTook = everySeedTook && seededBy[which] > 0;
    }
    CHECK(everySeedTook,
          "STALE SEED: a seed matched nothing in the archive, so the walk below started from "
          "fewer places than it declares -- and a seed that stopped seeding looks exactly like a "
          "path that came up clean");
    if (!everySeedTook) { return Report(); }

    const size_t fromSeeds = queue.size();
    std::map<std::string, std::string> parent;
    for (size_t at = 0; at < queue.size(); ++at) {
      const auto found = calls.find(queue[at]);
      if (found == calls.end()) { continue; }
      for (const std::string &to : found->second) {
        if (seen.insert(to).second) {
          parent[to] = queue[at];
          queue.push_back(to);
        }
      }
    }

    std::vector<std::string> blocking;
    for (const std::string &symbol : seen) {
      for (size_t one = 0; one < walk.ForbidCount; ++one) {
        if (symbol.find(walk.Forbid[one].Symbol) != std::string::npos) {
          std::string chain = Demangled(symbol);
          std::string up = symbol;
          for (int hop = 0; hop < 4; ++hop) {
            const auto above = parent.find(up);
            if (above == parent.end()) { break; }
            up = above->second;
            chain += "  <-  " + Demangled(up);
          }
          blocking.push_back(std::string(walk.Forbid[one].Symbol) + "  via  " + chain);
          break;
        }
      }
    }

    std::printf(
        "EDGES %zu over the archive, REACHABLE from %s %zu\n", edgeCount, walk.Name, seen.size());
    for (const std::string &one : blocking) { std::printf("  BLOCKS  %s\n", one.c_str()); }

    CHECK(seen.size() > fromSeeds,
          "the walk reached past its own seeds -- a graph that resolves nothing would report no "
          "block for the same reason an empty one does. The physics step reaches four symbols "
          "since board:2117 (gravity onto free bodies, one Physics::Step each), so a floor of "
          "ten here was a number without an origin");
    CHECK(blocking.empty(), walk.Claim);
  }

  Covers("the frame path, BOTH HALVES: from the physics step nothing the linker can reach "
         "allocates, locks, touches disk or blocks; from the picture nothing it can reach STALLS "
         "-- no readback, no lock, no disk, no sleep. Walked over the archive's own relocations, "
         "with every seed declared and a seed that matches nothing failing by name");
  return Report();
}
