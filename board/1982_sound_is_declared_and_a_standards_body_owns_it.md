Type: feature
State: active
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
- [ ] `wpt/webaudio` is fetched and pinned the way `wpt/css` is

**MEASURED, AND THE PATH IS SHORTER THAN IT LOOKS.** WPT's webaudio suite runs `testharness.js`
against `OfflineAudioContext`, so it needs a JS engine AND the API bound to one -- not a mixer with
its own interface. This tree already has the engine: `src/base/format/Script` passes **813 of 813**
test262 cases, and it is there because test262 asked for it. So the distance to a certified audio
layer is a BINDING -- `OfflineAudioContext`, `AudioNode` and the node set, over the mixer that
already evaluates those semantics -- rather than an implementation.

That is the same shape as the interface layer's argument and it uses the same machinery: a
standards body ships the corpus, a script engine we already run executes it, and what we own is
the DSP underneath.

Until that binding stands, `outshine/audio` proves the FORMULAS at TRUTH grade -- the three Web
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
