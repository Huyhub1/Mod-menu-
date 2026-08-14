-- ================================================================
-- ARK MOBILE MOD MENU v3.0 — DUMP + TEST CODE ENGINE
-- Script GameGuardian Lua — Chạy qua GG (Root Required)
-- Author: GoGs Ultimate GSV  |  Build: 2026-08-13
-- ================================================================
-- Cấu trúc:
--   [1] MODULE DUMP    — Zone, XYZ, AimTarget, AllCreatures, Realtime
--   [2] MODULE TEST    — WriteFloat, TeleportXYZ, SearchPatch, Batch, Verify
--   [3] MODULE SESSION — Save/Load file, Compare 2 scan
--   [4] CONFIG + LOOP
-- ================================================================

gg.showUiButton()

-- ================================================================
-- GLOBAL STATE
-- ================================================================

local VERSION = "v3.0"
local SAVE_FILE = "/sdcard/GG_ARK_Dump.txt"

local config = {
    testMode       = false,
    targetFilter   = "",
    maxResults     = 100,
    realtimeDelay  = 3000,
}

local session = {
    zoneList     = {},
    playerXYZ    = { addr = {x=0, y=0, z=0}, val = {x=0, y=0, z=0} },
    aimTarget    = "Chua co",
    creatures    = {},
    rawText      = "Chua co du lieu",
    lastScanTime = "N/A",
    prevZoneList = {},
}

-- ================================================================
-- HELPER FUNCTIONS
-- ================================================================

local function fmtHex(n)
    return string.format("0x%X", n or 0)
end

local function now()
    return os.date and os.date("%H:%M:%S") or "??:??:??"
end

local function trimStr(s)
    return s:match("^%s*(.-)%s*$")
end

local function dedup(list)
    local seen = {}
    local out  = {}
    for _, v in ipairs(list) do
        if not seen[v] then
            seen[v] = true
            out[#out + 1] = v
        end
    end
    return out
end

local function buildText(header, lines)
    local t = header .. "\n" .. string.rep("-", 48) .. "\n"
    for i, l in ipairs(lines) do
        t = t .. string.format("[%02d] %s\n", i, l)
    end
    t = t .. string.rep("-", 48) .. "\n" .. now()
    return t
end

local function alertAndCopy(text)
    session.rawText      = text
    session.lastScanTime = now()
    gg.alert(text)
    gg.copyText(text)
    gg.toast("Da Copy vao Clipboard!")
end

-- ================================================================
-- MODULE DUMP
-- ================================================================

local function dumpZones()
    gg.toast("Dang quet DinoSpawnEntries_...")
    gg.clearResults()

    session.prevZoneList = {}
    for _, z in ipairs(session.zoneList) do
        session.prevZoneList[#session.prevZoneList + 1] = z
    end

    gg.searchNumber("DinoSpawnEntries_", gg.TYPE_STRING)
    local results = gg.getResults(config.maxResults)

    session.zoneList = {}
    local rawZones   = {}

    for _, res in ipairs(results) do
        local val = res.value or ""
        if string.len(val) > 17 then
            if config.targetFilter == "" or
               string.lower(val):find(string.lower(config.targetFilter), 1, true) then
                rawZones[#rawZones + 1] = string.format("%s  [RAM: %s]", val, fmtHex(res.address))
                session.zoneList[#session.zoneList + 1] = val
            end
        end
    end

    session.zoneList = dedup(session.zoneList)

    if #rawZones == 0 then
        gg.alert("[-] Chua quet duoc Zone nao!\n\nGoi y:\n- Di chuyen nhan vat mot chut\n- Dam bao dang o trong map ARK\n- Bat quyen Root cho GG")
        return
    end

    local header = string.format("[D1] DUMP KHU VUC SPAWN (%d zone)\nFilter: \"%s\"",
        #session.zoneList, config.targetFilter == "" and "Tat ca" or config.targetFilter)
    alertAndCopy(buildText(header, rawZones))
end

local function dumpPlayerXYZ()
    gg.toast("Dang tim toa do nhan vat trong RAM...")

    local prompt = gg.prompt(
        {"Nhap gia tri Z hien tai cua nhan vat (xem ingame HUD):",
         "Sai so cho phep (mac dinh 5.0):"},
        {"200", "5.0"},
        {"number", "number"}
    )
    if not prompt then return end

    local zVal  = tonumber(prompt[1]) or 200
    local delta = tonumber(prompt[2]) or 5.0

    gg.clearResults()
    gg.searchNumber(zVal - delta, gg.TYPE_FLOAT, false, gg.SIGN_FUZZY_EQUAL, 0, -1)
    gg.searchNumber(zVal + delta, gg.TYPE_FLOAT, false, gg.SIGN_FUZZY_EQUAL, 0, -1)
    local results = gg.getResults(50)

    if #results == 0 then
        gg.alert("[-] Khong tim thay dia chi Float Z!\n\nHay nhap dung gia tri Z tu HUD ingame.")
        return
    end

    local lines = {}
    for i, res in ipairs(results) do
        lines[#lines + 1] = string.format("ADDR: %s  VAL: %.2f", fmtHex(res.address), res.value)
        if i >= 20 then break end
    end

    if results[1] then
        session.playerXYZ.addr.z = results[1].address
        session.playerXYZ.val.z  = results[1].value
    end

    local header = string.format("[D2] DIA CHI FLOAT Z (search %.1f+/-%.1f)", zVal, delta)
    local text = buildText(header, lines)
    text = text .. "\n\nDung TEST > Teleport XYZ de ghi gia tri vao cac dia chi nay!"
    alertAndCopy(text)
end

local function dumpAimTarget()
    gg.toast("Dang lay thong tin Dino/Target...")
    gg.clearResults()

    gg.searchNumber("APrimalDinoCharacter", gg.TYPE_STRING)
    local results = gg.getResults(30)

    local lines = {}
    for i, res in ipairs(results) do
        local val = res.value or ""
        if string.len(val) > 5 then
            lines[#lines + 1] = string.format("TYPE: %-35s  RAM: %s", val, fmtHex(res.address))
        end
        if #lines >= 8 then break end
    end

    gg.searchNumber("APrimalCharacter", gg.TYPE_STRING)
    local results2 = gg.getResults(20)
    for _, res in ipairs(results2) do
        local val = res.value or ""
        if string.len(val) > 5 then
            lines[#lines + 1] = string.format("TYPE: %-35s  RAM: %s", val, fmtHex(res.address))
        end
        if #lines >= 15 then break end
    end

    if #lines == 0 then
        gg.alert("[-] Khong tim thay Actor/Dino nao!\n\nHay dung gan Dino va ngam vao no truoc khi dump.")
        return
    end

    session.aimTarget = lines[1] or "N/A"
    alertAndCopy(buildText("[D3] AIM TARGET — DINO / CREATURE INFO", lines))
end

local function dumpAllCreatures()
    gg.toast("Dang quet tat ca Creatures trong vung nho...")

    local patterns = { "Dino_Character_BP_", "BP_Dino_", "APrimalDinoCharacter", "DinoCharMesh" }
    local allLines = {}
    local seen     = {}

    for _, pat in ipairs(patterns) do
        gg.clearResults()
        gg.searchNumber(pat, gg.TYPE_STRING)
        local results = gg.getResults(40)
        for _, res in ipairs(results) do
            local val = res.value or ""
            local key = fmtHex(res.address)
            if string.len(val) > 5 and not seen[key] then
                seen[key] = true
                if config.targetFilter == "" or
                   string.lower(val):find(string.lower(config.targetFilter), 1, true) then
                    allLines[#allLines + 1] = string.format("[%s] %s | %s", pat:sub(1,4), val, key)
                end
            end
        end
    end

    if #allLines == 0 then
        gg.alert("[-] Khong tim thay Creature nao!\nFilter: \"" .. config.targetFilter .. "\"\n\nGoi y: Vao khu rung, dao, hoac xoa filter.")
        return
    end

    session.creatures = allLines
    local header = string.format("[D4] ALL CREATURES — %d found (Filter: \"%s\")",
        #allLines, config.targetFilter == "" and "Tat ca" or config.targetFilter)
    alertAndCopy(buildText(header, allLines))
end

local function realtimeZoneWatcher()
    gg.toast("Bat Real-time Watcher! Tu dong quet moi " .. (config.realtimeDelay/1000) .. "s\nBam nut GG de thoat")

    local lastZone   = ""
    local iterations = 0

    while true do
        if gg.isVisible(true) then
            gg.setVisible(false)
            gg.toast("Da dung Real-time Watcher")
            break
        end

        iterations = iterations + 1
        gg.clearResults()
        gg.searchNumber("DinoSpawnEntries_", gg.TYPE_STRING)
        local results = gg.getResults(10)

        local currentZone = ""
        for _, res in ipairs(results) do
            local val = res.value or ""
            if string.len(val) > 17 then
                currentZone = val
                break
            end
        end

        if currentZone ~= "" and currentZone ~= lastZone then
            lastZone = currentZone
            session.zoneList[#session.zoneList + 1] = currentZone
            gg.toast("[" .. iterations .. "] ZONE: " .. currentZone)
        end

        gg.sleep(config.realtimeDelay)
    end
end

-- ================================================================
-- MODULE TEST CODE
-- ================================================================

local function testWriteFloat()
    local prompt = gg.prompt(
        {"Dia chi RAM (Hex, vd: 0x12A3F000):",
         "Gia tri Float muon ghi:"},
        {"", "0"},
        {"number", "number"}
    )
    if not prompt or prompt[1] == "" then return end

    local addr = tonumber(prompt[1], 16) or tonumber(prompt[1])
    local val  = tonumber(prompt[2]) or 0

    if not addr or addr == 0 then
        gg.toast("[-] Dia chi khong hop le!")
        return
    end

    gg.setValues({{ address = addr, flags = gg.TYPE_FLOAT, value = val }})
    gg.alert(string.format(
        "[T1] WRITE FLOAT THANH CONG!\n\nADDR : %s\nVALUE: %.4f\n\nNhin man hinh game de xac nhan.",
        fmtHex(addr), val
    ))
end

local function testTeleportXYZ()
    if not config.testMode then
        local confirm = gg.choice({"Bat Test Mode va tiep tuc", "Huy"}, nil,
            "Test Mode dang OFF!\nBat Test Mode de dung tinh nang nay?")
        if confirm == 1 then config.testMode = true else return end
    end

    local prompt = gg.prompt(
        {"Dia chi X (Hex):", "Dia chi Y (Hex):", "Dia chi Z (Hex):",
         "Gia tri X:", "Gia tri Y:", "Gia tri Z (+2000 de test):"},
        {"", "", "", "0", "0", "2000"},
        {"number", "number", "number", "number", "number", "number"}
    )
    if not prompt then return end

    local addrX = tonumber(prompt[1], 16) or tonumber(prompt[1])
    local addrY = tonumber(prompt[2], 16) or tonumber(prompt[2])
    local addrZ = tonumber(prompt[3], 16) or tonumber(prompt[3])
    local valX  = tonumber(prompt[4]) or 0
    local valY  = tonumber(prompt[5]) or 0
    local valZ  = tonumber(prompt[6]) or 2000

    local writes = {}
    if addrX and addrX ~= 0 then writes[#writes+1] = {address=addrX, flags=gg.TYPE_FLOAT, value=valX} end
    if addrY and addrY ~= 0 then writes[#writes+1] = {address=addrY, flags=gg.TYPE_FLOAT, value=valY} end
    if addrZ and addrZ ~= 0 then writes[#writes+1] = {address=addrZ, flags=gg.TYPE_FLOAT, value=valZ} end

    if #writes == 0 then
        gg.toast("[-] Can nhap it nhat 1 dia chi hop le!")
        return
    end

    gg.setValues(writes)

    session.playerXYZ.addr = {x = addrX or 0, y = addrY or 0, z = addrZ or 0}
    session.playerXYZ.val  = {x = valX, y = valY, z = valZ}

    gg.alert(string.format(
        "[T2] TELEPORT XYZ THUC THI!\n\n" ..
        "X: %s -> %.1f\n" ..
        "Y: %s -> %.1f\n" ..
        "Z: %s -> %.1f\n\n" ..
        "Nhan vat nay len = DUNG DIA CHI!",
        fmtHex(addrX or 0), valX,
        fmtHex(addrY or 0), valY,
        fmtHex(addrZ or 0), valZ
    ))
end

local function testSearchAndPatch()
    local prompt = gg.prompt(
        {"Nhap chuoi can Search (vd: DinoSpawnEntries_Beach):"},
        {"DinoSpawnEntries_"},
        {"text"}
    )
    if not prompt or prompt[1] == "" then return end

    local pattern = trimStr(prompt[1])
    gg.toast("Dang search: " .. pattern)
    gg.clearResults()
    gg.searchNumber(pattern, gg.TYPE_STRING)
    local results = gg.getResults(20)

    if #results == 0 then
        gg.alert("[-] Khong tim thay chuoi: \"" .. pattern .. "\"")
        return
    end

    local choices = {}
    for i, res in ipairs(results) do
        choices[i] = string.format("[%s] %s", fmtHex(res.address), (res.value or ""):sub(1, 40))
        if i >= 15 then break end
    end
    choices[#choices+1] = "Huy"

    local sel = gg.choice(choices, nil, "Chon dia chi de PATCH:")
    if not sel or sel == #choices then return end

    local targetAddr = results[sel].address

    local patchPrompt = gg.prompt(
        {"Gia tri moi (Float):",
         "Offset tu ket qua (Hex, mac dinh 0):"},
        {"0", "0"},
        {"number", "number"}
    )
    if not patchPrompt then return end

    local newVal    = tonumber(patchPrompt[1]) or 0
    local offset    = tonumber(patchPrompt[2], 16) or tonumber(patchPrompt[2]) or 0
    local patchAddr = targetAddr + offset

    gg.setValues({{ address = patchAddr, flags = gg.TYPE_FLOAT, value = newVal }})
    gg.alert(string.format(
        "[T3] PATCH THANH CONG!\n\nSearch: \"%s\"\nDia chi goc: %s\nOffset: +%s\nPatch tai: %s\nGia tri: %.4f",
        pattern, fmtHex(targetAddr), fmtHex(offset), fmtHex(patchAddr), newVal
    ))
end

local function testBatchEdit()
    gg.alert(
        "[T4] BATCH EDIT — HUONG DAN:\n\n" ..
        "Nhap danh sach ADDRESS:VALUE:\n\n" ..
        "  0x12A3F000:2000\n" ..
        "  0x12A3F004:100.5\n" ..
        "  0x12A3F008:0\n\n" ..
        "Moi dong = 1 dia chi Float."
    )

    local prompt = gg.prompt(
        {"Dan danh sach ADDRESS:VALUE (moi lenh 1 dong):"},
        {""},
        {"text"}
    )
    if not prompt or prompt[1] == "" then return end

    local input  = prompt[1]
    local writes = {}
    local errors = {}

    for line in (input .. "\n"):gmatch("([^\n]*)\n") do
        line = trimStr(line)
        if line ~= "" then
            local hexPart, valPart = line:match("^(0x%x+):([%d%.-]+)$")
            if not hexPart then
                hexPart, valPart = line:match("^(%x+):([%d%.-]+)$")
            end
            if hexPart and valPart then
                local addr = tonumber(hexPart, 16)
                local val  = tonumber(valPart)
                if addr and val then
                    writes[#writes+1] = {address = addr, flags = gg.TYPE_FLOAT, value = val}
                else
                    errors[#errors+1] = "Parse err: " .. line
                end
            else
                errors[#errors+1] = "Format err: " .. line
            end
        end
    end

    if #writes == 0 then
        gg.alert("[-] Khong parse duoc lenh nao!\n\nLoi:\n" .. table.concat(errors, "\n"))
        return
    end

    gg.setValues(writes)

    local msg = string.format("[T4] BATCH EDIT HOAN TAT!\n\nDa ghi: %d dia chi", #writes)
    if #errors > 0 then
        msg = msg .. "\nBo qua " .. #errors .. " dong loi:\n" .. table.concat(errors, "\n")
    end
    gg.alert(msg)
end

local function testVerifyAddress()
    local prompt = gg.prompt(
        {"Dia chi RAM can doc (Hex):",
         "Gia tri ky vong (de trong = chi doc):"},
        {"", ""},
        {"number", "number"}
    )
    if not prompt or prompt[1] == "" then return end

    local addr     = tonumber(prompt[1], 16) or tonumber(prompt[1])
    local expected = prompt[2] ~= "" and tonumber(prompt[2]) or nil

    if not addr or addr == 0 then
        gg.toast("[-] Dia chi khong hop le!")
        return
    end

    local readFloat = gg.getValues({{ address=addr, flags=gg.TYPE_FLOAT }})
    local readDword = gg.getValues({{ address=addr, flags=gg.TYPE_DWORD }})

    local fVal = readFloat and readFloat[1] and readFloat[1].value or "ERR"
    local dVal = readDword and readDword[1] and readDword[1].value or "ERR"

    local match = ""
    if expected ~= nil and type(fVal) == "number" then
        local diff = math.abs(fVal - expected)
        match = diff < 0.01 and "\n\nKET QUA: KHOP GIA TRI KY VONG!" or
                               string.format("\n\nLECH: %.4f (ky vong %.4f, diff=%.4f)", fVal, expected, diff)
    end

    gg.alert(string.format(
        "[T5] DOC DIA CHI: %s\n\nFLOAT : %s\nDWORD : %s%s",
        fmtHex(addr),
        type(fVal) == "number" and string.format("%.6f", fVal) or tostring(fVal),
        type(dVal) == "number" and string.format("%d (0x%X)", dVal, dVal) or tostring(dVal),
        match
    ))
end

-- ================================================================
-- MODULE SESSION
-- ================================================================

local function saveSession()
    local lines = {
        "=== GG ARK DUMP SESSION ===",
        "Thoi gian: " .. now(),
        "",
        "--- ZONE LIST (" .. #session.zoneList .. " zones) ---",
    }
    for i, z in ipairs(session.zoneList) do
        lines[#lines+1] = string.format("[%02d] %s", i, z)
    end
    lines[#lines+1] = ""
    lines[#lines+1] = "--- PLAYER XYZ ---"
    lines[#lines+1] = string.format("X: %s -> %.2f", fmtHex(session.playerXYZ.addr.x), session.playerXYZ.val.x)
    lines[#lines+1] = string.format("Y: %s -> %.2f", fmtHex(session.playerXYZ.addr.y), session.playerXYZ.val.y)
    lines[#lines+1] = string.format("Z: %s -> %.2f", fmtHex(session.playerXYZ.addr.z), session.playerXYZ.val.z)
    lines[#lines+1] = ""
    lines[#lines+1] = "--- CREATURES (" .. #session.creatures .. ") ---"
    for i, c in ipairs(session.creatures) do
        lines[#lines+1] = c
        if i >= 30 then lines[#lines+1] = "... (truncated)"; break end
    end
    lines[#lines+1] = "==========================="

    local content = table.concat(lines, "\n")

    local ok = false
    local f = io.open and io.open(SAVE_FILE, "w")
    if f then f:write(content); f:close(); ok = true end

    gg.copyText(content)
    if ok then
        gg.alert("[S1] SESSION DA LUU!\n\nFile: " .. SAVE_FILE .. "\nDa copy vao Clipboard\n\nZones: " .. #session.zoneList)
    else
        gg.alert("[S1] Khong the ghi file!\n\nNhung da Copy vao Clipboard!\nZones: " .. #session.zoneList)
    end
end

local function loadSession()
    local f = io.open and io.open(SAVE_FILE, "r")
    if not f then
        gg.alert("[-] Khong tim thay file:\n" .. SAVE_FILE .. "\n\nHay Save session truoc!")
        return
    end
    local content = f:read("*all")
    f:close()

    if not content or content == "" then gg.alert("[-] File rong!"); return end

    session.zoneList = {}
    for line in content:gmatch("[^\n]+") do
        local zone = line:match("%[%d+%] (DinoSpawnEntries_[%w_]+)")
        if zone then session.zoneList[#session.zoneList+1] = zone end
    end

    session.rawText = content
    gg.copyText(content)
    gg.alert("[S2] SESSION DA LOAD!\n\nFile: " .. SAVE_FILE ..
             "\nZones: " .. #session.zoneList .. "\n\n(Da Copy vao Clipboard)")
end

local function compareSession()
    if #session.prevZoneList == 0 and #session.zoneList == 0 then
        gg.alert("[-] Chua co du lieu de so sanh!\nHay Dump Zone it nhat 2 lan.")
        return
    end

    local prevSet = {}
    for _, z in ipairs(session.prevZoneList) do prevSet[z] = true end
    local currSet = {}
    for _, z in ipairs(session.zoneList) do currSet[z] = true end

    local newZones  = {}
    local goneZones = {}
    local sameZones = {}

    for _, z in ipairs(session.zoneList) do
        if prevSet[z] then sameZones[#sameZones+1] = z else newZones[#newZones+1] = z end
    end
    for _, z in ipairs(session.prevZoneList) do
        if not currSet[z] then goneZones[#goneZones+1] = z end
    end

    local lines = {
        "[S3] SO SANH 2 LAN DUMP ZONE",
        string.rep("-", 40),
        string.format("Zone MOI xuat hien: %d", #newZones),
    }
    for _, z in ipairs(newZones)  do lines[#lines+1] = "  + " .. z end
    lines[#lines+1] = string.format("Zone da MAT: %d", #goneZones)
    for _, z in ipairs(goneZones) do lines[#lines+1] = "  - " .. z end
    lines[#lines+1] = string.format("Zone GIONG NHAU: %d", #sameZones)
    lines[#lines+1] = string.rep("-", 40)
    lines[#lines+1] = "Tip: Di chuyen sang khu vuc khac roi Dump lai!"

    local text = table.concat(lines, "\n")
    gg.copyText(text)
    gg.alert(text)
end

-- ================================================================
-- SUB-MENUS
-- ================================================================

local function menuDump()
    while true do
        local sel = gg.choice({
            "[D1] Dump Tat Ca Zone Spawn",
            "[D2] Dump Toa Do XYZ Nhan Vat",
            "[D3] Dump Aim Target (Dino/Actor Dang Ngam)",
            "[D4] Dump Tat Ca Creatures Trong Vung",
            "[D5] Bat Real-time Zone Watcher",
            "Quay lai Menu Chinh"
        }, nil, "=== MODULE DUMP (" .. VERSION .. ") ===")

        if not sel or sel == 6 then break end
        if sel == 1 then dumpZones()           end
        if sel == 2 then dumpPlayerXYZ()       end
        if sel == 3 then dumpAimTarget()       end
        if sel == 4 then dumpAllCreatures()    end
        if sel == 5 then realtimeZoneWatcher(); break end
    end
end

local function menuTestCode()
    while true do
        local sel = gg.choice({
            "[T1] Write Float — Ghi Gia Tri Vao 1 Dia Chi",
            "[T2] Teleport XYZ — Ghi 3 Dia Chi (X,Y,Z) Cung Luc",
            "[T3] Search & Patch — Tim Chuoi -> Chon -> Patch",
            "[T4] Batch Edit — Paste Danh Sach ADDRESS:VALUE",
            "[T5] Verify Address — Doc & Kiem Tra Dia Chi",
            "Quay lai Menu Chinh"
        }, nil, "=== MODULE TEST CODE (" .. VERSION .. ") ===\nTest Mode: " ..
                (config.testMode and "ON (BAT)" or "OFF (TAT)"))

        if not sel or sel == 6 then break end
        if sel == 1 then testWriteFloat()       end
        if sel == 2 then testTeleportXYZ()      end
        if sel == 3 then testSearchAndPatch()   end
        if sel == 4 then testBatchEdit()        end
        if sel == 5 then testVerifyAddress()    end
    end
end

local function menuSession()
    while true do
        local zCount = #session.zoneList
        local pCount = #session.prevZoneList
        local cCount = #session.creatures

        local sel = gg.choice({
            "[S1] Save Session -> File /sdcard/ + Clipboard",
            "[S2] Load Session <- File /sdcard/",
            "[S3] So Sanh 2 Lan Dump (Compare)",
            string.format("[S4] Xem Ket Qua Moi Nhat (Zones:%d | Prev:%d | Crt:%d)", zCount, pCount, cCount),
            "Quay lai Menu Chinh"
        }, nil, "=== MODULE SESSION (" .. VERSION .. ") ===\n" .. session.lastScanTime)

        if not sel or sel == 5 then break end
        if sel == 1 then saveSession()   end
        if sel == 2 then loadSession()   end
        if sel == 3 then compareSession() end
        if sel == 4 then
            if session.rawText ~= "Chua co du lieu" then
                gg.copyText(session.rawText)
                gg.alert(session.rawText)
            else
                gg.alert("Chua co du lieu. Hay vao MODULE DUMP truoc!")
            end
        end
    end
end

local function menuConfig()
    while true do
        local sel = gg.choice({
            "Dat Bo Loc Ten (Filter: \"" .. (config.targetFilter == "" and "Tat ca" or config.targetFilter) .. "\")",
            "Test Mode: " .. (config.testMode and "ON (BAT)" or "OFF (TAT)"),
            "Realtime Delay: " .. config.realtimeDelay .. "ms",
            "Max Results moi lan Scan: " .. config.maxResults,
            "Thong Tin Script & Phien Ban",
            "Quay lai Menu Chinh"
        }, nil, "=== CAU HINH (" .. VERSION .. ") ===")

        if not sel or sel == 6 then break end

        if sel == 1 then
            local p = gg.prompt({"Nhap ten Dino/Zone can loc (vd: Beach, Rex)\nDe trong = Tat ca:"},
                                {config.targetFilter}, {"text"})
            if p then config.targetFilter = trimStr(p[1]) end

        elseif sel == 2 then
            config.testMode = not config.testMode
            gg.toast("Test Mode: " .. (config.testMode and "BAT" or "TAT"))

        elseif sel == 3 then
            local p = gg.prompt({"Delay (ms, min 500):"},
                                {tostring(config.realtimeDelay)}, {"number"})
            if p and tonumber(p[1]) then config.realtimeDelay = math.max(500, tonumber(p[1])) end

        elseif sel == 4 then
            local p = gg.prompt({"So ket qua toi da (10-500):"},
                                {tostring(config.maxResults)}, {"number"})
            if p and tonumber(p[1]) then config.maxResults = math.min(500, math.max(10, tonumber(p[1]))) end

        elseif sel == 5 then
            gg.alert(
                "=== ARK MOD MENU " .. VERSION .. " ===\n\n" ..
                "[D] DUMP MODULE : Zone, XYZ, Target, Creatures, Realtime\n" ..
                "[T] TEST MODULE : WriteFloat, TeleportXYZ, Search&Patch, Batch, Verify\n" ..
                "[S] SESSION     : Save/Load File, Compare 2 Scan\n\n" ..
                "Platform: GameGuardian + Android Root\n" ..
                "Target  : ARK Mobile (studiowildcard)\n" ..
                "File    : " .. SAVE_FILE .. "\n" ..
                "Build   : 2026-08-13 | GoGs Ultimate GSV"
            )
        end
    end
end

-- ================================================================
-- MAIN MENU
-- ================================================================

local function main_menu()
    local sel = gg.choice({
        "[1] DUMP MODULE    — Zone / XYZ / Target / Creatures",
        "[2] TEST CODE      — Write / Teleport / Patch / Batch / Verify",
        "[3] SESSION        — Save / Load / Compare",
        "[4] CAU HINH       — Filter / Test Mode / Delay / MaxResults",
        "[5] KET QUA MOI NHAT + COPY",
        "[6] HIDE / MINIMIZE (An Menu)"
    }, nil,
    "=== ARK MOD MENU " .. VERSION .. " — DUMP+TEST ===\n" ..
    "Filter: \"" .. (config.targetFilter == "" and "Tat ca" or config.targetFilter) ..
    "\" | Test: " .. (config.testMode and "ON" or "OFF") ..
    " | Zones: " .. #session.zoneList)

    if sel == 1 then menuDump()
    elseif sel == 2 then menuTestCode()
    elseif sel == 3 then menuSession()
    elseif sel == 4 then menuConfig()
    elseif sel == 5 then
        if session.rawText ~= "Chua co du lieu" then
            gg.copyText(session.rawText)
            gg.alert(session.rawText)
        else
            gg.alert("Chua co du lieu. Hay vao MODULE DUMP truoc!")
        end
    elseif sel == 6 then
        gg.toast("Menu da an! Cham icon GG de mo lai.")
    end
end

-- ================================================================
-- MAIN LOOP
-- ================================================================

gg.toast("ARK Mod Menu " .. VERSION .. " da khoi dong!\nBam nut GG bat cu luc nao de mo menu.")

while true do
    if gg.isVisible(true) then
        gg.setVisible(false)
        main_menu()
    end
    gg.sleep(100)
end
