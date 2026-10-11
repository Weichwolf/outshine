const $ = id => document.getElementById(id);
const escaped = value => String(value ?? '').replace(/[&<>"']/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
const categoryNames = {all:'Alle',landscape:'Landschaften',organs:'Blätter und Gras',proxy:'Formproxy',wood:'Aststruktur',comparison:'Vergleich'};
let manifest = null;
let filter = 'all';
let selectedPlot = 0;

function buttons(target, entries, selected, callback) {
  target.replaceChildren(...entries.map(([key, title]) => {
    const button = document.createElement('button');
    button.type = 'button';
    button.textContent = title;
    button.setAttribute('aria-pressed', key === selected);
    button.addEventListener('click', () => callback(key));
    return button;
  }));
}

function card(entry, index, group) {
  const image = entry.missing ? '<div class="missing">Bild oder Messdaten fehlen</div>' : `<button type="button" class="image-button" data-group="${group}" data-index="${index}" aria-label="${escaped(entry.title)} vergrößern"><img src="${escaped(entry.url)}" alt="${escaped(entry.title)}" loading="lazy"></button>`;
  const engine = entry.engine === 'BLENDER_EEVEE_NEXT' ? 'Eevee' : entry.engine;
  const provenance = [entry.commit ? `Quelle ${escaped(entry.commit.slice(0, 10))}` : 'Quellcommit fehlt', entry.resolution ? `${entry.resolution.join(' × ')} · ${escaped(engine)}` : 'Lokale Python-Messung', entry.source_modified === true ? 'Uncommitteter Versuchsstand' : ''].filter(Boolean);
  return `<figure class="card">${image}<figcaption><div class="caption-top"><h3>${escaped(entry.title)}</h3>${entry.label ? `<span class="badge">${escaped(entry.label)}</span>` : ''}</div><p>${escaped(entry.description)}</p>${entry.quality ? `<div class="quality">${escaped(entry.quality)}</div>` : ''}<div class="details">${provenance.map(text => `<span>${text}</span>`).join('')}</div></figcaption></figure>`;
}

function renderGallery() {
  const categories = new Set(manifest.renders.map(entry => entry.category));
  buttons($('filters'), Object.entries(categoryNames).filter(([key]) => key === 'all' || categories.has(key)), filter, key => { filter = key; renderGallery(); });
  $('gallery').innerHTML = manifest.renders.map((entry, index) => ({entry, index})).filter(({entry}) => filter === 'all' || entry.category === filter).map(({entry, index}) => card(entry, index, 'renders')).join('');
}

function renderPlot() {
  selectedPlot = Math.min(selectedPlot, manifest.plots.length - 1);
  buttons($('plot-filters'), manifest.plots.map((entry, index) => [index, entry.title]), selectedPlot, index => { selectedPlot = index; renderPlot(); });
  const entry = manifest.plots[selectedPlot];
  $('plot').innerHTML = entry ? card(entry, selectedPlot, 'plots') : '<div class="missing">Messdaten fehlen</div>';
  $('download').hidden = !entry?.svg_url;
  if (entry?.svg_url) $('download').href = entry.svg_url;
}

function render(data) {
  manifest = data;
  const state = [`${data.branch} · ${data.commit.slice(0, 10)}`, data.pushed ? 'Commit gepusht' : 'Push ausstehend', data.working_tree_modified ? 'Laufende Experimente mit Änderungen' : 'Arbeitsverzeichnis sauber'];
  $('state').innerHTML = state.map(value => `<span>${escaped(value)}</span>`).join('');
  $('stats').innerHTML = data.metrics.map(metric => `<div class="stat"><strong>${escaped(metric.value)}</strong><span>${escaped(metric.label)}</span></div>`).join('');
  $('current-work').textContent = data.current_work;
  $('quality-gap').textContent = data.quality_gap;
  const reference = data.reference;
  const image = reference.missing ? '<div class="missing">Gepinnte Bildreferenz fehlt oder stimmt nicht mit dem gespeicherten Hash überein.</div>' : `<button type="button" class="image-button" data-group="reference" aria-label="Bildziel vergrößern"><img src="${escaped(reference.url)}" alt="Vom Nutzer vorgegebene Vegetationsreferenz" loading="lazy"></button>`;
  $('reference').innerHTML = `<figure class="card">${image}<figcaption><h3>Bildziel zum Qualitätsmaßstab ${escaped(reference.quality_standard)}</h3><p>${escaped(reference.purpose)}</p><p>${escaped(reference.current_gap)}</p><div class="details"><span>${reference.resolution.join(' × ')} · Bildvorlage vom Nutzer</span><a href="${escaped(reference.source_url)}" target="_blank" rel="noreferrer">Originalquelle</a></div></figcaption></figure>`;
  $('updated').textContent = `Aktualisiert ${new Date(data.updated_at).toLocaleString('de-CH')}`;
  renderGallery();
  renderPlot();
}

async function load() {
  const button = $('reload');
  button.disabled = true;
  button.textContent = 'Lädt …';
  $('error').hidden = true;
  try {
    const response = await fetch('status.json', {cache:'no-store'});
    if (!response.ok) throw new Error(`HTTP ${response.status}`);
    const data = await response.json();
    if (data.project !== 'outshine-vegetation') throw new Error('Ungültiger Showcase-Stand');
    if (manifest?.revision !== data.revision) render(data);
  } catch (error) {
    $('error').textContent = `Stand konnte nicht geladen werden: ${error.message}`;
    $('error').hidden = false;
  } finally {
    button.disabled = false;
    button.textContent = 'Stand laden';
  }
}

$('reload').addEventListener('click', load);
document.addEventListener('click', event => {
  const button = event.target.closest('[data-group]');
  if (!button) return;
  const entry = button.dataset.group === 'reference' ? {...manifest.reference, title:'Vegetation · Bildziel', description:manifest.reference.purpose} : manifest[button.dataset.group][Number(button.dataset.index)];
  $('large').src = entry.url;
  $('large').alt = entry.title;
  $('large-caption').textContent = entry.description;
  $('viewer').showModal();
});
$('close').addEventListener('click', () => $('viewer').close());
$('viewer').addEventListener('click', event => { if (event.target === $('viewer')) $('viewer').close(); });
load();
