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

Sharpened (review 2026-08-23): the by-value sitting landed the right form in only 5 of 9
Refusers. Where the message LANDS unmodified, value+move holds (ReferenceLine, Emit,
Subject, Store — all `Error_ = std::move(why)`). The other four miss the point of the form:
- src/core/io/Png.cpp:30 takes by value and then COPIES (`out.Error = why;`) — the move was
  the entire reason for the signature; as written it is one copy WORSE than const-ref for
  lvalue callers. `std::move(why)`.
- src/scenario/Mod.cpp:11, src/scenario/Fields.h:68-71, src/gltf/Document.cpp:334 only
  CONCATENATE the parameter (`Path_ + ": " + why`) — ownership is never taken, so by-value
  buys nothing and costs lvalue callers (Mod.cpp:28,30 pass the lvalue `err`) a dead copy;
  Fields.h discards the copy entirely when Err_ is already set. The right form for a
  read-only concatenand is this item's own thesis: `std::string_view`. Document's
  empty-Path_ branch may move (`Path_.empty() ? std::move(why) : …`) if it stays by value.
Cold paths all — this is form, not frame time; but the form is the claim being closed.

Sharpening repaid: Png moves its message; Mod, Fields and Document Refuse take string_view
(they only concatenate -- ownership was never used, and lvalue callers paid a dead copy);
Store, Subject, Emit, ReferenceLine keep value+move where the message truly lands.

Sharpened (review 2026-08-23, round 9): one residue of that repayment -- the TWO-ARG
overload src/scenario/Fields.h:65 `Refuse(const char *key, const std::string &why)` sits
directly above the converted one-arg form, concatenates exactly the same way, and kept the
const-ref. Same verdict, same line of reasoning: string_view.

Residual repaid: Fields' two-arg Refuse joins its sibling on string_view.

Progress: the Script Host interface and Program::Named speak string_view -- the virtuals,
the three overrider fixtures and the test262 host followed in one motion; 829/829 with the
full script corpus.

Sharpened (review 2026-08-23, round 14): the Host/Named conversion landed clean (829/829),
three residues on the same touched surface:
- src/core/Script.h:100 `Program::Read(const std::string &text, …)` kept the string ref.
  The stated deferral ground — virtual overrider ripple — fell with this commit: Read is
  non-virtual, and Tokenise/Reading ride the same view. string_view through.
- src/core/Script.cpp:689-691 (WhyOutside): `const std::string row(boundary.Name)` allocates
  per boundary per call only to prefix-compare; C++23's `name.starts_with(boundary.Name)`
  is the strictly clearer form and allocates nothing.
- src/core/Script.h:33-50: Value::OfNumber/OfText/OfRef are value-returning factories
  without [[nodiscard]] — the mechanical bar puts it on every factory.

Sharpening repaid, with the corpus as judge: Program::Read and the tokeniser speak
string_view end to end -- the number scan moved from strtod to from_chars (the C++23 form),
which surfaced that hex literals had only ever parsed by strtod's accident; they parse
EXPLICITLY now (0x via from_chars base 16), proven by the full test262 corpus that caught
the drift on first run. WhyOutside uses starts_with; the Value factories are [[nodiscard]].

Sharpened (review 2026-08-23, round 15): the strtod family the Script repayment just buried
lives on one directory over, in src/ui — and there it is not only a form question. All three
are locale-dependent or allocate to reach a C string:
- src/ui/Style.cpp:167-169 (ReadValue): `const std::string held(trimmed)` allocated per
  value solely to call `std::strtod` — which under a comma-decimal locale reads "1.5px" as 1.
  from_chars on the view, as Script.cpp now does.
- src/ui/Style.cpp:488 (ReadCompound): `std::atoi(std::string(inside).c_str())` for
  nth-child — from_chars on the view, no allocation, and the error is checkable.
- src/ui/Markup.cpp:51-52 (Resolve): `std::string digits(...)` + `std::strtol` for numeric
  character references — same move.
The correctness residue of the Script conversion itself (unchecked `ec`, hex overflow to a
silent zero) is board:1688, not this item.

Progress: the ui parsers left the locale -- Style's number scan, nth-child and Markup's
entity codes speak from_chars (with CSS's legal leading '+' consumed explicitly and proven);
a locale-dependent strtod in a style engine was a correctness hole, not a form nit. WPT CSS
corpus 162/162 beside the unit proofs.

Sharpened (review 2026-08-23, round 18): the newest boundary regressed the rule on arrival —
src/scenario/InputMap.h:19 `Build(const std::vector<Binding> &declared, ...)` takes the owning
container where `std::span<const Binding>` says what is meant. Convert before the door grows
callers.

---

Progress -- the two young doors converted before callers grew: InputMap::Build and
TableBook::Build take std::span<const Binding>/std::span<const Table> (unit/scenario green).
The tree-wide sweep residue stands as surveyed.
---

Reviewer sharpening (2026-08-23, morning round) -- the hygiene rider is behind in render/:
- 177 `(void)` parameter lists remain in src/ (`grep -rn '(void)' src/`), nine of them in
  render/stages alone (MediumTransmittanceStage.h:16,26, PresentStage.cpp:78, SkyStage.cpp:106,
  ...). Commit daa3485b fixed exactly ONE in Transport.h; the sweep owns the rest.
- `RenderPlan::Compile(const PlanSpec&, std::shared_ptr<const RenderPlan>*, std::string&)`
  (src/render/plan/RenderPlan.h:51) is bool-plus-out-pointer-plus-error-string -- the C++23
  form is `std::expected<std::shared_ptr<const RenderPlan>, std::string>`; same for
  `StageByName` (an `std::optional<Stage>`).
- src/render/plan/RenderCatalogue.h:300-320: `Row`, `Names`, `Produces`, `ProducerCount`
  are value-returning queries used at runtime and carry no `[[nodiscard]]`, while
  `CarriesSceneRadiance` beside them does.

---

Progress (render sharpening repaid): the (void) parameter lists are gone tree-wide (only
discard-casts and MSL macros remain, which are not the C-ism), RenderCatalogue's
Row/Names/Produces/ProducerCount carry [[nodiscard]], and RenderPlan speaks C++23 at its
door: Compile returns std::expected<shared_ptr<const RenderPlan>, std::string>, StageByName
returns std::optional<Stage> -- ONE public spelling, the out-pointer form died with all 28
call sites converted (Live.cpp reads the expected directly; the tests wrap it). Gate
155/155, link audit closed.

Sharpened (review 2026-08-23, round 26): the C++23 refusal form stopped at the render
plan's door. `grep -rn 'std::string &error' src/ include/` counts 238 sites, and the ui
layer -- worked this hour under the new sanitised arm -- carries the pattern on its two
public verbs:

- src/ui/Layout.h:76 `bool Build(const Markup&, Stylesheet&, double, double, const Font&, std::string &error)`
- src/ui/Paint.h:41 `bool Build(const Layout&, const Font&, std::string &error, const Page& = {})`
- src/ui/Markup.h `bool Read(..., std::string &error)`

This is precisely the shape `RenderPlan::Compile` shed at 4552b4c ("ONE public spelling,
the out-pointer form died"): a refusal that carries its reason is `std::expected`. The ui
door is the next honest slice -- three functions, callers in `src/clients/Live.cpp:456-461`
and `test/unit/ui/*`, no virtual ripple. Note that board:1754 will give `Layout::Build` a
NEW refusal (nesting past the bound), so the door is being touched anyway; convert it then
rather than adding a second reason to a bool.

---

Reviewer sharpening (2026-08-23, round 27) — the string-boundary half has a site where the
view is taken and then thrown away, which is the failure mode this item exists to prevent:

`TableBook::Number`/`Text`/`At` (src/scenario/Tables.h:24-28, Tables.cpp:64-73) take
`std::string_view` at the boundary and then allocate an owning `std::string` on BOTH
lookups to feed the maps — `Held_.find(std::string(table))` (:65) and
`stood.ByKey.find(std::string(row))` (:68). Two heap allocations per table read, on the path
a script will read damage from per tick. This is the same case the item's own note deferred
for `TilePool` keys ("convert when the map does"): the C++23 answer is the transparent
comparator — `std::unordered_map<std::string, T, Hash, std::equal_to<>>` with
`is_transparent`, so `find(string_view)` buys nothing. `board:1489`'s 2026-08-23 sharpening
files the same site from the tables side; whichever item pays it, the pattern is the item
this sweep owns: **a `string_view` parameter that allocates in the first line of the body is
worse than the `const std::string&` it replaced, because it looks converted.**
