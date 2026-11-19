-- Bounded versioned text codec. Legacy writer aliases remain readable.
local M={}
local nums={floor={1,15},maxFloor={1,15},gold={0,1000000},playerHp={0,100000},playerMaxHp={10,100000},playerWard={0,100000},bonusWard={0,100000},permStrength={0,10000},baseMaxInk={1,6},ink={0,10000},maxInk={1,10000},eventId={0,8},selectedHandIdx={1,512},rewardIdx={1,3},turn={1,100000},pendingDiscard={0,1}}
local statuses={strength='n',weak='n',vuln='n',poison='n',retainWard='b',extraDraw='n',transcendence='b',scriptPlayed='n'}
local screens={combat=true,reward=true,shop=true,scriptorium=true,outcome=true}
local meters={None=true,Blade=true,Ward=true,Script=true}
local function number(s,lo,hi)
 local n=type(s)=='number' and s or (type(s)=='string' and s:match('^%d+$') and tonumber(s))
 assert(n and n==math.floor(n) and n>=lo and n<=hi,'invalid number');return n
end
local function list(s,hi,sold)
 assert(type(s)=='string' and #s<=4096,'invalid list');local t={}
 if s=='' then return t end
 for v in (s..','):gmatch('(.-),')do
  assert(#t<512,'list limit');t[#t+1]=(sold and v=='-1') and -1 or number(v,0,hi)
 end
 return t
end
local function text(s,n)assert(type(s)=='string' and #s<=n and not s:find('[\r\n%z]'),'invalid text');return s end
local function boolean(s)assert(s=='0' or s=='1','invalid boolean');return s=='1'end
function M.encode()
 local lines={'schema=2'}
 local function put(k,v)lines[#lines+1]=k..'='..tostring(v)end
 for k in pairs(nums)do if state[k]~=nil then put(k,state[k])end end
 for _,k in ipairs({'deck','relics','hand','drawPile','discardPile','cardRewards'})do put(k,table.concat(state[k] or {},','))end
 for _,k in ipairs({'savedScreen','phase','lastPlayedMeter','outcomeText'})do if state[k]~=nil then put(k,state[k])end end
 for k,t in pairs(statuses)do local v=state.playerStatus[k];if v~=nil then put('ps_'..k,t=='b' and (v and '1' or '0') or v)end end
 if state.enemy then
  local e=state.enemy
  for _,k in ipairs({'name','hp','maxHp','ward','spriteId','intent','intentVal'})do put('enemy_'..k,e[k])end
  put('enemy_isElite',e.isElite and '1' or '0');put('enemy_isBoss',e.isBoss and '1' or '0')
  for _,k in ipairs({'strength','weak','vuln','poison'})do put('es_'..k,(e.status or {})[k] or 0)end
 end
 put('shop_cards',table.concat(shop_state.cards or {},','));put('shop_relics',table.concat(shop_state.relics or {},','));put('shop_heal',shop_state.heal and '1' or '0')
 return table.concat(lines,'\n')
end
function M.decode(data)
 assert(type(data)=='string' and #data<=32768,'save size')
 local v={};for line in (data..'\n'):gmatch('(.-)\r?\n')do
  if line~='' then local k,s=line:match('^([%w_]+)=(.*)$');assert(k and not v[k],'invalid/duplicate field');v[k]=s end
 end
 assert(not v.schema or v.schema=='2','unknown schema')
 local aliases={playerHp='hp',playerMaxHp='maxHp',playerWard='ward',drawPile='draw',discardPile='disc'}
 for k,a in pairs(aliases)do if not v[k] then v[k]=v[a] end end
 assert(v.floor,'missing floor');local out={}
 for k,range in pairs(nums)do
  if v.schema then assert(v[k],'missing numeric field')end
  if v[k] then out[k]=number(v[k],range[1],range[2])end
 end
 for _,k in ipairs({'deck','hand','drawPile','discardPile','cardRewards','relics'})do out[k]=list(v[k] or '',k=='relics' and 11 or 29)end
 assert(#out.deck>0,'empty deck');assert((out.playerHp or 50)<=(out.playerMaxHp or 50),'HP exceeds maximum')
 out.savedScreen=v.savedScreen or 'combat';if out.savedScreen=='title' or out.savedScreen=='deck_view' then out.savedScreen='combat'end
 assert(screens[out.savedScreen],'invalid screen')
 out.phase=v.phase or (out.savedScreen=='reward' and 'reward' or 'battle');assert(out.phase=='battle' or out.phase=='reward','invalid phase')
 out.lastPlayedMeter=v.lastPlayedMeter or 'None';assert(meters[out.lastPlayedMeter],'invalid meter')
 out.outcomeText=text(v.outcomeText or '',1024)
 out.playerStatus={};for k,t in pairs(statuses)do
  if t=='b' then out.playerStatus[k]=false else out.playerStatus[k]=0 end
  if v['ps_'..k] then if t=='b' then out.playerStatus[k]=boolean(v['ps_'..k])else out.playerStatus[k]=number(v['ps_'..k],0,100000)end end
 end
 if not v.ps_strength then out.playerStatus.strength=out.permStrength or 0 end
 assert((out.selectedHandIdx or 1)<=math.max(1,#out.hand),'hand selection')
 assert((out.pendingDiscard or 0)<=#out.hand,'discard choice')
 if v.enemy_name or v.enemyName then
  local e={status={}};for _,k in ipairs({'name','hp','maxHp','ward','spriteId','intent','intentVal','isElite','isBoss'})do
   local legacy='enemy'..k:sub(1,1):upper()..k:sub(2);local s=v['enemy_'..k] or v[legacy]
   if k=='name' then e[k]=text(s,128)
   elseif k=='intent' then e[k]=s or 'Attack';assert(e[k]=='Attack' or e[k]=='Defend' or e[k]=='AttackAndDefend','invalid intent')
   elseif k=='isElite' or k=='isBoss' then e[k]=boolean(s or '0')
   else e[k]=number(s or '0',0,k=='spriteId' and 11 or 100000)end
  end
  assert(e.maxHp>0 and e.hp<=e.maxHp,'enemy HP');for _,k in ipairs({'strength','weak','vuln','poison'})do e.status[k]=number(v['es_'..k] or '0',0,100000)end;out.enemy=e
  -- Old victory saves labeled combat but omitted reward offers. Regenerate only
  -- unavailable offers; never replay the gold/heal payout already recorded.
  if e.hp==0 and out.savedScreen=='combat' then out.savedScreen='reward';out.phase='reward' end
 end
 if out.savedScreen=='reward' and #out.cardRewards==0 and not v.schema then
  for i=1,3 do out.cardRewards[i]=math.random(0,29)end
 end
 if v.schema and out.savedScreen=='reward' then assert(#out.cardRewards==3,'reward choices')end
 local shop={cards=list(v.shop_cards or '',29,true),relics=list(v.shop_relics or '',11,true),heal=boolean(v.shop_heal or '0')}
 if v.schema and out.savedScreen=='shop' then assert(#shop.cards==3 and #shop.relics==2,'shop slots')end
 out.inRun=true;return out,shop
end
return M
