/* Logical framebuffer adapter. Matches production Lua sprite pixel semantics;
 * rect/dither implement GfxRenderer's documented logical raster semantics.
 * This is a host raster capture, not a device screenshot. */
static unsigned char framebuffer[800 * 800];
static void pixel(int x, int y, int black) {
  if (x >= 0 && y >= 0 && x < width && y < height) framebuffer[y * width + x] = black;
}
static int clear_image(lua_State* L) {
  memset(framebuffer, 0, sizeof(framebuffer));
  return 0;
}
static int rect_image(lua_State* L) {
  int x = lua_tointeger(L, 1), y = lua_tointeger(L, 2), w = lua_tointeger(L, 3), h = lua_tointeger(L, 4);
  int fill = lua_toboolean(L, 5), thick = 1, black = 1;
  if (lua_gettop(L) >= 6) {
    if (fill)
      black = lua_toboolean(L, 6);
    else if (lua_isboolean(L, 6)) {
      black = lua_toboolean(L, 6);
      if (lua_gettop(L) >= 7) thick = lua_tointeger(L, 7);
    } else {
      thick = lua_tointeger(L, 6);
      if (lua_gettop(L) >= 7) black = lua_toboolean(L, 7);
    }
  }
  for (int yy = 0; yy < h; yy++)
    for (int xx = 0; xx < w; xx++)
      if (fill || xx < thick || yy < thick || xx >= w - thick || yy >= h - thick) pixel(x + xx, y + yy, black);
  return 0;
}
static int dither_image(lua_State* L) {
  int x = lua_tointeger(L, 1), y = lua_tointeger(L, 2), w = lua_tointeger(L, 3), h = lua_tointeger(L, 4);
  for (int yy = y; yy < y + h; yy++)
    for (int xx = x; xx < x + w; xx++) pixel(xx, yy, (xx % 2 == 0) && (yy % 2 == 0));
  return 0;
}
static int bitmap_image(lua_State* L) {
  int x = lua_tointeger(L, 1), y = lua_tointeger(L, 2), w = lua_tointeger(L, 3), h = lua_tointeger(L, 4);
  size_t n;
  const unsigned char* data = (const unsigned char*)luaL_checklstring(L, 5, &n);
  int inverted = lua_toboolean(L, 6);
  size_t stride = (w + 7) / 8;
  if (x < 0 || y < 0 || x + w > width || y + h > height) return luaL_error(L, "bitmap outside viewport");
  if (n < stride * h) return luaL_error(L, "short bitmap");
  for (int cy = 0; cy < h; cy++)
    for (int cx = 0; cx < w; cx++) {
      size_t byteIdx = cy * stride + cx / 8;
      int bitIdx = 7 - cx % 8;
      int isBlack = ((data[byteIdx] >> bitIdx) & 1) == 0;
      if (isBlack) pixel(x + cx, y + cy, !inverted);
    }
  return 0;
}
static int capture(lua_State* L) {
  FILE* f = fopen(luaL_checkstring(L, 1), "wb");
  if (!f) return luaL_error(L, "capture open");
  fprintf(f, "P5\n%d %d\n255\n", width, height);
  for (int i = 0; i < width * height; i++) fputc(framebuffer[i] ? 0 : 255, f);
  fclose(f);
  return 0;
}
