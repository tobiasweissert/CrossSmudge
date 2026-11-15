local U,M,S=dofile('ui.lua'),dofile('core.lua'),dofile('storage.lua')
local deck,index,answer,menu= nil,1,false,false
local confirmDelete=false
local function save() return U.save(S,M.encode(deck)) end
local function nextCard(delta) if #deck.cards>0 then index=(index-1+delta)%#deck.cards+1 end;answer=false;U.redraw() end
local function add()
 if #deck.cards>=24 then U.notice='Deck limit: 24 cards';return end
 U.edit('Front - question','',160,function(q)
  if not M.validText(q,160) then U.notice='Enter a question';return end
  U.edit('Back - answer','',160,function(a)
   if not M.validText(a,160) then U.notice='Enter an answer';return end
   local c={q,a};deck.cards[#deck.cards+1]=c
   if #M.encode(deck)>3800 then table.remove(deck.cards);U.notice='Deck is full';return end
   index=#deck.cards;answer=false;save()
  end)
 end)
end
function on_init()
 deck=S.load(M.decode) or {title='World capitals',cards={{'What is the capital of France?','Paris'},{'What is the capital of Italy?','Rome'},{'What is the capital of Switzerland?','Bern'},{'What is the capital of Japan?','Tokyo'},{'What is the capital of Canada?','Ottawa'}}}
 U.redraw()
end
function on_draw()
 if U.editor then U.editDraw();return end
 U.begin(menu and 'Deck options' or 'Flashcards',deck.title)
 local p,w,t,b=U.pad,U.w,U.top,U.bottom
 if confirmDelete then
  U.button('Cancel',p,t,w-2*p,44,function()confirmDelete=false end)
  U.button('Delete card',p,t+56,w-2*p,44,function()if #deck.cards>0 then table.remove(deck.cards,index);index=math.max(1,math.min(index,#deck.cards));answer=false;save()end;confirmDelete=false;menu=false end)
 elseif menu then
  local actions={{'Add card',add},{'Rename deck',function()U.edit('Deck name',deck.title,32,function(s)if M.validText(s,32) then deck.title=s;save() end end)end},
   {'Delete current card',function()confirmDelete=true;U.focus=1 end},
   {'Retry save',save},{'Return to cards',function()menu=false end}}
  for i,a in ipairs(actions) do U.button(a[1],p,t+(i-1)*52,w-2*p,44,a[2]) end
 else
  U.button('+',w-p-48,t,48,36,add)
  U.text(p,t+6,#deck.cards==0 and 'Empty deck' or ('Card '..index..' of '..#deck.cards),'small')
  local bars=math.min(24,#deck.cards);local bw=(w-2*p)/math.max(1,bars)
  for i=1,bars do smudge.rounded_rect(math.floor(p+(i-1)*bw),t+46,math.max(2,math.floor(bw)-3),12,3,i<index,1);if i==index then smudge.rect_dither(math.floor(p+(i-1)*bw)+2,t+48,math.max(1,math.floor(bw)-7),8) end end
  local cy,ch=t+76,b-t-174
  smudge.rounded_rect(p+4,cy+5,w-2*p,ch,18,true,1);smudge.rounded_rect(p,cy,w-2*p,ch,18,true,1,false);smudge.rounded_rect(p,cy,w-2*p,ch,18,false,3)
  if #deck.cards>0 then
   U.text(w/2,cy+20,answer and 'Answer' or 'Question','small',false,'center')
   local lines=smudge.wrapped_text(deck.cards[index][answer and 2 or 1],w-2*p-32,'bitter14','bold',math.max(1,(ch-80)//smudge.line_height('bitter14')))
   for i,s in ipairs(lines.lines) do U.text(w/2,cy+(ch-lines.total_height)/2+(i-1)*smudge.line_height('bitter14'),s,'bitter14',true,'center') end
  else U.text(w/2,cy+ch/2,'Add your first card','ui12',true,'center') end
  U.button(answer and 'Next card' or 'Show answer',p,b-76,w-2*p,54,function()if answer then nextCard(1)else answer=true end end,true)
 end
 U.footer()
end
function on_button(key,pressed)
 if not pressed then return end
 if U.editor then if confirmDelete then if key=='back'then confirmDelete=false else U.key(key)end;U.redraw();return end
 if key=='back' then U.closeEditor();U.redraw()else U.key(key)end;return end
 if key=='back' then if menu then menu=false else smudge.exit()end
 elseif key=='up' then menu=not menu;U.focus=1
 elseif not menu and (key=='left' or key=='page_back') then nextCard(-1)
 elseif not menu and (key=='right' or key=='down' or key=='page_forward') then nextCard(1)
 elseif not menu and key=='confirm' then if answer then nextCard(1)else answer=true end
 else U.key(key)end
 U.redraw()
end
function on_tap(x,y)
 if U.tap(x,y) or U.editor or menu then return end
 if y>U.top+76 and y<U.bottom-98 then answer=not answer;U.redraw()end
end
function on_swipe(dir) if not U.editor and not menu then nextCard(dir=='left' and 1 or -1)end end
function on_update()end
function on_exit()end
