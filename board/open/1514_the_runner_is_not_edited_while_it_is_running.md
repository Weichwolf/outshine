Type: bug
Area: harness
Tags: instrument

**The runner is not edited while it is running**

[MEASURED] a full run died at test 3114 of ~1718 with

```
./test/run.sh: line 762: syntax error near unexpected token `done'
```

and `sh -n test/run.sh` on the very same file came back clean. **The file was not broken. It was
MOVED under the interpreter.**

**`sh` reads a script incrementally, by byte offset, as it executes.** Rewriting `test/run.sh` while a
run is in flight shifts every line after the edit, so the interpreter's next read lands in the middle
of something -- and the failure appears **wherever the run happened to be**, minutes later, with a
message about a line that is correct.

**This session has been careful never to edit `src/` during a run** and had no rule about the runner
itself, which is the one file whose edit is destructive *while being read* rather than merely
confusing.

## What must be true

- [ ] **A run works from a copy**, so the tree is free while it is in flight: `test/run.sh` copies
      itself to the build directory and re-executes, or the first thing it does is read itself whole
- [ ] **The failure is unmistakable if it happens anyway** -- a run that dies on its own syntax says
      so rather than reporting a shell error at a line that is fine

## Comments

**The tell is that the error names a line that is correct.** A syntax error in a file that passes
`sh -n` is not a syntax error; it is a file that changed. *Worth writing down because the next
occurrence will look exactly as baffling as this one did.*
