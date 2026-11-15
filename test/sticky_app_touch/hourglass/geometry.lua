-- Production app, neutral drawing/file/clock adapters, no substitute UI model.
local files, calls = {}, {}
local now, width, height, held = 1000, 480, 800, false
smudge = {}
for _,name in ipairs({'clear','rect','rounded_rect','circle','line','thick_line','rect_dither','text','header','button_hints','full_refresh','invert_screen','exit','prevent_sleep','request_update'}) do
    smudge[name] = function(...)
        local args=table.pack(...); local parts={name}
        for i=1,args.n do parts[#parts+1]=tostring(args[i]) end
        if MODE=='trace' then print(table.concat(parts,'|')) end
        calls[#calls+1]={name=name,args=args}
    end
end
smudge.get_bounds=function() return width,height end
smudge.millis=function() return now end
smudge.is_button_down=function() return held end
smudge.read_file=function(path) return files[path] end
smudge.write_file=function(path,data) files[path]=data; if MODE=='trace' then print('SAVE|'..path..'|'..data) end end
if DEVICE~='missing' then smudge.get_device=function() return DEVICE end end
local function state(name)
    local seen={}
    local function search(fn)
        if seen[fn] then return end; seen[fn]=true
        for i=1,100 do
            local key,value=debug.getupvalue(fn,i); if not key then break end
            if key==name then return true,value end
            if type(value)=='function' then local ok,result=search(value); if ok then return true,result end end
        end
    end
    for _,fn in ipairs({on_touch,on_button,on_draw,on_update}) do local ok,value=search(fn); if ok then return value end end
    error('unreachable state '..name)
end
local function eq(name,value) assert(state(name)==value,name..': expected '..tostring(value)..', got '..tostring(state(name))) end
local function reset()
    files={}; now=1000; held=false
    assert(loadfile(SOURCE))(); on_init(); calls={}; on_draw()
end
local function tap(x,y,event)
    if DEVICE=='sticky' then on_touch(x,y,event or 'tap') else on_touch(event or 'tap',x,y) end
end
local function enter_settings()
    if height==480 then
        -- Inherited fixed-height main UI puts the card below the landscape viewport.
        -- Enter through the real physical hold path instead of an offscreen tap.
        held=true; on_update(); now=now+600; on_update(); held=false; on_button('confirm',true)
    else tap(math.floor(width/2),550) end
    eq('in_settings',true)
end
local function open()
    reset(); enter_settings(); calls={}; on_draw()
    local boxes={}
    for _,c in ipairs(calls) do
        if c.name=='rounded_rect' and c.args[3]==260 and c.args[4]==36 and c.args[6]==false then
            boxes[#boxes+1]={x=c.args[1],y=c.args[2],w=c.args[3],h=c.args[4]}
        end
    end
    assert(#boxes==2,'expected two production-rendered value boxes')
    return boxes
end
local failed,total=0,0
local function test(id,fn)
    total=total+1
    local ok,err=pcall(fn)
    print((ok and 'PASS ' or 'FAIL ')..DEVICE..' '..width..'x'..height..' '..id..(ok and '' or ': '..tostring(err)))
    if not ok then failed=failed+1 end
end
for _,bounds in ipairs({{480,800},{800,480}}) do
    width,height=bounds[1],bounds[2]
    if DEVICE=='sticky' then
        test('upper-portion-repro',function() open(); tap(math.floor(width/2)+60,280); eq('step_top_sec',600); eq('step_bot_sec',60) end)
        for row=1,2 do
            for _,edge in ipairs({'upper-left','upper-right','lower-left','lower-right','left','right'}) do
                test('row'..row..'-'..edge,function()
                    local b=open()[row]
                    local right=edge:find('right')~=nil
                    local x=right and b.x+b.w-1 or b.x+1
                    local y=b.y+math.floor(b.h/2)
                    if edge:find('upper') then y=b.y+1 elseif edge:find('lower') then y=b.y+b.h-1 end
                    tap(x,y)
                    eq('step_top_sec',row==1 and (right and 600 or 180) or 300)
                    eq('step_bot_sec',row==2 and (right and 90 or 45) or 60)
                    eq('setting_row',row); eq('in_settings',true)
                    assert(files['hourglass.dat']==('900;'..state('step_top_sec')..';'..state('step_bot_sec')))
                end)
            end
            for _,edge in ipairs({'above','below','left','right'}) do
                test('row'..row..'-miss-'..edge,function()
                    local b=open()[row]; local x,y=b.x+b.w/2,b.y+b.h/2
                    if edge=='above' then y=b.y-1 elseif edge=='below' then y=b.y+b.h else x=edge=='left' and b.x-1 or b.x+b.w end
                    tap(x,y); eq('step_top_sec',300); eq('step_bot_sec',60); eq('in_settings',true)
                end)
            end
        end
        test('gap-outside-nontap',function()
            local b=open(); for _,p in ipairs({{width/2,330},{0,300},{width-1,390},{width/2,100}}) do tap(p[1],p[2]) end
            tap(b[1].x+200,b[1].y+5,'drag'); eq('step_top_sec',300); eq('step_bot_sec',60); eq('in_settings',true)
        end)
    else
        test('legacy-touch-grid-and-stock-order-ignored',function()
            for _,p in ipairs({{width/2+60,280},{0,300},{width/2-100,310},{width/2+100,390},{width-1,400},{width/2,330}}) do
                open(); tap(p[1],p[2]); on_draw()
            end
            open(); on_touch(width/2,300,'tap'); eq('step_top_sec',300); eq('step_bot_sec',60); on_draw()
        end)
    end
    test('save-reload-back-and-done',function()
        open(); on_button('right',true); eq('step_top_sec',600)
        on_button('back',true); eq('in_settings',false); assert(files['hourglass.dat']=='900;600;60')
        on_exit(); assert(loadfile(SOURCE))(); on_init(); eq('step_top_sec',600)
        enter_settings(); tap(width/2,510); eq('in_settings',false)
    end)
    test('timer-controls-alarm-and-exit',function()
        reset(); on_button('right',false); eq('total_sec',900)
        on_button('up',true); eq('total_sec',600); on_button('down',true); eq('total_sec',900)
        tap(width/2,200); eq('is_running',true); now=3000; on_update(); eq('remaining_sec',898)
        tap(width/2,200,'click'); eq('is_running',false); now=5000; on_update(); eq('remaining_sec',898)
        on_button('confirm',true); eq('is_running',true); on_button('left',true); eq('remaining_sec',900); eq('has_started',false)
        on_button('confirm',true); now=1000000; on_update(); eq('is_alarm',true); on_draw()
        tap(0,0); eq('is_alarm',false); eq('remaining_sec',900); on_button('back',true); on_exit()
    end)
    test('physical-settings-hold-select-and-save',function()
        reset(); held=true; on_update(); now=1600; on_update(); eq('in_settings',true)
        held=false; on_button('confirm',true); eq('setting_row',1)
        on_button('confirm',true); eq('setting_row',2); on_button('left',true); eq('step_bot_sec',45)
        on_button('up',true); on_button('right',true); eq('step_top_sec',600)
        on_button('down',true); eq('setting_row',2); on_button('back',true); eq('in_settings',false)
        on_draw()
    end)
end
print('TOTAL '..total..' FAILED '..failed)
assert(failed==0,'geometry/control failures: '..failed)
