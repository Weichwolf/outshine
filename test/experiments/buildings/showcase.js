'use strict';
const $ = id => document.getElementById(id);
const integer = value => new Intl.NumberFormat('de-CH').format(value);
const short = value => (value || 'unbekannt').slice(0, 9);
const names = { all: 'Alle', nah: 'Nahdetail', fern: 'Ferne', stadt: 'Stadt', cache: 'Cache', vergleich: 'Vergleich', neu: 'Neu' };
const methods = { expanded: ['Split-Liste', '#ad9165'], vectorized: ['Voll-Liste / NumPy', '#7e899b'], compact: ['Kompakter Repeat', '#496b66'] };
const metrics = {
  median_generation_ms: ['Erzeugung · ms', 1, 'ms'],
  repeat_payload_bytes: ['Plan-Payload · MiB', 1 / 1048576, 'MiB'],
  process_peak_rss_bytes: ['Prozesspeak · MiB', 1 / 1048576, 'MiB']
};
let manifest, filter = 'all', metric = 'median_generation_ms';

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
    for (const text of [`${integer(item.triangles || 0)} Dreiecke`, `${integer(item.openings || 0)} Öffnungen`,
      `${integer(item.admitted_products || 0)} Körper`, `Bild ${short(item.commit)}${item.experiment_tree_dirty ? ' · Arbeitsstand' : ''}`]) {
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
  buttons($('metrics'), Object.entries(metrics).map(([key, value]) => [key, value[0]]), metric,
    value => { metric = value; renderPlot(); });
  const benchmark = manifest.benchmark;
  if (!benchmark) { $('plot').textContent = 'Noch kein abgeschlossener Kostenvergleich.'; return; }
  const rows = benchmark.measurements, [, factor, unit] = metrics[metric];
  const sites = [...new Set(rows.map(row => row.site))];
  const maximum = Math.max(...rows.map(row => row[metric] * factor));
  const svg = svgNode('svg', { viewBox: '0 0 960 280', role: 'img', 'aria-label': `${metrics[metric][0]} nach Stadt und Verfahren`,
    xmlns: 'http://www.w3.org/2000/svg' });
  svg.append(svgNode('rect', { width: 960, height: 280, fill: '#faf9f6' }));
  for (const [index, site] of sites.entries()) {
    const x = 16 + index * 320;
    svg.append(svgNode('text', { x, y: 28, fill: '#252b30', 'font-family': 'sans-serif', 'font-size': 18 }, site === 'CentralPark' ? 'Central Park' : site === 'Vienna' ? 'Wien' : site));
    const siteRows = rows.filter(row => row.site === site);
    svg.append(svgNode('text', { x, y: 51, fill: '#677077', 'font-family': 'sans-serif', 'font-size': 11 }, `${integer(siteRows[0].products)} Pläne · ${integer(siteRows[0].potential_openings)} mögliche Öffnungen`));
    for (const [rowIndex, [key, [label, color]]] of Object.entries(methods).entries()) {
      const row = siteRows.find(value => value.method === key);
      if (!row) continue;
      const value = row[metric] * factor, y = 82 + rowIndex * 54;
      svg.append(svgNode('text', { x, y, fill: '#677077', 'font-family': 'sans-serif', 'font-size': 11 }, label));
      svg.append(svgNode('rect', { x, y: y + 7, width: 213, height: 16, fill: '#eeeee9', rx: 2 }));
      svg.append(svgNode('rect', { x, y: y + 7, width: 213 * value / maximum, height: 16, fill: color, rx: 2 }));
      svg.append(svgNode('text', { x: x + 222, y: y + 20, fill: '#252b30', 'font-family': 'sans-serif', 'font-size': 11 }, `${value.toFixed(2)} ${unit}`));
    }
  }
  svg.append(svgNode('text', { x: 16, y: 267, fill: '#677077', 'font-family': 'sans-serif', 'font-size': 10 }, `Gemeinsame lineare Skala · Mess-Commit ${short(benchmark.commit)} · Repeat-Encoding, keine Framezeit`));
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
  const stats = [[ready.length, 'abgeschlossene Renderings'], [rows.reduce((sum, row) => sum + row.products, 0), 'Pläne im Kostenvergleich'],
    [rows.reduce((sum, row) => sum + row.potential_openings, 0), 'mögliche Öffnungen im Plan']];
  $('stats').replaceChildren(...stats.map(([value, label]) => {
    const node = element('div', undefined, 'stat'); node.append(element('strong', integer(value)), element('span', label)); return node;
  }));
  $('gaps').replaceChildren(...manifest.gaps.map(gap => element('li', gap)));
  $('legend').replaceChildren(...Object.values(methods).map(([label, color]) => {
    const node = element('span'), swatch = element('i'); swatch.style.background = color;
    node.append(swatch, document.createTextNode(label)); return node;
  }));
  $('updated').textContent = 'Aktualisiert: ' + new Date(manifest.updated).toLocaleString('de-CH');
  renderGallery(); renderPlot();
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
  const link = element('a'); link.href = url; link.download = `outshine-buildings-${metric}-${short(manifest.benchmark.commit)}.svg`; link.click();
  setTimeout(() => URL.revokeObjectURL(url), 1000);
};
refresh();
setInterval(refresh, 5000);
