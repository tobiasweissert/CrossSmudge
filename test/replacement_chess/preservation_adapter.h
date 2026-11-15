/* Preservation-only host instrumentation: no Lua tables/strings retained for traces.
 * Real firmware implements these drawing/storage APIs in C as well. */
static char preserve_keys[16][64],preserve_values[16][8192];
static int preserve_count;
static void preserve_guard(lua_State *L,const char *s){
 if(getenv("PRESERVE_DEVICE") && (!strncmp(s,"cp_",3)||!strcmp(s,"app.lua")))
  luaL_error(L,"replacement module on legacy device: %s",s);
 if(getenv("PRESERVE_DEVICE") && strcmp(s,"main.lua") && strcmp(s,"legacy_main.lua"))printf("module|%s\n",s);
}
static void preserve_args(lua_State *L,const char *name){
 fputs(name,stdout);
 for(int i=1;i<=lua_gettop(L);i++){
  fputc('|',stdout);
  switch(lua_type(L,i)){
   case LUA_TSTRING:{size_t n;const char*s=lua_tolstring(L,i,&n);fwrite(s,1,n,stdout);break;}
   case LUA_TNUMBER:fprintf(stdout,"%.9g",(double)lua_tonumber(L,i));break;
   case LUA_TBOOLEAN:fputs(lua_toboolean(L,i)?"true":"false",stdout);break;
   case LUA_TNIL:fputs("nil",stdout);break;
   default:luaL_error(L,"non-scalar preservation trace");
  }
 }
 fputc('\n',stdout);
}
static int preserve_draw(lua_State *L){preserve_args(L,lua_tostring(L,lua_upvalueindex(1)));return 0;}
static int preserve_save(lua_State *L){
 const char*k=luaL_checkstring(L,1);size_t n;const char*v=luaL_checklstring(L,2,&n);int i;
 for(i=0;i<preserve_count;i++)if(!strcmp(k,preserve_keys[i]))break;
 if(i==16||strlen(k)>=64||n>=8192)return luaL_error(L,"preservation storage bound exceeded");
 if(i==preserve_count)preserve_count++;
 strcpy(preserve_keys[i],k);memcpy(preserve_values[i],v,n);preserve_values[i][n]=0;
 preserve_args(L,"save");lua_pushboolean(L,1);return 1;
}
static int preserve_load(lua_State *L){
 const char*k=luaL_checkstring(L,1);
 for(int i=0;i<preserve_count;i++)if(!strcmp(k,preserve_keys[i])){lua_pushstring(L,preserve_values[i]);return 1;}
 if(lua_gettop(L)>=2)lua_pushvalue(L,2);else lua_pushnil(L);return 1;
}
static int preserve_device(lua_State *L){lua_pushstring(L,getenv("PRESERVE_DEVICE"));return 1;}
static int preserve_height(lua_State *L){lua_pushinteger(L,24);return 1;}
static int preserve_false(lua_State *L){lua_pushboolean(L,0);return 1;}
static int preserve_rect(lua_State *L){
 lua_Number x=luaL_checknumber(L,1),y=luaL_checknumber(L,2),rx=luaL_checknumber(L,3),ry=luaL_checknumber(L,4),w=luaL_checknumber(L,5),h=luaL_checknumber(L,6);
 lua_pushboolean(L,x>=rx&&y>=ry&&x<rx+w&&y<ry+h);return 1;
}
static void preserve_setup(lua_State *L){
 const char*d=getenv("PRESERVE_DEVICE");if(!d)return;
 lua_getglobal(L,"smudge");
 const char*names[]={"clear","text","centered_text","rect","rect_dither","rounded_rect","line","thick_line","circle","header","button_hints","draw_sprite","pixel","request_update","full_refresh","exit","prevent_sleep",NULL};
 for(int i=0;names[i];i++){lua_pushstring(L,names[i]);lua_pushcclosure(L,preserve_draw,1);lua_setfield(L,-2,names[i]);}
 const luaL_Reg api[]={{"save",preserve_save},{"load",preserve_load},{"get_device",preserve_device},{"line_height",preserve_height},{"is_button_down",preserve_false},{"in_rect",preserve_rect},{"point_in_rect",preserve_rect},{NULL,NULL}};
 luaL_setfuncs(L,api,0);
 if(!strcmp(d,"missing")){lua_pushnil(L);lua_setfield(L,-2,"get_device");}
 lua_pop(L,1);
}
