-- Dice: original random rules, selection key and session history.
local U = dofile('unified_ui.lua')
local numeral = dofile('numerals.lua')
local draw_shape = dofile('dice_shapes.lua')
local dice_types = {{name='Coin',sides=2},{name='D4',sides=4},{name='D6',sides=6},{name='D8',sides=8},{name='D10',sides=10},{name='D12',sides=12},{name='D20',sides=20}}
local selected_die,current_roll,running_total=3,0,0
local roll_history,footer={},{}
local history_page,history_pages=0,1
local w,h=480,800
function on_init()
 w,h=smudge.get_bounds();math.randomseed(os.time())
 selected_die=tonumber(smudge.load('selected_die','3')) or 3
 if selected_die<1 or selected_die>#dice_types then selected_die=3 end
end
local function roll_dice()
 local die=dice_types[selected_die];current_roll=math.random(1,die.sides)
 roll_history[#roll_history+1]={dieName=die.name,value=current_roll}
 running_total=running_total+current_roll;history_page=0
end
local function reset_history()roll_history={};running_total=0;current_roll=0;history_page=0 end
local function cycle_die()selected_die=selected_die%#dice_types+1;current_roll=0 end
local function history_lines(width)
 local lines,line={},''
 for _,entry in ipairs(roll_history)do
  local token=entry.dieName=='Coin' and ('Coin['..(entry.value==1 and 'H' or 'T')..']') or string.format('%s[%d]',entry.dieName,entry.value)
  local nextline=line=='' and token or line..' '..token
  if smudge.text_width(nextline,'ui10',true)>width and line~='' then lines[#lines+1]=line;line=token else line=nextline end
 end
 local total='= '..running_total
 if smudge.text_width(line..' '..total,'ui10',true)>width then lines[#lines+1]=line;line=total else line=line..' '..total end
 lines[#lines+1]=line;return lines
end
function on_draw()
 smudge.clear();local m=U.metrics();w,h=m.w,m.h
 local die=dice_types[selected_die];U.header('Dice',die.name)
 local size=m.landscape and 130 or 180
 local cy=m.body_top+(m.body_bottom-m.body_top)//2
 local top=cy-size//2
 local lineHeight=smudge.line_height('ui10');local lineGap=1
 local historyY=m.landscape and m.body_top or top-lineHeight-m.gap
 if #roll_history==0 then U.text(w//2,historyY,'Press Roll to start','ui10',true,'center')
 else
  local lines=history_lines(w-28);local capacity=m.landscape and 2 or 3
  history_pages=math.max(1,(#lines+capacity-1)//capacity)
  history_page=math.min(history_page,history_pages-1)
  local first=math.max(1,#lines-(history_page+1)*capacity+1)
  local last=math.min(#lines,#lines-history_page*capacity)
  if not m.landscape then historyY=top-((last-first+1)*lineHeight+(last-first)*lineGap)-m.gap end
  for i=first,last do U.text(w//2,historyY+(i-first)*(lineHeight+lineGap),lines[i],'ui10',true,'center')end
  if history_pages>1 then U.text(w-14,m.body_bottom-26,'History '..(history_pages-history_page)..'/'..history_pages..'  Swipe up/down','small',false,'right')end
 end
 -- The result surface remains central and mode-labelled; coin artwork is original.
 if die.name=='D6' then
  U.panel({x=(w-size)//2,y=top,w=size,h=size})
 else draw_shape(die,current_roll,w//2,cy,size//2)end
 if current_roll>0 and die.name~='Coin' then
  numeral(w//2,cy,current_roll,size-24)
 end
 local label=die.name
 if die.name=='Coin' and current_roll>0 then label=current_roll==1 and 'Coin (Heads)' or 'Coin (Tails)'end
 U.text(w//2,top+size+12,label,'ui10',true,'center')
 footer=U.footer({'Back','Reset','Dice','Roll'},4)
end
function on_button(btn,pressed)
 if not pressed then return end
 if btn=='back' then smudge.save('selected_die',tostring(selected_die));smudge.exit()
 elseif btn=='confirm' then reset_history()
 elseif btn=='up' or btn=='left' or btn=='page_back' then cycle_die()
 elseif btn=='down' or btn=='right' or btn=='page_forward' then roll_dice()end
end
function on_tap(x,y)
 local cw,ch=smudge.get_bounds();if cw~=w or ch~=h then return end
 for i,r in ipairs(footer)do if U.hit(r,x,y)then on_button(({'back','confirm','left','right'})[i],true);return end end
 local m=U.metrics();if x<0 or x>=m.w or y<m.body_top or y>=m.body_bottom then return end
 roll_dice()
end
function on_swipe(dir)
 if dir=='up' then history_page=math.min(history_pages-1,history_page+1)
 elseif dir=='down' then history_page=math.max(0,history_page-1)end
end
