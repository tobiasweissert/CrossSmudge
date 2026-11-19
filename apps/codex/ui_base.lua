-- Codex: Ink & Iron — Base UI Primitives

function draw_codex_divider(y, dw, margin)
    dw = dw or w
    margin = margin or 24
    local midX = math.floor(dw / 2)
    smudge.rect(midX - 3, y - 3, 7, 7, true)
    smudge.pixel(midX, y, false)
    smudge.rect(midX - 16, y - 2, 5, 5, true)
    smudge.rect(midX + 12, y - 2, 5, 5, true)
    smudge.pixel(midX - 26, y, true)
    smudge.pixel(midX + 26, y, true)
    smudge.line(margin, y, midX - 32, y)
    smudge.line(midX + 32, y, dw - margin, y)
end

function draw_miniature_frame(x,y,fw,fh)
    U.panel({x=x,y=y,w=fw,h=fh})
end
function draw_page_frame() end

function draw_monster(x, y, spriteId)
    spriteId = spriteId or 0
    draw_miniature_frame(x, y, 136, 136)
    smudge.draw_sprite(x + 4, y + 4, 128, 128, "sprites/monster_" .. tostring(spriteId) .. ".raw")
end

function draw_enemy_status(y, e)
    if not e then return y end
    draw_monster(w - 156, y, e.spriteId or 0)
    local cy = y
    local tag = e.isElite and " [ELITE]" or (e.isBoss and " [BOSS]" or "")
    smudge.text(22, cy, (e.name or "Enemy") .. tag, "ui12", "bold", "left")
    cy = cy + smudge.line_height("ui12") + 4

    local bw = w - 188
    smudge.rect(22, cy, bw, 12, false)
    if (e.maxHp or 0) > 0 then
        local fw = math.max(0, math.min(bw - 4, math.floor(((e.hp or 0) * (bw - 4)) / e.maxHp)))
        smudge.rect(24, cy + 2, fw, 8, true)
    end
    cy = cy + 16
    smudge.text(22, cy, (e.hp or 0) .. "/" .. (e.maxHp or 0) .. " HP (Ward: " .. (e.ward or 0) .. ")", "small", "regular", "left")
    cy = cy + smudge.line_height("small") + 4
    smudge.text(22, cy, "Intent: " .. (e.intent or "Attack") .. " (" .. (e.intentVal or 0) .. ")", "ui12", "bold", "left")
    cy = cy + smudge.line_height("ui12") + 2
    smudge.text(22, cy, "Rhythm: " .. (state.lastPlayedMeter or "None"), "small", "bold", "left")
    return y + 142
end

function draw_combat_header(ty)
    smudge.header("Codex Delve", "Ch.1 Pg " .. state.floor .. "/15")
    local y = ty + 6
    smudge.centered_text(y, "HP " .. state.playerHp .. "/" .. state.playerMaxHp .. "  Ward " .. state.playerWard .. "  Ink " .. state.ink .. "/" .. state.maxInk, "ui12", "bold", true)
    y = y + smudge.line_height("ui12") + 4
    draw_codex_divider(y, w, 24)
    y = draw_enemy_status(y + 8, state.enemy)
    draw_codex_divider(y, w, 24)
    return y + 8
end

function draw_codex_stats(y)
    smudge.centered_text(y, "HP: " .. state.playerHp .. "/" .. state.playerMaxHp .. " | Gold: " .. state.gold .. " | Deck: " .. #state.deck .. " | Relics: " .. #state.relics, "small", "bold", true)
    y = y + smudge.line_height("small") + 6
    draw_codex_divider(y, w, 28)
    return y + 10
end


