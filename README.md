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

## How to run code ?

run command
```
make 
```
---

## Usage

### Start the server

```bash
./server
```
server gives one private key using that key you can connect client.

### start the client 

```bash
./client
```

first client ask username , private key , seerver ip that you enter then you connect with server.

On success, a raw interactive bash session starts. Press **Ctrl-Q** to disconnect.

---

