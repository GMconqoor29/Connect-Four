/**
 * @file protocol.h
 * @brief Application-layer protocol definition for Connect Four Online (NDJSON over TCP).
 *
 * Nhóm 10:
 *  - Trần Trung Hiếu  (20235934)
 *  - Phạm Song Hào    (20235930)
 *  - Nguyễn Đức Hoàng (20235938)
 *
 * Specification: Protocol Version 2.0 (NDJSON)
 * Reference: docs/protocol_ndjson.md, phan_chia_viec_va_cau_truc.md
 *
 * This header defines all constants, command names, event names, JSON keys,
 * error codes, error reason strings, enums, data structures, and helper validators
 * shared between the C server modules and the Python client.
 */

#ifndef PROTOCOL_H
#define PROTOCOL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>

/* ========================================================================= */
/* 1. Protocol Version & Transport Configuration                             */
/* ========================================================================= */

#define PROTOCOL_VERSION                "2.0"
#define PROTOCOL_DEFAULT_PORT           5000

/**
 * Maximum line length for an NDJSON message in bytes (including trailing \r?\n).
 * If a line exceeds this limit, the server drops the line and replies with ERR 108.
 */
#define PROTOCOL_MAX_LINE_LEN           4096

/** Recommended socket receive buffer capacity */
#define PROTOCOL_RECV_BUF_SIZE          8192

/** Heartbeat, timeout, and turn timer intervals (in seconds) */
#define PROTOCOL_PING_INTERVAL_SEC      15
#define PROTOCOL_TIMEOUT_SEC            45
#define PROTOCOL_CHALLENGE_TIMEOUT_SEC  30
#define PROTOCOL_DEFAULT_TURN_TIME_SEC  30
#define PROTOCOL_MAX_PAUSE_SEC          60

/* ========================================================================= */
/* 2. Format & Validation Limits                                             */
/* ========================================================================= */

#define USERNAME_MIN_LEN                3
#define USERNAME_MAX_LEN                16

#define PASSWORD_MIN_LEN                6
#define PASSWORD_MAX_LEN                32

#define MAX_LIST_PLAYERS_LIMIT          40
#define MAX_LIST_ROOMS_LIMIT            40

#define HISTORY_DEFAULT_LIMIT           20
#define HISTORY_MAX_LIMIT               30

/* ========================================================================= */
/* 3. Connect Four Game Board & ELO Constants                                */
/* ========================================================================= */

#define BOARD_ROWS                      6   /* Rows: 0 (bottom) to 5 (top) */
#define BOARD_COLS                      7   /* Columns: 0 (left) to 6 (right) */
#define TOTAL_CELLS                     (BOARD_ROWS * BOARD_COLS) /* 42 */
#define CONNECT_TARGET                  4   /* 4 in a row to win */

#define DEFAULT_ELO                     1200
#define ELO_K_FACTOR                    32

/* Board cell character representations */
#define CELL_EMPTY                      '.'
#define CELL_RED                        'R'
#define CELL_YELLOW                     'Y'

/* ========================================================================= */
/* 4. String Literals for Protocol Values                                     */
/* ========================================================================= */

/* Piece colors and players */
#define COLOR_RED_STR                   "RED"
#define COLOR_YELLOW_STR                "YELLOW"
#define COLOR_DRAW_STR                  "DRAW"

/* Player presence status */
#define STATUS_ONLINE_STR               "ONLINE"
#define STATUS_OFFLINE_STR              "OFFLINE"
#define STATUS_IDLE_STR                 "IDLE"
#define STATUS_IN_ROOM_STR              "IN_ROOM"
#define STATUS_IN_GAME_STR              "IN_GAME"

/* Room status */
#define ROOM_STATUS_WAITING_STR         "WAITING"
#define ROOM_STATUS_PLAYING_STR         "PLAYING"

/* Game match result (from player's perspective) */
#define RESULT_WIN_STR                  "WIN"
#define RESULT_LOSS_STR                 "LOSS"
#define RESULT_DRAW_STR                 "DRAW"

/* Game over reasons */
#define REASON_CONNECT4_STR             "CONNECT4"
#define REASON_BOARD_FULL_STR           "BOARD_FULL"
#define REASON_RESIGN_STR               "RESIGN"
#define REASON_DRAW_AGREED_STR          "DRAW_AGREED"
#define REASON_DISCONNECT_STR           "DISCONNECT"
#define REASON_TIMEOUT_STR              "TIMEOUT"

/* ========================================================================= */
/* 5. Message Types: Responses, Client Commands, Server Events               */
/* ========================================================================= */

/* Responses */
#define MSG_TYPE_OK                     "OK"
#define MSG_TYPE_ERR                    "ERR"

/* Client Commands */
#define CMD_REGISTER                    "REGISTER"
#define CMD_LOGIN                       "LOGIN"
#define CMD_LOGOUT                      "LOGOUT"
#define CMD_CHANGEPASS                  "CHANGEPASS"
#define CMD_PROFILE                     "PROFILE"
#define CMD_LIST_PLAYERS                "LIST_PLAYERS"
#define CMD_CREATE_ROOM                 "CREATE_ROOM"
#define CMD_JOIN_ROOM                   "JOIN_ROOM"
#define CMD_LEAVE_ROOM                  "LEAVE_ROOM"
#define CMD_LIST_ROOMS                  "LIST_ROOMS"
#define CMD_CHALLENGE                   "CHALLENGE"
#define CMD_ACCEPT                      "ACCEPT"
#define CMD_DECLINE                     "DECLINE"
#define CMD_MOVE                        "MOVE"
#define CMD_SYNC                        "SYNC"
#define CMD_RESIGN                      "RESIGN"
#define CMD_DRAW_OFFER                  "DRAW_OFFER"
#define CMD_DRAW_ACCEPT                 "DRAW_ACCEPT"
#define CMD_DRAW_DECLINE                "DRAW_DECLINE"
#define CMD_PAUSE_OFFER                 "PAUSE_OFFER"
#define CMD_PAUSE_ACCEPT                "PAUSE_ACCEPT"
#define CMD_PAUSE_DECLINE               "PAUSE_DECLINE"
#define CMD_RESUME                      "RESUME"
#define CMD_REMATCH                     "REMATCH"
#define CMD_REMATCH_DECLINE             "REMATCH_DECLINE"
#define CMD_HISTORY                     "HISTORY"
#define CMD_GET_GAME                    "GET_GAME"
#define CMD_FIND_MATCH                  "FIND_MATCH"
#define CMD_CANCEL_MATCH                "CANCEL_MATCH"
#define CMD_PING                        "PING"

/* Server Events (Push Notifications, no "id") */
#define EVT_STATUS_CHANGE               "STATUS_CHANGE"
#define EVT_ROOM_JOINED                 "ROOM_JOINED"
#define EVT_ROOM_LEFT                   "ROOM_LEFT"
#define EVT_CHALLENGED                  "CHALLENGED"
#define EVT_CHALLENGE_DECLINED          "CHALLENGE_DECLINED"
#define EVT_CHALLENGE_EXPIRED           "CHALLENGE_EXPIRED"
#define EVT_GAME_START                  "GAME_START"
#define EVT_OPP_MOVE                    "OPP_MOVE"
#define EVT_GAME_OVER                   "GAME_OVER"
#define EVT_DRAW_OFFERED                "DRAW_OFFERED"
#define EVT_DRAW_DECLINED               "DRAW_DECLINED"
#define EVT_PAUSE_OFFERED               "PAUSE_OFFERED"
#define EVT_PAUSED                      "PAUSED"
#define EVT_RESUMED                     "RESUMED"
#define EVT_PAUSE_DECLINED              "PAUSE_DECLINED"
#define EVT_REMATCH_OFFERED             "REMATCH_OFFERED"
#define EVT_REMATCH_DECLINED            "REMATCH_DECLINED"

/* ========================================================================= */
/* 6. JSON Field Keys                                                        */
/* ========================================================================= */

#define KEY_TYPE                        "type"
#define KEY_ID                          "id"
#define KEY_CMD                         "cmd"
#define KEY_CODE                        "code"
#define KEY_REASON                      "reason"

#define KEY_USER                        "user"
#define KEY_PASS                        "pass"
#define KEY_OLD                         "old"
#define KEY_NEW                         "new"

#define KEY_ELO                         "elo"
#define KEY_WINS                        "wins"
#define KEY_LOSSES                      "losses"
#define KEY_DRAWS                       "draws"

#define KEY_STATUS                      "status"
#define KEY_PLAYERS                     "players"
#define KEY_ROOMS                       "rooms"
#define KEY_ROOM_ID                     "room_id"
#define KEY_HOST                        "host"

#define KEY_FROM                        "from"
#define KEY_GAME_ID                     "game_id"
#define KEY_OPPONENT                    "opponent"
#define KEY_COLOR                       "color"
#define KEY_FIRST                       "first"
#define KEY_TURN_TIME                   "turn_time"

#define KEY_COL                         "col"
#define KEY_ROW                         "row"
#define KEY_BOARD                       "board"
#define KEY_TURN                        "turn"
#define KEY_DURATION                    "duration"

#define KEY_RESULT                      "result"
#define KEY_ELO_DELTA                   "elo_delta"
#define KEY_NEW_ELO                     "new_elo"

#define KEY_LIMIT                       "limit"
#define KEY_GAMES                       "games"
#define KEY_RED                         "red"
#define KEY_YELLOW                      "yellow"
#define KEY_WINNER                      "winner"
#define KEY_MOVES                       "moves"
#define KEY_TIME                        "time"

/* ========================================================================= */
/* 7. Error Codes and Reason Strings                                         */
/* ========================================================================= */

typedef enum {
    /* Success indicator */
    ERR_NONE                                = 0,

    /* 1xx: Syntax, Auth & General Errors */
    ERR_UNKNOWN_COMMAND                     = 100,
    ERR_BAD_ARGUMENTS                       = 101,
    ERR_BAD_CREDENTIALS                     = 102,
    ERR_USERNAME_TAKEN                      = 103,
    ERR_ALREADY_LOGGED_IN                   = 104,
    ERR_INVALID_USERNAME_OR_PASSWORD_FORMAT = 105,
    ERR_WRONG_OLD_PASSWORD                  = 106,
    ERR_MALFORMED_JSON                      = 107,
    ERR_LINE_TOO_LONG                       = 108,

    /* 2xx: Gameplay Errors */
    ERR_NOT_YOUR_TURN                       = 201,
    ERR_INVALID_COLUMN                      = 202,
    ERR_COLUMN_FULL                         = 203,

    /* 3xx: Lobby & Challenge Errors */
    ERR_PLAYER_NOT_FOUND                    = 301,
    ERR_PLAYER_BUSY                         = 302,
    ERR_NO_SUCH_CHALLENGE                   = 303,
    ERR_CANNOT_CHALLENGE_SELF               = 304,

    /* 4xx: State, Room & Match Flow Errors */
    ERR_INVALID_STATE                       = 400,
    ERR_ROOM_NOT_FOUND                      = 401,
    ERR_ROOM_FULL                           = 402,
    ERR_NO_PENDING_OFFER                    = 403,
    ERR_GAME_NOT_FOUND                      = 404,

    /* 5xx: Server Internal Errors */
    ERR_SERVER_ERROR                        = 500
} ProtocolErrorCode;

/* String literals for Error Reasons */
#define REASON_UNKNOWN_COMMAND                  "unknown_command"
#define REASON_BAD_ARGUMENTS                    "bad_arguments"
#define REASON_BAD_CREDENTIALS                  "bad_credentials"
#define REASON_USERNAME_TAKEN                   "username_taken"
#define REASON_ALREADY_LOGGED_IN                "already_logged_in"
#define REASON_INVALID_USERNAME_OR_PASSWORD_FORMAT "invalid_username_or_password_format"
#define REASON_WRONG_OLD_PASSWORD               "wrong_old_password"
#define REASON_MALFORMED_JSON                   "malformed_json"
#define REASON_LINE_TOO_LONG                    "line_too_long"

#define REASON_NOT_YOUR_TURN                    "not_your_turn"
#define REASON_INVALID_COLUMN                   "invalid_column"
#define REASON_COLUMN_FULL                      "column_full"

#define REASON_PLAYER_NOT_FOUND                 "player_not_found"
#define REASON_PLAYER_BUSY                      "player_busy"
#define REASON_NO_SUCH_CHALLENGE                "no_such_challenge"
#define REASON_CANNOT_CHALLENGE_SELF            "cannot_challenge_self"

#define REASON_INVALID_STATE                    "invalid_state"
#define REASON_ROOM_NOT_FOUND                   "room_not_found"
#define REASON_ROOM_FULL                        "room_full"
#define REASON_NO_PENDING_OFFER                 "no_pending_offer"
#define REASON_GAME_NOT_FOUND                   "game_not_found"

#define REASON_SERVER_ERROR                     "server_error"

/**
 * @brief Map an error code enum to its standardized reason string.
 * @param code ProtocolErrorCode enum value
 * @return const char* string literal, or "unknown_error" if code not recognized
 */
static inline const char *protocol_error_code_to_reason(ProtocolErrorCode code) {
    switch (code) {
        case ERR_UNKNOWN_COMMAND:
            return REASON_UNKNOWN_COMMAND;
        case ERR_BAD_ARGUMENTS:
            return REASON_BAD_ARGUMENTS;
        case ERR_BAD_CREDENTIALS:
            return REASON_BAD_CREDENTIALS;
        case ERR_USERNAME_TAKEN:
            return REASON_USERNAME_TAKEN;
        case ERR_ALREADY_LOGGED_IN:
            return REASON_ALREADY_LOGGED_IN;
        case ERR_INVALID_USERNAME_OR_PASSWORD_FORMAT:
            return REASON_INVALID_USERNAME_OR_PASSWORD_FORMAT;
        case ERR_WRONG_OLD_PASSWORD:
            return REASON_WRONG_OLD_PASSWORD;
        case ERR_MALFORMED_JSON:
            return REASON_MALFORMED_JSON;
        case ERR_LINE_TOO_LONG:
            return REASON_LINE_TOO_LONG;

        case ERR_NOT_YOUR_TURN:
            return REASON_NOT_YOUR_TURN;
        case ERR_INVALID_COLUMN:
            return REASON_INVALID_COLUMN;
        case ERR_COLUMN_FULL:
            return REASON_COLUMN_FULL;

        case ERR_PLAYER_NOT_FOUND:
            return REASON_PLAYER_NOT_FOUND;
        case ERR_PLAYER_BUSY:
            return REASON_PLAYER_BUSY;
        case ERR_NO_SUCH_CHALLENGE:
            return REASON_NO_SUCH_CHALLENGE;
        case ERR_CANNOT_CHALLENGE_SELF:
            return REASON_CANNOT_CHALLENGE_SELF;

        case ERR_INVALID_STATE:
            return REASON_INVALID_STATE;
        case ERR_ROOM_NOT_FOUND:
            return REASON_ROOM_NOT_FOUND;
        case ERR_ROOM_FULL:
            return REASON_ROOM_FULL;
        case ERR_NO_PENDING_OFFER:
            return REASON_NO_PENDING_OFFER;
        case ERR_GAME_NOT_FOUND:
            return REASON_GAME_NOT_FOUND;

        case ERR_SERVER_ERROR:
            return REASON_SERVER_ERROR;

        default:
            return "unknown_error";
    }
}

/* ========================================================================= */
/* 8. Enums for Internal State Machine & Gameplay                            */
/* ========================================================================= */

/** Piece color on the Connect Four board */
typedef enum {
    PIECE_NONE   = 0,
    PIECE_RED    = 1,
    PIECE_YELLOW = 2
} PieceColor;

/** Connection / Session State Machine (Section 4) */
typedef enum {
    CLIENT_STATE_CONNECTED = 0, /* Connected, unauthenticated */
    CLIENT_STATE_IDLE      = 1, /* Authenticated, in lobby */
    CLIENT_STATE_IN_ROOM   = 2, /* In a waiting room */
    CLIENT_STATE_IN_GAME   = 3  /* Actively playing in a match */
} ClientState;

/** Player presence status for lobby listing */
typedef enum {
    PLAYER_STATUS_OFFLINE = 0,
    PLAYER_STATUS_ONLINE  = 1,
    PLAYER_STATUS_IDLE    = 2,
    PLAYER_STATUS_IN_ROOM = 3,
    PLAYER_STATUS_IN_GAME = 4
} PlayerStatus;

/** Room state */
typedef enum {
    ROOM_STATUS_WAITING = 0,
    ROOM_STATUS_PLAYING = 1
} RoomStatus;

/** Game match result from a player's perspective */
typedef enum {
    GAME_RESULT_NONE = 0,
    GAME_RESULT_WIN  = 1,
    GAME_RESULT_LOSS = 2,
    GAME_RESULT_DRAW = 3
} GameResult;

/** Reason why a game ended */
typedef enum {
    GAME_OVER_REASON_NONE        = 0,
    GAME_OVER_REASON_CONNECT4    = 1,
    GAME_OVER_REASON_BOARD_FULL  = 2,
    GAME_OVER_REASON_RESIGN      = 3,
    GAME_OVER_REASON_DRAW_AGREED = 4,
    GAME_OVER_REASON_DISCONNECT  = 5,
    GAME_OVER_REASON_TIMEOUT     = 6
} GameOverReason;

/* ========================================================================= */
/* 9. Data Structures for Server Modules & Handlers                          */
/* ========================================================================= */

/** User profile summary (for PROFILE command) */
typedef struct {
    char username[USERNAME_MAX_LEN + 1];
    int elo;
    int wins;
    int losses;
    int draws;
} ProtocolUserProfile;

/** Player entry in LIST_PLAYERS */
typedef struct {
    char username[USERNAME_MAX_LEN + 1];
    int elo;
    PlayerStatus status;
} ProtocolPlayerEntry;

/** Room entry in LIST_ROOMS */
typedef struct {
    int room_id;
    char host[USERNAME_MAX_LEN + 1];
    int players_count;
    RoomStatus status;
} ProtocolRoomEntry;

/** Game summary in HISTORY */
typedef struct {
    int game_id;
    char opponent[USERNAME_MAX_LEN + 1];
    GameResult result;
    int elo_delta;
    int64_t timestamp;
} ProtocolHistoryEntry;

/** Single move in a match */
typedef struct {
    int col;        /* 0 .. 6 */
    int row;        /* 0 .. 5 */
    PieceColor color;
} ProtocolMove;

/** Board state snapshot (for SYNC) */
typedef struct {
    PieceColor grid[BOARD_ROWS][BOARD_COLS]; /* grid[row][col], row 0 is bottom */
    PieceColor current_turn;
} ProtocolBoardState;

/** Rematch session tracking flags (for lobby and match lifecycle) */
typedef struct {
    char last_opponent[USERNAME_MAX_LEN + 1];
    bool rematch_available; /* true immediately after game ends until either leaves IDLE or timeout */
    bool rematch_offered;   /* true if this player has already sent REMATCH */
} ProtocolRematchState;

/** In-game match controls & turn timer state (for match.c) */
typedef struct {
    bool is_paused;
    PieceColor paused_by;       /* PIECE_NONE, PIECE_RED, or PIECE_YELLOW */
    PieceColor draw_offered_by;  /* PIECE_NONE, PIECE_RED, or PIECE_YELLOW */
    PieceColor pause_offered_by; /* PIECE_NONE, PIECE_RED, or PIECE_YELLOW */
    int turn_time_sec;          /* Turn timeout in seconds (e.g. 30, or 0 for unlimited) */
    int64_t turn_start_time;    /* Unix timestamp (seconds) when current turn started */
    int64_t pause_start_time;   /* Unix timestamp (seconds) when game was paused */
} ProtocolMatchControls;

/* ========================================================================= */
/* 10. Helper Conversion & Validation Functions (Inline)                     */
/* ========================================================================= */

/**
 * @brief Validate username format:
 *        Length must be between 3 and 16 characters.
 *        Allowed characters: [A-Za-z0-9_].
 */
static inline bool protocol_is_valid_username(const char *username) {
    if (!username) return false;
    size_t len = strlen(username);
    if (len < USERNAME_MIN_LEN || len > USERNAME_MAX_LEN) {
        return false;
    }
    for (size_t i = 0; i < len; ++i) {
        char c = username[i];
        if (!isalnum((unsigned char)c) && c != '_') {
            return false;
        }
    }
    return true;
}

/**
 * @brief Validate password format:
 *        Length must be between 6 and 32 characters.
 *        Must not contain any whitespace characters.
 */
static inline bool protocol_is_valid_password(const char *password) {
    if (!password) return false;
    size_t len = strlen(password);
    if (len < PASSWORD_MIN_LEN || len > PASSWORD_MAX_LEN) {
        return false;
    }
    for (size_t i = 0; i < len; ++i) {
        if (isspace((unsigned char)password[i])) {
            return false;
        }
    }
    return true;
}

/**
 * @brief Check if column index is within board boundaries [0, 6].
 */
static inline bool protocol_is_valid_col(int col) {
    return (col >= 0 && col < BOARD_COLS);
}

/**
 * @brief Check if row index is within board boundaries [0, 5].
 */
static inline bool protocol_is_valid_row(int row) {
    return (row >= 0 && row < BOARD_ROWS);
}

/**
 * @brief Convert PieceColor to character representation ('.', 'R', 'Y').
 */
static inline char protocol_piece_to_char(PieceColor color) {
    switch (color) {
        case PIECE_RED:    return CELL_RED;
        case PIECE_YELLOW: return CELL_YELLOW;
        default:           return CELL_EMPTY;
    }
}

/**
 * @brief Convert character representation ('.', 'R', 'Y') to PieceColor.
 */
static inline PieceColor protocol_char_to_piece(char c) {
    if (c == CELL_RED) return PIECE_RED;
    if (c == CELL_YELLOW) return PIECE_YELLOW;
    return PIECE_NONE;
}

/**
 * @brief Convert PieceColor to protocol string ("RED", "YELLOW", or "NONE").
 */
static inline const char *protocol_color_to_str(PieceColor color) {
    switch (color) {
        case PIECE_RED:    return COLOR_RED_STR;
        case PIECE_YELLOW: return COLOR_YELLOW_STR;
        default:           return "NONE";
    }
}

/**
 * @brief Parse PieceColor from protocol string ("RED" or "YELLOW").
 */
static inline PieceColor protocol_color_from_str(const char *str) {
    if (!str) return PIECE_NONE;
    if (strcmp(str, COLOR_RED_STR) == 0) return PIECE_RED;
    if (strcmp(str, COLOR_YELLOW_STR) == 0) return PIECE_YELLOW;
    return PIECE_NONE;
}

/**
 * @brief Convert winner piece color to protocol string for GET_GAME ("RED", "YELLOW", or "DRAW").
 */
static inline const char *protocol_winner_to_str(PieceColor winner_piece) {
    switch (winner_piece) {
        case PIECE_RED:    return COLOR_RED_STR;
        case PIECE_YELLOW: return COLOR_YELLOW_STR;
        default:           return COLOR_DRAW_STR;
    }
}

/**
 * @brief Parse winner piece from protocol string ("RED", "YELLOW", or "DRAW").
 *        Returns PIECE_NONE if the match ended in a draw or string is invalid.
 */
static inline PieceColor protocol_winner_from_str(const char *str) {
    if (!str) return PIECE_NONE;
    if (strcmp(str, COLOR_RED_STR) == 0) return PIECE_RED;
    if (strcmp(str, COLOR_YELLOW_STR) == 0) return PIECE_YELLOW;
    return PIECE_NONE;
}

/**
 * @brief Convert PlayerStatus enum to string literal.
 */
static inline const char *protocol_status_to_str(PlayerStatus status) {
    switch (status) {
        case PLAYER_STATUS_ONLINE:  return STATUS_ONLINE_STR;
        case PLAYER_STATUS_OFFLINE: return STATUS_OFFLINE_STR;
        case PLAYER_STATUS_IDLE:    return STATUS_IDLE_STR;
        case PLAYER_STATUS_IN_ROOM: return STATUS_IN_ROOM_STR;
        case PLAYER_STATUS_IN_GAME: return STATUS_IN_GAME_STR;
        default:                    return STATUS_OFFLINE_STR;
    }
}

/**
 * @brief Parse PlayerStatus enum from string literal.
 */
static inline PlayerStatus protocol_status_from_str(const char *str) {
    if (!str) return PLAYER_STATUS_OFFLINE;
    if (strcmp(str, STATUS_IDLE_STR) == 0) return PLAYER_STATUS_IDLE;
    if (strcmp(str, STATUS_IN_ROOM_STR) == 0) return PLAYER_STATUS_IN_ROOM;
    if (strcmp(str, STATUS_IN_GAME_STR) == 0) return PLAYER_STATUS_IN_GAME;
    if (strcmp(str, STATUS_ONLINE_STR) == 0) return PLAYER_STATUS_ONLINE;
    return PLAYER_STATUS_OFFLINE;
}

/**
 * @brief Convert GameResult enum to string literal.
 */
static inline const char *protocol_result_to_str(GameResult res) {
    switch (res) {
        case GAME_RESULT_WIN:  return RESULT_WIN_STR;
        case GAME_RESULT_LOSS: return RESULT_LOSS_STR;
        case GAME_RESULT_DRAW: return RESULT_DRAW_STR;
        default:               return "";
    }
}

/**
 * @brief Parse GameResult enum from string literal.
 */
static inline GameResult protocol_result_from_str(const char *str) {
    if (!str) return GAME_RESULT_NONE;
    if (strcmp(str, RESULT_WIN_STR) == 0) return GAME_RESULT_WIN;
    if (strcmp(str, RESULT_LOSS_STR) == 0) return GAME_RESULT_LOSS;
    if (strcmp(str, RESULT_DRAW_STR) == 0) return GAME_RESULT_DRAW;
    return GAME_RESULT_NONE;
}

/**
 * @brief Convert GameOverReason enum to string literal.
 */
static inline const char *protocol_game_over_reason_to_str(GameOverReason reason) {
    switch (reason) {
        case GAME_OVER_REASON_CONNECT4:    return REASON_CONNECT4_STR;
        case GAME_OVER_REASON_BOARD_FULL:  return REASON_BOARD_FULL_STR;
        case GAME_OVER_REASON_RESIGN:      return REASON_RESIGN_STR;
        case GAME_OVER_REASON_DRAW_AGREED: return REASON_DRAW_AGREED_STR;
        case GAME_OVER_REASON_DISCONNECT:  return REASON_DISCONNECT_STR;
        case GAME_OVER_REASON_TIMEOUT:     return REASON_TIMEOUT_STR;
        default:                           return "UNKNOWN";
    }
}

/**
 * @brief Parse GameOverReason enum from string literal.
 */
static inline GameOverReason protocol_game_over_reason_from_str(const char *str) {
    if (!str) return GAME_OVER_REASON_NONE;
    if (strcmp(str, REASON_CONNECT4_STR) == 0) return GAME_OVER_REASON_CONNECT4;
    if (strcmp(str, REASON_BOARD_FULL_STR) == 0) return GAME_OVER_REASON_BOARD_FULL;
    if (strcmp(str, REASON_RESIGN_STR) == 0) return GAME_OVER_REASON_RESIGN;
    if (strcmp(str, REASON_DRAW_AGREED_STR) == 0) return GAME_OVER_REASON_DRAW_AGREED;
    if (strcmp(str, REASON_DISCONNECT_STR) == 0) return GAME_OVER_REASON_DISCONNECT;
    if (strcmp(str, REASON_TIMEOUT_STR) == 0) return GAME_OVER_REASON_TIMEOUT;
    return GAME_OVER_REASON_NONE;
}

/**
 * @brief Validate if a command is allowed given the current connection state.
 *        Implements the transition matrix from Section 4 of protocol_ndjson.md.
 *
 * @param cmd The command string (e.g., "LOGIN", "MOVE", "PING")
 * @param state Current ClientState (CONNECTED, IDLE, IN_ROOM, IN_GAME)
 * @return true if command is valid in state, false otherwise (should return ERR 400).
 */
static inline bool protocol_is_cmd_allowed_in_state(const char *cmd, ClientState state) {
    if (!cmd) return false;

    /* PING is allowed in any state */
    if (strcmp(cmd, CMD_PING) == 0) {
        return true;
    }

    /* Common authenticated commands allowed in IDLE, IN_ROOM, and IN_GAME */
    if (strcmp(cmd, CMD_LOGOUT) == 0 ||
        strcmp(cmd, CMD_CHANGEPASS) == 0 ||
        strcmp(cmd, CMD_PROFILE) == 0 ||
        strcmp(cmd, CMD_HISTORY) == 0 ||
        strcmp(cmd, CMD_GET_GAME) == 0) {
        return (state != CLIENT_STATE_CONNECTED);
    }

    switch (state) {
        case CLIENT_STATE_CONNECTED:
            return (strcmp(cmd, CMD_REGISTER) == 0 ||
                    strcmp(cmd, CMD_LOGIN) == 0);

        case CLIENT_STATE_IDLE:
            return (strcmp(cmd, CMD_LIST_PLAYERS) == 0 ||
                    strcmp(cmd, CMD_LIST_ROOMS) == 0 ||
                    strcmp(cmd, CMD_CREATE_ROOM) == 0 ||
                    strcmp(cmd, CMD_JOIN_ROOM) == 0 ||
                    strcmp(cmd, CMD_CHALLENGE) == 0 ||
                    strcmp(cmd, CMD_ACCEPT) == 0 ||
                    strcmp(cmd, CMD_DECLINE) == 0 ||
                    strcmp(cmd, CMD_FIND_MATCH) == 0 ||
                    strcmp(cmd, CMD_CANCEL_MATCH) == 0 ||
                    strcmp(cmd, CMD_REMATCH) == 0 ||
                    strcmp(cmd, CMD_REMATCH_DECLINE) == 0);

        case CLIENT_STATE_IN_ROOM:
            return (strcmp(cmd, CMD_LIST_PLAYERS) == 0 ||
                    strcmp(cmd, CMD_LIST_ROOMS) == 0 ||
                    strcmp(cmd, CMD_LEAVE_ROOM) == 0);

        case CLIENT_STATE_IN_GAME:
            return (strcmp(cmd, CMD_MOVE) == 0 ||
                    strcmp(cmd, CMD_RESIGN) == 0 ||
                    strcmp(cmd, CMD_DRAW_OFFER) == 0 ||
                    strcmp(cmd, CMD_DRAW_ACCEPT) == 0 ||
                    strcmp(cmd, CMD_DRAW_DECLINE) == 0 ||
                    strcmp(cmd, CMD_PAUSE_OFFER) == 0 ||
                    strcmp(cmd, CMD_PAUSE_ACCEPT) == 0 ||
                    strcmp(cmd, CMD_PAUSE_DECLINE) == 0 ||
                    strcmp(cmd, CMD_RESUME) == 0 ||
                    strcmp(cmd, CMD_SYNC) == 0);

        default:
            return false;
    }
}

#ifdef __cplusplus
}
#endif

#endif /* PROTOCOL_H */
