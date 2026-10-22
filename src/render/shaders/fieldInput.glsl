layout(location = 0) in vec3 fieldPosition;
#if !FIELD_DEPTH
layout(location = 1) in vec3 fieldNormal;
#endif
layout(location = 2) in uint fieldKind;
layout(location = 3) in uint fieldFixedCell;
layout(location = 4) in uint fieldPatchColumn;
layout(location = 5) in uint fieldPatchRow;
layout(location = 6) in vec2 fieldLevelReserved;

uvec4 fieldDescriptor() {
  return uvec4(fieldKind, fieldFixedCell, fieldPatchColumn, fieldPatchRow);
}
