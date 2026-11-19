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
 U.begin(menu and 'Deck options' or 'Flashcards',#deck.cards>0 and ('Card '..index..' of '..#deck.cards)or 'Empty deck')
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
  U.text(p,t,U.style.fit(deck.title:upper(),w-2*p,'ui10',true),'ui10',true)
  local bars=math.min(24,#deck.cards);local gap=6;local bw=(w-2*p-gap*math.max(0,bars-1))//math.max(1,bars)
  for i=1,bars do U.style.panel({x=p+(i-1)*(bw+gap),y=t+32,w=bw,h=12},i==index)end
  local cy,ch=t+56,b-t-56
  U.style.panel({x=p,y=cy,w=w-2*p,h=ch})
  if #deck.cards>0 then
   U.text(w//2,cy+10,answer and 'Answer' or 'Question','ui10',false,'center')
   local font='lexend16';local lineH=smudge.line_height(font)
   local content=deck.cards[index][answer and 2 or 1]
   local all=smudge.wrapped_text(content,w-2*p-32,font,'bold',160)
   if all.total_height>ch-64 then font='ui10';lineH=smudge.line_height(font);all=smudge.wrapped_text(content,w-2*p-32,font,'bold',160)end
   for i,s in ipairs(all.lines)do U.text(w//2,cy+(ch-all.total_height)//2+(i-1)*lineH,s,font,true,'center')end
  else U.text(w//2,cy+ch//2,'Add your first card','ui10',true,'center')end
  U.actions({{'Option',function()menu=true;U.focus=1 end},{'+ Add',add},{answer and 'Next' or 'Answer',function()if answer then nextCard(1)else answer=true end end}},3)

 end
 U.footer()
end
function on_button(key,pressed)
 if not pressed then return end
 if confirmDelete then if key=='back'then confirmDelete=false else U.key(key)end;U.redraw();return end
 if U.editor then
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
 if #U.buttons>0 and U.style.hit({x=U.pad,y=U.top+56,w=U.w-2*U.pad,h=U.bottom-U.top-56},x,y)then answer=not answer;U.redraw()end
end
function on_swipe(dir) if not U.editor and not menu then nextCard(dir=='left' and 1 or -1)end end
function on_update()end
function on_exit()end
