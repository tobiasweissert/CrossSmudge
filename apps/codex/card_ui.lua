-- Card presentation; gameplay IDs and all effect text remain in kCards.
function coup_on(c)
    if not c or not c.couplet or c.couplet=='None' then return false end
    if state.playerStatus and state.playerStatus.transcendence then return true end
    local m=state.lastPlayedMeter
    return (c.couplet=='AfterBlade' and m=='Blade') or (c.couplet=='AfterWard' and m=='Ward') or (c.couplet=='AfterScript' and m=='Script')
end
local function wrap_all(s,width,font,bold)
    local out,line={},''
    local function emit()if line~=''then out[#out+1]=line;line=''end end
    for word in tostring(s):gmatch('%S+')do
        local candidate=line=='' and word or line..' '..word
        if smudge.text_width(candidate,font,bold)<=width then line=candidate
        else
            emit()
            for cp in word:gmatch('[%z\1-\127\194-\244][\128-\191]*')do
                if line~='' and smudge.text_width(line..cp,font,bold)>width then emit()end
                line=line..cp
            end
        end
    end
    emit();return out
end
function card_content_rows(c,cw)
    return wrap_all(string.upper(c.name),cw-12,'small',true),wrap_all(string.upper(c.desc),cw-12,'small',false),c.cDesc and wrap_all('COUPLET: '..string.upper(c.cDesc),cw-12,'small',coup_on(c))or {}
end
local function lines(x,y,s,width,font,bold,count,align)
    local r=wrap_all(s,width,font,bold)
    for i=1,math.min(#r,count or #r)do U.text(x,y,r[i],font,bold,align);y=y+smudge.line_height(font)end
    return y
end
function draw_card_item(x,y,cw,ch,c,isSel)
    if not c then return end
    U.panel({x=x,y=y,w=cw,h=ch})
    if isSel then smudge.rect_dither(x+2,y+2,cw-4,ch-4,false)end
    local lh=smudge.line_height('small')
    U.text(x+6,y+5,tostring(c.cost),'small')
    U.text(x+cw-6,y+5,U.fit(string.upper(c.meter),cw-26,'small'),'small',false,'right')
    local names,descs,bonuses=card_content_rows(c,cw)
    local cy=y+lh+8
    for _,line in ipairs(names)do U.text(x+6,cy,line,'small',true);cy=cy+lh end
    cy=cy+3
    for _,line in ipairs(descs)do U.text(x+6,cy,line,'small');cy=cy+lh end
    if #bonuses>0 then
        local by=math.max(cy+5,y+ch-6-#bonuses*lh)
        smudge.line(x+6,by-3,x+cw-6,by-3)
        for _,line in ipairs(bonuses)do U.text(x+6,by,line,'small',coup_on(c));by=by+lh end
    end
end
function render_card_inspector(ix,iy,iw,ih,c,checkCoup)
    if not c then return end
    U.panel({x=ix,y=iy,w=iw,h=ih})
    local cost='['..c.cost..' INK] '
    local meter=string.upper(c.meter)
    local mw=smudge.text_width(meter,'small',true)
    local title=cost..string.upper(c.name)
    local titleW=iw-20
    local withMeter=smudge.text_width(title,'small',true)+mw+16<=titleW
    if withMeter then U.text(ix+iw-10,iy+6,meter,'small',true,'right');titleW=titleW-mw-16 end
    local dy=lines(ix+10,iy+6,title,titleW,'small',true,3)+4
    local lh=smudge.line_height('small')
    dy=lines(ix+10,dy,c.desc,iw-20,'small',false,3)
    if c.cDesc then
        local bonus=wrap_all('COUPLET: '..c.cDesc,iw-20,'small',checkCoup and coup_on(c))
        local by=math.max(dy+4,iy+ih-6-#bonus*lh)
        smudge.line(ix+10,by-3,ix+iw-10,by-3)
        for _,line in ipairs(bonus)do U.text(ix+10,by,line,'small',checkCoup and coup_on(c));by=by+lh end
    end
end
