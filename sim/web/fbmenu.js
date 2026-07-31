/* The player layer's screens: campaign select, mission select, the run, the debriefing.
 * It owns exactly one piece of mutable state — the save in localStorage — and reads everything else
 * off the campaign/mission files and the two judges' own log lines (doc/player-layer.md §§1, 6).
 * The simulation below it is untouched: the only thing this file hands the client is WHICH mission
 * to fly (window.FB_MISSION). */
'use strict';
(function () {

const SAVE_KEY = 'fb.player.save.v1';
const DEFAULT_MISSION = 'payerne-full';   /* the client's own default: FBAppWasm.cpp kDefaultMissionUrl */
const PREVIEW =
  'PREVIEW — the browser flies, renders and is judged, but has no weapon release path, no damage ' +
  'path and no bound stick (doc/clients/clients.md 5.1–5.3). Combat rungs run to their timeout.';

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

/* `mission ../missions/x.fbm` in a .fbc is relative to the campaign directory. */
function MissionUrl(rel) { return new URL(rel, new URL('campaigns/', location.href)).pathname.replace(/^\//, ''); }
function MissionName(rel) { return rel.replace(/^.*\//, '').replace(/\.fbm$/, ''); }

async function LoadCampaigns() {
  const index = await Text('campaigns/index.txt');
  const files = index.split('\n').map((s) => s.trim()).filter((s) => s.endsWith('.fbc'));
  const out = [];
  for (const f of files) out.push(FBParseCampaign(await Text('campaigns/' + f), f));
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

async function ScreenCampaigns() {
  const body = Screen('FLIGHTBOX', PREVIEW);
  const save = LoadSave();
  let campaigns;
  try { campaigns = await LoadCampaigns(); }
  catch (e) { body.appendChild(el('p', 'fb-err', 'no campaigns: ' + e.message + ' (run `make -C sim wasm`)')); return; }

  const free = el('div', 'fb-card fb-open');
  free.appendChild(el('div', 'fb-card-t', 'FREE FLIGHT'));
  free.appendChild(el('div', 'fb-card-s', DEFAULT_MISSION + ' — the client\'s own default sortie, outside every ladder'));
  free.onclick = () => { location.search = '?mission=' + DEFAULT_MISSION; };
  body.appendChild(free);

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
    card.onclick = () => { location.search = '?campaign=' + encodeURIComponent(c.Name); };
    body.appendChild(card);
  }

  const foot = el('div', 'fb-foot');
  const reset = el('button', null, 'reset progress');
  reset.onclick = () => { localStorage.removeItem(SAVE_KEY); ScreenCampaigns(); };
  foot.appendChild(reset);
  const dump = el('pre', 'fb-save', FBFormatSave(save));
  foot.appendChild(dump);
  body.appendChild(foot);
}

async function ScreenMissions(name) {
  const campaigns = await LoadCampaigns();
  const c = campaigns.find((x) => x.Name === name);
  if (!c) { location.search = ''; return; }
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

    Text(MissionUrl(c.Missions[i])).then((text) => {
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

    if (open) card.onclick = () => { location.search = '?campaign=' + encodeURIComponent(c.Name) + '&step=' + step; };
  }

  const foot = el('div', 'fb-foot');
  const back = el('button', null, '← campaigns');
  back.onclick = () => { location.search = ''; };
  foot.appendChild(back);
  body.appendChild(foot);
}

/* ---------------- the run ---------------- */

const Run = { Lines: [], Mission: null, Campaign: '', Step: 0, Attempt: 0, Recorded: false, Booted: false };

/* Emscripten's stdout, i.e. exactly what the console shows — the two judges write their conclusions
 * there themselves (FBMissionMonitor::Conclude, FBFlightMonitor). Nothing else is intercepted. */
function CaptureLine(s) {
  console.log(s);
  const r = FBParseLogLine(s);
  if (!r) return;
  Run.Lines.push(r);
  if (r.Tag === 'gpu' && r.Event === 'world_ready') Run.Booted = true;
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
    back.onclick = () => { location.search = '?campaign=' + encodeURIComponent(Run.Campaign); };
    foot.appendChild(back);
  }
  const home = el('button', null, '← campaigns');
  home.onclick = () => { location.search = ''; };
  foot.appendChild(home);
  const keep = el('button', null, 'keep watching');
  keep.onclick = () => { $('fb-ui').style.display = 'none'; $('fb-ui').dataset.screen = ''; $('fb-bar').style.display = 'flex'; };
  foot.appendChild(keep);
  body.appendChild(foot);
}

function StartRun(missionName, campaign, step, manual) {
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
  leave.onclick = () => { location.search = campaign ? '?campaign=' + encodeURIComponent(campaign) : ''; };
  bar.appendChild(leave);

  if (campaign) {
    const save = LoadSave();
    const key = campaign + ' ' + step;
    const rec = save.Steps[key] || (save.Steps[key] = { Attempts: 0, Verdicts: 0, Completed: 0, Last: '-' });
    rec.Attempts++;
    Run.Attempt = rec.Attempts;
    StoreSave(save);
  }

  if (!manual)
    Text('missions/' + missionName + '.fbm')
      .then((t) => { Run.Mission = FBParseMission(t, missionName + '.fbm'); })
      .catch(() => { Run.Mission = null; });

  /* The ONE thing the layer hands the simulation: which file to fly. Read in FBAppWasm.cpp exactly
   * the way FB_TILES_URL / FB_ORIGIN_LAT already are — and the manual sandbox has no mission at all. */
  if (!manual) window.FB_MISSION = missionName;
  window.Module = window.Module || {};
  window.Module.print = CaptureLine;
  window.Module.printErr = CaptureLine;
  const s = document.createElement('script');
  s.src = 'gpu.js';
  document.body.appendChild(s);
}

/* ---------------- entry ---------------- */

function Boot() {
  const q = new URLSearchParams(location.search);
  if (q.get('ap') === 'manual') { StartRun('', '', 0, true); return; }   /* the existing sandbox link */
  const mission = q.get('mission');
  const campaign = q.get('campaign');
  const step = parseInt(q.get('step') || '0', 10);
  if (mission) { StartRun(mission.replace(/[^A-Za-z0-9._-]/g, ''), '', 0); return; }
  if (campaign && step > 0) {
    LoadCampaigns().then((cs) => {
      const c = cs.find((x) => x.Name === campaign);
      if (!c || step > c.Missions.length) { location.search = ''; return; }
      StartRun(MissionName(c.Missions[step - 1]), campaign, step);
    });
    return;
  }
  if (campaign) { ScreenMissions(campaign); return; }
  ScreenCampaigns();
}

window.addEventListener('DOMContentLoaded', Boot);

})();
