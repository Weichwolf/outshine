Type: defect
State: open
Area: engine
Tags: animation, door, corpora

# A declared animation MOVES the picture

**Benchmark** — Unreal: `UAnimSingleNodeInstance` plays a sequence at a declared time and the pose
reaches the render proxy the same frame. RAGE: `crClip` under `crmtObserver` likewise. **They
agree**, so the matter is closed; this item is that outshine reads the declaration and draws the
bind pose.

## What was measured

`test/khronos/generator/Animation_NodeMisc_01` through `outshine-client run`. The file carries one
animation channel, rotation on node 0, keyed at 1, 2, 3, 4 and 5 seconds with values 90 deg about
Y, identity, -90 deg, identity, 90 deg. glTF clamps below the first key, so the pose at t=0 is the
90 deg rotation. Blender's reference shows it: the silhouette spans 346 px. Ours spans 313 px,
which is the BIND pose.

    animation="ignore"                     879159e8
    animation="play"                       879159e8
    animation="loop"                       879159e8
    animation="play"  <clock rate="0"/>    879159e8
    animation="play"  fps=120  (t = 1.00 s)  879159e8
    animation="play"  fps=240  (t = 0.50 s)  879159e8

**One digest for four spellings and four instants.** `ScenarioRead` validates the spelling and
refuses a fifth, so it reads as a working feature.

## What is NOT the cause, measured

- `Keyframes::At` clamps correctly: `abscissa <= Frames_[0]` returns the first key's value
- The declaration reaches `Core::Declaration`: `declared.Animation = subject->Animation`
  (`Declaring.cpp:165`) and `Live::Stands` takes it (`Declaring.cpp:280`)
- `Asset::Stands` builds the motion when the file has animations and the spelling is Play or Loop
  (`Asset.cpp:31`), and sets `Moves_ = Motion_.EndS() > 0.0`
- Posing RUNS: `heap taken under pose-build` reads 58 416 bytes and `pose-assemble` 4 848

So the pose is built and the picture does not carry it. The next step wants a debug build and a
trace from `Live::Advance` to the vertex buffer, which is why this is its own item.

**One confound was found and removed.** `Shots::Draw` advances 120 times and `Live::Advance` steps
`1/Render.Fps` per call, not the clock's rate -- at fps 60 that lands on t = 2.000 s, where this
file's key is EXACTLY the identity. The first three rows above would have proved nothing. The fps
rows are the control that does.

## What it blocks

34 of the 47 render-corpus cases that disagree with their oracle are `Animation_*`, and this is
why. Fixing it is worth more than the other 13 together.

## What will be true

- [ ] `animation="play"` and `animation="ignore"` give DIFFERENT pictures for a file that moves
- [ ] the pose at t=0 is the first key's, per the glTF clamp, and the corpus's `Animation_*` cases
      agree with their reference at 99.99 per cent within 8 of 255
- [ ] the instant a scenario draws at is DECLARED rather than a consequence of how many times a
      caller advanced -- today it is 120 divided by `<render fps>`, which no reader would guess
