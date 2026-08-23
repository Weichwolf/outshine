Type: issue
Parent: 1583
Area: include/outshine, src/scene, src/scenario
Tags: component-model, tags, declarative, door

# The tag catalogue is one table the engine seeds and the scenario extends

`TagCatalogue` (include/outshine/Register.h:34) freezes the whole tag vocabulary in a header.
Two defects follow, and both bite at the first serious world.

## The encoding's hard limits, derived

`Tag` is one byte per level in a `uint32_t` (`0x01000000` = root, `0x01010000` = its first
child), and `Within` masks off the parent's trailing zero bytes. Therefore:

| | bound | what exceeds it |
|---|---|---|
| depth | 4 levels | GAS ships 5 routinely: `GameplayCue.Weapon.Rifle.Impact.Concrete` |
| siblings | 255 per node | Fallout 4's attach-point family is plausibly past it |

Both overflow SILENTLY -- a full byte slot carries into the parent's value, and a child
becomes its own uncle.

## The declarative defect

Content-side vocabulary (`Faction.Raider`, `Material.Concrete`, `Damage.Fire`) numbers 10^3
to 10^4 in a Fallout- or GTA-class world and changes with every scenario. Freezing it in a
header makes a new world an engine recompile -- the direct negation of "content = data,
engine = verbs".

## What the references do (fetched judgement, not recall of exact counts)

| engine | mechanism | declared in | order of magnitude |
|---|---|---|---|
| Creation (Skyrim/FO4) | `KYWD` record, FormID = 32-bit handle | ESM data, Creation Kit, moddable | 10^3 .. 10^4 |
| RAGE (GTA) | `atHashString` -- 32-bit Jenkins hash, strings stripped from retail; no tag TREE, many small enums | build-time metadata | ~10^3 across all axes |
| Unreal GAS | `FGameplayTag` = FName index + net index, dotted-path hierarchy | `GameplayTags.ini` / DataTable **and** `UE_DEFINE_GAMEPLAY_TAG` in C++ -- ONE table | 10^2 .. 10^4 |
| Flecs (our stated reference) | a tag IS an entity; hierarchy is `ChildOf` | C++ type or `.flecs` script -- the same store | unbounded |

No shipped system keeps the catalogue in a header. Unreal is the instructive one: native tags
and ini tags land in one `UGameplayTagsManager`, which is this tree's "one graph behind two
doors with identical refusal text".

## What will be true

ONE table, seeded by the engine and extended by the scenario.

| | engine vocabulary | scenario vocabulary |
|---|---|---|
| example | `Does`, `DoesSteer`, `Offers` | `Faction.Raider`, `Damage.Fire` |
| must be compile-time because | `kRules` names `tags::Does` in a `constexpr` table; a typo is a compile error | -- |
| declared in | `Register.h`, closed, ~50 | scenario XML, open, 10^3..10^4 |
| refuses at | compile | ASSEMBLY, never at runtime |

`Tag` becomes a 16-bit id into a table built at assembly in DFS pre-order. A subtree is then a
contiguous range, so containment is one unsigned compare instead of the byte mask, with no
depth and no sibling bound:

    descendants of p occupy [p.Id, p.Id + p.Subtree]
    Within(p)  <=>  Id - p.Id <= p.Subtree

The engine keeps its compile-time safety through a `constexpr TagName` enum that `kRules`
spells; `Store` resolves `TagName -> Tag` once at assembly. Pool bound `kMostTags = 4096`
derived from the observed AAA population above; a row is parent(u16) + subtree(u16) + name
offset(u32) = 8 bytes, so the table is 32 KB.

XML declares the tree NESTED, not as a dotted string, so an unknown tag in an assignment
refuses at assembly with the same text the C++ door gives.

## Depends

The move touches the public door (`include/outshine/Register.h`) and the store's tag queries;
it belongs after 1583's slot work lands, not before.

## Comments

- 2026-08-23 -- raised by the owner: "irgendwie muesste das dynamisch/deklarativ sein". The
  encoding limits above were derived from the header, not recalled; the reference counts are
  orders of magnitude with medium confidence, and the MECHANISMS (hash-in-retail, one manager,
  tag-is-entity) are the load-bearing part of the survey, not the counts.
