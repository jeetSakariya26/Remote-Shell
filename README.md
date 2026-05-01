# Remote Shell over TCP

A lightweight, multi-user remote terminal system written in C++ for Linux. Clients connect over TCP and get a fully interactive bash session running on the server, with raw terminal support, window resizing, and token-based authentication — all over a custom binary packet protocol.

---

## Features

- **Raw terminal mode** — full interactive shell (arrow keys, tab completion, colors)
- **Custom binary packet protocol** — framed messages with type, length, and payload
- **Token-based session auth** — server generates a one-time key; clients must authenticate before any data is accepted
- **Per-user PTY** — each client gets its own isolated pseudo-terminal running bash
- **Dynamic window resizing** — `SIGWINCH` propagated to the remote PTY via `TIOCSWINSZ`
- **epoll-based I/O** — non-blocking, event-driven handling for multiple simultaneous clients
- **Directory isolation** — each session gets a working directory scoped to `<ip>_<username>`

---

## File Structure

```
.
├── client.cpp      # Client — auth, raw mode, epoll I/O loop
├── server.cpp      # Server — accept, PTY spawn, multi-client dispatch
├── packet.cpp      # Packet encode/decode for all message types
├── packet.h        # Packet type declarations and structs
├── helper.cpp      # Raw terminal mode + epoll utilities
└── helper.h        # Helper function declarations
```

---

## Packet Protocol

Every message is framed as:

```
[ 4 bytes: total body length (network order) ]
[ 1 byte:  message type                      ]
[ N bytes: payload                           ]
```

| Type | Name    | Payload Format                                  |
|------|---------|-------------------------------------------------|
| `0`  | Login   | `[1B key_len][key][1B user_len][username]`       |
| `1`  | Logout  | `[1B reason_len][reason]`                       |
| `2`  | Data    | `[1B token_len][token][1B data_len][data]`       |
| `3`  | Winsize | `[1B token_len][token][2B width][2B height]`    |
| `6`  | ACK     | `[1B token_len][token]`                         |
| `7`  | Error   | `[1B error_len][error_message]`                 |

---

## Build

Requires GCC with C++17, and `libutil` (for `openpty`).

```bash
g++ -std=c++17 -o server server.cpp packet.cpp helper.cpp -lutil
g++ -std=c++17 -o client client.cpp packet.cpp helper.cpp -lutil
```

---

## Usage

### Start the server

```bash
./server
```

The server listens on port `9090` and prints a randomly generated private key on startup:

```
[server] listening on port 9090
[server] private key: 483921
```

### Connect with a client

```bash
./client
```

You will be prompted for:

```
Enter username : alice
Enter key      : 483921
Enter IP       : 127.0.0.1
```

On success, a raw interactive bash session starts. Press **Ctrl-Q** to disconnect.

---

## How It Works

### Connection & Auth Flow

```
Client                          Server
  │                               │
  │── TCP connect ──────────────► │
  │── Type 0 (Login) ───────────► │  validate key
  │◄─ Type 6 (ACK + token) ────── │  spawn PTY + bash
  │── Type 3 (Winsize) ─────────► │  ioctl TIOCSWINSZ
  │                               │
  │  ◄─── Shell is live ───────►  │
```

### Live I/O Loop

```
Keystroke ──► Type 2 packet ──► server writes to ptyMaster
                                      │
                                 bash processes input
                                      │
                             epoll detects ptyMaster output
                                      │
                        Type 2 packet ──► client writes to STDOUT
```

### Window Resize

When the terminal window changes size, the client catches `SIGWINCH`, reads the new dimensions with `ioctl(TIOCGWINSZ)`, and sends a Type 3 packet. The server applies it to the PTY with `ioctl(TIOCSWINSZ)` and signals the child process.

---

## Security Notes

- The shared key is a 6-digit random number generated at server startup — suitable for demos and local networks, not for production use.
- There is no encryption in transit; all data including keystrokes travel in plaintext over TCP.
- For production use, wrap the connection in TLS (e.g., via OpenSSL or stunnel).

---

## Known Limitations

- Token length is stored as a single byte (`uint8_t`), limiting tokens to 255 characters.
- Data payload length in `message_of_data` uses a single byte for the keystroke count — bursts larger than 255 bytes may be truncated. Consider extending to a 2-byte or 4-byte length field for robustness.
- No reconnection logic; if the connection drops, the client must restart.

---

## Dependencies

- Linux (epoll, PTY support required)
- GCC 8+ with `-std=c++17`
- `libutil` (`openpty`)