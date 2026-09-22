Type: task
State: active
Architecture: ready
Priority: P1
Parent: 2139
Depends:
Area: tooling

# Scope document checks and isolate verification worktrees

## Problem

`make lint` reruns every translation unit and tool self-test after board-only
edits. `test/run.sh` already separates binaries by absolute checkout path;
the global instruction to stop all editing during a gate wastes that isolation.

## Decision

Add `make lint-docs` without an `all` dependency. Reuse the existing board ID,
closure, edge, size and AGENTS reference claims; no second validator or cached
success flag. This gate does not certify C++, shaders, public API documentation
or scripts. Those changes still require full lint and relevant tests.

Document one editing worktree and one immutable verification worktree. Associate
results with the tested commit and toolchain. Separate build and mutable test
outputs; allow editing in the other worktree. Run only one heavy gate on this
machine, with explicit clang-tidy worker control. Do not run `spotless` while a
different worktree is testing: it removes test nests across checkouts.

## Acceptance

- `make lint-docs` passes existing rules without building the engine.
- An oversized temporary work item fails the same gate; removing it restores
  success. No source or rule removed to obtain a green result.
- `make format` and `make lint` pass for the tooling change.
- AGENTS states gate scope, commit association, failure handling and reuse only
  for unchanged previously tested code. Material work resumes after this step.
