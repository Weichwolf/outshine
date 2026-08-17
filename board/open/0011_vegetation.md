Type: bug
Area: generators
Tags: perf, instrument

**Vegetation**

- **A weeping willow is drawn with 31.9 % of its bark below the point it is planted at, and no shoot
  is constrained at the ground.** `assets/world/species/willow.json` (*Salix* × *sepulcralis*,
  `height_m: 18`, `crown: weeping`) grows a bark mesh whose lowest vertex sits at **−0.6893 of the
  tree's own height** — **12.4 m below the trunk foot** — and **87 928 of its 275 343 bark vertices,
  31.9 %**, are under that plane. Four further declarations do the same, less far: dog_rose −0.4985
  (7.5 %), hedge_privet −0.2883, blackthorn −0.1199, hedge_hornbeam −0.0424.
  Re-measured 2026-08-13 over all 31 declarations under `assets/world/species/`, native, at full
  detail, after the growth was separated from the budget; the figures this replaces were taken while
  a vertex ceiling still coarsened the "full detail" mesh, so they understated it. The figures are
  bark only and the first is what `test/outshine/unit/generators/draw/GrownBarkIsAClosedMesh.cpp` prints.
  *The leaf-point figures this entry once carried came from a probe that is not in the tree and stay
  withdrawn until something measures them.* Nothing downstream lifts the mesh:
  `TreePrototype.cpp:115` copies `BoxMin.Y` into `Crown_.Bottom`, which only bounds the in-crown query
  (the deleted stand field) and the impostor box (`render/stages/ModelDraw.cpp:749`), and the
  instance transform puts y = 0 on the terrain. Two consequences, and they are of different kinds. The
  **cost** is measured: on flat ground nearly a third of every willow's bark is transformed every frame for
  geometry that cannot be seen — vertex work only, since terrain depth kills most of those fragments
  before shading, so do not claim the fragment half without measuring it. The **picture** is inferred
  and not yet measured: geometry 12.4 m under the planting point emerges wherever terrain falls away
  faster than that within the crown radius, which is the bank of a watercourse, and a willow is placed
  on banks. It is decidable from one frame at a declared riverside standpoint and nobody has taken it.
  **The contract reading in the entry this replaces was wrong, and the harmless explanation is three
  lines from the code that produces the number.** `TreeSkeleton.h:1-6`'s *"origin at the trunk foot"* means the
  **trunk foot**, not the box minimum, and `TreeGrower.cpp:403-406` says so in its own words —
  *"Y = 0 IS THE TRUNK FOOT, not the lowest vertex … a branch below zero belongs below the terrain;
  that is where it grows"* — with a measurement behind it (taking the mesh minimum put a willow's foot
  6.87 m and a spruce's 3.67 m above the ground). The mesh therefore honours its contract, and this is
  not a contract violation. It is a **missing** constraint, so the feature line is
  `board/` § III.2 *A shoot stops at the ground*; what stands here is the picture and the
  cost the absent constraint produces today.
  The grower's ruling is right for a spruce skirt at −0.02 of height and wrong at −0.69: *Salix* ×
  *sepulcralis*'s branchlets tumble **to touch** the ground ([RHS](https://www.rhs.org.uk/plants/81798/salix-sepulcralis-var-chrysocoma/details))
  and *Rosa canina*'s arching stems climb **up** through neighbouring shrubs to 1–5 m
  ([RHS](https://www.rhs.org.uk/plants/16017/rosa-canina-s/details)) — neither grows downward.
  Right: the hanging tip is clamped at the base plane the way it is already bent back at the crown
  envelope, and the permitted dip is one small sourced number, not a free consequence of shoot length.
  Decides it: `min y ≥ −δ` over bark **and** leaf points for every declaration, in the test that
  already measures the bark half.
