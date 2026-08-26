Type: task
State: open
Parent: 1953
Area: architecture

# The build enforces the tier, and not an audit

`LayerReaches` in `test/run.sh` states what each directory may include and `--audit-layers` refuses
a source that crosses it. That is a check that runs AFTER a violation compiles.

**Unreal spends a whole build system on making this a compiler error instead.** A module's
`Build.cs` names its public dependencies; headers live in `Public/` or `Private/`; a module that
did not declare a dependency cannot even FIND the header, because the include path does not carry
it. The violation is unspellable rather than reported.

Take Unreal's answer: per-tier include paths, so `base/` compiles with no path to `content/` and a
cross-tier include fails at the `#include`, naming the file and the line. `--audit-layers` stays as
the guard for what the include path cannot see -- a cycle between two modules inside one tier.

- [ ] each tier compiles with only its declared tiers on the include path
- [ ] a deliberate cross-tier include fails at the `#include`, proven by a case
- [ ] `--audit-layers` still refuses an in-tier cycle, proven by the existing case
