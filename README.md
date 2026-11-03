# Connect Four Online (Nhóm 11)

Ứng dụng game đối kháng theo lượt **Connect Four Online** (bàn cờ cố định 6 hàng × 7 cột) sử dụng giao thức tầng ứng dụng **NDJSON v2.0** trên nền **TCP**.

## Thành viên Nhóm 11
- **A - Trần Trung Hiếu (20235934):** Network, `poll()`, Session, Lobby (`main.c`, `network.[hc]`, `lobby.[hc]`)
- **B - Nguyễn Đức Hoàng (20235938):** Game Engine, Match Lifecycle, ELO, Bot AI, PostgreSQL Database (`game_engine.[hc]`, `match.[hc]`, `elo.[hc]`, `ai_bot.[hc]`, `database.[hc]`)
- **C - Phạm Song Hào (20235930):** Client GUI (CustomTkinter), Network Thread, Mock Server & Replay (`client_python/`)

---

## Tài liệu dự án
- [`PLAN.md`](PLAN.md): Kế hoạch tổng thể, kiến trúc hệ thống, thiết kế DB, thuật toán ELO & Bot AI, 32 trường hợp biên và lộ trình.
- [`protocol_ndjson.md`](protocol_ndjson.md): Đặc tả giao thức tầng ứng dụng NDJSON v2.0 (Bản chuẩn Tiếng Việt).
- [`protocol_ndjson_en.md`](protocol_ndjson_en.md): Đặc tả giao thức tầng ứng dụng NDJSON v2.0 (Tiếng Anh).
- [`server_c/protocol.h`](server_c/protocol.h): Header C định nghĩa hằng số, mã lỗi, enum, struct và các hàm kiểm tra giao thức dùng chung.

---

## Hướng dẫn cài đặt và chạy

### 1. Cài đặt thư viện phụ thuộc (Ubuntu / Debian)
```bash
sudo apt update
sudo apt install -y build-essential postgresql libpq-dev libcjson-dev python3 python3-pip python3-tk
pip3 install customtkinter
```

### 2. Khởi tạo cơ sở dữ liệu PostgreSQL
```bash
# Tạo database connect_four và nạp bảng + tài khoản Bot (BOT_EASY, BOT_MEDIUM, BOT_HARD)
sudo -u postgres createdb connect_four
sudo -u postgres psql -d connect_four -f database/schema.sql

# Tạo file cấu hình kết nối DB từ file mẫu
cp database/db_config.env.example database/db_config.env
```

### 3. Biên dịch và chạy Server C
```bash
cd server_c
make check-protocol
make
./connect4_server
```

### 4. Test nhanh Server bằng `nc`
```bash
nc localhost 5000
{"type":"REGISTER","id":1,"user":"hieu","pass":"123456"}
{"type":"LOGIN","id":2,"user":"hieu","pass":"123456"}
{"type":"PLAY_BOT","id":3,"difficulty":"MEDIUM","turn_time":30}
```
