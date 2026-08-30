#include "Surfacing.h"

namespace outshine::Render {

void ResolveDeclaredSurface(const Shape &geometry,
                            const outshine::Material &row,
                            SurfaceTable &out) {
  out.Slots.clear();
  out.Material.clear();
  out.PartSlot.clear();
  out.Decoded.clear();

  out.PartSlot.assign(geometry.Parts.size(), 0u);
  if (geometry.Surfaces.empty()) {
    SubjectMaterial slot;
    slot.Row = row;
    out.Slots.push_back(slot);
    out.Decoded.push_back(SurfaceRasters());
    out.Material.push_back(0);
    return;
  }
  for (size_t part = 0; part < geometry.Parts.size(); ++part) {
    const int material = geometry.Parts[part].Material;
    size_t slot = out.Material.size();
    for (size_t at = 0; at < out.Material.size(); ++at) {
      if (out.Material[at] == material) {
        slot = at;
        break;
      }
    }
    if (slot == out.Material.size()) {
      SubjectMaterial surface;
      surface.Row = material >= 0 && (size_t)material < geometry.Surfaces.size()
                        ? geometry.Surfaces[(size_t)material]
                        : row;
      out.Slots.push_back(surface);
      out.Decoded.push_back(SurfaceRasters());
      out.Material.push_back(material);
    }
    out.PartSlot[part] = (uint32_t)slot;
  }
}

}
