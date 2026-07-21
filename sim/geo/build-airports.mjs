// Build geo/airports.json — the WORLD runway database — from OurAirports (CC0 public domain):
//   airports.csv + runways.csv -> { ICAO: { icao, runways: [{ident, heading_deg, lat, lon, elev_m, length_m, surface}] } }
// Each runway.csv row is one physical strip with TWO ends (le/he); each usable end (has a threshold
// lat/lon) becomes one entry the resolver can take off from / land on. Closed runways are dropped.
//
//   node geo/build-airports.mjs <airports.csv> <runways.csv> <out.json>
//   (or `node geo/build-airports.mjs` to fetch the current OurAirports data itself)
import { readFileSync, writeFileSync } from 'node:fs';

const FT = 0.3048;
const SRC = 'https://davidmegginson.github.io/ourairports-data';

function parseCSV(text) {
  const rows = []; let row = [], field = '', inq = false;
  for (let i = 0; i < text.length; i++) {
    const c = text[i];
    if (inq) {
      if (c === '"') { if (text[i + 1] === '"') { field += '"'; i++; } else inq = false; }
      else field += c;
    } else if (c === '"') inq = true;
    else if (c === ',') { row.push(field); field = ''; }
    else if (c === '\n') { row.push(field); rows.push(row); row = []; field = ''; }
    else if (c !== '\r') field += c;
  }
  if (field.length || row.length) { row.push(field); rows.push(row); }
  return rows;
}
const toObjects = (rows) => { const h = rows[0]; return rows.slice(1).map((r) => Object.fromEntries(h.map((k, i) => [k, r[i]]))); };

async function text(pathOrUrl) {
  if (pathOrUrl.startsWith('http')) { const r = await fetch(pathOrUrl); if (!r.ok) throw new Error(`${pathOrUrl} -> ${r.status}`); return r.text(); }
  return readFileSync(pathOrUrl, 'utf8');
}

const [, , aCsv, rCsv, outArg] = process.argv;
const airportsCsv = aCsv ?? `${SRC}/airports.csv`;
const runwaysCsv = rCsv ?? `${SRC}/runways.csv`;
const out = outArg ?? new URL('./airports.json', import.meta.url).pathname;

const airports = toObjects(parseCSV(await text(airportsCsv)));
const runways = toObjects(parseCSV(await text(runwaysCsv)));

const num = (v) => (v === '' || v == null ? null : Number(v));
const apInfo = new Map();                                    // ident -> {lat, lon, elFt} (airport reference point)
for (const a of airports) if (a.ident) apInfo.set(a.ident, { lat: num(a.latitude_deg), lon: num(a.longitude_deg), elFt: num(a.elevation_ft) });

const headFromIdent = (id) => { const m = String(id).match(/^\d+/); return m ? ((Number(m[0]) % 36) * 10) : null; };
// move along a great-circle bearing by dist metres — for placing a threshold from the airport reference point
const R = 6371000;
function geoOffset(lat, lon, brgDeg, dist) {
  const d = dist / R, b = brgDeg * Math.PI / 180, la = lat * Math.PI / 180, lo = lon * Math.PI / 180;
  const la2 = Math.asin(Math.sin(la) * Math.cos(d) + Math.cos(la) * Math.sin(d) * Math.cos(b));
  const lo2 = lo + Math.atan2(Math.sin(b) * Math.sin(d) * Math.cos(la), Math.cos(d) - Math.sin(la) * Math.sin(la2));
  return [la2 * 180 / Math.PI, lo2 * 180 / Math.PI];
}

const db = {};
for (const r of runways) {
  if (r.closed === '1') continue;
  const key = r.airport_ident; if (!key) continue;
  const lenM = num(r.length_ft) != null ? Math.round(num(r.length_ft) * FT) : null;
  const surface = r.surface || '';
  const ap = apInfo.get(key);
  const ends = [
    { id: r.le_ident, hd: num(r.le_heading_degT), lat: num(r.le_latitude_deg), lon: num(r.le_longitude_deg), el: num(r.le_elevation_ft) },
    { id: r.he_ident, hd: num(r.he_heading_degT), lat: num(r.he_latitude_deg), lon: num(r.he_longitude_deg), el: num(r.he_elevation_ft) },
  ];
  for (const e of ends) {
    if (!e.id) continue;
    const heading = e.hd != null ? e.hd : headFromIdent(e.id);
    if (heading == null) continue;
    let lat = e.lat, lon = e.lon;
    if (lat == null || lon == null) {
      // no surveyed threshold: derive it from the airport reference point — the threshold sits half the
      // runway length back along the takeoff direction (reciprocal heading) from the field midpoint.
      if (!ap || ap.lat == null || ap.lon == null) continue;
      [lat, lon] = geoOffset(ap.lat, ap.lon, (heading + 180) % 360, (lenM ?? 1500) / 2);
    }
    const elFt = e.el != null ? e.el : (ap ? ap.elFt : null);
    (db[key] ??= { icao: key, runways: [] }).runways.push({
      ident: e.id, heading_deg: Math.round(heading * 10) / 10, lat: Math.round(lat * 1e6) / 1e6, lon: Math.round(lon * 1e6) / 1e6,
      elev_m: elFt != null ? Math.round(elFt * FT * 10) / 10 : 0,
      length_m: lenM, surface,
    });
  }
}

writeFileSync(out, JSON.stringify(db));
const ends = Object.values(db).reduce((s, a) => s + a.runways.length, 0);
console.error(`airports.json: ${Object.keys(db).length} airports, ${ends} runway-ends -> ${out}`);
