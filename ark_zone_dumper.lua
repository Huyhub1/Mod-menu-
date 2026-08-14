-- =============================================================
-- GAMEGUARDIAN FLOATING MENU: DUMP KHU VỰC ARK MOBILE (ROOT)
-- =============================================================

gg.showUiButton()

function main()
    local menu = gg.choice({
        "📍 1. Dump Khu Vực Nơi Nhân Vật Đang Đứng",
        "⚡ 2. Bật Theo Dõi Real-time (Hiện Thẳng Màn Hình)",
        "📋 3. Xem & Copy Mã Zone Mới Nhất",
        "❌ 4. Thát Menu"
    }, nil, "=== MENU DUMP KHU VỰC ARK MOBILE (MUMU ROOT) ===")

    if menu == 1 then dumpCurrentZone() end
    if menu == 2 then startRealtimeWatcher() end
    if menu == 3 then showLastZone() end
    if menu == 4 then os.exit() end
end

local lastZoneName = "Chưa có dữ liệu"

function dumpCurrentZone()
    gg.clearResults()
    gg.searchNumber("DinoSpawnEntries_", gg.TYPE_STRING)
    local results = gg.getResults(50)
    
    local foundList = {}
    for i, res in ipairs(results) do
        local val = res.value
        if string.len(val) > 17 and not foundList[val] then
            foundList[val] = true
            lastZoneName = val
        end
    end
    
    if lastZoneName ~= "Chưa có dữ liệu" then
        gg.alert("📍 TÌM THẤY KHU VỰC HIỆN TẠI:\n\n" .. lastZoneName)
        gg.copyText(lastZoneName)
        gg.toast("Đã tự động Copy: " .. lastZoneName)
    else
        gg.toast("[-] Chưa quét thấy Zone. Hãy di chuyển nhân vật một chút!")
    end
end

function startRealtimeWatcher()
    gg.toast("⚡ Đã bật theo dõi Real-time! Hãy di chuyển nhân vật...")
    while true do
        if gg.isVisible(true) then
            gg.setVisible(false)
            main()
            break
        end
        
        dumpCurrentZone()
        gg.sleep(3000)
    end
end

function showLastZone()
    if lastZoneName ~= "Chưa có dữ liệu" then
        gg.copyText(lastZoneName)
        gg.alert("Mã Zone mới nhất:\n" .. lastZoneName .. "\n\n(Đã tự động Copy vào Clipboard)")
    else
        gg.alert("Chưa có dữ liệu Zone nào được dump!")
    end
end

-- Chạy vòng lặp Menu
while true do
    if gg.isVisible(true) then
        gg.setVisible(false)
        main()
    end
    gg.sleep(100)
end
