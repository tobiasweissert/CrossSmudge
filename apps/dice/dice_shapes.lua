-- Original candidate shape paths, fitted into the redesigned body.
return function(die,current_roll,centerX,centerY,size)
    local lineThickness=3
    if die.name == "Coin" then
        if current_roll == 1 then
            smudge.draw_sprite(centerX - 56, centerY - 56, 112, 112, "sprites/coin_heads.raw")
        elseif current_roll == 2 then
            smudge.draw_sprite(centerX - 56, centerY - 56, 112, 112, "sprites/coin_tails.raw")
        else
            smudge.circle(centerX, centerY, size, false, 5)
            smudge.circle(centerX, centerY, size - 8, false, 2)
            smudge.circle(centerX, centerY, size - 14, false, 1)
        end
    elseif die.name == "D4" then
        smudge.thick_line(centerX, centerY - size, centerX - size, centerY + size, lineThickness)
        smudge.thick_line(centerX - size, centerY + size, centerX + size, centerY + size, lineThickness)
        smudge.thick_line(centerX + size, centerY + size, centerX, centerY - size, lineThickness)
    elseif die.name == "D6" then
        local left = centerX - size
        local right = centerX + size
        local top = centerY - size
        local bottom = centerY + size
        smudge.thick_line(left, top, right, top, lineThickness)
        smudge.thick_line(right, top, right, bottom, lineThickness)
        smudge.thick_line(right, bottom, left, bottom, lineThickness)
        smudge.thick_line(left, bottom, left, top, lineThickness)
    elseif die.name == "D8" then
        smudge.thick_line(centerX, centerY - size, centerX + size, centerY, lineThickness)
        smudge.thick_line(centerX + size, centerY, centerX, centerY + size, lineThickness)
        smudge.thick_line(centerX, centerY + size, centerX - size, centerY, lineThickness)
        smudge.thick_line(centerX - size, centerY, centerX, centerY - size, lineThickness)
    elseif die.name == "D10" then
        local waistY = centerY - math.floor(size / 5)
        local waistX = math.floor(size * 9 / 10)
        local bottomY = centerY + size + 5
        smudge.thick_line(centerX, centerY - size, centerX + waistX, waistY, lineThickness)
        smudge.thick_line(centerX + waistX, waistY, centerX, bottomY, lineThickness)
        smudge.thick_line(centerX, bottomY, centerX - waistX, waistY, lineThickness)
        smudge.thick_line(centerX - waistX, waistY, centerX, centerY - size, lineThickness)
    elseif die.name == "D12" then
        local p1x, p1y = centerX, centerY - size
        local p2x, p2y = centerX + math.floor(size * 95 / 100), centerY - math.floor(size * 31 / 100)
        local p3x, p3y = centerX + math.floor(size * 59 / 100), centerY + math.floor(size * 81 / 100)
        local p4x, p4y = centerX - math.floor(size * 59 / 100), centerY + math.floor(size * 81 / 100)
        local p5x, p5y = centerX - math.floor(size * 95 / 100), centerY - math.floor(size * 31 / 100)
        smudge.thick_line(p1x, p1y, p2x, p2y, lineThickness)
        smudge.thick_line(p2x, p2y, p3x, p3y, lineThickness)
        smudge.thick_line(p3x, p3y, p4x, p4y, lineThickness)
        smudge.thick_line(p4x, p4y, p5x, p5y, lineThickness)
        smudge.thick_line(p5x, p5y, p1x, p1y, lineThickness)
    elseif die.name == "D20" then
        local hx1, hy1 = centerX, centerY - size
        local hx2, hy2 = centerX + math.floor(size * 87 / 100), centerY - math.floor(size / 2)
        local hx3, hy3 = centerX + math.floor(size * 87 / 100), centerY + math.floor(size / 2)
        local hx4, hy4 = centerX, centerY + size
        local hx5, hy5 = centerX - math.floor(size * 87 / 100), centerY + math.floor(size / 2)
        local hx6, hy6 = centerX - math.floor(size * 87 / 100), centerY - math.floor(size / 2)

        smudge.thick_line(hx1, hy1, hx2, hy2, lineThickness)
        smudge.thick_line(hx2, hy2, hx3, hy3, lineThickness)
        smudge.thick_line(hx3, hy3, hx4, hy4, lineThickness)
        smudge.thick_line(hx4, hy4, hx5, hy5, lineThickness)
        smudge.thick_line(hx5, hy5, hx6, hy6, lineThickness)
        smudge.thick_line(hx6, hy6, hx1, hy1, lineThickness)

        smudge.thick_line(hx1, hy1, hx3, hy3, lineThickness)
        smudge.thick_line(hx3, hy3, hx5, hy5, lineThickness)
        smudge.thick_line(hx5, hy5, hx1, hy1, lineThickness)
    end

end
