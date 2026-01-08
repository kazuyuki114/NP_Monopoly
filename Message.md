# Network Protocol Message Documentation

## Full Message Format

```
+------------------+------------------+------------------+------------------+
|      type        |    sender_id     |    target_id     | payload_length   |
|    (4 bytes)     |    (4 bytes)     |    (4 bytes)     |    (4 bytes)     |
+------------------+------------------+------------------+------------------+
|                                                                           |
|                         payload (JSON string)                             |
|                      (payload_length bytes)                               |
|                                                                           |
+---------------------------------------------------------------------------+
```

**Note:** All uint32_t fields are in **network byte order (big-endian)**.

### Header Structure
| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0 | 4 bytes | `type` | MessageType enum value |
| 4 | 4 bytes | `sender_id` | User ID of sender (0 if not logged in) |
| 8 | 4 bytes | `target_id` | User ID of target (0 for server) |
| 12 | 4 bytes | `payload_length` | Length of the JSON payload |

**Constants:**
- `MSG_HEADER_SIZE` = 16 bytes
- `MSG_MAX_PAYLOAD` = 4096 bytes

---

## Message Type Summary

| Value | Name | Direction | Description |
|-------|------|-----------|-------------|
| 1 | MSG_REGISTER | C→S | Register new account |
| 2 | MSG_REGISTER_RESPONSE | S→C | Registration result |
| 3 | MSG_LOGIN | C→S | Login request |
| 4 | MSG_LOGIN_RESPONSE | S→C | Login result |
| 5 | MSG_LOGOUT | C→S | Logout request |
| 10 | MSG_GET_ONLINE_PLAYERS | C→S | Request online players list |
| 11 | MSG_ONLINE_PLAYERS_LIST | S→C | Response with online players |
| 12 | MSG_SEARCH_MATCH | C↔S | Start matchmaking search |
| 13 | MSG_MATCH_FOUND | S→C | Match found notification |
| 14 | MSG_CANCEL_SEARCH | C→S | Cancel matchmaking |
| 15 | MSG_SEND_CHALLENGE | C↔S | Send challenge to player |
| 16 | MSG_CHALLENGE_REQUEST | S→C | Challenge request received |
| 17 | MSG_ACCEPT_CHALLENGE | C→S | Accept a challenge |
| 18 | MSG_DECLINE_CHALLENGE | C↔S | Decline a challenge |
| 20 | MSG_GAME_START | S→C | Game start notification |
| 21 | MSG_GAME_STATE | S→C | Full game state update |
| 22 | MSG_ROLL_DICE | C→S | Roll dice action |
| 23 | MSG_BUY_PROPERTY | C→S | Buy property action |
| 24 | MSG_SKIP_PROPERTY | C→S | Skip buying property |
| 25 | MSG_UPGRADE_PROPERTY | C→S | Build house/hotel |
| 26 | MSG_DOWNGRADE_PROPERTY | C→S | Sell house/hotel |
| 27 | MSG_MORTGAGE_PROPERTY | C→S | Mortgage property |
| 28 | MSG_PAY_JAIL_FINE | C→S | Pay to exit jail |
| 29 | MSG_DECLARE_BANKRUPT | C→S | Declare bankruptcy |
| 30 | MSG_GAME_END | S→C | Draw offer / Game ended |
| 31 | MSG_GAME_RESULT | S→C | Game results |
| 32 | MSG_REMATCH_REQUEST | C↔S | Request rematch |
| 33 | MSG_REMATCH_RESPONSE | C↔S | Rematch response |
| 34 | MSG_REMATCH_CANCELLED | S→C | Rematch cancelled |
| 35 | MSG_PAUSE_GAME | C→S | Pause game |
| 36 | MSG_RESUME_GAME | C→S | Resume game |
| 37 | MSG_SURRENDER | C→S | Surrender game |
| 100 | MSG_SUCCESS | S→C | Generic success |
| 101 | MSG_ERROR | S→C | Generic error |
| 102 | MSG_INVALID_MOVE | S→C | Invalid move attempted |
| 103 | MSG_NOT_YOUR_TURN | S→C | Action when not player's turn |
| 104 | MSG_HEARTBEAT | C→S | Keep-alive ping |
| 105 | MSG_HEARTBEAT_ACK | S→C | Keep-alive acknowledgment |

---

## 1. Authentication Messages

### MSG_REGISTER (type=1)
```
Header:
  type:           0x00000001 (1)
  sender_id:      0x00000000 (not logged in)
  target_id:      0x00000000 (server)
  payload_length: varies

Payload (JSON):
  {"username":"player1","password":"secret123","email":"player1@example.com"}
```

### MSG_REGISTER_RESPONSE (type=2)
```
Header:
  type:           0x00000002 (2)
  sender_id:      0x00000000 (server)
  target_id:      <user_id>
  payload_length: varies

Payload (JSON) - Success:
  {"success":true,"message":"Registration successful! Please login.","user_id":5}

Payload (JSON) - Failure:
  {"success":false,"error":"Username already exists"}
```

### MSG_LOGIN (type=3)
```
Header:
  type:           0x00000003 (3)
  sender_id:      0x00000000 (not logged in)
  target_id:      0x00000000 (server)
  payload_length: varies

Payload (JSON):
  {"username":"player1","password":"secret123"}
```

### MSG_LOGIN_RESPONSE (type=4)
```
Header:
  type:           0x00000004 (4)
  sender_id:      0x00000000 (server)
  target_id:      <user_id>
  payload_length: varies

Payload (JSON) - Success:
  {
    "success": true,
    "user_id": 5,
    "username": "player1",
    "session_id": "a1b2c3d4e5f6...",
    "elo_rating": 1200,
    "total_matches": 10,
    "wins": 6,
    "losses": 4
  }

Payload (JSON) - Failure:
  {"success":false,"error":"Invalid username or password"}
```

### MSG_LOGOUT (type=5)
```
Header:
  type:           0x00000005 (5)
  sender_id:      <user_id>
  target_id:      0x00000000 (server)
  payload_length: 0x00000000

Payload: (none)
```

---

## 2. Lobby & Matchmaking Messages

### MSG_GET_ONLINE_PLAYERS (type=10)
```
Header:
  type:           0x0000000A (10)
  sender_id:      <user_id>
  target_id:      0x00000000 (server)
  payload_length: 0x00000000

Payload: (none)
```

### MSG_ONLINE_PLAYERS_LIST (type=11)
```
Header:
  type:           0x0000000B (11)
  sender_id:      0x00000000 (server)
  target_id:      <user_id>
  payload_length: varies

Payload (JSON):
  {
    "success": true,
    "count": 3,
    "players": [
      {"user_id": 2, "username": "alice", "elo_rating": 1350, "status": "idle"},
      {"user_id": 7, "username": "bob", "elo_rating": 1180, "status": "searching"},
      {"user_id": 9, "username": "charlie", "elo_rating": 1420, "status": "in_game"}
    ]
  }
```

### MSG_SEARCH_MATCH (type=12)
**Client → Server:**
```
Header:
  type:           0x0000000C (12)
  sender_id:      <user_id>
  target_id:      0x00000000 (server)
  payload_length: 0x00000000

Payload: (none)
```

**Server → Client (confirmation):**
```
Header:
  type:           0x0000000C (12)
  sender_id:      0x00000000 (server)
  target_id:      <user_id>
  payload_length: varies

Payload (JSON):
  {"success":true,"message":"Searching for opponent...","your_elo":1200}
```

### MSG_MATCH_FOUND (type=13)
```
Header:
  type:           0x0000000D (13)
  sender_id:      0x00000000 (server)
  target_id:      <user_id>
  payload_length: varies

Payload (JSON):
  {
    "match_id": 456,
    "opponent_id": 789,
    "opponent_name": "bob",
    "opponent_elo": 1150,
    "your_player_num": 1,
    "message": "Match found! You go first."
  }
```

### MSG_CANCEL_SEARCH (type=14)
```
Header:
  type:           0x0000000E (14)
  sender_id:      <user_id>
  target_id:      0x00000000 (server)
  payload_length: 0x00000000

Payload: (none)
```

### MSG_SEND_CHALLENGE (type=15)
**Client → Server:**
```
Header:
  type:           0x0000000F (15)
  sender_id:      <user_id>
  target_id:      0x00000000 (server)
  payload_length: varies

Payload (JSON):
  {"target_id":123}
```

**Server → Client (confirmation):**
```
Header:
  type:           0x0000000F (15)
  sender_id:      0x00000000 (server)
  target_id:      <user_id>
  payload_length: varies

Payload (JSON):
  {
    "success": true,
    "message": "Challenge sent!",
    "challenge_id": 999,
    "target_id": 123,
    "target_name": "alice"
  }
```

### MSG_CHALLENGE_REQUEST (type=16)
```
Header:
  type:           0x00000010 (16)
  sender_id:      0x00000000 (server)
  target_id:      <challenged_user_id>
  payload_length: varies

Payload (JSON):
  {
    "challenge_id": 999,
    "challenger_id": 5,
    "challenger_name": "player1",
    "challenger_elo": 1200
  }
```

### MSG_ACCEPT_CHALLENGE (type=17)
```
Header:
  type:           0x00000011 (17)
  sender_id:      <user_id>
  target_id:      0x00000000 (server)
  payload_length: varies

Payload (JSON):
  {"challenge_id":999}
```

### MSG_DECLINE_CHALLENGE (type=18)
**Client → Server:**
```
Header:
  type:           0x00000012 (18)
  sender_id:      <user_id>
  target_id:      0x00000000 (server)
  payload_length: varies

Payload (JSON):
  {"challenge_id":999}
```

**Server → Challenger (notification):**
```
Header:
  type:           0x00000012 (18)
  sender_id:      0x00000000 (server)
  target_id:      <challenger_user_id>
  payload_length: varies

Payload (JSON):
  {
    "challenge_id": 999,
    "declined_by_id": 123,
    "declined_by_name": "alice",
    "message": "Your challenge was declined"
  }
```

---

## 3. Game Actions

### MSG_GAME_START (type=20)
```
Header:
  type:           0x00000014 (20)
  sender_id:      0x00000000 (server)
  target_id:      <user_id>
  payload_length: varies

Payload (JSON): (initial game state - same as MSG_GAME_STATE)
```

### MSG_GAME_STATE (type=21)
```
Header:
  type:           0x00000015 (21)
  sender_id:      0x00000000 (server)
  target_id:      <user_id>
  payload_length: varies

Payload (JSON):
  {
    "match_id": 456,
    "current_player": 0,
    "state": 1,
    "move_count": 15,
    "paused": false,
    "paused_by": -1,
    "dice": [3, 4],
    "message": "Rolled 7!",
    "message2": "",
    "players": [
      {
        "user_id": 5,
        "username": "player1",
        "money": 1500,
        "position": 10,
        "jailed": false,
        "turns_in_jail": 0
      },
      {
        "user_id": 8,
        "username": "player2",
        "money": 1200,
        "position": 24,
        "jailed": false,
        "turns_in_jail": 0
      }
    ],
    "properties": [
      {"owner": -1, "upgrades": 0, "mortgaged": false},
      {"owner": 0, "upgrades": 2, "mortgaged": false},
      ...
    ]
  }
```

**Game State Enum Values:**
| Value | State |
|-------|-------|
| 0 | GSTATE_WAITING_ROLL |
| 1 | GSTATE_WAITING_BUY |
| 2 | GSTATE_WAITING_DEBT |
| 3 | GSTATE_PAUSED |
| 4 | GSTATE_ENDED |

### MSG_ROLL_DICE (type=22)
```
Header:
  type:           0x00000016 (22)
  sender_id:      <user_id>
  target_id:      0x00000000 (server)
  payload_length: 0x00000000

Payload: (none)
```

### MSG_BUY_PROPERTY (type=23)
```
Header:
  type:           0x00000017 (23)
  sender_id:      <user_id>
  target_id:      0x00000000 (server)
  payload_length: 0x00000000

Payload: (none)
```

### MSG_SKIP_PROPERTY (type=24)
```
Header:
  type:           0x00000018 (24)
  sender_id:      <user_id>
  target_id:      0x00000000 (server)
  payload_length: 0x00000000

Payload: (none)
```

### MSG_UPGRADE_PROPERTY (type=25)
```
Header:
  type:           0x00000019 (25)
  sender_id:      <user_id>
  target_id:      0x00000000 (server)
  payload_length: varies

Payload (JSON):
  {"property_id":11}
```

### MSG_DOWNGRADE_PROPERTY (type=26)
```
Header:
  type:           0x0000001A (26)
  sender_id:      <user_id>
  target_id:      0x00000000 (server)
  payload_length: varies

Payload (JSON):
  {"property_id":11}
```

### MSG_MORTGAGE_PROPERTY (type=27)
```
Header:
  type:           0x0000001B (27)
  sender_id:      <user_id>
  target_id:      0x00000000 (server)
  payload_length: varies

Payload (JSON):
  {"property_id":11}
```

### MSG_PAY_JAIL_FINE (type=28)
```
Header:
  type:           0x0000001C (28)
  sender_id:      <user_id>
  target_id:      0x00000000 (server)
  payload_length: 0x00000000

Payload: (none)
```

### MSG_DECLARE_BANKRUPT (type=29)
```
Header:
  type:           0x0000001D (29)
  sender_id:      <user_id>
  target_id:      0x00000000 (server)
  payload_length: 0x00000000

Payload: (none)
```

---

## 4. Game End & Rematch

### MSG_GAME_END (type=30) - Draw Offer
```
Header:
  type:           0x0000001E (30)
  sender_id:      0x00000000 (server)
  target_id:      <user_id>
  payload_length: varies

Payload (JSON):
  {
    "from_id": 5,
    "from_name": "player1",
    "message": "Your opponent offers a draw"
  }
```

### MSG_GAME_RESULT (type=31)
```
Header:
  type:           0x0000001F (31)
  sender_id:      0x00000000 (server)
  target_id:      <user_id>
  payload_length: varies

Payload (JSON) - Win/Loss:
  {
    "match_id": 456,
    "is_draw": false,
    "reason": "surrender",
    "winner_id": 5,
    "winner_name": "player1",
    "loser_id": 8,
    "loser_name": "player2",
    "winner_elo_before": 1200,
    "winner_elo_after": 1216,
    "winner_elo_change": 16,
    "loser_elo_before": 1180,
    "loser_elo_after": 1164,
    "loser_elo_change": -16
  }

Payload (JSON) - Draw:
  {
    "match_id": 456,
    "is_draw": true,
    "player1_elo_change": 5,
    "player2_elo_change": -5,
    "player1_new_elo": 1205,
    "player2_new_elo": 1175
  }
```

**Possible reasons:** `"bankruptcy"`, `"surrender"`, `"disconnect"`

### MSG_REMATCH_REQUEST (type=32)
**Client → Server:**
```
Header:
  type:           0x00000020 (32)
  sender_id:      <user_id>
  target_id:      0x00000000 (server)
  payload_length: varies

Payload (JSON):
  {"opponent_id":8}
```

**Server → Opponent:**
```
Header:
  type:           0x00000020 (32)
  sender_id:      0x00000000 (server)
  target_id:      <opponent_user_id>
  payload_length: varies

Payload (JSON):
  {
    "from_id": 5,
    "from_name": "player1",
    "from_elo": 1216,
    "message": "Your opponent wants a rematch!"
  }
```

### MSG_REMATCH_RESPONSE (type=33)
**Client → Server:**
```
Header:
  type:           0x00000021 (33)
  sender_id:      <user_id>
  target_id:      0x00000000 (server)
  payload_length: varies

Payload (JSON) - Accept:
  {"accept":true,"opponent_id":5}

Payload (JSON) - Decline:
  {"accept":false,"opponent_id":5}
```

**Server → Original Requester (if declined):**
```
Header:
  type:           0x00000021 (33)
  sender_id:      0x00000000 (server)
  target_id:      <user_id>
  payload_length: varies

Payload (JSON):
  {"declined":true,"message":"Rematch declined"}
```

### MSG_REMATCH_CANCELLED (type=34)
```
Header:
  type:           0x00000022 (34)
  sender_id:      0x00000000 (server)
  target_id:      <user_id>
  payload_length: varies

Payload (JSON):
  {"message":"Rematch cancelled"}
```

### MSG_PAUSE_GAME (type=35)
```
Header:
  type:           0x00000023 (35)
  sender_id:      <user_id>
  target_id:      0x00000000 (server)
  payload_length: 0x00000000

Payload: (none)
```

### MSG_RESUME_GAME (type=36)
```
Header:
  type:           0x00000024 (36)
  sender_id:      <user_id>
  target_id:      0x00000000 (server)
  payload_length: 0x00000000

Payload: (none)
```

### MSG_SURRENDER (type=37)
```
Header:
  type:           0x00000025 (37)
  sender_id:      <user_id>
  target_id:      0x00000000 (server)
  payload_length: 0x00000000

Payload: (none)
```

---

## 5. Responses & Errors

### MSG_SUCCESS (type=100)
```
Header:
  type:           0x00000064 (100)
  sender_id:      0x00000000 (server)
  target_id:      <user_id>
  payload_length: varies

Payload (JSON):
  {"success":true,"message":"Match search cancelled"}
```

### MSG_ERROR (type=101)
```
Header:
  type:           0x00000065 (101)
  sender_id:      0x00000000 (server)
  target_id:      <user_id>
  payload_length: varies

Payload (JSON):
  {"success":false,"error":"Not logged in"}
```

### MSG_INVALID_MOVE (type=102)
```
Header:
  type:           0x00000066 (102)
  sender_id:      0x00000000 (server)
  target_id:      <user_id>
  payload_length: varies

Payload (JSON):
  {"error":"Cannot buy now"}
```

### MSG_NOT_YOUR_TURN (type=103)
```
Header:
  type:           0x00000067 (103)
  sender_id:      0x00000000 (server)
  target_id:      <user_id>
  payload_length: varies

Payload (JSON):
  {"error":"Not your turn"}
```

### MSG_HEARTBEAT (type=104)
```
Header:
  type:           0x00000068 (104)
  sender_id:      <user_id>
  target_id:      0x00000000 (server)
  payload_length: 0x00000000

Payload: (none)
```

### MSG_HEARTBEAT_ACK (type=105)
```
Header:
  type:           0x00000069 (105)
  sender_id:      0x00000000 (server)
  target_id:      <user_id>
  payload_length: 0x00000000

Payload: (none)
```

---

