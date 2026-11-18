-- Sticky presentation. Original bundled glyph masks use the real sprite ABI.
local UI={}
local U=dofile('unified_ui.lua')
local fonts,painted
local targets={}
function UI.init()fonts=smudge.dofile('fonts.lua')end
local function measure(text,font)
 local width=0;for i=1,#text do width=width+(font.glyphs[text:byte(i)] or font.glyphs[63])[5]end;return width
end
local sizes={52,40,32,24,20,16,12,8,6}
local function label(text,x,y,maxWidth,size,style,align,white)
 local font=fonts[style..size]
 if measure(text,font)>maxWidth then
  for _,s in ipairs(sizes)do if s<=size then local candidate=fonts[style..s] or fonts['b'..s];font=candidate;if measure(text,candidate)<=maxWidth then break end end end
 end
 local width=measure(text,font)
 if align=='center' then x=x-math.floor(width/2)elseif align=='right' then x=x-width end
 local cap=font.glyphs[string.byte('0')]
 local baseline=math.floor(y-(cap[4]+cap[2]/2))
 for i=1,#text do local g=font.glyphs[text:byte(i)] or font.glyphs[63];smudge.draw_sprite(math.floor(x+g[3]),baseline+g[4],g[1],g[2],g[6],white or false,false);x=x+g[5]end
end
function UI.geometry(w,h)
 local m=U.metrics();local gap=8
 local cell=(math.min(w-28,m.body_bottom-m.body_top-34)-3*gap)//4
 local side=cell*4+3*gap
 local modal={x=(w-360)//2,y=(h-180)//2,w=360,h=180}
 return {x=(w-side)//2,y=m.body_top,cell=cell,gap=gap,side=side,
 bx=14,by=m.footer_y,bw=w-28,bh=m.footer_h,
 modal=modal,cancel={x=modal.x+16,y=modal.y+108,w=156,h=56},reset={x=modal.x+188,y=modal.y+108,w=156,h=56}}
end
function UI.hit(x,y,modal,w,h)
 if not painted or painted.modal~=modal or painted.w~=w or painted.h~=h then return end
 for _,t in ipairs(targets)do if U.hit(t.r,x,y)then return t.action end end
end
function UI.draw(board,score,best,over,modal,w,h)
 smudge.clear();local g=UI.geometry(w,h)
 U.header('2048','SCORE '..score)
 for r=1,4 do for c=1,4 do
  local x,y=g.x+(c-1)*(g.cell+g.gap),g.y+(r-1)*(g.cell+g.gap)
  local value=board[r][c]
  U.panel({x=x,y=y,w=g.cell,h=g.cell},false)
  if value~=0 then
   if value>=32 then smudge.rect(x+2,y+2,g.cell-4,g.cell-4,true,true)
   else smudge.rect_dither(x+2,y+2,g.cell-4,g.cell-4,value>=8)end
   label(tostring(value),x+g.cell//2,y+g.cell/2,g.cell-12,52,'b','center',value>=32)
  end
 end end
 U.text(14,g.y+g.side+8,'BEST '..best,'ui10',true)
 if over then U.text(w-14,g.y+g.side+8,'GAME OVER','ui10',true,'right')end
 local footer=U.footer({'New game'},1)
 targets={{r=footer[1],action='confirm'}}
 if modal then
  U.panel(g.modal,false)
  U.text(w//2,g.modal.y+24,'Reset game?','lexend16',true,'center')
  local cancel=U.button('Cancel',g.cancel,false)
  local reset=U.button('New game',g.reset,true)
  targets={{r=cancel,action='back'},{r=reset,action='confirm'}}
 end
 painted={w=w,h=h,modal=modal}
end
return UI
