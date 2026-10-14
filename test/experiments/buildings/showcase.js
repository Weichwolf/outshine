"use strict";
document.getElementById("refresh").addEventListener("click",()=>location.reload());
(async()=>{
  try{
    const response=await fetch("/manifest.json",{cache:"no-store"});
    if(!response.ok)throw new Error("Der aktuelle Bildstand ist nicht verfügbar.");
    const manifest=await response.json();
    if(manifest.images.length!==4||manifest.images.some(row=>row.status!=="ready"||row.view!=="tile"))throw new Error("Die vier aktuellen Kachelbilder sind unvollständig.");
    const gallery=document.getElementById("gallery");
    for(const row of manifest.images){
      const figure=document.createElement("figure"),link=document.createElement("a"),image=document.createElement("img"),caption=document.createElement("figcaption"),title=document.createElement("strong");
      link.href=row.image;link.target="_blank";link.rel="noopener";image.src=row.image;image.alt=row.title;image.width=row.profile[0];image.height=row.profile[1];image.decoding="async";
      title.textContent=row.title;caption.append(title);link.append(image);figure.append(link,caption);gallery.append(figure);
    }
    document.getElementById("updated").textContent="Stand: "+new Date(manifest.updated).toLocaleString("de-DE");
  }catch(error){const message=document.getElementById("error");message.textContent=error.message;message.hidden=false;}
})();
