-- Skill list: while a player holds the charge button, their skills and specials slide in at the bottom corner on
-- their side (Sparking Zero style): the buttons that use them (all with charge held), skills' cost in blast stocks
-- (a number), specials' in ki bars (segments). Moves that cannot be paid for now are greyed.
-- Only a player's own list: not the computer's, and online only this copy's player (game.me), whose picture this is.
-- HUD units: the picture is hud.h (448) high and hud.w wide.

local PLATE, PLATE_EDGE = 0x16202CE6, 0x2C3A4ACC
local TEXT, TEXT_OFF, SPECIAL = 0xFFFFFFFF, 0x8C939BFF, 0xE9C46AFF
local KI_ON, KI_OFF = 0xF2C21BFF, 0x3B3112CC
local COST_BG, COST_EDGE = 0x020508F5, 0x56606DDD
local ICON, ICON_RING, ICON_MARK = 0xD8DDE3FF, 0x5B6570FF, 0x3A434DFF

local slide = { 0, 0 } -- per side: 0 hidden .. 1 shown
local hud              -- the drawing calls, given to on_hud

-- a slanted plate (its top shifted outwards by lean), mirrored with X
local function plate(X, x, y, w, h, lean, colour)
    hud.poly({ X(x + lean), y, X(x + w + lean), y, X(x + w), y + h, X(x), y + h }, colour)
end

-- Every cost keeps its own dark backing, independent of the optional row plate.
-- This remains readable over bright attacks without adding a panel to the list.
local function cost_plate(X, x, y, w, h, S)
    plate(X, x - S, y - S, w + 2 * S, h + 2 * S, 2 * S, COST_EDGE)
    plate(X, x, y, w, h, 2 * S, COST_BG)
end

-- a button icon: a grey disc with a circle or a triangle, or the left stick with an arrow
local function icon(cx, cy, r, what, dir)
    hud.disc(cx, cy, r, ICON)
    hud.circle(cx, cy, r, ICON_RING, 1)
    if what == "circle" then
        hud.circle(cx, cy, r * 0.45, ICON_MARK, 1.6)
    elseif what == "triangle" then
        local a = r * 0.5
        hud.path({ cx, cy - a, cx - a, cy + a * 0.75, cx + a, cy + a * 0.75 }, ICON_MARK, 1.6, true)
    else
        hud.text(cx, cy - r * 0.55, "L", ICON_MARK, r * 1.1, "center")
        local ay = dir == "up" and cy - r * 1.1 or cy + r * 1.1
        local tip = dir == "up" and ay - r * 0.35 or ay + r * 0.35
        hud.poly({ cx - r * 0.4, ay, cx + r * 0.4, ay, cx, tip }, ICON_MARK)
    end
end

local function list(f, left, t, S, compact, paneRight)
    local W = paneRight or hud.w
    local function X(x) return left and x or W - x end
    local rows = {}
    for _, m in ipairs(f.moves or {}) do
        if m.name ~= "" then rows[#rows + 1] = m end
    end
    local step = (compact and 25 or 31) * S
    local y0 = hud.h - 16 - #rows * step            -- the last row ends 16 above the bottom
    if paneRight then y0 = y0 - (1 - t) * 12 * S end
    local off = paneRight and 0 or (1 - t) * -210 * S
    for row, m in ipairs(rows) do
        local y = y0 + (row - 1) * step
        local ultimate = m.kind == "ultimate"
        local special = m.kind == "special" or ultimate
        local x = 16 + 12 * S + 24 * S + off + (special and 10 * S or 0)
        local cost = special and m.cost / 20000 or m.cost / 100000
        if not compact then
            plate(X, x - 2 * S, y - 2 * S, 196 * S, 25 * S, 11 * S, PLATE_EDGE)
            plate(X, x, y, 192 * S, 21 * S, 10 * S, PLATE)
        end
        hud.text(X(x + 14 * S), y + 3 * S, m.name, m.usable and (special and SPECIAL or TEXT) or TEXT_OFF,
                 13 * S, left and "left" or "right", { fit = (compact and 145 or 160) * S, font = "game",outline=0x06101AFF,outline_size=1.2 })
        if ultimate then
            -- The ultimate requires maximum power, rather than ordinary ki.
            local ready = f.powered_up and m.usable
            local color = ready and SPECIAL or TEXT_OFF
            if not compact then
                hud.text(X(x + 183 * S), y - 2 * S, "S", color, 19 * S, "center")
            end
            local locale = bt3.locale() or "en"
            local label = bt3.label("combo-max-power/0/0") or
                (locale == "pt-BR" and "PODER MÁXIMO" or locale == "es" and "MÁXIMO PODER" or "MAX POWER")
            if compact then
                local sx, sy = x + 14 * S, y + 17 * S
                cost_plate(X, sx, sy, 91 * S, 9 * S, S)
                hud.text(X(sx + 4 * S), sy, label, color, 8 * S,
                         left and "left" or "right", { fit = 83 * S, font = "game" })
            else
                hud.text(X(x + 14 * S), y + 23 * S, label, color,
                         9 * S, left and "left" or "right", { fit = 160 * S, font = "game" })
            end
        elseif special then                             -- ki cost: one segment a bar
            local count = math.max(1, math.floor(cost + 0.5))
            local sx, sy = x + 14 * S, y + (compact and 18 or 23) * S
            local pitch, width = (compact and 15 or 24) * S, (compact and 12 or 21) * S
            cost_plate(X, sx - 2 * S, sy - 2 * S, (count - 1) * pitch + width + 4 * S, 8 * S, S)
            for k = 1, count do
                plate(X, sx + (k - 1) * pitch, sy, width, 4 * S, 2 * S,
                      f.ki >= k * 20000 and KI_ON or KI_OFF)
            end
        else                                            -- blast stocks: retain the original numeral style
            if compact then
                -- Anchor to the measured name, including its fitted width.
                -- A fixed column left a large gap beside short technique names.
                hud.text(6 * S, y - 4 * S, tostring(math.floor(cost + 0.5)), m.usable and TEXT or TEXT_OFF,
                         24 * S, "center", { outline = 0x0B1118FF, lean = 0.12,
                         after = left, before = not left })
            else
                hud.text(X(x + 183 * S), y - 4 * S, tostring(math.floor(cost + 0.5)), m.usable and TEXT or TEXT_OFF,
                         24 * S, "center", { outline = 0x0B1118FF, lean = 0.12 })
            end
        end
        if bt3.setting("buttons") then                  -- the button, and the stick's direction before it
            local bx = x - 13 * S
            icon(X(bx), y + 10.5 * S, 9.5 * S, m.button)
            if m.direction ~= "" then icon(X(bx - 23 * S), y + 10.5 * S, 9.5 * S, "stick", m.direction) end
        end
    end
end

return {
    name = "Skill list",
    version = "1.0",
    description = "While charge is held: your skills and specials with their buttons and costs, at the bottom corner.",
    settings = {
        { id = "size", label = "Size", type = "slider", min = 50, max = 150, step = 5, default = 100 },
        { id = "always", label = "Always shown", type = "toggle", default = false,
          tip = "Show the list all the time, not only while charge is held." },
        { id = "buttons", label = "Show buttons", type = "toggle", default = true,
          tip = "The buttons (and stick directions) that use each move, beside it." },
    },

    on_hud = function(h, game)
        hud = h
        if not game.hud_shown or not game.fighters then return end
        local S = bt3.setting("size") / 100
        for side = 1, 2 do
            local f = game.fighters[side]
            local tag = game.tag_team
            local watching = tag and tag.spectating and tag.spectating[side]
            -- Boost, clash and switch prompts own this space while they animate.
            -- Do not suppress their buttons with an always-visible technique list.
            if watching or (f and f.prompt_shown) then
                slide[side] = 0
            elseif f and not f.cpu and (game.me == nil or game.me == side) then
                local want = (f.charge_held or bt3.setting("always")) and 1 or 0
                slide[side] = slide[side] + (want - slide[side]) * 0.35
                if slide[side] < 0.01 then slide[side] = 0 end
                if slide[side] > 0 then
                    -- the left side is player 1's unless this copy shows the sides swapped
                    if game.split and (tag or game.presentation_split) then
                        list(f, false, slide[side], math.min(S, .72), true, hud.w * side / 2)
                    elseif tag then
                        list(f,false,slide[side],math.min(S,.72),true)
                    else
                        list(f, (side == 1) ~= (game.swapped == true), slide[side], S)
                    end
                end
            end
        end
    end,
}
