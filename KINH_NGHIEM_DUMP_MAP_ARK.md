# HƯỚNG DẪN & KINH NGHIỆM KỸ THUẬT: DUMP KHU VỰC SPAWN & TOẠ ĐỘ MAP ARK MOBILE

> **Mục tiêu**: Trích xuất chính xác 100% **Toạ độ ($X, Y, Z$)** và **Mã Khu Vực Spawn Dino (`DinoSpawnEntries_..._C`)** trên giả lập Android (MuMu Player 12 / Android Root) để phục vụ modding, thêm thú mới vào bản đồ.

> **⚡ Phiên bản mới nhất**: `ark_mod_menu.lua` **v3.0** — DUMP+TEST ENGINE (2026-08-13)

---

## I. CƠ CHẾ BỘ NHỚ UNREAL ENGINE (ARK MOBILE)

1. **Cấu trúc Khu vực Spawn (`UDinoSpawnEntries`)**:
   - Mọi vùng spawn thú trên bản đồ ARK Mobile đều được định danh bởi một đối tượng `UDinoSpawnEntries` gắn với khối không gian `ADinoSpawnZoneVolume`.
   - Tên khu vực trong bộ nhớ RAM có định dạng chuẩn: `DinoSpawnEntries_Beach_C`, `DinoSpawnEntries_Jungle_1_C`, `DinoSpawnEntries_Redwood_C`, `DinoSpawnEntries_Mountain_C`, `DinoSpawnEntries_Swamp_C`...
2. **Tại sao quét số Float ngẫu nhiên dễ bị sai?**:
   - Trong 4GB RAM giả lập chứa hàng triệu số float (ma trận xoay đồ hoạ, âm thanh).
   - Quét lọc theo chuỗi cấu trúc **`DinoSpawnEntries_`** là phương pháp chính xác 100% tuyệt đối.

---

## II. TỔNG HỢP CÁC CÔNG CỤ & GIẢI PHÁP ĐÃ THIẾT LẬP

### 1. Dùng Termux trên MuMu Player (Nhanh & Chính xác nhất)

#### A. Lệnh 1 Dòng Duy Nhất (Không cần cài gì thêm):
```bash
su
PID=$(pgrep -f wildcard || pgrep -f ark || ps -A | grep -i wildcard | awk '{print $2}')
grep -a -o "DinoSpawnEntries_[A-Za-z0-9_]*" /proc/$PID/mem | sort -u
```

#### B. Khắc phục lỗi `I/O error` trong Linux RAM:
Khi đọc `/proc/$PID/mem`, gặp lỗi `I/O error` do chạm vào vùng nhớ chưa cấp phát. Sử dụng đoạn Python 1-liner lọc qua `/proc/$PID/maps`:
```bash
python -c '
import re
pid = "'$PID'"
found = set()
with open(f"/proc/{pid}/maps", "r") as f:
    for line in f:
        p = line.split()
        if len(p) >= 2 and ("rw" in p[1] or "r-" in p[1]):
            s, e = [int(x, 16) for x in p[0].split("-")]
            if e - s <= 20*1024*1024:
                try:
                    with open(f"/proc/{pid}/mem", "rb", 0) as m:
                        m.seek(s)
                        data = m.read(e - s)
                        for match in re.findall(b"DinoSpawnEntries_[A-Za-z0-9_]+", data):
                            val = match.decode("ascii", errors="ignore")
                            if len(val) > 17: found.add(val)
                except: pass
print("=== DUMP THÀNH CÔNG ===")
for z in sorted(found): print("  >>> " + z)
'
```

---

### 2. Dùng Native C++ Biên Dịch Trong Termux (`mod_menu_native.cpp`) — **v3.0 NATIVE ENGINE**

- **File nguồn**: [mod_menu_native.cpp](file:///c:/ark/mod_menu_native.cpp)
- **Ưu điểm**: Đọc/Ghi RAM với tốc độ miligiây (ms), giao diện Menu Terminal tương tác ANSI màu sắc, không cần cài GameGuardian.
- **Tính năng v3.0 (8 chức năng CLI Menu)**:
  1. `[1] Dump Spawn Zones`: Quét `DinoSpawnEntries_...` trong RAM, lọc trùng tên và xem RAM address.
  2. `[2] Realtime Watcher`: Tự động theo dõi vùng spawn khi di chuyển map (toast cập nhật mỗi N giây).
  3. `[3] Dump Aim Target`: Dò tìm `APrimalDinoCharacter`, `APrimalCharacter`, `Dino_Character_BP_`.
  4. `[4] Float Z & Teleport XYZ`: Tìm địa chỉ Z từ HUD và ghi toạ độ X, Y, Z trực tiếp vào RAM.
  5. `[5] Patch Float Memory`: Ghi giá trị Float tùy chỉnh vào bất kỳ địa chỉ Hex nào.
  6. `[6] Verify Address`: Đọc kiểm tra giá trị FLOAT & DWORD tại 1 địa chỉ RAM.
  7. `[7] Target Filter`: Đặt bộ lọc tên (ví dụ: `Beach`, `Rex`, `Jungle`).
  8. `[8] Export Session`: Lưu báo cáo chi tiết ra `/sdcard/ARK_Native_Dump.txt`.

#### Quy trình biên dịch và chạy:
1. Biên dịch ở giao diện thường (`$`):
   ```bash
   clang++ /sdcard/Download/mod_menu_native.cpp -o /data/local/tmp/modmenu
   ```
2. Chạy file bằng quyền Root (`#`):
   ```bash
   su
   chmod 755 /data/local/tmp/modmenu
   /data/local/tmp/modmenu
   ```

---

### 3. Cheat Engine trên PC (Soi RAM Giả lập MuMu Player)

1. Mở Cheat Engine ➔ Chọn tiến trình **`MuMuNxDevice.exe`** (hoặc `MuMuVMMHeadless.exe`).
2. Value Type chọn **String** ➔ Nhập `DinoSpawnEntries_` ➔ Click **First Scan**.
3. **Mẹo xem trọn chuỗi tên dài**:
   - Nếu tên bị ngắt thành `String[22]`, double click vào cột Type đổi số `22` thành `50` hoặc `100`.
   - Click chuột phải chọn **Text Encoding** ➔ **ASCII / UTF-8**.
   - Mở cửa sổ **Memory Viewer (`Ctrl + B`)** để xem chuỗi tự động thay đổi theo vị trí nhân vật.

---

### 4. Giao diện Menu Nổi LUA (`ark_mod_menu.lua`) — **v3.0 DUMP+TEST ENGINE**

- **File nguồn**: [ark_mod_menu.lua](file:///c:/ark/ark_mod_menu.lua)
- **Yêu cầu**: GameGuardian + Android Root
- **📦 MODULE DUMP** (5 tính năng):
  - `[D1] dumpZones()`: Quét `DinoSpawnEntries_`, dedup, hiện list đầy đủ + RAM address.
  - `[D2] dumpPlayerXYZ()`: Scan Float Z bằng giá trị HUD, trả về list địa chỉ RAM khớp.
  - `[D3] dumpAimTarget()`: Quét `APrimalDinoCharacter` / `APrimalCharacter` gần nhân vật.
  - `[D4] dumpAllCreatures()`: Quét nhiều pattern Dino, dedup theo địa chỉ, apply filter.
  - `[D5] realtimeZoneWatcher()`: Auto-scan mỗi N giây, toast khi zone thay đổi.
- **⚡ MODULE TEST CODE** (5 tính năng):
  - `[T1] testWriteFloat()`: Nhập địa chỉ hex + giá trị → `gg.setValues` ngay.
  - `[T2] testTeleportXYZ()`: Nhập 3 địa chỉ X/Y/Z + 3 giá trị → ghi cùng lúc.
  - `[T3] testSearchAndPatch()`: Search string → chọn địa chỉ từ list → patch float + offset.
  - `[T4] testBatchEdit()`: Paste `ADDRESS:VALUE` nhiều dòng → parse → apply batch.
  - `[T5] testVerifyAddress()`: Đọc FLOAT + DWORD tại địa chỉ → so sánh expected.
- **💾 MODULE SESSION** (3 tính năng):
  - `[S1] saveSession()`: Ghi toàn bộ zones/XYZ/creatures ra `/sdcard/GG_ARK_Dump.txt` + clipboard.
  - `[S2] loadSession()`: Load lại file, parse zone list vào bộ nhớ.
  - `[S3] compareSession()`: So sánh 2 lần dump, highlight zone mới/mất/giống nhau.

---

### 5. Ứng Dụng APK Menu Nổi Online Cloud-Controlled (`ModMenuApp`) — **v3.0 OVERLAY APK**

- **Thư mục nguồn Android Project**: [ModMenuApp](file:///c:/ark/ModMenuApp/)
- **File mẫu Cấu hình Server Online**: [menu_config.json](file:///c:/ark/menu_config.json)
- **Cấu trúc & Nguyên lý 1-APK Duy Nhất**:
  - `SYSTEM_ALERT_WINDOW`: Tạo Cửa sổ nổi Drag & Drop đè lên màn hình Game, có nút Bong bóng thu nhỏ (Badge Icon).
  - `RemoteConfigFetcher`: Tự động nạp danh sách nút bấm, giao diện, lệnh dump từ Server Online (URL JSON) mỗi khi mở menu.
  - `DynamicUIFactory`: Tự động vẽ nút bấm (Button), ô nhập (Input), công tắc (Toggle) trực tiếp từ JSON online.
  - **Admin Control**: Bạn có toàn quyền thêm/sửa nút bấm và câu lệnh trên Server Web mà **KHÔNG CẦN** người dùng tải lại file APK mới.

---

## III. CÁC LỖI THƯỜNG GẶP & CÁCH SỬA KHI THỰC HIỆN

| Sự Cố | Nguyên Nhân | Cách Khắc Phục |
| :--- | :--- | :--- |
| **`actively refused it (10061)`** | MuMu Player chưa khởi động vào màn hình chính Android. | Khởi động máy ảo MuMu đến màn hình chính Android rồi mới dùng ADB. |
| **`clang++: inaccessible or not found`** | Chạy `clang++` khi đang ở giao diện root (`su`). | Thoát khỏi `su`, biên dịch ở giao diện thường `$`, sau đó mới `su` để chạy file trong `/data/local/tmp/`. |
| **`sh: /sdcard/...: noexec`** | Thẻ nhớ `/sdcard/` trên Android bị cấm thực thi nhị phân. | Đưa file nhị phân ra thư mục hệ thống `/data/local/tmp/` và `chmod 755`. |
| **Android 12 báo "Không tương thích" với GameGuardian** | Play Protect hoặc Android 12 chặn target SDK cũ. | Tạo máy ảo **Android 7** hoặc **Android 9** trong **MuMu Multi-Drive**, hoặc dùng Native C++ / Termux. |
| **`Windows cannot access cdm.exe`** | Windows Defender tự chặn file cài Cheat Engine. | Mở Windows Security ➔ Protection History ➔ Chọn "Allow on device", hoặc tích "Unblock" trong Properties. |

---

*Tài liệu được tổng hợp và lưu trữ tự động tại workspace `c:\ark\`.*
