# 🚀 ARK Ultimate Mobile — Online Mod Menu & Dumper Engine

Bộ công cụ Mod Menu & Dump Code toàn diện dành cho game **ARK Mobile** trên Android (MuMu Player / Android Root).

---

## 🌟 Các Công Cụ Trong Repository

### 1. 📱 App Menu Nổi Online Cloud-Controlled ([ModMenuApp](file:///c:/ark/ModMenuApp/))
- **1 File APK Duy Nhất**: Hiển thị Menu Nổi đè lên game ARK Mobile.
- **Cập Nhật Online Trực Tiếp**: Nạp cấu hình giao diện & nút bấm từ file [`menu_config.json`](file:///c:/ark/menu_config.json) trên GitHub Repository này:
  ```text
  https://raw.githubusercontent.com/Huyhub1/Mod-menu-/main/menu_config.json
  ```
- **Admin Control**: Khi bạn chỉnh sửa file `menu_config.json` trên GitHub, tất cả người dùng ứng dụng APK sẽ tự động nhìn thấy nút bấm & tính năng mới mà không cần cài lại APK.

### 2. ⚡ Native C++ Binary Engine ([mod_menu_native.cpp](file:///c:/ark/mod_menu_native.cpp))
- Công cụ C++ chạy trực tiếp trên Termux Root với tốc độ cực nhanh (miligiây).
- Hỗ trợ menu tương tác CLI, quét Zone Spawn, Teleport XYZ, Patch Float RAM và lưu báo cáo Session.

### 3. 📜 GameGuardian LUA Mod Menu ([ark_mod_menu.lua](file:///c:/ark/ark_mod_menu.lua))
- Script Lua v3.0 chạy qua GameGuardian với đầy đủ module Dump Zone, Teleport, Creature Scanner & Session compare.

### 4. 📖 Tài Liệu Hướng Dẫn Kỹ Thuật ([KINH_NGHIEM_DUMP_MAP_ARK.md](file:///c:/ark/KINH_NGHIEM_DUMP_MAP_ARK.md))
- Tổng hợp toàn bộ kinh nghiệm kỹ thuật, cấu trúc bộ nhớ Unreal Engine và cách khắc phục sự cố.

---

## 🌐 Đường Link Raw Cấu Hình Online cho APK
Nhập đường link sau vào ứng dụng **ModMenuApp**:
```text
https://raw.githubusercontent.com/Huyhub1/Mod-menu-/main/menu_config.json
```
