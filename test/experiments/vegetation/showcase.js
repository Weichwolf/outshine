const $ = id => document.getElementById(id);
const escaped = value => String(value ?? '').replace(/[&<>"']/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
const page = location.pathname.split('/').filter(Boolean)[0] ?? '';
let activeLod = null;
let manifest = null;
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
  $('images-section').hidden = !activeLod;
  $('gallery-title').textContent = activeLod?.title ?? 'Aktueller Bildstand';
  const entries = manifest.renders.map((entry, index) => ({entry, index})).filter(({entry}) => entry.lod === activeLod?.id);
  const genera = manifest.genera.map(genus => {
    const products = entries.filter(({entry}) => entry.genus === genus.id);
    const images = products.length ? products.map(({entry, index}) => card(entry, index, 'renders')).join('') : '<p class="missing">Noch kein geprüftes Gattungsprodukt in dieser Detailstufe.</p>';
    const reference = genus.reference ? `<p class="species"><a href="${escaped(genus.reference.url)}" target="_blank" rel="noreferrer">Botanische Referenz · ${escaped(genus.reference.scope)}</a><br>${escaped(genus.reference.review)}</p>` : '';
    return `<section class="genus"><h3>${escaped(genus.common)} · <i>${escaped(genus.genus)}</i></h3><p class="species">Zielart: <i>${escaped(genus.species)}</i></p>${reference}<div class="gallery">${images}</div></section>`;
  });
  const neutral = entries.filter(({entry}) => !entry.genus);
  $('gallery').innerHTML = genera.join('') + (neutral.length ? `<h3>Weitere aktuelle Verfahrensproben</h3><div class="gallery">${neutral.map(({entry, index}) => card(entry, index, 'renders')).join('')}</div>` : '');
}

function renderPlot() {
  const entries = manifest.plots.map((entry, index) => ({entry, index})).filter(({entry}) => entry.lod === activeLod?.id);
  $('measurements').hidden = !entries.length;
  if (!entries.length) return;
  if (!entries.some(({index}) => index === selectedPlot)) selectedPlot = entries[0].index;
  buttons($('plot-filters'), entries.map(({entry, index}) => [index, entry.title]), selectedPlot, index => { selectedPlot = index; renderPlot(); });
  const entry = manifest.plots[selectedPlot];
  $('plot').innerHTML = card(entry, selectedPlot, 'plots');
  $('download').hidden = !entry.svg_url;
  if (entry.svg_url) $('download').href = entry.svg_url;
}

function render(data) {
  manifest = data;
  activeLod = data.lods.find(lod => lod.id === page);
  document.body.classList.toggle('detail-page', Boolean(activeLod));
  document.querySelector('h1').textContent = activeLod?.title ?? 'Vegetation im Versuch.';
  document.title = `${activeLod?.title ?? 'Aktueller Stand'} · Outshine Vegetation`;
  $('lod-nav').innerHTML = data.lods.map(lod => `<a href="/${escaped(lod.id)}/" ${lod.id === activeLod?.id ? 'aria-current="page"' : ''}><strong>${escaped(lod.title)}</strong><span>${escaped(lod.description)}</span></a>`).join('');
  $('lod-description').textContent = activeLod?.description ?? 'Eine Detailstufe wählen. Jede Seite zeigt ausschließlich ihre aktuellen Produkte; fehlende Stufen bleiben als offen gekennzeichnet.';
  const state = [`${data.branch} · ${data.commit.slice(0, 10)}`, data.pushed ? 'Commit gepusht' : 'Push ausstehend', data.working_tree_modified ? 'Laufende Experimente mit Änderungen' : 'Arbeitsverzeichnis sauber'];
  $('state').innerHTML = state.map(value => `<span>${escaped(value)}</span>`).join('');
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
    const response = await fetch('/status.json', {cache:'no-store'});
    if (!response.ok) throw new Error(`HTTP ${response.status}`);
    const data = await response.json();
    if (data.project !== 'outshine-vegetation') throw new Error('Ungültiger Showcase-Stand');
    if (manifest?.revision !== data.revision) render(data);
  } catch (error) {
    manifest = null;
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
