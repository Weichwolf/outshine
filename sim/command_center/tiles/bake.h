/* FlightBox renderer — upload one baked albedo (fetched ready-made from fb-tiles, decoded by stb)
 * as a mip-mapped, anisotropic GL texture. The LRU cache is the only caller. */
#ifndef W3_TILES_BAKE_H
#define W3_TILES_BAKE_H
/* Upload one baked albedo as a GL texture. Returns 0 if it has not arrived yet (retry later) or
 * the server has nothing for this tile. */
static GLuint w3_bake(uint32_t z,uint32_t x,uint32_t y,int TS,int mode){
  int photo = (mode==W3_GROUND_PHOTO);
  int n = w3_bake_size(photo,(int)z,(int)x,(int)y,TS);   /* also STARTS the fetch on a miss */
  if(n<=0) return 0;                                     /* pending (-1) or a genuine hole (0) */
  uint8_t*enc=(uint8_t*)malloc((size_t)n); if(!enc) return 0;
  w3_bake_copy(photo,(int)z,(int)x,(int)y,TS,enc);
  int w=0,h=0,comp=0;
  uint8_t*px=stbi_load_from_memory(enc,n,&w,&h,&comp,3);
  free(enc);
  if(!px) return 0;                                      /* undecodable: a hole, not a crash */

  GLuint tex; glGenTextures(1,&tex); glBindTexture(GL_TEXTURE_2D,tex);
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,w,h,0,GL_RGB,GL_UNSIGNED_BYTE,px);
  /* trilinear (mipmaps) kills shimmer on distant tiles; anisotropy keeps the ground sharp at
   * grazing angles (the terrain is seen almost edge-on). */
  glGenerateMipmap(GL_TEXTURE_2D); w3_mipmaps++;
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
  #ifndef GL_TEXTURE_MAX_ANISOTROPY_EXT
  #define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
  #endif
  glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MAX_ANISOTROPY_EXT,8.0f);   /* ignored if unsupported */
  stbi_image_free(px);
  return tex;
}
#endif
