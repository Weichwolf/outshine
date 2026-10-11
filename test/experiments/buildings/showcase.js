'use strict';
const $ = id => document.getElementById(id);
const integer = value => new Intl.NumberFormat('de-CH').format(value);
const short = value => (value || 'unbekannt').slice(0, 9);
const names = { all: 'Alle', plan: 'Gebäudepläne', esri: 'Esri-Referenz', quelle: 'OSM-Quelle', nah: 'Nahdetail', fern: 'Ferne', stadt: 'Stadt', cache: 'Cache', vergleich: 'Vergleich', neu: 'Neu' };
const methods = { expanded: ['Split-Liste', '#ad9165'], vectorized: ['Voll-Liste / NumPy', '#7e899b'], compact: ['Kompakter Repeat', '#496b66'],
  bands: ['Direkte Bandflächen', '#496b66'], union: ['Manifold · 3D-Union', '#ad9165'] };
const metrics = {
  median_generation_ms: ['Erzeugung · ms', 1, 'ms'],
  repeat_payload_bytes: ['Payload · MiB', 1 / 1048576, 'MiB'],
  process_peak_rss_bytes: ['Prozesspeak · MiB', 1 / 1048576, 'MiB']
};
let manifest, filter = 'all', metric = 'median_generation_ms', comparison = 'massing';

function element(tag, text, className) {
  const node = document.createElement(tag);
  if (text !== undefined) node.textContent = text;
  if (className) node.className = className;
  return node;
}

function buttons(target, entries, selected, callback) {
  target.replaceChildren(...entries.map(([key, label]) => {
    const button = element('button', label);
    button.type = 'button';
    button.setAttribute('aria-pressed', key === selected);
    button.onclick = () => callback(key);
    return button;
  }));
}

function renderGallery() {
  const categories = new Set(manifest.images.map(item => item.category));
  buttons($('filters'), Object.entries(names).filter(([key]) => key === 'all' || categories.has(key)), filter,
    value => { filter = value; renderGallery(); });
  $('gallery').replaceChildren(...manifest.images.filter(item => filter === 'all' || item.category === filter).map(item => {
    const card = element('article', undefined, 'card');
    if (item.status === 'ready') {
      const button = element('button', undefined, 'image-button');
      button.type = 'button';
      const image = element('img');
      image.src = item.image; image.alt = item.title; image.loading = 'lazy';
      button.setAttribute('aria-label', item.title + ' vergrößern');
      button.append(image);
      button.onclick = () => {
        $('large').src = item.image; $('large').alt = item.title;
        $('large-caption').textContent = `${item.title} · ${item.profile.join(' × ')} · Bild-Commit ${short(item.commit)}`;
        $('viewer').showModal();
      };
      card.append(button);
    } else card.append(element('div', 'Unvollständig: ' + item.error, 'incomplete'));
    const body = element('div', undefined, 'card-body');
    body.append(element('h3', item.title), element('p', item.caption));
    const details = element('div', undefined, 'details');
    for (const text of [`${integer(item.triangles || 0)} Dreiecke`, Number.isFinite(item.openings) ? `${integer(item.openings)} ${item.representation === 'functional-plan' ? 'Fenster' : 'Öffnungen'}` : 'Öffnungen nicht gezählt',
      item.representation === 'functional-plan' ? `${integer(item.rooms)} Räume · ${integer(item.doors)} Türportale` : `${integer(item.admitted_products || 0)} Körper`,
      ...(item.floor_bands ? [`${integer(item.floor_bands)} Geschossbänder · ${integer(item.planned_features)} Ausstattungselemente`] : []),
      `Bild ${short(item.commit)}${item.experiment_tree_dirty ? ' · Arbeitsstand' : ''}`]) {
      details.append(element('span', text));
    }
    body.append(details); card.append(body); return card;
  }));
}

function svgNode(tag, attrs, text) {
  const node = document.createElementNS('http://www.w3.org/2000/svg', tag);
  for (const [key, value] of Object.entries(attrs)) node.setAttribute(key, value);
  if (text !== undefined) node.textContent = text;
  return node;
}

function renderPlot() {
  const comparisons = [['massing', 'Baukörper'], ['repeat', 'Fassadenpläne']].filter(([key]) => key === 'massing' ? manifest.massing_benchmark : manifest.benchmark);
  if (!comparisons.some(([key]) => key === comparison)) comparison = comparisons[0]?.[0] || 'repeat';
  buttons($('comparisons'), comparisons, comparison, value => { comparison = value; renderPlot(); });
  buttons($('metrics'), Object.entries(metrics).map(([key, value]) => [key, value[0]]), metric,
    value => { metric = value; renderPlot(); });
  const benchmark = comparison === 'massing' ? manifest.massing_benchmark : manifest.benchmark;
  if (!benchmark) { $('plot').textContent = 'Noch kein abgeschlossener Kostenvergleich.'; return; }
  const rows = benchmark.measurements, [, factor, unit] = metrics[metric];
  const selectedMethods = Object.entries(methods).filter(([key]) => rows.some(row => row.method === key));
  $('legend').replaceChildren(...selectedMethods.map(([, [label, color]]) => {
    const node = element('span'), swatch = element('i'); swatch.style.background = color;
    node.append(swatch, document.createTextNode(label)); return node;
  }));
  const sites = [...new Set(rows.map(row => row.site))];
  const maximum = Math.max(...rows.map(row => row[metric] * factor));
  const svg = svgNode('svg', { viewBox: '0 0 960 280', role: 'img', 'aria-label': `${metrics[metric][0]} nach Stadt und Verfahren`,
    xmlns: 'http://www.w3.org/2000/svg' });
  svg.append(svgNode('rect', { width: 960, height: 280, fill: '#faf9f6' }));
  for (const [index, site] of sites.entries()) {
    const x = 16 + index * 320;
    svg.append(svgNode('text', { x, y: 28, fill: '#252b30', 'font-family': 'sans-serif', 'font-size': 18 }, site === 'CentralPark' ? 'Central Park' : site === 'Vienna' ? 'Wien' : site));
    const siteRows = rows.filter(row => row.site === site);
    const detail = comparison === 'massing' ? `${integer(siteRows[0].bands)} Geschossbänder · ${integer(siteRows[0].setbacks)} Staffelkörper` : `${integer(siteRows[0].potential_openings)} mögliche Öffnungen`;
    svg.append(svgNode('text', { x, y: 51, fill: '#677077', 'font-family': 'sans-serif', 'font-size': 11 }, `${integer(siteRows[0].products)} Pläne · ${detail}`));
    for (const [rowIndex, [key, [label, color]]] of selectedMethods.entries()) {
      const row = siteRows.find(value => value.method === key);
      if (!row) continue;
      const value = row[metric] * factor, y = 82 + rowIndex * 54;
      svg.append(svgNode('text', { x, y, fill: '#677077', 'font-family': 'sans-serif', 'font-size': 11 }, label));
      svg.append(svgNode('rect', { x, y: y + 7, width: 213, height: 16, fill: '#eeeee9', rx: 2 }));
      svg.append(svgNode('rect', { x, y: y + 7, width: 213 * value / maximum, height: 16, fill: color, rx: 2 }));
      svg.append(svgNode('text', { x: x + 222, y: y + 20, fill: '#252b30', 'font-family': 'sans-serif', 'font-size': 11 }, `${value.toFixed(2)} ${unit}`));
    }
  }
  svg.append(svgNode('text', { x: 16, y: 267, fill: '#677077', 'font-family': 'sans-serif', 'font-size': 10 }, `Gemeinsame lineare Skala · Mess-Commit ${short(benchmark.commit)} · ${comparison === 'massing' ? 'Körpererzeugung' : 'Repeat-Encoding'}, keine Framezeit`));
  $('plot').replaceChildren(svg);
  $('plot-context').textContent = `Vollständige Stichproben aus Wien, Tokyo und Central Park · Mess-Commit ${short(benchmark.commit)}`;
  $('plot-note').textContent = benchmark.scope + (metric === 'median_generation_ms' ? ' Median aus je drei Läufen; gemeinsame Vorbereitung separat.' : '');
}

function render() {
  $('progress').textContent = manifest.progress;
  $('state').replaceChildren(...[`${manifest.branch} · ${short(manifest.commit)}`, manifest.pushed ? 'Commit gepusht' : 'Push ausstehend',
    manifest.working_tree_dirty ? 'Arbeitsverzeichnis mit Änderungen' : 'Arbeitsverzeichnis sauber'].map(text => element('span', text)));
  const ready = manifest.images.filter(item => item.status === 'ready');
  const rows = manifest.benchmark?.measurements.filter(row => row.method === 'compact') || [];
  const bodies = (manifest.massing_benchmark?.measurements || []).filter(row => row.method === 'bands');
  const source = manifest.source_validation?.sites || [];
  const stats = [[ready.length, 'abgeschlossene Renderings'], ...(source.length
    ? [[source.reduce((sum, row) => sum + row.accepted, 0), 'vollständige OSM-Körper geprüft'],
       [source.reduce((sum, row) => sum + row.omitted.length, 0), 'hide_3d-Auslassungen protokolliert']]
    : bodies.length
    ? [[bodies.reduce((sum, row) => sum + row.products, 0), 'Gebäudepläne im Körpervergleich'],
       [bodies.reduce((sum, row) => sum + row.bands, 0), 'Geschossbänder im Körperplan']]
    : [[rows.reduce((sum, row) => sum + row.products, 0), 'Pläne im Fassadenvergleich'],
       [rows.reduce((sum, row) => sum + row.potential_openings, 0), 'mögliche Öffnungen im Fassadenplan']])];
  $('stats').replaceChildren(...stats.map(([value, label]) => {
    const node = element('div', undefined, 'stat'); node.append(element('strong', integer(value)), element('span', label)); return node;
  }));
  $('gaps').replaceChildren(...manifest.gaps.map(gap => element('li', gap)));
  $('updated').textContent = 'Aktualisiert: ' + new Date(manifest.updated).toLocaleString('de-CH');
  renderEsri(); renderPlans(); renderProvider(); renderGallery(); renderPlot();
}

function renderPlans() {
  const rows = manifest.images.filter(item => item.representation === 'functional-plan');
  $('plans').hidden = !rows.length;
  const vertical = rows.filter(item => item.stage === 'vertical');
  const recent = vertical.length ? vertical : rows.filter(item => item.stage === 'circulation');
  const connected = (recent.length ? recent : rows).filter(item => vertical.length ? item.vertical_status === 'connected-storeys' : recent.length ? item.circulation_status === 'common-access' : item.plan_status === 'connected-first-storey');
  const count = recent.length || rows.length;
  $('plans-note').textContent = (vertical.length ? `${connected.length} Gebäude mit ${connected.reduce((sum,row) => sum+row.storeys,0)} Geschossen und ${connected.reduce((sum,row) => sum+row.stair_connections,0)} konstruierten Verbindungen` : `${connected.length} Erdgeschosse ${recent.length ? 'mit gemeinsamem Flur und Kernreserve' : 'lokal verbunden'}`)+` · ${count-connected.length} Gegenfälle mit Auslassungsgrund. `+
    (recent.length ? `${rows.length-recent.length} frühere Grundrissansichten zum Vergleich. ` : '')+
    'Generische Nutzflächen, reale Wand-/Boden-/Deckenstärken und Tür-/Fensteröffnungen. '+
    (vertical.length ? 'Stufen, Podeste und Geländer sind konstruiert; Raumprogramme, native Spielerphysik und Geländeanschlüsse fehlen. ' : 'Raumprogramm, Obergeschosse, Treppen und Geländeanschlüsse fehlen. ')+
    'CPU-Einzelmessungen des Python-Prototyps; Meshdaten sind Array-Payload, keine Prozesspeaks oder Framezeiten.';
  $('plans-cases').replaceChildren(...rows.map(row => {
    const tr = element('tr');
    const alternatives = (row.alternatives || []).map(value => `${value.method}${value.seed_method ? `/${value.seed_method}` : ''}${value.core_candidate === undefined ? '' : ` #${value.core_candidate+1}`}: ${value.valid ? `${value.rooms} Flächen / ${value.elapsed_ms.toFixed(2)} ms` : 'ungeeignet'}`).join('; ');
    const costs = `${row.generation_ms?.plan?.toFixed(2) || '—'} / ${row.generation_ms?.mesh?.toFixed(2) || '—'}`;
    tr.append(...[row.title, alternatives || 'Kein brauchbarer Innenraum',
      row.plan_status === 'unplanned' ? 'Ausgelassen' : `${row.method} / ${row.rooms} / ${row.doors}${row.circulation ? ` · Flur ${row.circulation.corridor_area_m2.toFixed(1)} m²` : ''}`, costs,
      ((row.raw_mesh_bytes || 0)/1024).toFixed(1)].map(value => element('td', value)));
    return tr;
  }));
  const stairs = manifest.vertical_validation?.flight_comparison || [];
  $('stairs-benchmark').hidden = !stairs.length;
  $('stairs-cases').replaceChildren(...stairs.map(row => {
    const tr = element('tr');
    tr.append(...[row.rise_m.toFixed(3),row.method === 'profile' ? 'Geschlossenes Profil' : 'Einzelne Stufenbauteile',
      `${row.parts} / ${row.triangles}`,`${row.generation_ms.toFixed(3)} / ${row.p95_generation_ms.toFixed(3)}`,row.volume_m3.toFixed(5)].map(value => element('td',value)));
    return tr;
  }));
}

function renderEsri() {
  const data = manifest.esri, catalog = data?.catalog;
  $('esri').hidden = !catalog;
  if (!catalog) return;
  $('esri-note').textContent = `Lokales SDK ${catalog.sdk_version.join('.')} · Referenzausführung mit PRT ${catalog.references.prt_version.join('.')}. `+
    'Dokumentierte Fähigkeiten und ausgeführte Regeln sind getrennte Nachweise; die Versionen werden nicht vermischt.';
  const counts = catalog.counts;
  $('esri-counts').textContent = `${counts.operation} Operationsseiten · ${counts.function} Funktionsseiten · ${counts.attribute} Attributseiten · `+
    `${counts['library-function']} Bibliotheksseiten · ${catalog.references.packages.length} Regelpakete. Diese Zahlen zählen keine Architekturrezepte.`;
  $('esri-operations').replaceChildren(...catalog.groups.map(row => {
    const tr = element('tr');
    tr.append(...[row.name, row.operations.join(', '), row.building_use].map(value => element('td', value)));
    return tr;
  }));
  $('esri-packages').replaceChildren(...catalog.references.packages.map(row => {
    const tr = element('tr'), params = Object.entries(row.attributes).map(([key, info]) => `${key}: ${info.type}`);
    const executed = row.name === 'candler.rpk' && data.validation ? 'Geometrie und Determinismus geprüft; Referenzbilder unten' :
      row.name === 'extrusion_rule.rpk' && data.validation ? 'Volumen, Höhen und Höfe gegen analytische Erwartung geprüft' : 'Regelmetadaten gelesen';
    tr.append(...[row.name, params.join(', ') || 'Keine freien Regelattribute', executed].map(value => element('td', value)));
    return tr;
  }));
  const oracle = data.oracle, validation = data.validation;
  $('esri-proof').textContent = oracle ? `${oracle.external_reference_families} externe Esri-Rezeptfamilie · ${oracle.reference_variants} Varianten auf echten OSM-Grundrissen. `+
    `${validation?.analytic_controls.length || 0} analytische Kontrollkörper und ${validation?.real_extrusions.length || 0} echte Grundrisse geprüft. `+
    'Die eigenen Architekturrezepte haben den Esri-Maßstab noch nicht erreicht.' : 'Referenzrenderings stehen noch aus.';
  const inspection = data.ghidra;
  $('esri-analysis').textContent = inspection ? `Ghidra ${inspection.ghidra_version} · SDK ${inspection.sdk_version.join('.')} · `+
    `${inspection.selected_functions.length} gezielt disassemblierte/dekompilierte Funktionen. `+
    'Aufrufe sind Geometriebelege; ein vollständiger Raumplaner ist damit nicht nachgewiesen. Decompiler-Typen bleiben ungeprüft.' : 'Gezielte Analyse steht noch aus.';
  $('esri-primitives').replaceChildren(...(inspection?.selected_functions || []).map(row => {
    const tr = element('tr');
    const geometry = /CoreToolbox::CGALTools::\w+|CGAL::(?:create_|is_)\w+|CGAL::Polygon_offset_builder_2|AssetLookup::getGeometryAsset|util::(?:LargestRectangle(?:DanielsFast)?::\w+|Trapezoidalization::\w+|Mesh::(?:cut\w*|mergeDuplicateVertices|removeZeroFaces|transformInPlace|calcFaceNormals)|Matrix<double>::getInverseTransposeSafe|MeshUtils::splitToFaceMaterialMeshes|Material::combine|Polygon2d::(?:pointIsConcave|findClosestIntersection|signedDoubleArea))/;
    const calls = [...new Set((row.direct_calls || []).map(call => call.signature.match(geometry)?.[0]).filter(Boolean))].join('; ');
    const dynamic = row.indirect_call_sites ? ` · ${integer(row.indirect_call_sites)} indirekte Aufrufe, Ziele offen` : '';
    tr.append(...[row.signature || row.name, integer(row.instructions), (calls || 'Keine benannten Geometrieaufrufe')+dynamic].map(value => element('td', value)));
    return tr;
  }));
}

function renderProvider() {
  const provider = manifest.provider_comparison;
  $('provider').hidden = !provider;
  if (!provider) return;
  $('provider-context').textContent = `${provider.provider} · API-Prüfung ${new Date(provider.observed_utc).toLocaleDateString('de-CH')} · ${provider.fresh_tiles.length} frische Kontrollkacheln`;
  $('provider-fields').replaceChildren(...provider.building_fields.map(row => {
    const tr = element('tr');
    tr.append(...[row.field, row.type, row.target, integer(row.observed_features), row.meaning].map(value => element('td', value)));
    return tr;
  }));
  $('provider-note').textContent = provider.rule_parameters+' '+provider.context;
  $('provider-missing').textContent = 'Im Gebäudelayer nicht angeboten: '+provider.missing_building_fields.join(', ')+'.';
  const layers = provider.layer_recipes || [];
  $('layer-summary').textContent = `${layers.length} Layer · alle deklarierten Attribute und ihre Verwendung`;
  $('provider-layers').replaceChildren(...layers.map(row => {
    const tr = element('tr'), cells = [element('td', row.layer), element('td', row.role), element('td')];
    const details = element('details'), summary = element('summary', `${Object.keys(row.fields).length} Felder`);
    details.append(summary);
    for (const [key, field] of Object.entries(row.fields)) details.append(element('p', `${key} (${field.type}): ${field.role}`));
    cells[2].append(details); tr.append(...cells); return tr;
  }));
  const checks = manifest.context_validation;
  $('context-checks').hidden = !checks;
  if (!checks) return;
  $('context-note').textContent = `${checks.poi_classes} POI-Klassen / ${checks.poi_subclasses} Unterklassen erhalten eine Rolle. `+
    `Exakte Flächen-/Punktzuordnung; 64 Gebäude je Stadt gegen Vollsuche geprüft. Einzelmessungen in Python, keine Framezeiten. Commit ${short(checks.commit)}.`;
  $('context-sites').replaceChildren(...checks.sites.map(row => {
    const tr = element('tr');
    const values = [row.site, Object.entries(row.families).map(([key, n]) => `${key}: ${integer(n)}`).join(' · '),
      integer(row.partial_poi_uses), `${Math.round(row.context_preparation_ms)} ms`, `${Math.round(row.indexed_ms)} / ${Math.round(row.exhaustive_ms)} ms`];
    tr.append(...values.map(value => element('td', value))); return tr;
  }));
}

async function refresh() {
  try {
    const response = await fetch('manifest.json', { cache: 'no-store' });
    if (!response.ok) throw new Error('HTTP ' + response.status);
    const next = await response.json();
    if (!manifest || next.revision !== manifest.revision) { manifest = next; render(); }
    $('connection').textContent = '● Live';
  } catch (error) { $('connection').textContent = 'Verbindung unterbrochen'; console.error(error); }
}
$('close').onclick = () => $('viewer').close();
$('viewer').onclick = event => { if (event.target === $('viewer')) $('viewer').close(); };
$('download').onclick = () => {
  const svg = $('plot').querySelector('svg');
  if (!svg) return;
  const url = URL.createObjectURL(new Blob([new XMLSerializer().serializeToString(svg)], { type: 'image/svg+xml' }));
  const benchmark = comparison === 'massing' ? manifest.massing_benchmark : manifest.benchmark;
  const link = element('a'); link.href = url; link.download = `outshine-buildings-${comparison}-${metric}-${short(benchmark.commit)}.svg`; link.click();
  setTimeout(() => URL.revokeObjectURL(url), 1000);
};
refresh();
setInterval(refresh, 5000);
