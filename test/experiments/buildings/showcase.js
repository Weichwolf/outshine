'use strict';
const $ = id => document.getElementById(id);
const integer = value => new Intl.NumberFormat('de-CH').format(value);
const short = value => (value || 'unbekannt').slice(0, 9);
const viewNames = {all:'Alle',near:'Gesamtansicht',orbit:'Gegenansicht',window:'Fenster',entrance:'Eingang',overview:'Übersicht',scope:'Scope-Kontrollen',extrusion:'Extrusionskontrollen',plan:'2D-Pläne und Schnitte'};
const metrics = {triangles:['Mesh · Dreiecke',1,'Dreiecke'],raw_mesh_bytes:['Mesh · MiB',1/1048576,'MiB'],render_seconds:['Bilderzeugung · s',1,'s']};
let manifest, page, filter = 'all', metric = 'triangles';

function element(tag, text, className) {
  const node = document.createElement(tag);
  if (text !== undefined) node.textContent = text;
  if (className) node.className = className;
  return node;
}

function pageImages() { return manifest.images.filter(item => !page || item.page === page); }

function buttons(target, entries, selected, callback) {
  target.replaceChildren(...entries.map(([key,label]) => {
    const button = element('button',label); button.type = 'button';
    button.setAttribute('aria-pressed',key === selected); button.onclick = () => callback(key);
    return button;
  }));
}

function navigation() {
  const links = [['', {path:'',title:'Übersicht'}], ...Object.entries(manifest.pages)];
  $('pages').replaceChildren(...links.map(([key,info]) => {
    const link = element('a',info.title); link.href = '/' + (info.path ? info.path + '/' : '');
    if (key === (page || '')) link.setAttribute('aria-current','page');
    return link;
  }));
}

function renderOverview() {
  $('overview').replaceChildren(...Object.entries(manifest.pages).map(([key,info]) => {
    const link = element('a',undefined,'overview-card'); link.href = '/' + info.path + '/';
    const images = manifest.images.filter(item => item.page === key && item.status === 'ready');
    const first = images.find(item => item.label === 'vienna-compact') || images[0];
    if (first) { const image = element('img'); image.src = first.image; image.alt = first.title; link.append(image); }
    else link.append(element('div','Aktuelle Ansicht fehlt','incomplete'));
    const body = element('div',undefined,'card-body');
    body.append(element('h2',info.title),element('p',info.description),element('span',`${images.length} aktuelle Bilder →`,'hint'));
    link.append(body); return link;
  }));
}

function openImage(item) {
  $('large').src = item.image; $('large').alt = item.title;
  $('large-caption').textContent = `${item.title} · ${item.profile.join(' × ')} · Bild-Commit ${short(item.commit)}`;
  $('viewer').showModal();
}

function renderGallery() {
  const images = pageImages();
  const views = new Set(images.map(item => item.view));
  if (filter !== 'all' && !views.has(filter)) filter = 'all';
  buttons($('filters'),Object.entries(viewNames).filter(([key]) => key === 'all' || views.has(key)),filter,
    value => {filter = value; renderGallery();});
  $('empty').hidden = images.length > 0;
  $('gallery').replaceChildren(...images.filter(item => filter === 'all' || item.view === filter).map(item => {
    const card = element('article',undefined,'card');
    if (item.status === 'ready') {
      const button = element('button',undefined,'image-button'); button.type = 'button';
      button.setAttribute('aria-label',item.title + ' vergrößern');
      const image = element('img'); image.src = item.image; image.alt = item.title; image.loading = 'lazy';
      button.append(image); button.onclick = () => openImage(item); card.append(button);
    } else card.append(element('div','Aktueller Bildbeleg unvollständig: ' + item.error,'incomplete'));
    const body = element('div',undefined,'card-body');
    body.append(element('h3',item.title || 'Aktueller Bildbeleg'),element('p',item.caption));
    const details = element('div',undefined,'details');
    const counts = item.logical_faces !== undefined ? `${integer(item.logical_faces)} Polygonflächen · ${integer(item.source_holes)} Quellhöfe`
      : item.constructive_products === undefined ? `${integer(item.storeys)} Geschosse · ${integer(item.rooms)} Räume · ${integer(item.doors)} Türportale`
      : `${integer(item.constructive_products)} Prüfgebäude · ${integer(item.source_volume_products)} Quellkörper`;
    details.append(...[`${integer(item.triangles)} Dreiecke`,counts,
      item.profile ? item.profile.join(' × ') : 'Profil fehlt',`Bild ${short(item.commit)}`].map(value => element('span',value)));
    body.append(details);card.append(body);return card;
  }));
}

function svgNode(tag, attrs, text) {
  const node = document.createElementNS('http://www.w3.org/2000/svg',tag);
  for (const [key,value] of Object.entries(attrs)) node.setAttribute(key,value);
  if (text !== undefined) node.textContent = text;
  return node;
}

function renderPlot() {
  const rows = pageImages().filter(item => item.status === 'ready' && Number.isFinite(item[metric]));
  $('costs').hidden = !rows.length;
  buttons($('metrics'),Object.entries(metrics).map(([key,value]) => [key,value[0]]),metric,
    value => {metric = value;renderPlot();});
  if (!rows.length) return;
  const [,factor,unit] = metrics[metric]; const maximum = Math.max(...rows.map(row => row[metric] * factor));
  const svg = svgNode('svg',{viewBox:`0 0 960 ${rows.length*46+35}`,role:'img','aria-label':metrics[metric][0],xmlns:'http://www.w3.org/2000/svg'});
  svg.append(svgNode('rect',{width:960,height:rows.length*46+35,fill:'#faf9f6'}));
  rows.forEach((row,index) => {
    const y = 24 + index*46; const value = row[metric]*factor;
    svg.append(svgNode('text',{x:14,y,fill:'#252b30','font-family':'sans-serif','font-size':12},row.title));
    svg.append(svgNode('rect',{x:470,y:y-11,width:360,height:15,fill:'#eeeee9',rx:2}));
    svg.append(svgNode('rect',{x:470,y:y-11,width:maximum ? 360*value/maximum : 0,height:15,fill:'#496b66',rx:2}));
    svg.append(svgNode('text',{x:843,y,fill:'#252b30','font-family':'sans-serif','font-size':12},`${value.toFixed(metric === 'triangles' ? 0 : 2)} ${unit}`));
  });
  $('plot').replaceChildren(svg);
}

function render() {
  const path = location.pathname.split('/').filter(Boolean)[0] || '';
  page = Object.keys(manifest.pages).find(key => manifest.pages[key].path === path);
  const info = manifest.pages[page]; navigation();
  $('title').textContent = info?.title || 'Gebäude im Versuch.';
  document.title = 'Outshine · ' + (info?.title || 'Gebäude');
  $('description').textContent = info?.description || 'Vier Ansichtsebenen. Ein aktueller Bildstand.';
  $('scope').textContent = info?.scope || 'Innenräume → einzelne Gebäude → Gebäudegruppen → Städte';
  $('overview').hidden = Boolean(page); $('pictures').hidden = !page;
  $('state').replaceChildren(...[`${manifest.branch} · ${short(manifest.commit)}`,
    manifest.pushed ? 'Commit gepusht' : 'Push ausstehend',
    `Bildstand ${short(manifest.render_commit)}`,`${manifest.completed_recipe_families} vollständige Rezeptfamilien`].map(value => element('span',value)));
  $('gap').textContent = info?.gap || 'Esri-CityEngine-Qualität und architektonische Vielfalt sind noch nicht erreicht. Die einzelnen Bereiche zeigen ihre konkreten Lücken.';
  $('lod-note').textContent = manifest.lod_note;
  $('progress').textContent = manifest.progress;
  const ghidra = manifest.ghidra;
  $('esri-proof').textContent = ghidra ? `${ghidra.functions} gezielt untersuchte Esri-Funktionen · SDK ${ghidra.sdk.join('.')} · Ghidra ${ghidra.version}. Geometriepfade sind Referenzen; diese Zahl zählt keine Gebäude-Rezepte.` : 'Esri-Referenzprüfung steht aus.';
  $('updated').textContent = 'Aktualisiert: ' + new Date(manifest.updated).toLocaleString('de-CH');
  if (page) renderGallery(); else renderOverview();
  renderPlot();
}

async function refresh() {
  try {
    const response = await fetch('/manifest.json',{cache:'no-store'});
    if (!response.ok) throw new Error('HTTP ' + response.status);
    const next = await response.json();
    if (!manifest || next.revision !== manifest.revision) {manifest = next;render();}
    $('connection').textContent = '● Live';
  } catch (error) {$('connection').textContent = 'Verbindung unterbrochen';console.error(error);}
}
$('close').onclick = () => $('viewer').close();
$('viewer').onclick = event => {if (event.target === $('viewer')) $('viewer').close();};
$('download').onclick = () => {
  const svg = $('plot').querySelector('svg');if (!svg) return;
  const url = URL.createObjectURL(new Blob([new XMLSerializer().serializeToString(svg)],{type:'image/svg+xml'}));
  const link = element('a');link.href = url;link.download = `outshine-${page || 'overview'}-${metric}-${short(manifest.render_commit)}.svg`;link.click();
  setTimeout(() => URL.revokeObjectURL(url),1000);
};
refresh();setInterval(refresh,5000);
