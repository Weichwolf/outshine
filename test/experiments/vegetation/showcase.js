"use strict";

const gallery = document.querySelector("#gallery");
const state = document.querySelector("#state");
const error = document.querySelector("#error");
const reload = document.querySelector("#reload");
const viewer = document.querySelector("#viewer");

function showImage(entry) {
  const large = document.querySelector("#large");
  large.src = entry.url;
  large.alt = entry.title;
  document.querySelector("#large-caption").textContent = entry.title;
  viewer.showModal();
}

function tile(entry) {
  const figure = document.createElement("figure");
  const button = document.createElement("button");
  button.className = "image";
  button.type = "button";
  button.setAttribute("aria-label", `${entry.title} vergrößern`);
  const image = document.createElement("img");
  image.src = entry.url;
  image.alt = entry.title;
  image.width = entry.resolution[0];
  image.height = entry.resolution[1];
  button.append(image);
  button.addEventListener("click", () => showImage(entry));
  const caption = document.createElement("figcaption");
  const title = document.createElement("strong");
  title.textContent = entry.title;
  const label = document.createElement("span");
  label.textContent = entry.label;
  const note = document.createElement("small");
  note.textContent = `${entry.note} · ${entry.resolution.join("×")}`;
  caption.append(title, label, note);
  figure.append(button, caption);
  return figure;
}

async function load() {
  reload.disabled = true;
  error.hidden = true;
  try {
    const response = await fetch("/status.json", {cache: "no-store"});
    if (!response.ok) throw new Error(`HTTP ${response.status}`);
    const data = await response.json();
    if (data.project !== "outshine-vegetation" || data.renders.length !== 4) {
      throw new Error("Vier aktuelle Kachelbilder fehlen.");
    }
    gallery.replaceChildren(...data.renders.map(tile));
    const date = new Intl.DateTimeFormat("de-CH", {dateStyle: "short", timeStyle: "short"});
    state.textContent = `Stand ${data.commit.slice(0, 9)} · ${date.format(new Date(data.updated_at))}`;
  } catch (problem) {
    gallery.replaceChildren();
    state.textContent = "Bildstand nicht verfügbar";
    error.textContent = problem.message;
    error.hidden = false;
  } finally {
    reload.disabled = false;
  }
}

reload.addEventListener("click", load);
document.querySelector("#close").addEventListener("click", () => viewer.close());
viewer.addEventListener("click", event => {if (event.target === viewer) viewer.close();});
load();
