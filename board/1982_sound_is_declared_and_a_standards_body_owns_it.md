Type: feature
State: open
Area: audio
Progress: audio

# Sound is declared, and a standards body owns both halves of it

**Benchmark** — Unreal delivers sound three ways: `USoundWave` (a file), **MetaSounds** (a node
GRAPH compiled per voice), and `USoundWaveProcedural` (a buffer the game fills). RAGE has the same
three, with **`audSynthSound`** as its data-driven synth. Both bind a sound to an ENTITY --
`UAudioComponent` on an actor, `audEntity` -- and both mix through a BUS GRAPH. **Taking both, and
the GRAPH first**: a procedural world cannot ship a wave file for every engine, wind and stream,
so the graph is the only form that scales with the generator that made the thing making the noise.

**And unlike either engine, both halves here have a standards body with a corpus.**

| half | the standard | the oracle |
|---|---|---|
| WHERE a source is, how it falls off, its cone | glTF's `KHR_audio_emitter` — an emitter on a NODE | Khronos, and it is inside the promise to read every ratified `KHR_` |
| WHAT a source IS — oscillators, biquads, delays, convolvers, shapers, a panner with distance models, a listener | **Web Audio API**, W3C | **WPT's `webaudio` suite**, from the same repository this tree already fetches `wpt/css` out of |

That is the interface layer's argument a second time: after the document, sound becomes the second
subsystem whose correctness someone outside this tree certifies.

- [ ] `KHR_audio_emitter`'s status is READ from Khronos rather than assumed, and the item says
      whether it is ratified, and what the reader must accept

**MEASURED, AND THE PATH IS A LANGUAGE RATHER THAN A BINDING.** This item said the opposite and
the measurement refuted it. WPT's webaudio suite runs `testharness.js` against
`OfflineAudioContext`, and the claim here was that `src/base/format/Script` -- 813 of 813 test262
cases -- already carries the JavaScript half, so only the API binding was missing. Asked directly,
Script answers:

| what a webaudio case needs | Script |
|---|---|
| arithmetic, string concatenation | ok |
| a function of one's own | `expected a value and found 'function'` |
| an array literal | `expected a value and found '['` |
| an object literal | `carries ':' ... outside the subset this interpreter declares` |
| `new` | `expected a value and found 'new'` |
| a class | `expected a value and found 'class'` |
| try / throw | `expected a value and found 'try'` |
| a closure, for-of | refused the same way |

`Script::Value` is `Nothing | Number | Text | Ref`: there is no array, no object and no function
as a value, and `Call` goes to the HOST rather than to a body in the script. The keywords in the
tokeniser are reserved words, not constructs. And the 813 test262 cases are `assignment`, `less`,
`greater`, `if`, `logical`, `addition`, `equals`, `division`, `multiplication`, `modulus`, `while`,
`subtraction` -- expressions and control flow. **813 of 813 is a true number about a small
question**, and reading it as "the engine exists" was my error.

`webaudio` at the pin `wpt/css` already carries is **296 cases**: 46 audioparam, 36 audioworklet,
28 audiobuffersource, 20 audiocontext, 19 panner, 16 biquad, 14 convolver, 11 delay, 11 analyser,
7 waveshaper, 6 oscillator, 6 offlineaudiocontext, 4 gain, 4 stereopanner and the rest. Roughly
**129 sit on nodes this mixer already has**; 36 need AudioWorklet and 21 need media sources this
engine does not want.

**TAKEN: build the language, and take it for the DOCUMENT rather than for audio.** CLAUDE.md
already puts script in `base` because the interface layer is HTML/CSS/JS, so functions, objects,
arrays, exceptions and prototypes are owed to the document layer whether audio asks or not.
Spending it once is the reason to spend it. The etappe is measurable at every step because
test262 says how far the language reaches -- today the expression chapters, next a chapter that
needs a function body.

- [ ] test262 reaches a chapter that needs a function of the script's own, and the case count
      is the language's reach rather than the harness's
- [ ] `Script::Value` carries an object and an array, so a host can hand back a structure
- [ ] `wpt/webaudio` is fetched and pinned, and the run publishes how many of the 296 the
      subset reaches -- as `wpt/css` publishes its own

Until the language stands, `outshine/audio` proves the FORMULAS at TRUTH grade -- the three Web
Audio distance models and the classical Doppler ratio, each against the closed form that defines
it -- and that is agreement with a specification rather than with ourselves, but it is not the
vendor running its own suite against us.
- [x] a scenario declares a source as a GRAPH and the engine evaluates it -- oscillator, noise,
      gain, biquad, delay and mix, each reading its inputs by id; convolver and shaper refuse by
      name rather than sounding finished.
      proof: outshine/audio
- [ ] a source binds to an ENTITY and never to a coordinate
- [x] the declaration says WHICH of the three ways a source comes by, and refuses one that names
      none of them.
      proof: outshine/audio
- [ ] the file way and the buffer way carry sound, not only the graph
