# RemoteShell

A lightweight TCP-based remote terminal server and client written in C++. It spawns an isolated bash session per client using pseudo-terminals (PTYs), forwarding raw terminal I/O over a socket with non-blocking I/O via `epoll`.

---

## Features

- **PTY-backed sessions** — each client gets a real pseudo-terminal with full interactive shell support (job control, readline, etc.)
- **Isolated working directories** — each client's shell is sandboxed into its own directory (named after the client IP)
- **Raw terminal mode** — the client enters raw mode for transparent key forwarding (arrow keys, Ctrl sequences, etc.)
- **Non-blocking I/O** — both server and client use `epoll` for efficient event-driven I/O
- **Graceful cleanup** — connections and PTY file descriptors are cleaned up on disconnect

---

## Requirements

- Linux (uses `epoll`, `openpty`, `ioctl`)
- GCC with C++17 support
- `libpthread`

---

## Build

A `Makefile` is provided. Simply run:

```bash
make          # builds both server and client
make server   # builds server only
make client   # builds client only
make clean    # removes compiled binaries
```

The Makefile uses:
- **Compiler:** `g++`
- **Standard:** C++17
- **Flags:** `-Wall -Wextra -g`
- **Linker:** `-lpthread`

---

## Usage

### Start the server
```bash
./server
```
The server listens on port `9090` by default.

### Connect with the client
```bash
./client
```
You will be prompted to enter the server's IP address:
```
Enter the IP address of the server: 127.0.0.1
Connected to the server from port 9090
```

> **To quit:** Press `Ctrl-Q`

---

## How It Works

```
Client                          Server
  |                               |
  |-------- TCP connect --------> |
  |                               |-- fork() + openpty()
  |                               |-- bash runs on PTY slave
  |                               |
  |<==== raw keystrokes =========>|<===> PTY master <===> bash
  |<==== terminal output ========<|
```

1. **Server** accepts a TCP connection and calls `openpty()` to create a master/slave PTY pair.
2. It `fork()`s a child process, attaches the slave PTY as stdin/stdout/stderr, and `exec`s `/bin/bash`.
3. The master PTY fd and the client socket fd are both registered with `epoll`.
4. Data from the client socket is written to the PTY master (keystrokes → bash), and output from the PTY master is written back to the client socket (bash output → client).
5. **Client** prompts for the server IP, puts the local terminal into raw mode, and uses `epoll` to multiplex between `stdin` and the socket, forwarding data bidirectionally.

---

## Configuration

| Constant      | File        | Default | Description              |
|---------------|-------------|---------|--------------------------|
| `PORT`        | both        | `9090`  | TCP port to use          |
| `MAX_EVENTS`  | server      | `10`    | Max epoll events         |
| `MAX_EVENTS`  | client      | `2`     | Max epoll events         |
| `BUFFER_SIZE` | client      | `1024`  | Read buffer size (bytes) |

---

## Project Structure

```
.
├── server.cpp   # Server: accepts connections, spawns PTY-backed bash sessions
├── client.cpp   # Client: connects to server, forwards terminal I/O
└── Makefile     # Build system
```

---

## Security Notice

> ⚠️ **This project is intended for local/educational use only.**
>
> - There is **no authentication** — any TCP client can connect and get a shell.
> - The server should **never** be exposed to an untrusted network.

---

## Known Limitations

- No encryption (plaintext socket).
- Terminal resize (`SIGWINCH` / `TIOCSWINSZ`) is not propagated to the PTY.

---

## License

MIT
