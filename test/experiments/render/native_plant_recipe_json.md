# Native canonical recipe input — #1 / #48

`ReadPlantRecipe` consumes complete serialized `PlantPlan.recipe()` JSON in the
existing `branching-plant-2` and `branching-plant-3` formats. It uses the existing
Json parser; no second parser, source format, asset store or certificate is added.
It performs no file/network IO and returns owned level/blade parameters. Async
file acquisition and catalogue lookup remain responsibilities of the caller.

The supplied input-byte budget applies before document parsing. The envelope
requires exactly version, seed, levels and blades. Orders/blade variants are
bounded before parameter-vector allocation. Every serialized field is required;
unknown/missing/duplicate fields, wrong JSON types, unsupported enums/versions,
nonfinite numbers, invalid geometry and inconsistent petioles fail atomically.
Version3 carries explicit branch whorl size/tip inclination and permits bare wood;
version2 retains its existing fields. Counts/source-ID overflow and actual detail
budgets remain checked by native plan preflight before any growth.

Seeds are unsigned64-bit integer tokens. Conversion uses `from_chars` over the
original JSON source digits, preserving values above2^53 through2^64−1. The Json
parser's Float64 representation never determines the seed. Branch counts and
integer blade controls likewise require integer tokens. Angles/dimensions retain
their declared Float64 values. Native records and shared parameter validation
are owned by [PlantRecipe](native_plant_recipe.md).

## Verification

```sh
JOBS=6 make suite SUITE=outshine/src/generators/flora/PlantRecipeJson
PYTHONPATH=lib/src python3 test/experiments/render/native_plant_recipe_reference.py \
  --json --binary <native-reference-consumer> --output <system-temp-directory>
```

Two module cases run normally, with ASan/UBSan and container hardening:6/6PASS.
They exercise a usable canonical recipe, exact maximum64-bit seed, extended
version3 controls, every truncated prefix, insufficient input bytes, wrong types,
unknown/missing/duplicate fields, invalid dimensions and seed overflow.
The suite links the flora units and existing Json.cpp, without GPU/world/IO.
Both it and the dependent recipe suite pass together:12/12case arms. Product
units and independent consumer pass focused clang-tidy with full build flags;
case analysis retains only the existing shared Check.h findings (#603).

The independent consumer now accepts original canonical JSON as well as typed
fields. Both modes separately pass the same35-case Python comparison, including
full/subtree selection, five leaf arrangements, taper/rounded lengths, extended
branch controls, high seeds and bare wood. Every selected source ID, ownership,
path, variant and count agrees. Finite Float64 errors retain the existing2e-14
comparison tolerance; they are not a new surface/LOD/screen-space allowance.
No expected tables, hashes, pins or oracles are rewritten.

A private Metal probe reads the original recipe JSON, generates its native plan,
assembles/caches the prepared product, then renders it. A fresh hit process has
empty stdin and0generator calls. Both640×720PNG hashes equal the prior typed-input
portrait: `bc27866ac006a2ce934804c47381fb33c6de64a58d2b5b1ca8aa7b3baaaba279`.
The actual cold image was opened. It remains the same thin1.55-metre young tree
on black, not a visual improvement or a mature species. Inputs, compiler commands,
source/binary hashes, actual receipts and images remain privately under
`/tmp/outshine-native-plant-recipe-39bf9e07d/`.

## Remaining world work

This decoder is the recipe boundary, not catalogue/plan persistence or world
registration. Existing world trees still use whole-tree cards. The next consumer
must load recipes only when their prepared asset is missing, select budgeted
near products, keep valid far products, and prove forms/transitions/wind/materials
in actual Places. Finished asset hits must continue to avoid recipe parsing and
plan/geometry generation. No tree-count increase or mature-form scaling is implied.
