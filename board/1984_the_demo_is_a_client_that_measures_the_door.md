Type: feature
State: active
Parent: 1982
Area: client
Progress: client

# The demo is a client that measures the door

**Benchmark** — Unreal ships sample projects that are content plus a launcher; RAGE's game IS its
client. **Neither ships a demo whose purpose is to MEASURE its own engine's interface**, so this
is the tree's own instrument, and CLAUDE.md already states the rule it serves: a client is almost
no code, and when it needs much code the door is the finding.

`apps/demo` is a port of a 64k-style intro (`../OpenGL/demo.c`, 1599 lines of C: a raymarched
fragment shader through five scenes, and a full software synth -- wavetables, a Moog ladder
filter, a reverb, drums and chords). It is ported through `include/` ALONE.

What it will show, because it cannot be written any other way:

| the demo needs | and so the door needs |
|---|---|
| a full-screen shaded surface | a declared subject with a material the scenario names |
| an object that carries a shader | the same, and it is what "the blob" is |
| a synth as its sound | a source declared as a GRAPH (board:1982) |
| that sound to come from the blob | a source bound to an ENTITY (board:1982) |
| the sound to move with it | spatial mixing (board:1983) |

- [x] `apps/demo` builds and runs against `include/` alone -- 194 lines, one translation unit,
      and it declares its own scenario, mixes its own sound and drives its own frames without
      naming an outshine header other than the door's.
      proof: outshine/door/ScoreWhatAHeadlessRunDoes
- [ ] every line it needs that the door cannot give is a finding filed against the door
- [x] its line count stands beside `apps/driver`'s in `STATE.md`. The Clients section counts
      every program under `apps/` and says what each REACHES -- and the honest column there is
      whether it LINKS from `liboutshine.a` alone, not whether it compiles, because an
      include path into `src/` is only one of the two ways past the door.

          | 194 | 1 | apps/demo   | include/ alone
          | 248 | 1 | apps/driver | include/ alone
          | 706 | 3 | apps/viewer | does not link from the library alone

      proof: harness/claims/EveryClientIsMeasuredOnThatPage
