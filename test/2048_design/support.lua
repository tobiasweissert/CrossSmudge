local H = {}
function H.copy(t) local r={} for k,v in pairs(t or {}) do r[k]=v end return r end
function H.app(file, device, store)
    local env = setmetatable({}, {__index=_G})
    local api = H.copy(smudge)
    local state = H.copy(store)
    local writes, calls = {}, {}
    env.smudge=api
    env.os={time=function() return 2048 end}
    if device == 'missing' then api.get_device=nil else api.get_device=function() return device end end
    api.load=function(k,d) return state[k] or d end
    api.save=function(k,v) writes[#writes+1]=k..'='..v; if not env.fail_save then state[k]=v end end
    api.log=function(s) env.last_log=s end
    api.exit=function() env.exited=true end
    api.dofile=function(p)
        local path = p:find('/') and p or 'apps/2048/'..p
        return assert(load(smudge.read_file(path,1000000),'@'..path,'t',env))()
    end
    env.dofile=api.dofile
    -- Trace every legacy API argument, including booleans, without inventing fonts.
    for _,name in ipairs({'header','button_hints','text','centered_text','rect','rect_dither','clear'}) do
        local original=api[name]
        api[name]=function(...)
            local args={...}; local s=name
            for _,v in ipairs(args) do s=s..'|'..tostring(v) end
            calls[#calls+1]=s
            return original(...)
        end
    end
    api.dofile(file);env.on_init()
    return env,state,writes,calls
end
function H.equal(a,b)
    for _,k in ipairs({'board','score','game_over'}) do assert(a[k]==b[k],k..': '..tostring(a[k])..' ~= '..tostring(b[k])) end
end
function H.flat(values)
    local t={} for i=1,16 do t[i]=tostring(values[i] or 0) end
    return table.concat(t,',')
end
return H
