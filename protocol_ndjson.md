# Đặc tả giao thức tầng ứng dụng: Connect Four Online (NDJSON)

Nhóm 10. Phiên bản giao thức: **2.0** (chuyển từ dạng text sang NDJSON)

Tài liệu này là chuẩn chung giữa **server (C)** và **client (Python)**. Mọi thay đổi phải được cả nhóm đồng ý và cập nhật tại đây trước khi code.

---

## 1. Tổng quan

- **Tầng giao vận:** TCP, cổng mặc định **5000**.
- **Định dạng:** NDJSON (Newline Delimited JSON). Mỗi message là **một đối tượng JSON nằm trên đúng một dòng**, kết thúc bằng `\n`.
- **Mô hình:** client gửi **lệnh** (có `id`), server trả **phản hồi** (`OK` hoặc `ERR`, echo lại `id`). Ngoài ra server có thể tự gửi **sự kiện** (không có `id`) bất kỳ lúc nào, ví dụ có người thách đấu, đối thủ vừa đi.
- **Server là bên quyết định:** server giữ bàn cờ, kiểm tra nước đi và xác định kết quả. Client chỉ gửi ý định và hiển thị.

---

## 2. Quy tắc định dạng (framing và cú pháp)

| Mục | Quy định |
|---|---|
| Mã hóa | UTF-8. Các giá trị trong giao thức (username, tên lệnh, mã trạng thái) chỉ dùng ASCII |
| Ranh giới message | Mỗi message là **một dòng** kết thúc bằng `\n` (chấp nhận `\r\n`, bỏ `\r`) |
| Nội dung dòng | Đúng một **đối tượng JSON** (không phải mảng, chuỗi hay số đứng một mình) |
| Xuống dòng | JSON phải **compact**, không có ký tự xuống dòng thật bên trong. Xuống dòng trong chuỗi phải escape thành `\n` (thư viện JSON tự làm) |
| Độ dài tối đa | **4096 byte** mỗi dòng, kể cả `\n`. Dòng dài hơn: server trả `ERR 108`, bỏ toàn bộ dòng đó (đọc và bỏ cho đến `\n` kế tiếp) |
| Trường bắt buộc | Mọi message có trường `"type"` (chuỗi). Mọi lệnh của client có thêm `"id"` (số nguyên dương) |
| Tên lệnh / sự kiện | Chữ HOA, ví dụ `"LOGIN"` |
| Trường lạ | Bên nhận **bỏ qua** các trường không biết (giúp thêm trường mới mà không phá bản cũ) |
| Username | 3 đến 16 ký tự, chỉ gồm `A-Z a-z 0-9 _` |
| Password | 6 đến 32 ký tự, không chứa dấu cách |

**Xử lý stream (bắt buộc cho cả hai phía):** TCP là dòng byte, một lần `recv` có thể chứa nửa message, một message, hoặc nhiều message dính nhau. Mỗi phía phải giữ một buffer, nối dữ liệu mới vào và chỉ xử lý khi đã có đủ một dòng kết thúc bằng `\n`. Phần còn lại ở lại buffer. Chỉ sau khi tách được một dòng đầy đủ mới đưa cho bộ parse JSON.

**Danh sách:** mọi danh sách (người chơi, phòng, lịch sử) nằm trong **một mảng JSON trên một dòng**, nên sự kiện của server không thể chen vào giữa danh sách.

**Kiểm tra dữ liệu đầu vào (server):** mọi trường là dữ liệu không tin cậy. Server phải kiểm tra trường có tồn tại, đúng kiểu (ví dụ `col` phải là số nguyên, không phải chuỗi) và đúng khoảng giá trị trước khi dùng. Sai thì trả `ERR 101`.

---

## 3. Các giá trị dùng chung

| Khái niệm | Giá trị |
|---|---|
| Bàn cờ | 7 cột (đánh số **0 đến 6**), 6 hàng (đánh số **0 đến 5**, hàng 0 là dưới cùng) |
| Quân cờ | `RED` đi trước, `YELLOW` đi sau. Ký hiệu trong bàn cờ: `R`, `Y`, `.` (ô trống) |
| Trạng thái người chơi | `IDLE` (ở sảnh), `IN_ROOM` (trong phòng chờ), `IN_GAME` (đang chơi) |
| Kết quả | `WIN`, `LOSS`, `DRAW` (theo góc nhìn của người nhận) |
| ELO khởi đầu | 1200, hệ số K = 32 |
| Mã ván (`game_id`) | số nguyên dương do server cấp |

**Biểu diễn bàn cờ** (dùng trong `SYNC`): mảng **6 chuỗi**, mỗi chuỗi 7 ký tự `R`, `Y`, `.`. Phần tử đầu là **hàng trên cùng (hàng 5)**, phần tử cuối là hàng dưới cùng (hàng 0), mỗi chuỗi đi từ cột 0 đến 6.

Ví dụ bàn có quân vàng ở cột 2 và quân đỏ ở cột 3 (đều ở hàng 0, hàng dưới cùng):

```
[".......",".......",".......",".......",".......","..YR..."]
```

---

## 4. Máy trạng thái của một kết nối

```
CONNECTED --LOGIN--> IDLE --CREATE_ROOM/JOIN_ROOM--> IN_ROOM --đủ 2 người--> IN_GAME
                       ^  --CHALLENGE+ACCEPT----------------------------------^   |
                       |                                                          |
                       +--------------------- GAME_OVER --------------------------+
```

**Lệnh hợp lệ theo trạng thái:**

| Lệnh | CONNECTED | IDLE | IN_ROOM | IN_GAME |
|---|:-:|:-:|:-:|:-:|
| `PING` | ✔ | ✔ | ✔ | ✔ |
| `REGISTER`, `LOGIN` | ✔ | | | |
| `LOGOUT`, `CHANGEPASS`, `PROFILE`, `HISTORY`, `GET_GAME` | | ✔ | ✔ | ✔ |
| `LIST_PLAYERS`, `LIST_ROOMS` | | ✔ | ✔ | |
| `CREATE_ROOM`, `JOIN_ROOM` | | ✔ | | |
| `LEAVE_ROOM` | | | ✔ | |
| `CHALLENGE`, `ACCEPT`, `DECLINE` | | ✔ | | |
| `FIND_MATCH`, `CANCEL_MATCH` | | ✔ | | |
| `MOVE`, `RESIGN`, `DRAW_OFFER`, `DRAW_ACCEPT`, `DRAW_DECLINE`, `SYNC` | | | | ✔ |
| `PAUSE_OFFER`, `PAUSE_ACCEPT`, `PAUSE_DECLINE`, `RESUME` | | | | ✔ |
| `REMATCH`, `REMATCH_DECLINE` | | ✔ (sau khi vừa kết thúc ván) | | |

Lệnh gửi sai trạng thái: server trả `ERR 400 invalid_state`.

**Quy tắc chuyển trạng thái quan trọng:**
1. **Hủy thách đấu khi rời IDLE:** Khi người chơi chuyển trạng thái rời khỏi `IDLE` (tham gia phòng `IN_ROOM`, vào ván `IN_GAME`, `LOGOUT`, hoặc ngắt kết nối `DISCONNECT`), mọi lời thách đấu đang chờ liên quan đến người chơi đó (cả gửi đi và nhận về) đều lập tức bị hủy. Server gửi sự kiện `CHALLENGE_EXPIRED` (kèm `user`) cho đối tác còn lại để họ không phải chờ hết 30 giây.
2. **Điều kiện REMATCH:** `REMATCH` chỉ được phép gọi khi vừa kết thúc ván đấu và cả hai người chơi vẫn đang ở `IDLE`. Server lưu cờ `rematch_available` và `last_opponent`. Nếu đối thủ đã rời đi hoặc người chơi chưa đấu ván nào, gửi `REMATCH` sẽ nhận `ERR 400 invalid_state` (hoặc `ERR 301 player_not_found`).
3. **Đăng nhập trên cùng kết nối:** Nếu kết nối đã đăng nhập thành công (`IDLE`, `IN_ROOM`, `IN_GAME`) mà gửi tiếp lệnh `LOGIN`, server trả `ERR 400 invalid_state` (chỉ cho phép `LOGIN` ở `CONNECTED`). Khác với `ERR 104 already_logged_in` là khi một socket khác cố đăng nhập cùng tài khoản.

---

## 5. Phản hồi và sự kiện

### 5.1 Phản hồi thành công

```json
{"type":"OK","id":<id>,"cmd":"<TÊN_LỆNH>", ...các trường dữ liệu...}
```

- `id` là `id` của lệnh tương ứng.
- `cmd` là tên lệnh đang được trả lời (giúp debug, client không bắt buộc dùng).
- Dữ liệu trả về (nếu có) nằm ngay trong object này, không có kiểu phản hồi riêng như `MOVE_OK`, `PLAYERS`.

### 5.2 Phản hồi lỗi

```json
{"type":"ERR","id":<id>,"code":<mã>,"reason":"<lý_do>"}
```

Nếu server không đọc được `id` (JSON hỏng, thiếu `id`), `id` trong lỗi là `null`.

### 5.3 Quy tắc ghép phản hồi và phân biệt sự kiện

- Server trả **đúng một** phản hồi (`OK` hoặc `ERR`) cho mỗi lệnh, mang đúng `id` của lệnh đó.
- Client phân biệt bằng trường `type`: `OK` và `ERR` là **phản hồi** (luôn có `id`). Mọi `type` khác là **sự kiện** (không có `id`).
- Sự kiện có thể đến **bất kỳ lúc nào**, kể cả giữa lúc lệnh đang chờ phản hồi. Client ghép phản hồi theo `id`, không dựa vào thứ tự đến.
- Thứ tự đảm bảo trên cùng một kết nối: phản hồi `OK` của lệnh gây ra sự kiện được gửi **trước** các sự kiện do lệnh đó sinh ra cho chính người gửi (ví dụ `OK` của `MOVE` đến trước `GAME_OVER`).

### 5.4 Bảng mã lỗi

| Mã | Lý do (`reason`) | Ý nghĩa |
|---|---|---|
| 100 | `unknown_command` | `type` không tồn tại |
| 101 | `bad_arguments` | Thiếu trường, sai kiểu dữ liệu (chuỗi thay vì số, âm, ngoài khoảng đối với `col`, `limit`, `turn_time`...) hoặc sai giá trị |
| 102 | `bad_credentials` | Sai username hoặc password |
| 103 | `username_taken` | Username đã tồn tại |
| 104 | `already_logged_in` | Tài khoản đang đăng nhập ở một kết nối/socket khác |
| 105 | `invalid_username_or_password_format` | Không đúng quy tắc định dạng |
| 106 | `wrong_old_password` | Mật khẩu cũ không đúng |
| 107 | `malformed_json` | Dòng không phải JSON hợp lệ, không phải object, thiếu `type` hoặc `id` |
| 108 | `line_too_long` | Dòng vượt quá 4096 byte |
| 201 | `not_your_turn` | Chưa đến lượt |
| 202 | `invalid_column` | Cột ngoài 0 đến 6 |
| 203 | `column_full` | Cột đã đầy |
| 301 | `player_not_found` | Không có người chơi đó |
| 302 | `player_busy` | Người chơi đang không rảnh (đang chơi hoặc đang trong phòng) |
| 303 | `no_such_challenge` | Không có lời thách đấu đó |
| 304 | `cannot_challenge_self` | Không thể thách đấu chính mình |
| 400 | `invalid_state` | Lệnh không hợp lệ ở trạng thái hiện tại (hoặc gửi lời mời trùng lặp) |
| 401 | `room_not_found` | Không có phòng đó |
| 402 | `room_full` | Phòng đã đủ 2 người |
| 403 | `no_pending_offer` | Không có lời đề nghị hòa, hoãn hoặc đấu lại nào đang chờ |
| 404 | `game_not_found` | Không có ván đó |
| 500 | `server_error` | Lỗi nội bộ server |

---

## 6. Lệnh của client và phản hồi của server

Trong các bảng dưới, cột "Lệnh" chỉ liệt kê các trường ngoài `type` và `id` (hai trường này luôn có). Cột "Dữ liệu trong `OK`" liệt kê các trường ngoài `type`, `id`, `cmd`.

### 6.1 Tài khoản

| Lệnh (trường) | Dữ liệu trong `OK` | Lỗi có thể gặp |
|---|---|---|
| `REGISTER` (`user`, `pass`) | không có | 103, 105 |
| `LOGIN` (`user`, `pass`) | `elo` | 102, 104, 105 |
| `LOGOUT` | không có (rồi server đóng kết nối) | 400 |
| `CHANGEPASS` (`old`, `new`) | không có | 105, 106 |
| `PROFILE` (`user` tuỳ chọn) | `user`, `elo`, `wins`, `losses`, `draws` | 301 |

Nếu bỏ `user` trong `PROFILE`, server trả hồ sơ của chính người gửi.

### 6.2 Người chơi online

`LIST_PLAYERS` (không có trường):

```json
{"type":"OK","id":5,"cmd":"LIST_PLAYERS","players":[{"user":"hieu","elo":1200,"status":"IDLE"},{"user":"hao","elo":1200,"status":"IN_GAME"}]}
```

Server trả tối đa **40** người chơi (ưu tiên người đang rảnh).

Sự kiện server tự gửi cho người đang ở sảnh khi trạng thái ai đó đổi (tuỳ chọn, giúp danh sách cập nhật thời gian thực):

```json
{"type":"STATUS_CHANGE","user":"hao","status":"IN_GAME"}
```

`status` là một trong `ONLINE`, `OFFLINE`, `IDLE`, `IN_ROOM`, `IN_GAME`.

### 6.3 Phòng

| Lệnh (trường) | Dữ liệu trong `OK` | Lỗi |
|---|---|---|
| `CREATE_ROOM` | `room_id` | 400 |
| `JOIN_ROOM` (`room_id`) | `room_id` | 401, 402 |
| `LEAVE_ROOM` | không có | 400 |
| `LIST_ROOMS` | `rooms`: mảng các `{"room_id","host","players","status"}` với `status` là `WAITING` hoặc `PLAYING` | |

`LIST_ROOMS` trả tối đa **40** phòng.

Sự kiện:

```json
{"type":"ROOM_JOINED","user":"hao"}
{"type":"ROOM_LEFT","user":"hao"}
```

- `ROOM_JOINED` gửi cho chủ phòng khi có người vào phòng.
- `ROOM_LEFT` gửi cho người còn lại khi có người rời.

**Lưu ý:** Phòng trong Connect Four có sức chứa 2 người. Khi khách (guest) vào phòng, server gửi `ROOM_JOINED` cho host để thông báo, rồi tự động bắt đầu ván đấu ngay bằng `GAME_START` gửi cho cả hai bên (chủ phòng cầm quân `RED`). Không thể `LEAVE_ROOM` khi đang trong ván; dùng `RESIGN`.

### 6.4 Thách đấu

| Lệnh (trường) | Dữ liệu trong `OK` | Lỗi |
|---|---|---|
| `CHALLENGE` (`user`, `turn_time` tuỳ chọn) | không có | 101, 301, 302, 304 |
| `ACCEPT` (`user`) | không có, rồi sự kiện `GAME_START` | 303, 400 |
| `DECLINE` (`user`) | không có | 303, 400 |

`turn_time`: thời gian tối đa cho mỗi nước đi (tính bằng giây, mặc định 30, đặt 0 nếu không giới hạn).

Sự kiện:

```json
{"type":"CHALLENGED","from":"hieu"}
{"type":"CHALLENGE_DECLINED","user":"hao"}
{"type":"CHALLENGE_EXPIRED","user":"hao"}
```

- `CHALLENGED`: gửi cho người bị thách đấu.
- `CHALLENGE_DECLINED`: gửi cho người thách đấu khi bị từ chối.
- `CHALLENGE_EXPIRED`: gửi cho người còn lại khi lời thách đấu hết hạn (sau 30 giây không phản hồi, hoặc đối tác vừa rời `IDLE` như vào phòng, vào ván, mất mạng).

Người bị thách đấu bận hoặc offline: người thách đấu nhận `ERR 302` hoặc `ERR 301` ngay.

### 6.5 Bắt đầu ván và chơi

Sự kiện bắt đầu, gửi cho **cả hai người**:

```json
{"type":"GAME_START","game_id":7,"opponent":"hao","color":"RED","first":true,"turn_time":30}
```

- `color` là `"RED"` hoặc `"YELLOW"`.
- `first` là `true` nếu người nhận đi trước, `false` nếu đi sau.
- `turn_time` là thời gian cho mỗi lượt đi (tính bằng giây, mặc định 30).

| Lệnh (trường) | Dữ liệu trong `OK` | Lỗi |
|---|---|---|
| `MOVE` (`col`: số nguyên) | `col`, `row` | 101, 201, 202, 203 |

Sau mỗi nước đi hợp lệ, server gửi cho **đối thủ**:

```json
{"type":"OPP_MOVE","col":3,"row":0}
```

Khi một nước đi kết thúc ván, server gửi `OK` (cho người đi) và `OPP_MOVE` (cho đối thủ) trước, rồi gửi `GAME_OVER` cho cả hai.

**Quy tắc tính giờ lượt (Turn Timer):**
- Đồng hồ được đặt lại thành `turn_time` giây sau mỗi nước đi hợp lệ (`MOVE` / `OPP_MOVE`).
- Server tính giờ bằng timestamp `turn_start_time` trong vòng lặp `poll()`.
- Nếu quá `turn_time` giây mà người đến lượt không gửi nước đi hợp lệ, server gửi `GAME_OVER` với `reason: "TIMEOUT"`, bên hết giờ bị xử thua (`LOSS`), đối thủ thắng (`WIN`).

**Đồng bộ lại bàn cờ** (dùng khi client cần vẽ lại, ví dụ sau lỗi hoặc reconnect):

| Lệnh | Dữ liệu trong `OK` |
|---|---|
| `SYNC` | `board` (mảng 6 chuỗi, xem mục 3), `turn` (`"RED"` hoặc `"YELLOW"`) |

### 6.6 Kết thúc ván

```json
{"type":"GAME_OVER","game_id":7,"result":"WIN","reason":"CONNECT4","elo_delta":16,"new_elo":1216}
```

`result` theo góc nhìn của người nhận. `elo_delta` là số nguyên có dấu (ví dụ `16` hoặc `-16`).

| `reason` | Khi nào |
|---|---|
| `CONNECT4` | Có 4 quân liên tiếp |
| `BOARD_FULL` | Bàn đầy, không ai thắng (hòa) |
| `RESIGN` | Một bên đầu hàng |
| `DRAW_AGREED` | Hai bên đồng ý hòa |
| `DISCONNECT` | Một bên mất kết nối (bên còn lại thắng) |
| `TIMEOUT` | Hết giờ lượt đi (`turn_time`) |

### 6.7 Đầu hàng, xin hòa và tạm dừng (Game Controls)

| Lệnh | Dữ liệu trong `OK` | Sự kiện gửi đi |
|---|---|---|
| `RESIGN` | không có | `GAME_OVER` (`RESIGN`) cho cả hai |
| `DRAW_OFFER` | không có | `{"type":"DRAW_OFFERED"}` gửi cho đối thủ |
| `DRAW_ACCEPT` | không có | `GAME_OVER` (`DRAW`, `DRAW_AGREED`) cho cả hai |
| `DRAW_DECLINE` | không có | `{"type":"DRAW_DECLINED"}` gửi cho người xin hòa |
| `PAUSE_OFFER` | không có | `{"type":"PAUSE_OFFERED"}` gửi cho đối thủ |
| `PAUSE_ACCEPT` | không có | `{"type":"PAUSED"}` gửi cho cả hai |
| `PAUSE_DECLINE` | không có | `{"type":"PAUSE_DECLINED"}` gửi cho người xin pause |
| `RESUME` | không có | `{"type":"RESUMED"}` gửi cho cả hai |

**Quy tắc xin hòa (Draw):**
- Lời xin hòa tự hủy khi bất kỳ bên nào đi một nước `MOVE` hợp lệ.
- Gửi `DRAW_OFFER` khi đang có một lời xin hòa chưa được phản hồi: server trả `ERR 400 invalid_state`.
- `DRAW_ACCEPT`/`DRAW_DECLINE` khi không có lời xin hòa đang chờ: `ERR 403 no_pending_offer`.

**Quy tắc tạm dừng (Pause):**
- Bất kỳ bên nào cũng có thể gửi `PAUSE_OFFER`. Đối thủ nhận sự kiện `PAUSE_OFFERED`.
- Khi đối thủ gửi `PAUSE_ACCEPT`, ván đấu chuyển sang trạng thái tạm dừng, server gửi `PAUSED` cho cả hai và đồng hồ `turn_time` bị đóng băng.
- Thời gian tạm dừng tối đa là **60 giây**. Nếu hết 60 giây không ai gửi `RESUME`, server tự động gửi `RESUMED` và kích hoạt lại đồng hồ.
- Bất kỳ bên nào cũng có thể gửi `RESUME` để tiếp tục ván đấu ngay. Server gửi `RESUMED` cho cả hai bên.
- `PAUSE_ACCEPT`/`PAUSE_DECLINE` khi không có lời xin pause: `ERR 403 no_pending_offer`.

### 6.8 Đấu lại cùng đối thủ

| Lệnh | Dữ liệu trong `OK` | Sự kiện cho đối thủ |
|---|---|---|
| `REMATCH` | không có | `{"type":"REMATCH_OFFERED"}` |
| `REMATCH_DECLINE` | không có | `{"type":"REMATCH_DECLINED"}` |

- Khi **cả hai** cùng gửi `REMATCH`, server bắt đầu ván mới bằng `GAME_START` cho cả hai, **đổi màu quân** so với ván trước.
- Nếu đối thủ đã rời hoặc thoát ứng dụng: `REMATCH` trả `ERR 301 player_not_found`.
- **Nếu đang chờ rematch mà đối thủ ngắt kết nối, đăng xuất hoặc rời sảnh:** Server lập tức gửi sự kiện `{"type":"REMATCH_DECLINED","user":"<tên_đối_thủ>"}` cho bên đang chờ để client không bị treo màn hình chờ.
- Không có lời đề nghị đang chờ khi gửi `REMATCH_DECLINE`: `ERR 403 no_pending_offer`.

### 6.9 Lịch sử và replay

`HISTORY` (`limit` tuỳ chọn, mặc định 20, tối đa 30): trả các ván gần nhất trước.

```json
{"type":"OK","id":9,"cmd":"HISTORY","games":[{"game_id":7,"opponent":"hao","result":"WIN","elo_delta":16,"time":1760000000}]}
```

`GET_GAME` (`game_id`): lỗi 404 nếu không có ván.

```json
{"type":"OK","id":10,"cmd":"GET_GAME","game_id":7,"red":"hieu","yellow":"hao","winner":"RED","moves":[3,2,3,2,3,2,3,6,3]}
```

- `winner` là `"RED"`, `"YELLOW"` hoặc `"DRAW"`.
- `moves` là mảng số cột theo thứ tự đã đi (nước đầu của `RED`, xen kẽ). Client replay bằng cách thả quân lần lượt theo mảng.

### 6.10 Ghép trận tự động (tuỳ chọn, giai đoạn mở rộng)

| Lệnh | Dữ liệu trong `OK` |
|---|---|
| `FIND_MATCH` | không có (server bắt đầu tìm) |
| `CANCEL_MATCH` | không có |

Khi tìm được đối thủ có ELO gần nhau, server gửi `GAME_START` cho cả hai. Khoảng chênh ELO cho phép nới rộng dần theo thời gian chờ.

### 6.11 Giữ kết nối

| Lệnh | Dữ liệu trong `OK` |
|---|---|
| `PING` | không có |

- Client nên gửi `PING` mỗi **15 giây** khi đang đăng nhập.
- Nếu server không nhận được **bất kỳ dữ liệu nào** từ một kết nối trong **45 giây**, server coi kết nối đã chết và xử lý như mất kết nối (xem mục 7, quy tắc 3).

---

## 7. Quy tắc xử lý của server

1. **Kiểm tra mọi nước đi:** đúng lượt, `col` là số nguyên trong 0 đến 6, cột chưa đầy. Sai thì trả `ERR` và **không đổi trạng thái ván**.
2. **Sau mỗi nước đi hợp lệ:** kiểm tra 4 hướng (ngang, dọc, hai đường chéo) từ quân vừa thả; nếu không thắng mà bàn đầy 42 quân thì hòa.
3. **Mất kết nối giữa ván** (đóng socket hoặc quá 45 giây không có dữ liệu): coi như người đó đầu hàng (`reason = DISCONNECT`), đối thủ nhận `GAME_OVER ... WIN`.
4. **Đăng nhập trùng:** nếu tài khoản đang đăng nhập, `LOGIN` mới bị từ chối bằng `ERR 104`.
5. **Cập nhật ELO** cho cả hai khi ván kết thúc (kể cả đầu hàng, mất kết nối), rồi mới gửi `GAME_OVER`:
   `E = 1 / (1 + 10^((R_đối_thủ - R_mình)/400))`, `R' = R + K·(S - E)` với `S` = 1 (thắng), 0.5 (hòa), 0 (thua). Làm tròn `elo_delta` về số nguyên.
6. **Mật khẩu:** server lưu dạng băm (có salt), không lưu bản rõ.
7. **Ghi log ván:** khi ván kết thúc, ghi người chơi, thời gian, danh sách nước đi và kết quả để phục vụ `HISTORY` và `GET_GAME`.
8. **Dòng hỏng:** JSON sai cú pháp hoặc thiếu `type`/`id` thì trả `ERR 107` (với `id` là `null` nếu không đọc được) và **giữ kết nối**, chỉ bỏ dòng đó.
9. **Lưu ý cài đặt trong C:** dùng thư viện JSON nhẹ (ví dụ cJSON hoặc jsmn). Luôn kiểm tra kiểu từng trường trước khi lấy giá trị, và giải phóng đối tượng JSON sau mỗi message để tránh rò rỉ bộ nhớ.

---

## 8. Phiên mẫu

Ký hiệu `C1 -> S` là client gửi, `S -> C1` là server gửi.

### 8.1 Đăng ký, đăng nhập, thách đấu, chơi đến khi thắng

```
C1 -> S : {"type":"REGISTER","id":1,"user":"hieu","pass":"123456"}
S  -> C1: {"type":"OK","id":1,"cmd":"REGISTER"}
C1 -> S : {"type":"LOGIN","id":2,"user":"hieu","pass":"123456"}
S  -> C1: {"type":"OK","id":2,"cmd":"LOGIN","elo":1200}
C2 -> S : {"type":"LOGIN","id":1,"user":"hao","pass":"abc123"}
S  -> C2: {"type":"OK","id":1,"cmd":"LOGIN","elo":1200}

C1 -> S : {"type":"LIST_PLAYERS","id":3}
S  -> C1: {"type":"OK","id":3,"cmd":"LIST_PLAYERS","players":[{"user":"hieu","elo":1200,"status":"IDLE"},{"user":"hao","elo":1200,"status":"IDLE"}]}

C1 -> S : {"type":"CHALLENGE","id":4,"user":"hao"}
S  -> C1: {"type":"OK","id":4,"cmd":"CHALLENGE"}
S  -> C2: {"type":"CHALLENGED","from":"hieu"}
C2 -> S : {"type":"ACCEPT","id":2,"user":"hieu"}
S  -> C2: {"type":"OK","id":2,"cmd":"ACCEPT"}
S  -> C1: {"type":"GAME_START","game_id":7,"opponent":"hao","color":"RED","first":true}
S  -> C2: {"type":"GAME_START","game_id":7,"opponent":"hieu","color":"YELLOW","first":false}

C1 -> S : {"type":"MOVE","id":5,"col":3}
S  -> C1: {"type":"OK","id":5,"cmd":"MOVE","col":3,"row":0}
S  -> C2: {"type":"OPP_MOVE","col":3,"row":0}
C2 -> S : {"type":"MOVE","id":3,"col":2}
S  -> C2: {"type":"OK","id":3,"cmd":"MOVE","col":2,"row":0}
S  -> C1: {"type":"OPP_MOVE","col":2,"row":0}
... (các nước tiếp theo) ...
C1 -> S : {"type":"MOVE","id":9,"col":6}
S  -> C1: {"type":"OK","id":9,"cmd":"MOVE","col":6,"row":0}
S  -> C2: {"type":"OPP_MOVE","col":6,"row":0}
S  -> C1: {"type":"GAME_OVER","game_id":7,"result":"WIN","reason":"CONNECT4","elo_delta":16,"new_elo":1216}
S  -> C2: {"type":"GAME_OVER","game_id":7,"result":"LOSS","reason":"CONNECT4","elo_delta":-16,"new_elo":1184}
```

### 8.2 Lỗi nước đi

```
C2 -> S : {"type":"MOVE","id":11,"col":4}            (khi chưa đến lượt C2)
S  -> C2: {"type":"ERR","id":11,"code":201,"reason":"not_your_turn"}

C1 -> S : {"type":"MOVE","id":12,"col":9}
S  -> C1: {"type":"ERR","id":12,"code":202,"reason":"invalid_column"}

C1 -> S : {"type":"MOVE","id":13,"col":3}            (cột 3 đã đầy)
S  -> C1: {"type":"ERR","id":13,"code":203,"reason":"column_full"}

C1 -> S : {"type":"MOVE","id":14,"col":"3"}          (sai kiểu: chuỗi thay vì số)
S  -> C1: {"type":"ERR","id":14,"code":101,"reason":"bad_arguments"}

C1 -> S : {"type":"MOVE","id":15,"col":3             (JSON hỏng)
S  -> C1: {"type":"ERR","id":null,"code":107,"reason":"malformed_json"}
```

### 8.3 Xin hòa

```
C1 -> S : {"type":"DRAW_OFFER","id":20}
S  -> C1: {"type":"OK","id":20,"cmd":"DRAW_OFFER"}
S  -> C2: {"type":"DRAW_OFFERED"}
C2 -> S : {"type":"DRAW_ACCEPT","id":8}
S  -> C2: {"type":"OK","id":8,"cmd":"DRAW_ACCEPT"}
S  -> C1: {"type":"GAME_OVER","game_id":7,"result":"DRAW","reason":"DRAW_AGREED","elo_delta":0,"new_elo":1200}
S  -> C2: {"type":"GAME_OVER","game_id":7,"result":"DRAW","reason":"DRAW_AGREED","elo_delta":0,"new_elo":1200}
```

### 8.4 Đầu hàng rồi đấu lại

```
C2 -> S : {"type":"RESIGN","id":9}
S  -> C2: {"type":"OK","id":9,"cmd":"RESIGN"}
S  -> C1: {"type":"GAME_OVER","game_id":7,"result":"WIN","reason":"RESIGN","elo_delta":16,"new_elo":1216}
S  -> C2: {"type":"GAME_OVER","game_id":7,"result":"LOSS","reason":"RESIGN","elo_delta":-16,"new_elo":1184}

C1 -> S : {"type":"REMATCH","id":21}
S  -> C1: {"type":"OK","id":21,"cmd":"REMATCH"}
S  -> C2: {"type":"REMATCH_OFFERED"}
C2 -> S : {"type":"REMATCH","id":10}
S  -> C2: {"type":"OK","id":10,"cmd":"REMATCH"}
S  -> C1: {"type":"GAME_START","game_id":8,"opponent":"hao","color":"YELLOW","first":false}     (đổi màu)
S  -> C2: {"type":"GAME_START","game_id":8,"opponent":"hieu","color":"RED","first":true}
```

### 8.5 Phòng

```
C1 -> S : {"type":"CREATE_ROOM","id":30}
S  -> C1: {"type":"OK","id":30,"cmd":"CREATE_ROOM","room_id":3}
C2 -> S : {"type":"LIST_ROOMS","id":11}
S  -> C2: {"type":"OK","id":11,"cmd":"LIST_ROOMS","rooms":[{"room_id":3,"host":"hieu","players":1,"status":"WAITING"}]}
C2 -> S : {"type":"JOIN_ROOM","id":12,"room_id":3}
S  -> C2: {"type":"OK","id":12,"cmd":"JOIN_ROOM","room_id":3}
S  -> C1: {"type":"ROOM_JOINED","user":"hao"}
S  -> C1: {"type":"GAME_START","game_id":9,"opponent":"hao","color":"RED","first":true}
S  -> C2: {"type":"GAME_START","game_id":9,"opponent":"hieu","color":"YELLOW","first":false}
```

### 8.6 Xem lại ván

```
C1 -> S : {"type":"HISTORY","id":40}
S  -> C1: {"type":"OK","id":40,"cmd":"HISTORY","games":[{"game_id":7,"opponent":"hao","result":"WIN","elo_delta":16,"time":1760000000}]}
C1 -> S : {"type":"GET_GAME","id":41,"game_id":7}
S  -> C1: {"type":"OK","id":41,"cmd":"GET_GAME","game_id":7,"red":"hieu","yellow":"hao","winner":"RED","moves":[3,2,3,2,3,2,3,6,3]}
```

### 8.7 Sự kiện chen giữa lúc chờ phản hồi

Vì phản hồi ghép theo `id`, client không bị nhầm khi sự kiện đến xen vào:

```
C1 -> S : {"type":"LIST_PLAYERS","id":50}
S  -> C1: {"type":"CHALLENGED","from":"hao"}                      (sự kiện đến trước phản hồi)
S  -> C1: {"type":"OK","id":50,"cmd":"LIST_PLAYERS","players":[...]}
```

### 8.8 Tạm dừng và tiếp tục ván đấu (Pause / Resume)

```
C1 -> S : {"type":"PAUSE_OFFER","id":60}
S  -> C1: {"type":"OK","id":60,"cmd":"PAUSE_OFFER"}
S  -> C2: {"type":"PAUSE_OFFERED"}
C2 -> S : {"type":"PAUSE_ACCEPT","id":25}
S  -> C2: {"type":"OK","id":25,"cmd":"PAUSE_ACCEPT"}
S  -> C1: {"type":"PAUSED"}                                      (đồng hồ turn_time tạm dừng)
S  -> C2: {"type":"PAUSED"}

C1 -> S : {"type":"RESUME","id":61}
S  -> C1: {"type":"OK","id":61,"cmd":"RESUME"}
S  -> C1: {"type":"RESUMED"}                                     (ván tiếp tục chạy)
S  -> C2: {"type":"RESUMED"}
```

### 8.9 Đăng nhập lại trên cùng kết nối (ERR 400) vs Socket khác (ERR 104)

```
(Kết nối C1 đã LOGIN thành công, đang ở trạng thái IDLE)
C1 -> S : {"type":"LOGIN","id":70,"user":"hieu","pass":"123456"}
S  -> C1: {"type":"ERR","id":70,"code":400,"reason":"invalid_state"}

(Một kết nối socket khác C_other cố tình đăng nhập tài khoản hieu đang online)
C_other -> S : {"type":"LOGIN","id":1,"user":"hieu","pass":"123456"}
S       -> C_other: {"type":"ERR","id":1,"code":104,"reason":"already_logged_in"}
```

### 8.10 Đối thủ thoát khi đang chờ REMATCH

```
(Sau khi GAME_OVER, cả hai trở về IDLE)
C1 -> S : {"type":"REMATCH","id":80}
S  -> C1: {"type":"OK","id":80,"cmd":"REMATCH"}
S  -> C2: {"type":"REMATCH_OFFERED"}
(C2 đột ngột ngắt kết nối hoặc bấm LOGOUT)
S  -> C1: {"type":"REMATCH_DECLINED","user":"hao"}                (C1 không bị treo chờ)
```

---

## 9. Hướng dẫn test nhanh bằng `nc`

Khi server đã chạy, mở hai terminal:

```
nc localhost 5000
{"type":"REGISTER","id":1,"user":"hieu","pass":"123456"}
{"type":"LOGIN","id":2,"user":"hieu","pass":"123456"}
{"type":"LIST_PLAYERS","id":3}
```

Gõ hoặc dán từng dòng JSON rồi Enter. Cách này giúp test server trước khi GUI sẵn sàng. Để đọc phản hồi dễ hơn, có thể chuyển qua `jq`:

```
nc localhost 5000 | jq -c .
```

Client Python tối thiểu (đọc từng dòng, gửi từng dòng):

```python
import socket, json

s = socket.create_connection(("localhost", 5000))
f = s.makefile("rwb")          # makefile xu ly buffer va tach dong

def send(msg):
    f.write((json.dumps(msg, separators=(",", ":")) + "\n").encode())
    f.flush()

def recv():
    line = f.readline()
    return json.loads(line) if line else None

send({"type": "LOGIN", "id": 1, "user": "hieu", "pass": "123456"})
print(recv())
```

