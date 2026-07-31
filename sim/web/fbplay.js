/* The player layer's reading half: campaign/mission files in, judge lines in, display rows out.
 * DOM-free and side-effect-free so the identical code answers in the browser and in a node harness
 * over a recorded events.log (doc/player-layer.md §1). It READS and OMITS; it adds no fact. */
'use strict';
(function (root) {

/* Every displayed row names the artefact line it came from, and the key is a CONSTRUCTOR ARGUMENT:
 * a row without a provenance key cannot be built at all (doc/player-layer.md §1, check 2). */
function FBRow(src, label, value) {
  if (!src) throw new Error('fbplay: row without a provenance key');
  return { Src: src, Label: label, Value: value === undefined ? '' : String(value) };
}

function StripComment(line) {
  const h = line.indexOf('#');
  return (h < 0 ? line : line.slice(0, h)).trim();
}

/* .fbc — one statement per line, `mission <path>` in ladder order. Unknown keywords are the C++
 * parser's error, not ours: we read the three keys we display and ignore the rest. */
function FBParseCampaign(text, file) {
  const c = { File: file, Name: file.replace(/\.fbc$/, ''), Headline: '', Missions: [], Time: '', Carry: '' };
  let headline = '';
  for (const raw of text.split('\n')) {
    if (!headline) {
      const m = raw.match(/^#\s*(\S.*?)\s*$/);
      if (m && /^(CAMPAIGN|[A-Z])/.test(m[1])) headline = m[1];
    }
    const line = StripComment(raw);
    if (!line) continue;
    const sp = line.indexOf(' ');
    const key = sp < 0 ? line : line.slice(0, sp);
    const val = sp < 0 ? '' : line.slice(sp + 1).trim();
    if (key === 'name') c.Name = val;
    else if (key === 'time') c.Time = val;
    else if (key === 'carry') c.Carry = val;
    else if (key === 'mission') c.Missions.push(val);
  }
  /* The headline is the file's own first comment sentence — quoted, never composed. */
  c.Headline = headline.replace(/^CAMPAIGN\s+[A-Z]?\d*\s*[-—]\s*/i, '').split(/\.\s|\.$/)[0];
  return c;
}

/* .fbm — the seat is the FIRST unit block, which is exactly the actor the wasm client flies
 * (FBAppWasm.cpp: gOwnship = gActors.front()). Only the fields the menus display are read. */
function FBParseMission(text, file) {
  const m = { File: file, Name: file.replace(/\.fbm$/, ''), TimeoutS: 0, Units: [], Headline: '' };
  let unit = null, headline = '';
  for (const raw of text.split('\n')) {
    if (!headline) {
      const h = raw.match(/^#\s*(\S.*?)\s*$/);
      if (h) headline = h[1];
    }
    const line = StripComment(raw);
    if (!line) continue;
    const sp = line.indexOf(' ');
    const key = sp < 0 ? line : line.slice(0, sp);
    const val = sp < 0 ? '' : line.slice(sp + 1).trim();
    if (key === 'unit') { unit = { Id: val, Team: 'friendly', Module: '', Objectives: [], Waypoints: 0 }; m.Units.push(unit); }
    else if (key === 'name') m.Name = val;
    else if (key === 'timeout') m.TimeoutS = parseFloat(val) || 0;
    else if (!unit) continue;
    else if (key === 'team') unit.Team = val;
    else if (key === 'module') unit.Module = val;
    else if (key === 'objective') unit.Objectives.push(val);
    else if (key === 'wp') unit.Waypoints++;
  }
  m.Headline = headline.split(/\.\s|\.$/)[0];
  m.Seat = m.Units.length ? m.Units[0] : null;
  return m;
}

/* One log line as the sinks write it: `t=12.3 INFO mission OBJECTIVE unit=x kind="…" state=met`.
 * Values are bare or double-quoted; the tag/event pair is positional. */
function FBParseLogLine(line) {
  const m = line.match(/^t=(-?[\d.]+)\s+(DEBUG|INFO|WARN|ERROR)\s+(\S+)\s+(\S+)\s*(.*)$/);
  if (!m) return null;
  const rec = { T: parseFloat(m[1]), Level: m[2], Tag: m[3], Event: m[4], F: {}, Raw: line };
  const re = /(\w+)=("([^"]*)"|\S+)/g;
  let f;
  while ((f = re.exec(m[5]))) rec.F[f[1]] = f[3] !== undefined ? f[3] : f[2];
  return rec;
}

/* The judge's own output for ONE seat, collected. Nothing is recomputed here: every field is a line
 * the two monitors published themselves (core/FBMissionMonitor.cpp, core/FBFlightMonitor.cpp).
 * A single-unit mission logs without a `unit=` field, so an unattributed line belongs to the seat. */
function FBCollectJudge(lines, seatId) {
  const j = { Objectives: [], Result: null, Ko: null, Suppressed: [], Waypoints: [], Lines: 0 };
  const mine = (r) => r.F.unit === undefined || r.F.unit === seatId;
  for (const line of lines) {
    const r = typeof line === 'string' ? FBParseLogLine(line) : line;
    if (!r || !mine(r)) continue;
    if (r.Tag === 'mission' && r.Event === 'OBJECTIVE') {
      j.Objectives.push({ Kind: r.F.kind, State: r.F.state, T: r.T }); j.Lines++;
    } else if (r.Tag === 'mission' && r.Event === 'RESULT') {
      j.Result = { Result: r.F.result, Reason: r.F.reason, T: r.T }; j.Lines++;
    } else if (r.Tag === 'mission' && r.Event === 'SUPPRESSED') {
      j.Suppressed.push({ Target: r.F.target, EmittingS: r.F.emittingS, AllowanceS: r.F.allowanceS });
    } else if (r.Tag === 'mission' && r.Event === 'WP_REACHED') {
      j.Waypoints.push({ Idx: r.F.idx, By: r.F.by, T: r.T });
    } else if (r.Tag === 'monitor' && r.Event === 'KO') {
      j.Ko = { Reason: r.F.reason, Detail: r.F.detail, T: r.T }; j.Lines++;
    }
  }
  /* The judge concludes ONCE per unit; a later conclusion would be a defect, not a re-read. */
  return j;
}

/* doc/player-layer.md §3.1, with the §2.2 default that an unmarked objective is PRIMARY — and today
 * NOTHING is marked, because no briefing format is built. So (a) reads every objective the SEAT
 * DECLARES in the .fbm, and an objective the judge published no state for is not `state=met`: a run
 * that died before the judge could speak completes nothing.
 *
 * (c) is read from the PHYSICAL judge's own `monitor KO` line instead of `UNIT_RESULT`, which the
 * browser never emits (only FBMissionRunner does, gym-side). The substitution can only be STRICTER:
 * no KO line ⇒ the flight monitor never tripped ⇒ UNIT_RESULT is neither CRASH nor LOC. So a
 * completion claimed here is a completion §3.1 grants; the converse is the gap in the doc. */
function FBCompletion(judge, declared) {
  const judged = !!judge.Result || !!judge.Ko;
  const met = judge.Objectives.filter((o) => o.State === 'met').length;
  const violated = judge.Objectives.filter((o) => o.State === 'violated').length;
  const known = declared === undefined || declared === null ? null : declared;
  return {
    Judged: judged,
    Declared: known,
    Completed: judged && known !== null && met === known && violated === 0 && !judge.Ko,
    MetCount: met,
    UnmetCount: known === null ? judge.Objectives.length - met : known - met,
    ViolatedCount: violated,
    Ko: !!judge.Ko,
    /* No declared objective at all: the flight plan is the whole verdict (§3.3, net-blind-cue). */
    Vacuous: judged && known === 0,
  };
}

/* The debrief as ROWS, each carrying its source. Postcondition (§1 check 2): the objective rows are
 * exactly the seat's OBJECTIVE lines — no set grew. */
function FBBuildDebrief(mission, judge, ctx) {
  const seat = mission && mission.Seat ? mission.Seat : null;
  const c = FBCompletion(judge, seat ? seat.Objectives.length : null);
  const src = (mission ? mission.File : 'events.log') + ': ';
  const head = [];
  if (ctx && ctx.Campaign) head.push(FBRow('save: step', 'campaign', ctx.Campaign + (ctx.Step ? '  rung ' + ctx.Step : '')));
  head.push(FBRow(src + 'name', 'mission', mission ? mission.Name : '(unknown)'));
  if (ctx && ctx.Attempt) head.push(FBRow('save: attempts', 'attempt', ctx.Attempt));
  head.push(FBRow(src + 'unit', 'seat', mission && mission.Seat ? mission.Seat.Id : '(unknown)'));

  const objectives = judge.Objectives.map((o) =>
    FBRow('events.log: mission OBJECTIVE kind=' + o.Kind, o.Kind, o.State));
  if (objectives.length !== judge.Objectives.length) throw new Error('fbplay: objective row set grew');
  /* The shortfall is NOT turned into rows — the objective table stays exactly the judge's lines
   * (§1 check 2). It is stated once, out of the mission file's own declaration. */
  const missing = seat && seat.Objectives.length > judge.Objectives.length
    ? FBRow(src + 'objective', 'declared without a published state',
            (seat.Objectives.length - judge.Objectives.length) + ' of ' + seat.Objectives.length)
    : null;

  const ending = [];
  if (judge.Result) {
    ending.push(FBRow('events.log: mission RESULT result', 'judge verdict', judge.Result.Result));
    ending.push(FBRow('events.log: mission RESULT reason', 'reason', judge.Result.Reason));
    ending.push(FBRow('events.log: mission RESULT t', 'concluded at', judge.Result.T.toFixed(1) + ' s'));
  }
  if (judge.Ko) {
    ending.push(FBRow('events.log: monitor KO reason', 'physical K.O.', judge.Ko.Reason));
    ending.push(FBRow('events.log: monitor KO detail', 'detail', judge.Ko.Detail));
  }
  for (const s of judge.Suppressed)
    ending.push(FBRow('events.log: mission SUPPRESSED', s.Target, s.EmittingS + ' s of ' + s.AllowanceS + ' s allowed'));
  if (judge.Waypoints.length)
    ending.push(FBRow('events.log: mission WP_REACHED', 'waypoints reached', judge.Waypoints.length +
      (mission && mission.Seat ? ' of ' + mission.Seat.Waypoints : '')));

  if (missing) ending.unshift(missing);
  return { Completion: c, Head: head, Objectives: objectives, Ending: ending };
}

/* The save: canonical text, one fact per line, campaign declaration order — the discipline
 * campaign-state.txt has, for the same reason (doc/player-layer.md §6). It is the layer's ONLY
 * mutable state and it lives in the client. */
function FBParseSave(text) {
  const save = { Steps: {} };
  for (const raw of (text || '').split('\n')) {
    const line = StripComment(raw);
    if (!line) continue;
    const p = line.split(/\s+/);
    if (p[0] !== 'step' || p.length < 3) continue;
    const rec = { Attempts: 0, Verdicts: 0, Completed: 0, Last: '-' };
    for (const kv of p.slice(3)) {
      const i = kv.indexOf('=');
      if (i < 0) continue;
      const k = kv.slice(0, i), v = kv.slice(i + 1);
      if (k === 'attempts') rec.Attempts = parseInt(v, 10) || 0;
      else if (k === 'verdicts') rec.Verdicts = parseInt(v, 10) || 0;
      else if (k === 'completed') rec.Completed = parseInt(v, 10) || 0;
      else if (k === 'last') rec.Last = v;
    }
    save.Steps[p[1] + ' ' + p[2]] = rec;
  }
  return save;
}

function FBFormatSave(save) {
  const keys = Object.keys(save.Steps).sort((a, b) => {
    const A = a.split(' '), B = b.split(' ');
    return A[0] === B[0] ? parseInt(A[1], 10) - parseInt(B[1], 10) : (A[0] < B[0] ? -1 : 1);
  });
  let out = '# flightbox player save v1\n';
  for (const k of keys) {
    const r = save.Steps[k];
    out += 'step ' + k + ' attempts=' + r.Attempts + ' verdicts=' + r.Verdicts +
           ' completed=' + r.Completed + ' last=' + r.Last + '\n';
  }
  return out;
}

function FBSaveStep(save, campaign, step) {
  return save.Steps[campaign + ' ' + step] || { Attempts: 0, Verdicts: 0, Completed: 0, Last: '-' };
}

/* doc/player-layer.md §4.2: the ladder is the .fbc's own mission order — there is no second graph.
 * Rung 1 is always open; rung k+1 opens once rung k has been flown TO A VERDICT. (§4.2 spells the
 * condition COMPLETED; the built preview uses "a verdict", see doc/player-layer.md §11.) */
function FBUnlocked(save, campaign, step) {
  return step <= 1 || FBSaveStep(save, campaign, step - 1).Verdicts > 0;
}

const api = { FBRow, FBParseCampaign, FBParseMission, FBParseLogLine, FBCollectJudge, FBCompletion,
              FBBuildDebrief, FBParseSave, FBFormatSave, FBSaveStep, FBUnlocked };
if (typeof module !== 'undefined' && module.exports) module.exports = api;
else Object.assign(root, api);

})(typeof window !== 'undefined' ? window : globalThis);
