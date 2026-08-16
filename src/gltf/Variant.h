/* WHICH MATERIAL VARIANT A DECLARATION RENDERS (board:1188), and it is a DECLARATION rather than a
 * capability: `KHR_materials_variants` says a viewer has "up to one single active variant at a time"
 * and says nothing about which, so the consumer names it and the file answers.
 *
 * IT CARRIES THE NAME AND NOT AN INDEX, and that is what makes a selection un-mixable between two
 * files: an index resolved against one document and applied to another selects a picture silently,
 * because every index is in range somewhere. A name is resolved against the document that is about
 * to be drawn, or it is a refusal naming both sides -- so there is no state here that can be true of
 * the wrong file.
 *
 * THE DEFAULT IS NO VARIANT, and that is the ordinary case rather than a special one: every asset in
 * this tree but one declares none, and the extension's own rule for a primitive no active variant
 * maps is the same sentence as for no active variant at all -- vanilla glTF, the primitive's own
 * `material`. `Primitive::MaterialUnder` is the one place both are answered.
 *
 * WHERE IT IS CONSUMED IS THE WHOLE OF THE PRECEDENT. A selection extension resolves while the
 * subject is flattened, into the material index a part wears, which is what the draw list's material
 * slot is built from -- so it is spent BEFORE a draw list exists and the render path never learns
 * that variants are a thing. That is why it costs no fragment arm, no pipeline and no interpolant,
 * and it is the reason a selection extension does NOT go where board:1177 put a data extension. */
#ifndef GLTF_VARIANT_H
#define GLTF_VARIANT_H

#include <optional>
#include <string>

namespace outshine::Gltf {

class Document;

class VariantSelection {
public:
  /* No active variant: every primitive wears the material it names, and no mapping is consulted. */
  VariantSelection() = default;
  explicit VariantSelection(std::string name) : Name_(std::move(name)) {}

  /* WHICH ENTRY OF `document.Variants()` THIS NAMES, or -1 where nothing is selected -- which is
   * what `Primitive::MaterialUnder` takes. A name the document does not declare is a REFUSAL naming
   * the name asked for and the names available, never the default and never the first: a selection
   * that quietly fell back would render the file's own materials and look entirely correct, which
   * is the one failure a picture cannot show. */
  [[nodiscard]] bool Against(const Document &document, int &index, std::string &why) const;

private:
  std::optional<std::string> Name_;
};

} // namespace outshine::Gltf
#endif
