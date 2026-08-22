Type: task
Area: src
Tags: hygiene, optimisation

# Boundaries speak span and string_view

Every read-only boundary that today takes `const std::vector<T>&` or `const std::string&`
takes `std::span<const T>` / `std::string_view` instead, and no call site copies into an
owning container just to traverse. Owner directive 2026-08-22; the architecture reviewer's
mechanical bar enforces it on every touched file from now on — this item is the sweep over
the existing tree.

Survey first (`grep -rn 'const std::vector<.*> &\|const std::string &' src/ include/`),
convert layer by layer, fast gate green per layer. `[[nodiscard]]`, `explicit`, `noexcept`
and `constexpr` hygiene ride the same sweep where the touched signature is missing them.


---

Enabling move landed: the whole tree builds C++20 (one CXXSTD; gate 122/122 warm in 52 s).
First conversion: the Fit family (Simplify, Fit, KeepBetween, AwayFromChordM) takes
std::span<const double> -- callers convert implicitly, zero churn. Survey: 98 vector-ref and
76 string-ref boundary parameters remain, layer by layer.
---

Reviewer sharpening (2026-08-22, evening round) -- three residues of this hour's own sweep:
- src/ground/TileWatermark.h:59 `Done(const std::vector<OsmField::Feature> &)` kept the
  vector ref while Ask and Advance beside it went span -- half-converted class.
- src/core/io/Log.h: only Info gained a span overload; Debug/Warn/Error still take
  initializer_list alone, so a caller holding a built field array can only log at Info.
- src/core/io/Log.cpp:39-44 (Emit): under an active LogUnitScope every log call still
  allocates the withUnit vector -- 40d99ce's "the allocation is gone" holds only for the
  unit-less path. Prepend the unit field sink-side or on a fixed-capacity buffer.

---

Survey refreshed (board queue, night): the three sharpened residues are repaid at HEAD --
TileWatermark::Done takes span, all four Log levels carry the span overload, and Emit hands
the unit as a parameter (no per-call vector under any scope). The tree-wide sweep itself has
NOT advanced: 124 `const std::vector<>&` and 144 `const std::string&` boundary refs stand
(up from 98/76 -- new code outpaces the sweep; the reviewer's mechanical bar catches touched
files, not new neighbours). Heaviest layers: gltf (25), ground (14+), render (11+). Convert
layer by layer with judgment -- a stored string wants value+move, not string_view.

---

Survey corrected and the vector half CLEARED (board queue, night): the 124-count was
dominated by GETTERS returning const& to owned storage -- idiomatic, not this item's target.
The true taking-boundary population was 11, of which Capacity.h needs the vector by nature
(capacity() is not a span notion) and two are loop-locals. The seven real parameter sites
are converted: SubjectDraw::SetMaterials/SetLights, Renderer::SetSubjectMaterials/Lights,
Track::Build (times, values), Subject's skin joints, RoofSurface::Cover + its EarClip.
Callers convert implicitly; SetLights keeps its copy via assign. 129/129 warm. Remaining:
the ~41 const std::string& parameters, each judged (stored strings want value+move, not
string_view).

Progress: four judgment-clear string boundaries converted -- Sha256Hex, GroundMaterials::Find,
RenderPlan::StageByName, Document::Honours (all compare/hash, nothing stored). 129/129 warm.
Deferred with grounds: the Script interface (virtual overriders ripple), TilePool keys (the
map's find would copy anyway without a transparent hash -- convert when the map does), the
Refuse/store family (value+move is the right form, a separate sitting).

Progress: the Refuse family (13 sites) takes its message by VALUE and moves it into Error_
where it lands unmodified -- a refusal built from concatenation now moves instead of copying.
Remaining, each with named grounds: the Script virtual interface (overrider ripple), TilePool
keys (map lacks a transparent hash), the read-only string params judged site by site.
