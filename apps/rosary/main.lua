-- Rosary: unchanged four drop + 54 loop beads, completion index and save key.
local U=dofile('unified_ui.lua')
local current_bead=0
local w,h=480,800
local beads,footer={},{}
local totalSteps=0
local function build_beads()
 local m=U.metrics();w,h=m.w,m.h
 local available=m.body_bottom-m.body_top
 local ry=math.min((w-124)//2,(available-152)//2)
 local rx=ry
 local top=m.body_top+(available-(2*ry+152))//2
 local centerX,centerY=w//2,top+ry
 local dropSpacing=18
 local startY=centerY+ry+22+dropSpacing*3
 beads={}
 for i=0,3 do beads[#beads+1]={x=centerX,y=startY-dropSpacing*i,r=i==0 and 8 or 5,isOurFather=i==0}end
 for i=0,53 do
  local angle=math.pi/2-2*math.pi*i/54
  local spacer=i==0 or i==11 or i==22 or i==33 or i==44
  beads[#beads+1]={x=centerX+math.floor(rx*math.cos(angle)),y=centerY+math.floor(ry*math.sin(angle)),r=spacer and 8 or 5,isOurFather=spacer}
 end
 totalSteps=#beads+1
end
function on_init()
 build_beads();current_bead=tonumber(smudge.load('current_bead','0')) or 0
 if current_bead<0 or current_bead>=totalSteps then current_bead=0 end
end
function on_draw()
 smudge.clear();build_beads();U.header('Rosary')
 -- Keep the authentic immutable crucifix, not the reference's schematic cross.
 smudge.draw_sprite(beads[1].x-32,beads[1].y+8,64,64,'sprites/crucifix.raw')
 local complete=current_bead==#beads
 local active=complete and 5 or current_bead+1
 for i,b in ipairs(beads)do
  local box={x=b.x-b.r,y=b.y-b.r,w=2*b.r,h=2*b.r}
  U.panel(box,i==active or b.isOurFather)
  if i==active and not b.isOurFather then U.panel({x=b.x-b.r-3,y=b.y-b.r-3,w=2*b.r+6,h=2*b.r+6},true)end
  if i==active and b.isOurFather then smudge.rect(b.x-b.r-4,b.y-b.r-4,2*b.r+8,2*b.r+8,false,2,true)end
  if i==5 and complete then smudge.rect(b.x-b.r-7,b.y-b.r-7,2*b.r+14,2*b.r+14,false,1,true)end
 end
 footer=U.footer({'Back','Reset','Prev','Next'},4)
end
function on_button(btn,pressed)
 if not pressed then return end
 if btn=='back' then smudge.save('current_bead',tostring(current_bead));smudge.exit()
 elseif btn=='confirm' then current_bead=0
 elseif btn=='right' or btn=='down' or btn=='page_forward' then if current_bead+1<totalSteps then current_bead=current_bead+1 end
 elseif btn=='left' or btn=='up' or btn=='page_back' then if current_bead>0 then current_bead=current_bead-1 end end
end
function on_tap(x,y)
 local cw,ch=smudge.get_bounds();if cw~=w or ch~=h then return end
 for i,r in ipairs(footer)do if U.hit(r,x,y)then on_button(({'back','confirm','left','right'})[i],true);return end end
 local m=U.metrics();if x<0 or x>=m.w or y<m.body_top or y>=m.body_bottom then return end
 on_button(y<h//2 and 'right' or 'left',true)
end
