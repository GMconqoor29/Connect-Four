# Application-Layer Protocol Specification: Connect Four Online (NDJSON)

Group 11. Protocol version: **2.0** (migrated from text format to NDJSON)

This document is the shared standard between the **server (C)** and the **client (Python)**. Any change must be agreed on by the whole group and updated here before coding.

---

## 1. Overview

- **Transport layer:** TCP, default port **5000**.
- **Format:** NDJSON (Newline Delimited JSON). Each message is **one JSON object on exactly one line**, terminated by `\n`.
- **Model:** the client sends **commands** (with an `id`), and the server returns **responses** (`OK` or `ERR`, echoing the `id`). The server may also send **events** (no `id`) at any time, for example when someone challenges you or the opponent has just moved.
- **The server is the authority:** the server holds the board, validates moves and determines the result. The client only sends intent and displays state.

---

## 2. Format Rules (framing and syntax)

| Item | Rule |
|---|---|
| Encoding | UTF-8. Protocol values (username, command names, status codes) use ASCII only |
| Message boundary | Each message is **one line** terminated by `\n` (`\r\n` is accepted; `\r` is stripped) |
| Line content | Exactly one **JSON object** (not an array, string or standalone number) |
| Newlines | JSON must be **compact**, with no literal newline characters inside. Newlines inside strings must be escaped as `\n` (the JSON library does this automatically) |
| Maximum length | **4096 bytes** per line, including the `\n`. Longer line: the server returns `ERR 108` and discards the entire line (reads and drops everything up to the next `\n`) |
| Required fields | Every message has a `"type"` field (string). Every client command additionally has an `"id"` (positive integer) |
| Command / event names | UPPERCASE, e.g. `"LOGIN"` |
| Unknown fields | The receiver **ignores** fields it does not recognize (so new fields can be added without breaking older versions) |
| Username | 3 to 16 characters, only `A-Z a-z 0-9 _` |
| Password | 6 to 32 characters, no spaces |

**Stream handling (mandatory for both sides):** TCP is a byte stream, so one `recv` may contain half a message, one message, or several messages stuck together. Each side must keep a buffer, append new data to it, and only process once a full line ending in `\n` is available. The remainder stays in the buffer. Only after a complete line has been extracted is it passed to the JSON parser.

**Lists:** every list (players, rooms, history) is contained in **a single JSON array on one line**, so a server event cannot be interleaved in the middle of a list.

**Input validation (server):** every field is untrusted data. The server must check that the field exists, has the correct type (e.g. `col` must be an integer, not a string) and is within the valid range before using it. Otherwise, it returns `ERR 101`.

---

## 3. Shared Values

| Concept | Value |
|---|---|
| Board | **Fixed** 7 columns (numbered **0 to 6**), 6 rows (numbered **0 to 5**, row 0 is the bottom) |
| Pieces | `RED` moves first, `YELLOW` moves second. Board symbols: `R`, `Y`, `.` (empty cell) |
| Player status | `IDLE` (in the lobby), `IN_ROOM` (in a waiting room), `IN_GAME` (playing), `OFFLINE` (disconnected/logged out - used in `STATUS_CHANGE` events) |
| Result | `WIN`, `LOSS`, `DRAW` (from the receiver's point of view) |
| Starting ELO | 1200, K factor = 32 |
| AI Bot levels | `EASY` (`BOT_EASY`, ELO 800), `MEDIUM` (`BOT_MEDIUM`, ELO 1200), `HARD` (`BOT_HARD`, ELO 1600) |
| Game ID (`game_id`) | positive integer obtained from the `matches` primary key in PostgreSQL (`INSERT ... RETURNING id` at `GAME_START`) |

**Board representation** (used in `SYNC`): an array of **6 strings**, each 7 characters of `R`, `Y`, `.`. The first element is the **top row (row 5)**, the last element is the bottom row (row 0), and each string runs from column 0 to 6.

Example of a board with a yellow piece in column 2 and a red piece in column 3 (both in row 0, the bottom row):

```
[".......",".......",".......",".......",".......","..YR..."]
```

---

## 4. Connection State Machine

```
CONNECTED --LOGIN--> IDLE --CREATE_ROOM/JOIN_ROOM--> IN_ROOM --2 players present--> IN_GAME
                       ^  --CHALLENGE+ACCEPT / FIND_MATCH / PLAY_BOT-------------^   |
                       |                                                             |
                       +--------------------- GAME_OVER -----------------------------+
```

**Valid commands per state:**

| Command | CONNECTED | IDLE | IN_ROOM | IN_GAME |
|---|:-:|:-:|:-:|:-:|
| `PING` | ✔ | ✔ | ✔ | ✔ |
| `REGISTER`, `LOGIN` | ✔ | | | |
| `LOGOUT`, `CHANGEPASS`, `PROFILE`, `HISTORY`, `GET_GAME` | | ✔ | ✔ | ✔ |
| `LIST_PLAYERS`, `LIST_ROOMS` | | ✔ | ✔ | |
| `CREATE_ROOM`, `JOIN_ROOM` | | ✔ | | |
| `LEAVE_ROOM` | | | ✔ | |
| `CHALLENGE`, `ACCEPT`, `DECLINE` | | ✔ | | |
| `PLAY_BOT` | | ✔ | | |
| `FIND_MATCH`, `CANCEL_MATCH` | | ✔ | | |
| `MOVE`, `RESIGN`, `DRAW_OFFER`, `DRAW_ACCEPT`, `DRAW_DECLINE`, `SYNC` | | | | ✔ |
| `PAUSE_OFFER`, `PAUSE_ACCEPT`, `PAUSE_DECLINE`, `RESUME` | | | | ✔ |
| `REMATCH`, `REMATCH_DECLINE` | | ✔ (right after a game ends) | | |

A command sent in the wrong state: the server returns `ERR 400 invalid_state`.

**Important state transition rules:**
1. **Challenges are cancelled when leaving IDLE:** When a player leaves `IDLE` (joins a room → `IN_ROOM`, enters a game → `IN_GAME`, starts `FIND_MATCH`, `LOGOUT`, or connection loss `DISCONNECT`), every pending challenge involving that player (both sent and received) is cancelled immediately. The server sends a `CHALLENGE_EXPIRED` event (with `user`) to the other party so they do not have to wait out the 30 seconds.
2. **Duplicate or cross challenges (A challenges B while B has already challenged A):**
   - Each player may have at most **1 outgoing challenge** pending at a time. Sending another `CHALLENGE` while waiting for a response returns `ERR 400 invalid_state`.
   - If A has already sent `CHALLENGE` to B (pending), and B sends `CHALLENGE` back to A (instead of `ACCEPT`), the server cancels the pending challenge between the two players (sending `CHALLENGE_EXPIRED` to A) and returns `ERR 400 invalid_state` to B, requiring one of the two players to send a fresh `CHALLENGE`.
3. **While searching for a match (`FIND_MATCH`):** The player remains in `IDLE` with the internal flag `is_searching_match = true`. While searching, the player **must send `CANCEL_MATCH` first** before invoking other lobby actions (`CREATE_ROOM`, `JOIN_ROOM`, `CHALLENGE`, `ACCEPT`, `PLAY_BOT`, or `FIND_MATCH` again); attempting those commands without cancelling first returns `ERR 400 invalid_state`.
4. **REMATCH conditions:** `REMATCH` is only allowed right after a game ends and while both players are still `IDLE`. The server stores the flags `rematch_available` and `last_opponent`. As soon as either player leaves `IDLE` (or starts `FIND_MATCH`), `rematch_available` is cleared for both. If the opponent has left, or the player has not played any game yet, sending `REMATCH` returns `ERR 400 invalid_state` (or `ERR 301 player_not_found`).
5. **Logging in on the same connection:** If a connection has already logged in successfully (`IDLE`, `IN_ROOM`, `IN_GAME`) and sends `LOGIN` again, the server returns `ERR 400 invalid_state` (`LOGIN` is only allowed in `CONNECTED`). This differs from `ERR 104 already_logged_in`, which is when a different socket tries to log in to the same account.

---

## 5. Responses and Events

### 5.1 Success response

```json
{"type":"OK","id":<id>,"cmd":"<COMMAND_NAME>", ...data fields...}
```

- `id` is the `id` of the corresponding command.
- `cmd` is the name of the command being answered (useful for debugging; the client is not required to use it).
- Returned data (if any) is placed directly inside this object; there are no separate response types such as `MOVE_OK` or `PLAYERS`.

### 5.2 Error response

```json
{"type":"ERR","id":<id>,"code":<code>,"reason":"<reason>"}
```

If the server cannot read the `id` (broken JSON, missing `id`), the `id` in the error is `null`.

### 5.3 Matching responses and distinguishing events

- The server returns **exactly one** response (`OK` or `ERR`) per command, carrying that command's `id`.
- The client distinguishes by the `type` field: `OK` and `ERR` are **responses** (always have an `id`). Any other `type` is an **event** (no `id`).
- Events may arrive **at any time**, including while a command is awaiting its response. The client matches responses by `id`, not by arrival order.
- Ordering guarantee on a single connection: the `OK` response to a command that triggers events is sent **before** the events that command generates for the sender itself (e.g. the `OK` for `MOVE` arrives before `GAME_OVER`).

### 5.4 Error code table

| Code | Reason (`reason`) | Meaning |
|---|---|---|
| 100 | `unknown_command` | `type` does not exist |
| 101 | `bad_arguments` | Missing field, wrong data type (string instead of number, negative, out of range for `col`, `limit`, `turn_time`, ...) or wrong value |
| 102 | `bad_credentials` | Wrong username or password |
| 103 | `username_taken` | Username already exists |
| 104 | `already_logged_in` | The account is already logged in on another connection/socket |
| 105 | `invalid_username_or_password_format` | Does not follow the format rules |
| 106 | `wrong_old_password` | Old password is incorrect |
| 107 | `malformed_json` | Line is not valid JSON, is not an object, or is missing `type` or `id` |
| 108 | `line_too_long` | Line exceeds 4096 bytes |
| 201 | `not_your_turn` | Not your turn |
| 202 | `invalid_column` | Column outside 0 to 6 |
| 203 | `column_full` | Column is full |
| 301 | `player_not_found` | No such player |
| 302 | `player_busy` | Player is not available (playing or in a room) |
| 303 | `no_such_challenge` | No such challenge |
| 304 | `cannot_challenge_self` | Cannot challenge yourself |
| 400 | `invalid_state` | Command not valid in the current state (or a duplicate request was sent) |
| 401 | `room_not_found` | No such room |
| 402 | `room_full` | Room already has 2 players |
| 403 | `no_pending_offer` | No pending draw, pause or rematch offer |
| 404 | `game_not_found` | No such game |
| 500 | `server_error` | Internal server error or PostgreSQL database query/connection failure |

---

## 6. Client Commands and Server Responses

In the tables below, the "Command" column lists only the fields other than `type` and `id` (these two are always present). The "Data in `OK`" column lists the fields other than `type`, `id`, `cmd`.

### 6.1 Accounts

| Command (fields) | Data in `OK` | Possible errors |
|---|---|---|
| `REGISTER` (`user`, `pass`) | none | 103, 105, 500 |
| `LOGIN` (`user`, `pass`) | `elo` | 102, 104, 105, 500 |
| `LOGOUT` | none (then the server closes the connection) | 400 |
| `CHANGEPASS` (`old`, `new`) | none | 105, 106, 500 |
| `PROFILE` (`user` optional) | `user`, `elo`, `wins`, `losses`, `draws`, `bot_wins`, `bot_losses`, `bot_draws` | 301, 500 |

- If `user` is omitted in `PROFILE`, the server returns the sender's own profile (`wins`, `losses`, `draws` track PvP matches; `bot_wins`, `bot_losses`, `bot_draws` track Bot matches for the **vs Bot** tab).
- **Blocking registration and login for Bot accounts:**
  - `REGISTER` with a username starting with the reserved system prefix `BOT_` (case-insensitive) is rejected with `ERR 103 username_taken`.
  - System Bot accounts (`BOT_EASY`, `BOT_MEDIUM`, `BOT_HARD`) have `password_hash = NULL` in the DB; any attempt to `LOGIN` to a Bot account is rejected with `ERR 102 bad_credentials`.

### 6.2 Online players

`LIST_PLAYERS` (no fields):

```json
{"type":"OK","id":5,"cmd":"LIST_PLAYERS","players":[{"user":"hieu","elo":1200,"status":"IDLE"},{"user":"hao","elo":1200,"status":"IN_GAME"}]}
```

The server returns at most **40** players (available players are prioritized).

Event the server sends on its own to players in the lobby when someone's status changes (optional, makes the list update in real time):

```json
{"type":"STATUS_CHANGE","user":"hao","status":"IN_GAME"}
```

`status` is one of `IDLE`, `IN_ROOM`, `IN_GAME`, or `OFFLINE` (when the player logs out or disconnects).

### 6.3 Rooms

| Command (fields) | Data in `OK` | Errors |
|---|---|---|
| `CREATE_ROOM` (`turn_time` optional) | `room_id`, `turn_time` | 101, 400 |
| `JOIN_ROOM` (`room_id`) | `room_id` | 401, 402 |
| `LEAVE_ROOM` | none | 400 |
| `LIST_ROOMS` | `rooms`: array of `{"room_id","host","players","status","turn_time"}` where `status` is `WAITING` or `PLAYING` | |

- `turn_time` (in `CREATE_ROOM`): maximum time per move (in seconds, default **30**, set to **0** for no limit, or between **5 and 120** seconds; values outside this range return `ERR 101`).
- `LIST_ROOMS` returns at most **40** rooms.

Events:

```json
{"type":"ROOM_JOINED","user":"hao"}
{"type":"ROOM_LEFT","user":"hao"}
```

- `ROOM_JOINED` is sent to the room host when someone joins the room.
- `ROOM_LEFT` is sent to the remaining player when someone leaves.

**Note:** A Connect Four room holds 2 players. When the guest joins, the server sends `ROOM_JOINED` to the host as a notification, then immediately starts the game automatically with `GAME_START` sent to both sides (the room host plays `RED`, using the room's `turn_time`). `LEAVE_ROOM` is not possible during a game; use `RESIGN`.

### 6.4 Challenges and Playing against AI Bot

| Command (fields) | Data in `OK` | Errors |
|---|---|---|
| `CHALLENGE` (`user`, `turn_time` optional) | none | 101, 301, 302, 304, 400 |
| `ACCEPT` (`user`) | none, followed by a `GAME_START` event | 303, 400 |
| `DECLINE` (`user`) | none | 303, 400 |
| `PLAY_BOT` (`difficulty`, `turn_time` optional) | none, followed by a `GAME_START` event | 101, 400 |

- `turn_time`: maximum time per move (in seconds, default **30**, set to **0** for no limit, or between **5 and 120** seconds; values outside this range return `ERR 101`).
- `difficulty` (in `PLAY_BOT`): required string, must be one of:
  - `"EASY"`: opponent is `"BOT_EASY"` (representative ELO: **800**).
  - `"MEDIUM"`: opponent is `"BOT_MEDIUM"` (representative ELO: **1200**).
  - `"HARD"`: opponent is `"BOT_HARD"` (representative ELO: **1600**).
  - When a valid `PLAY_BOT` command is received, the server replies with `OK` and immediately sends `GAME_START` (the human player plays `RED` and moves first `first: true`).

Events (for human challenges):

```json
{"type":"CHALLENGED","from":"hieu"}
{"type":"CHALLENGE_DECLINED","user":"hao"}
{"type":"CHALLENGE_EXPIRED","user":"hao"}
```

- `CHALLENGED`: sent to the player being challenged.
- `CHALLENGE_DECLINED`: sent to the challenger when the challenge is declined.
- `CHALLENGE_EXPIRED`: sent to the other party when a challenge expires (30 seconds without a response, or the counterpart has just left `IDLE`, e.g. entering a room, entering a game, starting matchmaking, or losing connection).

**Challenge edge cases:**
- If the challenged player is busy (`IN_ROOM`, `IN_GAME`, or searching via `FIND_MATCH`) or offline: the challenger immediately receives `ERR 302 player_busy` or `ERR 301 player_not_found`.
- Each player may hold **at most 1 outgoing challenge** at a time; sending another `CHALLENGE` while one is still pending returns `ERR 400 invalid_state`.
- **Cross-challenge (A sends `CHALLENGE B`, then B also sends `CHALLENGE A`):** The server cancels the pending challenge between the two players (sending `CHALLENGE_EXPIRED` to A) and returns `ERR 400 invalid_state` to B, requiring one of the two players to send a new challenge.

### 6.5 Starting and playing a game

When starting a new match, the server inserts a row into the `matches` table (`INSERT INTO matches ... RETURNING id`) to obtain the authoritative `game_id` from PostgreSQL, then sends the `GAME_START` event to **both players** (or to the human player when playing against a Bot):

```json
{"type":"GAME_START","game_id":7,"opponent":"hao","color":"RED","first":true,"turn_time":30}
```

- `game_id`: match ID obtained from PostgreSQL (used consistently across `GAME_START`, `GAME_OVER`, `HISTORY`, and `GET_GAME`).
- `color` is `"RED"` or `"YELLOW"`.
- `first` is `true` if the receiver moves first, `false` if they move second.
- `turn_time` is the time allowed for each turn (in seconds, default 30).
- **When the Bot moves first (upon a color-swapped `REMATCH` with a Bot):** If the Bot is assigned `"RED"` (`first: false` for the human player), right after sending `GAME_START`, the server computes the Bot's opening move and immediately sends `OPP_MOVE` to the player.

| Command (fields) | Data in `OK` | Errors |
|---|---|---|
| `MOVE` (`col`: integer) | `col`, `row` | 101, 201, 202, 203, 400 |

After each valid move, the server sends to the **opponent**:

```json
{"type":"OPP_MOVE","col":3,"row":0}
```

(When playing against a Bot, after the player makes a valid move that does not end the game, the Bot computes its move and the server sends `OPP_MOVE` to the player).

When a move ends the game, the server first sends `OK` (to the mover) and `OPP_MOVE` (to the opponent), then sends `GAME_OVER` to both.

**Turn timer rules:**
- The clock is reset to `turn_time` seconds after every valid move (`MOVE` / `OPP_MOVE`).
- The server tracks time using the `turn_start_time` timestamp inside the `poll()` loop.
- If more than `turn_time` seconds pass without the player whose turn it is sending a valid move, the server sends `GAME_OVER` with `reason: "TIMEOUT"`; the side that ran out of time loses (`LOSS`) and the opponent wins (`WIN`).

**Board resync** (used when the client needs to redraw):

| Command | Data in `OK` |
|---|---|
| `SYNC` | `board` (array of 6 strings, see section 3), `turn` (`"RED"` or `"YELLOW"`) |

### 6.6 Game end

```json
{"type":"GAME_OVER","game_id":7,"result":"WIN","reason":"CONNECT4","elo_delta":16,"new_elo":1216}
```

`result` is from the receiver's point of view. `elo_delta` is a signed integer (e.g. `16`, `-16`, or `0`).
- **ELO rule when playing against a Bot (Option A):** When playing against an AI Bot (`BOT_EASY` ELO 800, `BOT_MEDIUM` ELO 1200, `BOT_HARD` ELO 1600), ELO is calculated using the Bot's ELO as $R_{\text{opponent}}$, but **the player's ELO is never reduced**: $\text{elo\_delta} = \max\big(0, \text{round}(32 \times (S - E))\big)$. Winning against a Bot grants ELO points based on the Bot's rating, while losing or drawing results in `elo_delta = 0`, keeping `new_elo` unchanged. The match is saved in PostgreSQL and displayed under the **vs Bot** tab in `HISTORY` and `GET_GAME`.

| `reason` | When |
|---|---|
| `CONNECT4` | 4 pieces in a row |
| `BOARD_FULL` | Board is full, no winner (draw) |
| `RESIGN` | One side resigns |
| `DRAW_AGREED` | Both sides agree to a draw |
| `DISCONNECT` | One side loses connection (the other side wins) |
| `TIMEOUT` | Turn time (`turn_time`) ran out |

### 6.7 Resigning, offering a draw and pausing (Game Controls)

| Command | Data in `OK` | Event sent |
|---|---|---|
| `RESIGN` | none | `GAME_OVER` (`RESIGN`) to both |
| `DRAW_OFFER` | none | `{"type":"DRAW_OFFERED"}` sent to the opponent |
| `DRAW_ACCEPT` | none | `GAME_OVER` (`DRAW`, `DRAW_AGREED`) to both |
| `DRAW_DECLINE` | none | `{"type":"DRAW_DECLINED"}` sent to the player who offered the draw |
| `PAUSE_OFFER` | none | `{"type":"PAUSE_OFFERED"}` sent to the opponent |
| `PAUSE_ACCEPT` | none | `{"type":"PAUSED"}` sent to both |
| `PAUSE_DECLINE` | none | `{"type":"PAUSE_DECLINED"}` sent to the player who requested the pause |
| `RESUME` | none | `{"type":"RESUMED"}` sent to both |

**Draw offer rules:**
- A draw offer is automatically cancelled when either side makes a valid `MOVE`.
- Sending `DRAW_OFFER` while a draw offer is still unanswered, while the game is `PAUSED`, or when playing against a Bot: the server returns `ERR 400 invalid_state`.
- `DRAW_ACCEPT`/`DRAW_DECLINE` when no draw offer is pending (or responding to one's own offer): `ERR 403 no_pending_offer`.

**Pause rules:**
- Either side can send `PAUSE_OFFER`. The opponent receives the `PAUSE_OFFERED` event (when playing against a Bot, the Bot automatically accepts and the server sends `PAUSED` immediately). A pending pause offer is automatically cancelled if a valid `MOVE` is made before it is answered.
- When the opponent sends `PAUSE_ACCEPT`, the game enters the paused state, the server records `pause_start_time = now`, sends `PAUSED` to both, and the `turn_time` clock is frozen.
- **While `PAUSED`:** Any `MOVE`, `DRAW_OFFER`, or `PAUSE_OFFER` command is rejected with `ERR 400 invalid_state` until the game is `RESUMED`.
- The maximum pause duration is **60 seconds**. If 60 seconds pass and nobody sends `RESUME`, the server automatically sends `RESUMED` and restarts the clock.
- Either side can send `RESUME` to continue the game immediately. The server offsets the turn start timestamp by the paused duration (`turn_start_time += now - pause_start_time`, preserving the exact remaining turn time) and sends `RESUMED` to both sides.
- `PAUSE_ACCEPT`/`PAUSE_DECLINE` when no pause offer is pending (or accepting one's own offer): `ERR 403 no_pending_offer`.

### 6.8 Rematch with the same opponent

| Command | Data in `OK` | Event to the opponent |
|---|---|---|
| `REMATCH` | none | `{"type":"REMATCH_OFFERED"}` |
| `REMATCH_DECLINE` | none | `{"type":"REMATCH_DECLINED","user":"<decliner_name>"}` |

- When **both** players send `REMATCH`, the server starts a new game with `GAME_START` to both, **swapping piece colors** compared to the previous game. (If the previous opponent was a Bot, sending `REMATCH` immediately starts a new game with that Bot; if the Bot plays `RED` first in the new game, the server immediately sends the Bot's opening `OPP_MOVE` right after `GAME_START`).
- Sending `REMATCH` a second time while already waiting for the opponent's response returns `ERR 400 invalid_state`.
- If the opponent has left or quit the application: `REMATCH` returns `ERR 301 player_not_found` (or `ERR 400 invalid_state` if the opponent has entered another room/match).
- **If, while waiting for a rematch, the opponent disconnects, logs out or leaves the lobby:** the server immediately sends the event `{"type":"REMATCH_DECLINED","user":"<opponent_name>"}` to the waiting side so the client's waiting screen does not hang.
- `REMATCH_DECLINE` when no offer is pending (or declining one's own offer): `ERR 403 no_pending_offer`.

### 6.9 History and replay

`HISTORY` (`limit` optional, default 20, maximum 30; `mode` optional: `"PVP"`, `"BOT"`, or omitted for all): returns the most recent games first. On the GUI, matches are separated into two tabs: **PvP** and **vs Bot**.

```json
{"type":"OK","id":9,"cmd":"HISTORY","games":[{"game_id":7,"opponent":"hao","result":"WIN","elo_delta":16,"time":1760000000},{"game_id":6,"opponent":"BOT_HARD","result":"LOSS","elo_delta":0,"time":1759999000}]}
```

`GET_GAME` (`game_id`): error 404 if the game does not exist.

```json
{"type":"OK","id":10,"cmd":"GET_GAME","game_id":7,"red":"hieu","yellow":"hao","winner":"RED","moves":[3,2,3,2,3,2,3,6,3]}
```

- `winner` is `"RED"`, `"YELLOW"` or `"DRAW"`.
- `moves` is an array of column numbers in the order played (the first move belongs to `RED`, alternating). The client replays by dropping pieces one by one following the array.

### 6.10 Automatic matchmaking (ELO Matchmaking)

| Command | Data in `OK` | Errors |
|---|---|---|
| `FIND_MATCH` | none (the server adds the player to the matchmaking queue) | 400 |
| `CANCEL_MATCH` | none (leaves the matchmaking queue) | 400 |

When an opponent with a close ELO is found, the server sends `GAME_START` to both. The allowed ELO gap gradually widens with waiting time. While searching for a match, the player **must send `CANCEL_MATCH` first** before creating/joining a room, challenging someone, or playing against a Bot (sending those commands without cancelling first returns `ERR 400 invalid_state`).

### 6.11 Keep-alive

| Command | Data in `OK` |
|---|---|
| `PING` | none |

- The client should send `PING` every **15 seconds** while logged in.
- If the server receives **no data at all** from a connection for **45 seconds**, it considers the connection dead and handles it as a lost connection (see section 7, rule 3).

---

## 7. Server Processing Rules

1. **Validate every move:** game is not `PAUSED`, correct turn, `col` is an integer from 0 to 6, column not full. If invalid, return `ERR` and **do not change the game state**.
2. **After every valid move:** check 4 directions (horizontal, vertical, both diagonals) from the piece just dropped on the fixed 6×7 board; if there is no win and the board holds 42 pieces, it is a draw.
3. **Connection lost mid-game** (socket closed or more than 45 seconds without data): treat it as that player resigning (`reason = DISCONNECT`); the opponent receives `GAME_OVER ... WIN`.
4. **Duplicate login:** if the account is already logged in, a new `LOGIN` is rejected with `ERR 104`.
5. **Update ELO** when the game ends (including resignation and connection loss), and only then send `GAME_OVER`:
   - **PvP matches:** `E = 1 / (1 + 10^((R_opponent - R_self)/400))`, `elo_delta = round(K·(S - E))`, `R' = R + elo_delta` where `S` = 1 (win), 0.5 (draw), 0 (loss), `K = 32`.
   - **Bot matches (`BOT_EASY` ELO 800, `BOT_MEDIUM` ELO 1200, `BOT_HARD` ELO 1600) — Option A:** Compute `E` using the Bot's ELO and set `elo_delta = max(0, round(K·(S - E)))`. Winning against a Bot grants ELO points, while losing or drawing never reduces the player's ELO (`elo_delta = 0`). The Bot's own ELO remains fixed.
6. **Passwords and system accounts:** the server stores passwords hashed (with salt) in PostgreSQL, never in plaintext. Bot accounts (`BOT_EASY`, `BOT_MEDIUM`, `BOT_HARD`) have `password_hash = NULL` and cannot be logged into; `REGISTER` also blocks any username starting with `BOT_` (`ERR 103`).
7. **Game logging in PostgreSQL (`game_id` from DB):**
   - Right at `GAME_START`, the server inserts a new row into `matches` (`RETURNING id`) to obtain `game_id`.
   - When a game ends (`GAME_OVER`, including Bot matches), inside a single transaction: update the match outcome in `matches`, insert all moves into `match_moves`, and update ELO + stats (`wins/losses/draws` or `bot_wins/bot_losses/bot_draws`) in `users` to serve `PROFILE`, `HISTORY` and `GET_GAME`.
   - If a PostgreSQL query or connection fails while processing a client command, the server returns `ERR 500 server_error`, logs the error, and attempts `PQreset`.
8. **Malformed lines:** JSON with a syntax error or missing `type`/`id` returns `ERR 107` (with `id` set to `null` if it cannot be read) and **keeps the connection open**, only discarding that line.
9. **Implementation notes in C:** use a lightweight JSON library (e.g. cJSON) and `libpq` for PostgreSQL. Always check the type of each field before reading its value, and free the JSON object after each message to avoid memory leaks.

---

## 8. Sample Sessions

Notation: `C1 -> S` means the client sends, `S -> C1` means the server sends.

### 8.1 Register, log in, challenge, play until a win

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
S  -> C1: {"type":"GAME_START","game_id":7,"opponent":"hao","color":"RED","first":true,"turn_time":30}
S  -> C2: {"type":"GAME_START","game_id":7,"opponent":"hieu","color":"YELLOW","first":false,"turn_time":30}

C1 -> S : {"type":"MOVE","id":5,"col":3}
S  -> C1: {"type":"OK","id":5,"cmd":"MOVE","col":3,"row":0}
S  -> C2: {"type":"OPP_MOVE","col":3,"row":0}
C2 -> S : {"type":"MOVE","id":3,"col":2}
S  -> C2: {"type":"OK","id":3,"cmd":"MOVE","col":2,"row":0}
S  -> C1: {"type":"OPP_MOVE","col":2,"row":0}
... (subsequent moves) ...
C1 -> S : {"type":"MOVE","id":9,"col":6}
S  -> C1: {"type":"OK","id":9,"cmd":"MOVE","col":6,"row":0}
S  -> C2: {"type":"OPP_MOVE","col":6,"row":0}
S  -> C1: {"type":"GAME_OVER","game_id":7,"result":"WIN","reason":"CONNECT4","elo_delta":16,"new_elo":1216}
S  -> C2: {"type":"GAME_OVER","game_id":7,"result":"LOSS","reason":"CONNECT4","elo_delta":-16,"new_elo":1184}
```

### 8.2 Move errors

```
C2 -> S : {"type":"MOVE","id":11,"col":4}            (when it is not C2's turn)
S  -> C2: {"type":"ERR","id":11,"code":201,"reason":"not_your_turn"}

C1 -> S : {"type":"MOVE","id":12,"col":9}
S  -> C1: {"type":"ERR","id":12,"code":202,"reason":"invalid_column"}

C1 -> S : {"type":"MOVE","id":13,"col":3}            (column 3 is full)
S  -> C1: {"type":"ERR","id":13,"code":203,"reason":"column_full"}

C1 -> S : {"type":"MOVE","id":14,"col":"3"}          (wrong type: string instead of number)
S  -> C1: {"type":"ERR","id":14,"code":101,"reason":"bad_arguments"}

C1 -> S : {"type":"MOVE","id":15,"col":3             (broken JSON)
S  -> C1: {"type":"ERR","id":null,"code":107,"reason":"malformed_json"}
```

### 8.3 Offering a draw

```
C1 -> S : {"type":"DRAW_OFFER","id":20}
S  -> C1: {"type":"OK","id":20,"cmd":"DRAW_OFFER"}
S  -> C2: {"type":"DRAW_OFFERED"}
C2 -> S : {"type":"DRAW_ACCEPT","id":8}
S  -> C2: {"type":"OK","id":8,"cmd":"DRAW_ACCEPT"}
S  -> C1: {"type":"GAME_OVER","game_id":7,"result":"DRAW","reason":"DRAW_AGREED","elo_delta":0,"new_elo":1200}
S  -> C2: {"type":"GAME_OVER","game_id":7,"result":"DRAW","reason":"DRAW_AGREED","elo_delta":0,"new_elo":1200}
```

### 8.4 Resigning, then a rematch

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
S  -> C1: {"type":"GAME_START","game_id":8,"opponent":"hao","color":"YELLOW","first":false,"turn_time":30}     (colors swapped)
S  -> C2: {"type":"GAME_START","game_id":8,"opponent":"hieu","color":"RED","first":true,"turn_time":30}
```

### 8.5 Rooms

```
C1 -> S : {"type":"CREATE_ROOM","id":30,"turn_time":30}
S  -> C1: {"type":"OK","id":30,"cmd":"CREATE_ROOM","room_id":3,"turn_time":30}
C2 -> S : {"type":"LIST_ROOMS","id":11}
S  -> C2: {"type":"OK","id":11,"cmd":"LIST_ROOMS","rooms":[{"room_id":3,"host":"hieu","players":1,"status":"WAITING","turn_time":30}]}
C2 -> S : {"type":"JOIN_ROOM","id":12,"room_id":3}
S  -> C2: {"type":"OK","id":12,"cmd":"JOIN_ROOM","room_id":3}
S  -> C1: {"type":"ROOM_JOINED","user":"hao"}
S  -> C1: {"type":"GAME_START","game_id":9,"opponent":"hao","color":"RED","first":true,"turn_time":30}
S  -> C2: {"type":"GAME_START","game_id":9,"opponent":"hieu","color":"YELLOW","first":false,"turn_time":30}
```

### 8.6 Reviewing a game

```
C1 -> S : {"type":"HISTORY","id":40}
S  -> C1: {"type":"OK","id":40,"cmd":"HISTORY","games":[{"game_id":7,"opponent":"hao","result":"WIN","elo_delta":16,"time":1760000000}]}
C1 -> S : {"type":"GET_GAME","id":41,"game_id":7}
S  -> C1: {"type":"OK","id":41,"cmd":"GET_GAME","game_id":7,"red":"hieu","yellow":"hao","winner":"RED","moves":[3,2,3,2,3,2,3,6,3]}
```

### 8.7 An event arriving while waiting for a response

Because responses are matched by `id`, the client is not confused when an event arrives in between:

```
C1 -> S : {"type":"LIST_PLAYERS","id":50}
S  -> C1: {"type":"CHALLENGED","from":"hao"}                      (event arrives before the response)
S  -> C1: {"type":"OK","id":50,"cmd":"LIST_PLAYERS","players":[...]}
```

### 8.8 Pausing and resuming a game (Pause / Resume)

```
C1 -> S : {"type":"PAUSE_OFFER","id":60}
S  -> C1: {"type":"OK","id":60,"cmd":"PAUSE_OFFER"}
S  -> C2: {"type":"PAUSE_OFFERED"}
C2 -> S : {"type":"PAUSE_ACCEPT","id":25}
S  -> C2: {"type":"OK","id":25,"cmd":"PAUSE_ACCEPT"}
S  -> C1: {"type":"PAUSED"}                                      (turn_time clock paused)
S  -> C2: {"type":"PAUSED"}

C1 -> S : {"type":"RESUME","id":61}
S  -> C1: {"type":"OK","id":61,"cmd":"RESUME"}
S  -> C1: {"type":"RESUMED"}                                     (game continues running)
S  -> C2: {"type":"RESUMED"}
```

### 8.9 Logging in again on the same connection (ERR 400) vs. another socket (ERR 104)

```
(Connection C1 has already logged in successfully and is in the IDLE state)
C1 -> S : {"type":"LOGIN","id":70,"user":"hieu","pass":"123456"}
S  -> C1: {"type":"ERR","id":70,"code":400,"reason":"invalid_state"}

(A different socket connection C_other tries to log in to the account hieu, which is online)
C_other -> S : {"type":"LOGIN","id":1,"user":"hieu","pass":"123456"}
S       -> C_other: {"type":"ERR","id":1,"code":104,"reason":"already_logged_in"}
```

### 8.10 Opponent leaves while a REMATCH is pending

```
(After GAME_OVER, both return to IDLE)
C1 -> S : {"type":"REMATCH","id":80}
S  -> C1: {"type":"OK","id":80,"cmd":"REMATCH"}
S  -> C2: {"type":"REMATCH_OFFERED"}
(C2 suddenly disconnects or clicks LOGOUT)
S  -> C1: {"type":"REMATCH_DECLINED","user":"hao"}                (C1 is not left hanging)
```

### 8.11 Playing against an AI Bot (no ELO loss, history still saved)

```
C1 -> S : {"type":"PLAY_BOT","id":90,"difficulty":"HARD","turn_time":30}
S  -> C1: {"type":"OK","id":90,"cmd":"PLAY_BOT"}
S  -> C1: {"type":"GAME_START","game_id":12,"opponent":"BOT_HARD","color":"RED","first":true,"turn_time":30}
C1 -> S : {"type":"MOVE","id":91,"col":3}
S  -> C1: {"type":"OK","id":91,"cmd":"MOVE","col":3,"row":0}
S  -> C1: {"type":"OPP_MOVE","col":3,"row":1}
... (game ends, suppose the player loses to the Bot) ...
S  -> C1: {"type":"GAME_OVER","game_id":12,"result":"LOSS","reason":"CONNECT4","elo_delta":0,"new_elo":1200}
```

---

## 9. Quick Testing Guide with `nc`

Once the server is running, open two terminals:

```
nc localhost 5000
{"type":"REGISTER","id":1,"user":"hieu","pass":"123456"}
{"type":"LOGIN","id":2,"user":"hieu","pass":"123456"}
{"type":"LIST_PLAYERS","id":3}
```

Type or paste each JSON line and press Enter. This lets you test the server before the GUI is ready. To make responses easier to read, you can pipe them through `jq`:

```
nc localhost 5000 | jq -c .
```

Minimal Python client (reads line by line, sends line by line):

```python
import socket, json

s = socket.create_connection(("localhost", 5000))
f = s.makefile("rwb")          # makefile handles buffering and line splitting

def send(msg):
    f.write((json.dumps(msg, separators=(",", ":")) + "\n").encode())
    f.flush()

def recv():
    line = f.readline()
    return json.loads(line) if line else None

send({"type": "LOGIN", "id": 1, "user": "hieu", "pass": "123456"})
print(recv())
```
