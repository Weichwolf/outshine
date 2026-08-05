/* The player layer's screens: title (mod) select, campaign select, mission select, the run, the
 * debriefing. It owns exactly one piece of mutable state — the save in localStorage — and reads
 * everything else off the mod manifest, the campaign/mission files and the two judges' own log lines
 * (doc/player-layer.md §§1, 6). The simulation below it is untouched: the only things this file hands
 * the client are WHICH mod and WHICH mission (window.FB_MOD / window.FB_MISSION). */
'use strict';
(function () {

const SAVE_KEY = 'fb.player.save.v1';
const PREVIEW =
  'PLAYABLE — keyboard stick, master arm, station select, pickle and gun trigger all travel the ' +
  'command bus the AI uses, and a released round flies, hits and damages through the same apparatus ' +
  'fb-gym drives. Still missing: cockpit displays, lock/TD-box symbology and a campaign carry ' +
  '(doc/clients/clients.md 5.2/5.4).';

/* The cockpit strip is the same VIEW the debrief is: it reads the run's own log lines and omits.
 * Every row below resolves to one line the simulation wrote about the seat's own instruments. */
const CONTROLS = [
  ['P', 'take / release the stick'], ['↑↓', 'pitch'], ['←→', 'roll'],
  [', .', 'rudder'], ['W S', 'throttle'], ['B', 'speedbrake'], ['G', 'gear'],
  ['M', 'master arm'], ['1..9', 'station select'], ['ENTER', 'pickle'], ['SPACE', 'gun'],
  ['TAB', 'cockpit / tactical map'], ['C', 'directed view'], ['T', 'ground albedo'],
  ['DIR: K', 'hold the cut'], ['DIR: N [ ]', 'step the subject'],
  ['MAP: N B', 'select own unit'], ['MAP: [ ]', 'zoom'], ['MAP: E R', 'emcon silent / radiate'],
  ['MAP: H G', 'weapons hold / free'], ['MAP: A', 'engage the contact'], ['MAP: X', 'abort'],
];
const FEED_TAGS = {
  'sms RELEASE': 'ok', 'sms RELEASE_REJECTED': 'no', 'sms RELEASE_ENVELOPE': 'no',
  'sms LAUNCH_OUT_OF_ZONE': 'no', 'gun TRIGGER': 'ok', 'gun BURST_DROPPED': 'no',
  'gun HIT': 'ok', 'gun DRY': 'no', 'cmd CMD_REJECT': 'no', 'damage SYSTEM': 'no',
  'damage KILL': 'ok', 'damage DAMAGE': 'ok', 'damage CLUSTER': 'ok', 'stores IMPACT': 'ok',
  'stores DETONATION': 'ok', 'stores EXPIRED': 'no', 'hotas NO_STICK': 'no',
  'hotas QUEUE_FULL': 'no', 'hotas STICK': 'ok', 'monitor KO': 'no',
  // The commander's half: the word he gave, and what the receiving AI did with it.
  'order ISSUE': 'ok', 'order ACCEPTED': 'ok', 'order REFUSED': 'no', 'order WEAPONS_HOLD': 'no',
  'net JOIN': 'ok', 'net LOST': 'no', 'view MODE': 'ok',
};

const $ = (id) => document.getElementById(id);
const el = (tag, cls, text) => {
  const n = document.createElement(tag);
  if (cls) n.className = cls;
  if (text !== undefined) n.textContent = text;
  return n;
};
const esc = (s) => String(s === undefined || s === null ? '' : s);

function LoadSave() { try { return FBParseSave(localStorage.getItem(SAVE_KEY) || ''); } catch (e) { return FBParseSave(''); } }
function StoreSave(save) { try { localStorage.setItem(SAVE_KEY, FBFormatSave(save)); } catch (e) { /* private mode: the run still flies */ } }

async function Text(url) {
  const r = await fetch(url, { cache: 'no-cache' });
  if (!r.ok) throw new Error(url + ' -> HTTP ' + r.status);
  return r.text();
}

/* WHICH TITLES THIS BUILD CARRIES. `make wasm` writes one id per line for every mod manifest it found
 * and copies each manifest beside its missions, so this file names no mod, no directory and no
 * mission — five titles and one behave identically here. */
async function LoadMods() {
  const index = await Text('mods/index.txt');
  const ids = index.split('\n').map((s) => s.trim()).filter((s) => s);
  const out = [];
  for (const id of ids) {
    const m = JSON.parse(await Text('mods/' + id + '/mod.json'));
    m.Id = id;
    m.Root = 'mods/' + id + '/';
    /* A directory this manifest does not NAME is not this mod's — empty, never guessed. */
    m.MissionDir = m.missions ? m.Root + m.missions + '/' : '';
    m.CampaignDir = m.campaigns ? m.Root + m.campaigns + '/' : '';
    out.push(m);
  }
  return out;
}

/* `mission ../missions/x.fbm` in a .fbc is relative to the campaign directory — the mod's own, which
 * is why the manifest and not this file decides what that directory is called. */
function MissionUrl(mod, rel) {
  return new URL(rel, new URL(mod.CampaignDir, location.href)).pathname.replace(/^\//, '');
}
function MissionName(rel) { return rel.replace(/^.*\//, '').replace(/\.fbm$/, ''); }

async function LoadCampaigns(mod) {
  if (!mod.CampaignDir) return [];
  const index = await Text(mod.CampaignDir + 'index.txt');
  const files = index.split('\n').map((s) => s.trim()).filter((s) => s.endsWith('.fbc'));
  const out = [];
  for (const f of files) out.push(FBParseCampaign(await Text(mod.CampaignDir + f), f));
  return out;
}

/* ---------------- screens ---------------- */

function Screen(title, sub) {
  const ui = $('fb-ui');
  ui.innerHTML = '';
  ui.style.display = 'block';
  const head = el('div', 'fb-head');
  head.appendChild(el('h1', null, title));
  if (sub) head.appendChild(el('p', 'fb-sub', sub));
  ui.appendChild(head);
  const body = el('div', 'fb-body');
  ui.appendChild(body);
  return body;
}

/* THE FRONT DOOR: one card per title this build carries. It is not skipped when there is only one —
 * the screen is the same with one mod and with five, so nothing about it has to change when the next
 * title lands. */
async function ScreenMods() {
  const body = Screen('FLIGHTBOX', PREVIEW);
  let mods;
  try { mods = await LoadMods(); }
  catch (e) { body.appendChild(el('p', 'fb-err', 'no mods: ' + e.message + ' (run `make -C sim wasm`)')); return; }
  for (const m of mods) {
    const card = el('div', 'fb-card fb-open');
    card.appendChild(el('div', 'fb-card-t', m.id));
    card.appendChild(el('div', 'fb-card-s', m.name || ''));
    card.appendChild(el('div', 'fb-card-m',
      ['aircraft ' + (m.aircraft || '—'), 'missions ' + (m.missions || '—'),
       'campaigns ' + (m.campaigns || '—'), m.meshes ? 'meshes ' + m.meshes : null,
       m.depends ? 'depends ' + m.depends : null].filter((x) => x).join(' · ')));
    card.onclick = () => { location.search = '?mod=' + encodeURIComponent(m.Id); };
    body.appendChild(card);
  }
}

async function ScreenCampaigns(mod) {
  const body = Screen(mod.id.toUpperCase(), mod.name + ' — ' + PREVIEW);
  const save = LoadSave();
  let campaigns;
  try { campaigns = await LoadCampaigns(mod); }
  catch (e) { body.appendChild(el('p', 'fb-err', 'no campaigns: ' + e.message + ' (run `make -C sim wasm`)')); return; }

  if (mod.default_mission && mod.MissionDir) {
    const free = el('div', 'fb-card fb-open');
    free.appendChild(el('div', 'fb-card-t', 'FREE FLIGHT'));
    free.appendChild(el('div', 'fb-card-s', mod.default_mission + ' — the mod\'s own default sortie, outside every ladder'));
    free.onclick = () => { location.search = ModQ(mod) + '&mission=' + mod.default_mission; };
    body.appendChild(free);
  }

  for (const c of campaigns) {
    let flown = 0, done = 0;
    for (let i = 1; i <= c.Missions.length; i++) {
      const s = FBSaveStep(save, c.Name, i);
      if (s.Verdicts > 0) flown++;
      if (s.Completed > 0) done++;
    }
    const card = el('div', 'fb-card fb-open');
    card.appendChild(el('div', 'fb-card-t', c.Name));
    card.appendChild(el('div', 'fb-card-s', c.Headline));
    card.appendChild(el('div', 'fb-card-m',
      c.Missions.length + ' rungs · flown ' + flown + '/' + c.Missions.length +
      ' · completed ' + done + '/' + c.Missions.length + (c.Time ? ' · ' + c.Time : '')));
    card.onclick = () => { location.search = ModQ(mod) + '&campaign=' + encodeURIComponent(c.Name); };
    body.appendChild(card);
  }

  const foot = el('div', 'fb-foot');
  const back = el('button', null, '← titles');
  back.onclick = () => { location.search = ''; };
  foot.appendChild(back);
  const reset = el('button', null, 'reset progress');
  reset.onclick = () => { localStorage.removeItem(SAVE_KEY); ScreenCampaigns(mod); };
  foot.appendChild(reset);
  const dump = el('pre', 'fb-save', FBFormatSave(save));
  foot.appendChild(dump);
  body.appendChild(foot);
}

async function ScreenMissions(mod, name) {
  const campaigns = await LoadCampaigns(mod);
  const c = campaigns.find((x) => x.Name === name);
  if (!c) { location.search = ModQ(mod); return; }
  const body = Screen(c.Name, c.Headline +
    ' — the .fbc order IS the ladder; rung k+1 opens once rung k has been flown to a verdict. ' +
    'Each rung is flown STANDALONE: FBCampaignRunner is gym-only, so the browser carries neither the ' +
    'campaign state (units, ground, stores) nor the campaign clock from the rung before it ' +
    '(doc/missions/campaign.md §State, doc/clients/clients.md).');
  const save = LoadSave();

  for (let i = 0; i < c.Missions.length; i++) {
    const step = i + 1;
    const rec = FBSaveStep(save, c.Name, step);
    const open = FBUnlocked(save, c.Name, step);
    const card = el('div', 'fb-card ' + (open ? 'fb-open' : 'fb-locked'));
    const t = el('div', 'fb-card-t', 'RUNG ' + step + '   ' + MissionName(c.Missions[i]));
    if (!open) t.appendChild(el('span', 'fb-lock', '  LOCKED'));
    card.appendChild(t);
    const sub = el('div', 'fb-card-s', 'reading the mission file…');
    card.appendChild(sub);
    const meta = el('div', 'fb-card-m',
      rec.Attempts ? ('attempts ' + rec.Attempts + ' · verdicts ' + rec.Verdicts + ' · completed ' +
                      rec.Completed + ' · last ' + rec.Last)
                   : 'never flown');
    card.appendChild(meta);
    body.appendChild(card);

    Text(MissionUrl(mod, c.Missions[i])).then((text) => {
      const m = FBParseMission(text, MissionName(c.Missions[i]) + '.fbm');
      const seat = m.Seat;
      sub.textContent = m.Headline;
      const objs = el('div', 'fb-objs');
      objs.appendChild(el('span', 'fb-k', 'seat ' + (seat ? seat.Id + ' (' + seat.Module + ')' : '—') +
        ' · cast ' + m.Units.length + ' · timeout ' + m.TimeoutS + ' s'));
      if (seat && seat.Objectives.length)
        for (const o of seat.Objectives) objs.appendChild(el('span', 'fb-o', o));
      else objs.appendChild(el('span', 'fb-o fb-none', 'no objective declared — the flight plan is the whole verdict'));
      card.appendChild(objs);
    }).catch((e) => { sub.textContent = 'mission file unreadable: ' + e.message; });

    if (open) card.onclick = () => { location.search = ModQ(mod) + '&campaign=' + encodeURIComponent(c.Name) + '&step=' + step; };
  }

  const foot = el('div', 'fb-foot');
  const back = el('button', null, '← campaigns');
  back.onclick = () => { location.search = ModQ(mod); };
  foot.appendChild(back);
  body.appendChild(foot);
}

/* ---------------- the run ---------------- */

const Run = { Lines: [], Mod: null, Mission: null, Campaign: '', Step: 0, Attempt: 0, Recorded: false, Booted: false };

/* WER GERADE UMGESCHALTET HAT, WANN UND AUS WELCHER LAGE — die Zeile, die den Seitenwechsel lesbar
 * macht statt nur richtig. Beide Quellen sind Zeilen, die der Lauf ohnehin schreibt: die Phase, die
 * der Client im Sekundentakt loggt, und der Kommandostrom des Busses selbst (`cmd CMD_ISSUE/CMD_ACK
 * target=mfd_page`). Nichts wird hier berechnet: WELCHE Seite steht auf dem MFD, diese Zeile sagt nur,
 * wessen Bedienhandlung sie dorthin gebracht hat und wie das Geraet geantwortet hat. */
function Attention(r) {
  const att = $('fb-att');
  if (!att) return;
  const d = att.dataset;
  if (r.Tag === 'pilot' && r.Event === 'phase') d.phase = r.F.phase || '?';
  else if (r.Tag === 'cmd' && r.F.target === 'mfd_page') {
    if (r.Event === 'CMD_ISSUE') d.issue = 't=' + r.T.toFixed(1) + '  page ' + r.F.value;
    else if (r.Event === 'CMD_ACK') d.ack = r.F.outcome + ' +' + r.F.latencyS + 's';
    else if (r.Event === 'CMD_REJECT') d.ack = 'rejected ' + r.F.reason;
  } else return;
  att.innerHTML = '';
  const cell = (k, v) => {
    const n = el('span', 'fb-c');
    n.appendChild(el('b', 'fb-ck', k));
    n.appendChild(document.createTextNode(' ' + v));
    att.appendChild(n);
  };
  cell('PILOT PHASE', d.phase || '—');
  cell('MFD SELECT', d.issue || 'none yet');
  cell('ACK', d.ack || '—');
}

/* ---------------- the cockpit strip ----------------
 * Armament state and what just happened, both read off the run's own log lines and nothing else. The
 * armament row is the seat's PUBLISHED blocks (`hotas armament`, written from FBState); the feed
 * quotes the box that spoke. Nothing here is computed, so the strip can show no fact the run lacks. */
function Cockpit(r) {
  const arm = $('fb-arm'), feed = $('fb-feed');
  if (!arm || !feed) return;
  Attention(r);
  if (r.Tag === 'hotas' && r.Event === 'armament') {
    const f = r.F;
    arm.innerHTML = '';
    const cell = (k, v, cls) => { const n = el('span', 'fb-c' + (cls ? ' ' + cls : '')); n.appendChild(el('b', 'fb-ck', k)); n.appendChild(document.createTextNode(' ' + v)); arm.appendChild(n); };
    cell('STICK', f.stick === 'human' ? 'HUMAN' : 'AI', f.stick === 'human' ? 'fb-ok' : '');
    cell('ARM', f.arm === '1' ? 'ARM' : 'SAFE', f.arm === '1' ? 'fb-ok' : 'fb-no');
    cell('STA', f.station === '-1' ? '—' : f.station);
    cell('STORES', f.loaded + (f.released !== '0' ? ' (-' + f.released + ')' : ''));
    cell('GUN', f.rounds, f.gunReady === '1' ? 'fb-ok' : 'fb-no');
    cell('HITS', f.hits, f.effective === '1' ? '' : 'fb-no');
    if (f.effective !== '1') cell('STATE', 'COMBAT INEFFECTIVE', 'fb-no');
    return;
  }
  const cls = FEED_TAGS[r.Tag + ' ' + r.Event];
  if (!cls) return;
  const text = r.T.toFixed(1) + '  ' + r.Tag + ' ' + r.Event + '  ' +
    Object.keys(r.F).filter((k) => k !== 'unit').slice(0, 4).map((k) => k + '=' + r.F[k]).join(' ') +
    (r.F.unit ? '  [' + r.F.unit + ']' : '');
  /* A burst writes the same refusal fifty times a second. Collapsing consecutive repeats of one
   * tag/event OMITS — it never merges two different things and never invents a count. */
  const top = feed.firstChild;
  if (top && top.dataset.key === r.Tag + ' ' + r.Event) {
    top.dataset.n = String((parseInt(top.dataset.n, 10) || 1) + 1);
    top.textContent = text + '   x' + top.dataset.n;
    return;
  }
  const line = el('div', 'fb-fl ' + (cls === 'no' ? 'fb-no' : 'fb-ok'), text);
  line.dataset.key = r.Tag + ' ' + r.Event;
  line.dataset.n = '1';
  feed.insertBefore(line, feed.firstChild);
  while (feed.childNodes.length > 9) feed.removeChild(feed.lastChild);
}

/* Emscripten's stdout, i.e. exactly what the console shows — the two judges write their conclusions
 * there themselves (FBMissionMonitor::Conclude, FBFlightMonitor). Nothing else is intercepted. */
function CaptureLine(s) {
  console.log(s);
  const r = FBParseLogLine(s);
  if (!r) return;
  Run.Lines.push(r);
  if (r.Tag === 'gpu' && r.Event === 'world_ready') Run.Booted = true;
  Cockpit(r);
  const seat = Run.Mission && Run.Mission.Seat ? Run.Mission.Seat.Id : undefined;
  const mine = r.F.unit === undefined || r.F.unit === seat;
  if (!mine) return;
  if ((r.Tag === 'mission' && r.Event === 'RESULT') || (r.Tag === 'monitor' && r.Event === 'KO'))
    setTimeout(ShowDebrief, 50);   /* the OBJECTIVE lines precede RESULT in the same emit */
}

function RecordAttempt(judge) {
  if (Run.Recorded || !Run.Campaign) return;
  Run.Recorded = true;
  const save = LoadSave();
  const key = Run.Campaign + ' ' + Run.Step;
  const rec = save.Steps[key] || (save.Steps[key] = { Attempts: 0, Verdicts: 0, Completed: 0, Last: '-' });
  const seat = Run.Mission && Run.Mission.Seat ? Run.Mission.Seat : null;
  const c = FBCompletion(judge, seat ? seat.Objectives.length : null);
  if (c.Judged) {
    rec.Verdicts++;
    rec.Last = judge.Ko ? ('KO/' + judge.Ko.Reason) : judge.Result.Result;
    if (c.Completed) rec.Completed++;
  }
  StoreSave(save);
}

function ShowDebrief() {
  if ($('fb-ui').dataset.screen === 'debrief') return;
  const seat = Run.Mission && Run.Mission.Seat ? Run.Mission.Seat.Id : undefined;
  const judge = FBCollectJudge(Run.Lines, seat);
  RecordAttempt(judge);
  const d = FBBuildDebrief(Run.Mission, judge, { Campaign: Run.Campaign, Step: Run.Step, Attempt: Run.Attempt });

  const body = Screen('DEBRIEFING', PREVIEW);
  $('fb-ui').dataset.screen = 'debrief';
  $('fb-hud').style.display = 'none';
  const verdict = el('div', 'fb-verdict ' + (d.Completion.Completed ? 'fb-ok' : 'fb-no'),
    d.Completion.Judged ? (d.Completion.Completed ? 'MISSION COMPLETED' : 'MISSION NOT COMPLETED')
                        : 'NO VERDICT — the judge had not concluded when the run was left');
  body.appendChild(verdict);

  const table = (rows, caption) => {
    if (!rows.length) return;
    body.appendChild(el('h2', null, caption));
    const t = el('table', 'fb-t');
    for (const r of rows) {
      const tr = el('tr');
      tr.dataset.src = r.Src;                       /* the provenance key, inspectable in the DOM */
      tr.appendChild(el('td', 'fb-l', esc(r.Label)));
      tr.appendChild(el('td', 'fb-v', esc(r.Value)));
      t.appendChild(tr);
    }
    body.appendChild(t);
  };
  table(d.Head, 'run');
  if (d.Objectives.length) table(d.Objectives, 'objectives, as the mission declared them');
  else body.appendChild(el('p', 'fb-none',
    'This seat declared no objective, so the judge published no objective vector: the flight plan is the whole verdict.'));
  table(d.Ending, 'how it ended');

  body.appendChild(el('p', 'fb-note',
    'Primary and secondary are not separated: the .fbm vocabulary does not carry the distinction and no ' +
    'briefing format is built, so every declared objective counts as primary — the completion rule then ' +
    'degrades exactly into the judge\'s own verdict (doc/player-layer.md §2.2, §3.2). No score, by decision.'));

  const foot = el('div', 'fb-foot');
  const again = el('button', null, 'fly again');
  again.onclick = () => location.reload();
  foot.appendChild(again);
  if (Run.Campaign) {
    const back = el('button', null, '← mission select');
    back.onclick = () => { location.search = ModQ(Run.Mod) + '&campaign=' + encodeURIComponent(Run.Campaign); };
    foot.appendChild(back);
  }
  const home = el('button', null, '← campaigns');
  home.onclick = () => { location.search = ModQ(Run.Mod); };
  foot.appendChild(home);
  const keep = el('button', null, 'keep watching');
  keep.onclick = () => { $('fb-ui').style.display = 'none'; $('fb-ui').dataset.screen = ''; $('fb-bar').style.display = 'flex'; $('fb-hud').style.display = 'block'; };
  foot.appendChild(keep);
  body.appendChild(foot);
}

function StartRun(mod, missionName, campaign, step, manual) {
  Run.Mod = mod;
  Run.Campaign = campaign || '';
  Run.Step = step || 0;
  $('fb-ui').style.display = 'none';
  const bar = $('fb-bar');
  bar.style.display = 'flex';
  bar.appendChild(el('span', 'fb-bar-t', manual ? '?ap=manual sandbox — no mission, no judge'
    : (campaign ? campaign + ' · rung ' + step + ' · ' : '') + missionName));
  const end = el('button', null, 'end run & debrief');
  end.onclick = ShowDebrief;
  bar.appendChild(end);
  const leave = el('button', null, '← menu');
  leave.onclick = () => { location.search = ModQ(mod) + (campaign ? '&campaign=' + encodeURIComponent(campaign) : ''); };
  bar.appendChild(leave);

  const hud = $('fb-hud');
  hud.style.display = 'block';
  const keys = el('div', 'fb-keys');
  for (const [k, what] of CONTROLS) {
    const n = el('span', 'fb-c');
    n.appendChild(el('b', 'fb-ck', k));
    n.appendChild(document.createTextNode(' ' + what));
    keys.appendChild(n);
  }
  hud.appendChild(keys);
  const att = el('div', 'fb-armrow'); att.id = 'fb-att';
  att.appendChild(el('span', 'fb-c', 'waiting for the pilot…'));
  hud.appendChild(att);
  const arm = el('div', 'fb-armrow'); arm.id = 'fb-arm';
  arm.appendChild(el('span', 'fb-c', 'waiting for the jet…'));
  hud.appendChild(arm);
  const feed = el('div', 'fb-feedrow'); feed.id = 'fb-feed';
  hud.appendChild(feed);

  if (campaign) {
    const save = LoadSave();
    const key = campaign + ' ' + step;
    const rec = save.Steps[key] || (save.Steps[key] = { Attempts: 0, Verdicts: 0, Completed: 0, Last: '-' });
    rec.Attempts++;
    Run.Attempt = rec.Attempts;
    StoreSave(save);
  }

  if (!manual)
    Text(mod.MissionDir + missionName + '.fbm')
      .then((t) => { Run.Mission = FBParseMission(t, missionName + '.fbm'); })
      .catch(() => { Run.Mission = null; });

  /* The ONLY things the layer hands the simulation: which mod, and which file of it to fly. Read in
   * FBAppWasm.cpp exactly the way FB_TILES_URL / FB_ORIGIN_LAT already are — the client resolves both
   * against the same manifest this file read, and the manual sandbox has no mission at all. */
  window.FB_MOD = mod.Id;
  if (!manual) window.FB_MISSION = missionName;
  window.Module = window.Module || {};
  window.Module.print = CaptureLine;
  window.Module.printErr = CaptureLine;
  const s = document.createElement('script');
  s.src = 'gpu.js';
  document.body.appendChild(s);
}

/* ---------------- entry ---------------- */

/* Every link below the title screen carries the mod, because every path below it is the mod's. */
function ModQ(mod) { return '?mod=' + encodeURIComponent(mod.Id); }

async function Boot() {
  const q = new URLSearchParams(location.search);
  const mods = await LoadMods().catch(() => []);
  const mod = mods.find((m) => m.Id === q.get('mod')) || (q.get('ap') === 'manual' ? mods[0] : null);
  if (!mod) { ScreenMods(); return; }
  if (q.get('ap') === 'manual') { StartRun(mod, '', '', 0, true); return; }   /* the existing sandbox link */
  const mission = q.get('mission');
  const campaign = q.get('campaign');
  const step = parseInt(q.get('step') || '0', 10);
  if (mission) { StartRun(mod, mission.replace(/[^A-Za-z0-9._-]/g, ''), '', 0); return; }
  if (campaign && step > 0) {
    const cs = await LoadCampaigns(mod);
    const c = cs.find((x) => x.Name === campaign);
    if (!c || step > c.Missions.length) { location.search = ModQ(mod); return; }
    StartRun(mod, MissionName(c.Missions[step - 1]), campaign, step);
    return;
  }
  if (campaign) { ScreenMissions(mod, campaign); return; }
  ScreenCampaigns(mod);
}

window.addEventListener('DOMContentLoaded', Boot);

})();
