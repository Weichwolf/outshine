Type: defect
State: active
Architecture: ready
Priority: P0
Parent: 2169
Depends:
Area: harness, tooling
Tags: timeout, ownership, realtime

# Timed-out tests release their entire process group

## Problem and evidence

3420ca30a Places suite reports both Graz variants TIMEOUT after 120 s, yet clients
42173/42232 remain alive under PID 1 in groups 42169/42228. Subsequent Husum is
UNPREPARED while these heavy renders still run. The suite was explicitly aborted;
all owned clients were killed and their actual exit confirmed. No full-suite PASS.
The earlier direct bounded client loop correctly waited for each client and gave 7/10
Refined captures. Do not attribute the later contaminated failures to new engine defects.

RunWithTimeout sends TERM, then schedules KILL after two seconds. Parent exit wakes
its caller, which invokes KillRunning and terminates the watchdog before escalation.
The former KillRunning only sends TERM. A TERM-ignoring descendant therefore survives.

## Binding repair

Owner: test/run.sh, test/scripts/test_timeout_cleanup.py. Preserve isolated owned groups,
120-s execution budgets, markers, measured verdicts, nest locks and foreign processes.
After the case owner exits or the gate aborts, TERM then KILL every owned process group;
reap direct children and confirm group disappearance before admitting another case.
Bound cleanup; if a group cannot disappear, fail the gate instead of starting more work.
The watchdog retains graceful escalation while the case owner remains alive. No process
name scan, broad kill, detached child allowance or relaxed test/frame budget.

## Independent acceptance

Use a real subprocess that installs TERM-ignore before publishing its PID. Its parent
exits immediately on TERM; test timeout, normal owner exit and runner interruption.
Assert descendant death and a separately owned live sentinel remains unaffected.
Restored TERM-only cleanup must produce actual test FAIL; cleanup the fixture afterward.
Commands: python3 test/scripts/test_timeout_cleanup.py; make format; relevant harness
claims; full Places suite and all Place PNGs opened; LINT_JOBS=2 make lint. Logs in temp.

## Repair evidence

320e3c9a2 implements bounded TERM/KILL cleanup before the next case, without changing
execution budgets. All three real subprocess cases PASS; restored TERM-only cleanup
produces three actual FAILs. Foreign sentinel remains alive in each repaired case.
Harness claims and both Husum render variants: 34/34 PASS, Husum 17.5/17.3 s.
No client survives this gate. Husum PNG opened: buildings and harbor visible; sawtooth
shoreline, uniform facades and flat water remain. At 4844c3196 full lint/tidy/API PASS.
Complete Places: 38/44 PASS, 4 TIMEOUT and 2 UNPREPARED in the three known red Places.
All four timed-out clients are gone; Hockenheim separately PASS. Seven Refined PNGs and
three fresh Playable diagnostics opened. No global quality PASS; no execution budget changed.
Logs: /tmp/outshine-timeout-cleanup-{bounded-test,negative}.log and
/tmp/outshine-timeout-cleanup-claims-and-husum.log.
