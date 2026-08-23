Type: task
Area: src, include, tools, test/harness/claims
Tags: rule, hygiene, walk

# The source carries no commentary

Owner directive, 2026-08-23, binding and absolute: `src/`, `include/` and `tools/` carry no
`//` and no `/* */`. Not a line above a function, not a trailing note, not a derivation, not a
TODO, not a board number. Names and structure carry the meaning; a number's origin lives in its
board item and in the commit that set it. `test/` is the one place prose may stand, because a
proof explains what it proves.

The rule is a walk, not a habit repeated per file: `test/harness/claims/TheSourceCarriesNoCommentary`
reads every `.cpp`, `.h` and `.msl` under the three roots with a literal-aware scanner -- a `//`
inside a string is the data `Script.cpp` and `Style.cpp` parse, never prose -- and refuses the
first comment it finds with its `file:line`.

## Comments

- 2026-08-23 -- 636 comment lines stood in 125 files plus one three-line block in
  `subjectLitTextured.msl`; all removed, 702 deletions against 187 re-emitted lines (a line
  that carried a trailing comment is re-emitted trimmed).
- **Proving test**: `test/harness/claims/TheSourceCarriesNoCommentary` -- 426 source files
  walked, zero narrating.
- **Negative control**: one `// a comment the rule forbids` inserted at the head of
  `src/audio/BusGraph.cpp` -> `FAIL harness/claims/TheSourceCarriesNoCommentary`, the run
  naming `src/audio/BusGraph.cpp:1`; removed, green again.
- The rule is also written into `.claude/agents/architecture-reviewer.md`, so the hourly
  review files every comment it meets -- and files harder any comment the hour's own work
  introduced, because that one was written after the rule.
