-- Decode into a candidate before applying anything. Unreadable saves stay intact.
return function()
    local data = smudge.load('codex_save', '')
    if not data or data == '' then return false end
    local ok, candidate, shop = pcall(dofile('save_codec.lua').decode, data)
    if not ok then
        state.saveBlocked = true
        state.saveNotice = 'Save preserved: unable to resume'
        return false
    end
    for k,v in pairs(candidate) do state[k]=v end
    shop_state=shop
    return true
end
