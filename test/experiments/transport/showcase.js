"use strict";
let manifest;
let selected = "boden";
const byId = id => document.getElementById(id);
const element = (tag, text, className) => {
  const node = document.createElement(tag);
  if (text !== undefined) node.textContent = text;
  if (className) node.className = className;
  return node;
};
const number = value => {
  if (typeof value !== "number") return String(value);
  if (value !== 0 && Math.abs(value) < 0.0001) return value.toExponential(2).replace(".", ",");
  return value.toLocaleString("de-DE", {maximumSignificantDigits: 5});
};

function openImage(row) {
  byId("large").src = row.image;
  byId("large").alt = row.title;
  byId("caption").textContent = `${row.title} · ${row.dimensions.join("×")} Pixel · ${row.scope}`;
  byId("download").href = row.image;
  byId("download").download = `${row.id}.png`;
  byId("viewer").showModal();
}

function card(row) {
  const node = element("article", undefined, "card");
  if (row.status === "ready") {
    const button = element("button", undefined, "picture");
    button.type = "button";
    button.setAttribute("aria-label", `${row.title} vergrößern`);
    const picture = element("img");
    picture.src = row.image;
    picture.alt = row.title;
    picture.loading = "lazy";
    picture.addEventListener("error", () => {
      button.replaceWith(element("p", "Veröffentlichtes Bild fehlt oder ist nicht lesbar.", "incomplete"));
    }, {once: true});
    button.append(picture);
    button.addEventListener("click", () => openImage(row));
    node.append(button);
  } else {
    node.append(element("div", "Bildnachweis fehlt", "missing-picture"));
  }
  const content = element("div", undefined, "card-content");
  content.append(element("span", row.kind, "tag"), element("h3", row.title), element("p", row.description));
  if (row.status === "ready") {
    const metrics = element("dl", undefined, "metrics");
    for (const metric of row.metrics) {
      const item = element("div", undefined, "metric");
      item.append(element("dt", metric.label), element("dd", `${number(metric.value)} ${metric.unit}`));
      metrics.append(item);
    }
    content.append(metrics, element("p", row.scope, "hint"));
    const origin = `Erzeugt bei ${row.render_commit.slice(0, 9)}${row.working_tree_dirty ? " + lokale Änderungen" : ""} · ${row.dimensions.join("×")} Pixel`;
    content.append(element("p", origin, "hint"));
  } else {
    content.append(element("p", row.missing_note || row.error, "incomplete"));
  }
  node.append(content);
  return node;
}

function render() {
  const section = manifest.sections.find(row => row.id === selected) || manifest.sections[0];
  selected = section.id;
  byId("sections").replaceChildren();
  for (const row of manifest.sections) {
    const button = element("button", row.title);
    button.type = "button";
    button.setAttribute("aria-pressed", String(row.id === selected));
    button.addEventListener("click", () => { selected = row.id; render(); });
    byId("sections").append(button);
  }
  byId("gallery-title").textContent = section.title;
  byId("description").textContent = section.description;
  byId("gallery").replaceChildren(...manifest.images.filter(row => row.section === selected).map(card));
  const ready = manifest.images.filter(row => row.status === "ready").length;
  byId("state").replaceChildren(element("span", `${ready} Bilder vorhanden`), element("span", `${manifest.images.length - ready} unvollständig`, "warning"), element("span", "Offline-Python · keine Framezeitmessung"));
  byId("note").textContent = manifest.note;
  byId("gap").textContent = manifest.gap;
  byId("updated").textContent = `Veröffentlicht ${new Date(manifest.updated).toLocaleString("de-DE")} · ${manifest.commit.slice(0, 9)}`;
}

async function load() {
  byId("reload").disabled = true;
  try {
    const response = await fetch("/manifest.json", {cache: "no-store"});
    if (!response.ok) throw new Error(`HTTP ${response.status}`);
    const next = await response.json();
    if (!Array.isArray(next.images) || !Array.isArray(next.sections) || !next.sections.length) throw new Error("Bildkatalog unvollständig");
    manifest = next;
    render();
    byId("error").hidden = true;
  } catch (error) {
    byId("error").textContent = `Bildstand konnte nicht geladen werden: ${error.message}`;
    byId("error").hidden = false;
  } finally {
    byId("reload").disabled = false;
  }
}
byId("reload").addEventListener("click", load);
byId("close").addEventListener("click", () => byId("viewer").close());
byId("viewer").addEventListener("click", event => { if (event.target === byId("viewer")) byId("viewer").close(); });
load();
