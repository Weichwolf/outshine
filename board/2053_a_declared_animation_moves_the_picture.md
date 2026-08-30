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

## THE SHARP VERSION, with the instant printed beside the digest

Three measures were added to see this at all -- the subject's animation duration, the frames its
rate makes, and THE INSTANT IT IS POSED AT:

    fps  60   posed at 2.033 s   between the identity key and -90 deg   digest 879159e8
    fps 240   posed at 0.508 s   clamped to the first key, +90 deg      digest 879159e8

Two genuinely different poses and ONE picture. The t = 2.000 s confound is gone with it: 2.033 s is
not the identity key either.

## What is NOT the cause, measured

- `Keyframes::At` clamps correctly: `abscissa <= Frames_[0]` returns the first key's value
- The declaration reaches `Core::Declaration`: `declared.Animation = subject->Animation`
  (`Declaring.cpp:165`) and `Live::Stands` takes it (`Declaring.cpp:280`)
- `Asset::Stands` builds the motion when the file has animations and the spelling is Play or Loop
  (`Asset.cpp:31`), and sets `Moves_ = Motion_.EndS() > 0.0`
- Posing RUNS: `heap taken under pose-build` reads 58 416 bytes and `pose-assemble` 4 848

- `Posed::PoseInto` rebuilds `Assembled_` from the file with the animated locals every pose
- `Live::Advance` calls `Submit()` in the same branch that advances `AtS`, so the posed geometry
  is submitted rather than left behind
- The `restand:` measures -- including `the geometry handed over, digested` and `uploads the
  residency made` -- do NOT print on these runs at all, so the geometry is handed to the device
  ONCE, at stand time, and a re-pose never reaches it

## WHERE IT BREAKS, narrowed to one hop

`the geometry the device last took, digested` was added beside the instant, and it settles it:

    fps  60   posed at 2.033 s   geometry 8864644284851
    fps 240   posed at 0.508 s   geometry 8864644284851

The device is handed the SAME vertices at two different poses. Everything either side of that reads
correct on the page:

    Pose::At            samples every channel and writes locals[node] from the animated TRS;
                        the matrix branch that would discard it is not taken -- node 0 has no matrix
    Posed::PoseInto     calls Motion_.At then Assembled_.Build(File_, locals, weights, variant)
    Subject::Build      refuses a pose of the wrong size, then Flattens WITH the pose
    Live::Pose          Held_.Poses then Reshape, which rebuilds Shaped_ from Assembled_
    Live::Advance       Advances, Pose(Held_.AtS()), Submit -- one branch, and AtS proves it ran
    Move                packs Shaped_'s positions and calls SetSubjectPose every advance
    SubjectDraw::SetPose refuses a mismatched vertex count, then HandStreams

## THE RUNTIME PROBE, and it puts the break on the DEVICE side

Two more measures were added -- the pose's own local transforms, digested, and the vertices
assembled from them:

    fps  60   posed 2.033 s   locals 556671865   assembled 395731835   device 263376276556099
    fps 240   posed 0.508 s   locals 986415778   assembled 580343474   device 211891159521651
    picture, both                                                      879159e8

**Everything up to and including the hand-over DIFFERS, and the picture does not.** The animation
is sampled, the vertices are assembled from it, and the device is given them. What it draws is the
same frame either way.

**A measure had to be repaired to see this, and it nearly cost a wrong root cause.**
`gGeometryDigest` was written only inside `Place`, which runs once when the subject first stands;
`Move`, which runs on every advance, left it alone. Read as "the geometry the device last took" it
said one number forever and would have proved the hand-over was stale. It is now written in `Move`
as well, and it is what the two device digests above come from.

So the remaining hop is `SubjectDraw::HandStreams` -> `SubjectResidency::Cross(deferred)` ->
`FlushCrossings`, and the question for the next round is whether a pose's staged copy is flushed
against the residency it was staged into. That is a GPU-side check and it is where this item
resumes.

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
