# TurnThem

A 2D "Breakout PvP" game built with Raylib. Two players place cannons on their
own colour; every shell converts the first enemy tile it hits. Whoever owns more
of the arena when the 3-minute timer runs out wins.

## Build Instructions

### Prerequisites
- CMake (version 3.16 or higher; CMake 4.x works)
- C++ compiler with C++17 support
- Git (for fetching ENet, and Raylib if it isn't installed)
- nlohmann-json (client only), e.g. `brew install nlohmann-json`

### Building the Project

1. Clone the repository:
   ```bash
   git clone https://github.com/yourusername/TurnThem.git
   cd TurnThem
   ```

2. Generate build files with CMake:
   ```bash
   cmake -S . -B build
   ```

3. Build everything (game client, server, test bot, tests):
   ```bash
   cmake --build build
   ```

4. Run the tests:
   ```bash
   ctest --test-dir build --output-on-failure
   ```

The build system will automatically download and compile Raylib if it's not already installed on your system.

## Playing

Start the game with `./build/TurnThem`.

- **Practice**: play against a bot on your own machine, no server needed.
- **Find Match**: connect to a server and get paired with the next player who is waiting.

Drag a card from the deck onto one of your own tiles (not next to another weapon). Press Esc to leave a match.

Client options:

| Option | Meaning |
|---|---|
| `--server host[:port]` | Server to use for Find Match. Defaults to `$TURNTHEM_SERVER`, then `127.0.0.1:7777`. |
| `--practice-side white\|black` | Which side you play in practice. Black sees the board rotated. |
| `--sim-lag S`, `--sim-jitter S` | Add artificial network delay (in seconds), for testing. |

## Running a server

The server pairs players first come, first served, and runs every match itself, so neither player can cheat or gets a latency advantage. It uses one UDP port and doesn't need Raylib:

```bash
./build/TurnThemServer --port 7777
```

Options: `--match-seconds`, `--countdown-seconds`, `--max-players`. It logs one line when each match starts and ends, and shuts down cleanly on Ctrl-C.

To try it locally, start the server, then play against the test bot (or open two game windows):

```bash
./build/TurnThemBot --server 127.0.0.1:7777 --count 1
```

`TurnThemBot --count N --matches M` also works as a load and soak test. It exits non-zero if any client ever disagrees with the server's board.

### On a Linux VPS

Build only the server; this needs no graphics libraries:

```bash
cmake -S . -B build -DTURNTHEM_BUILD_CLIENT=OFF -DTURNTHEM_BUILD_TESTS=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build --target TurnThemServer
```

Then:

1. Open UDP port 7777 in the firewall.
2. Install `deploy/turnthem-server.service` (edit the paths and user inside it).
3. Start the service:
   ```bash
   sudo cp deploy/turnthem-server.service /etc/systemd/system/
   sudo systemctl enable --now turnthem-server
   journalctl -u turnthem-server -f
   ```
4. Point players at it with `--server your.host.name` or `TURNTHEM_SERVER=your.host.name`.

Players need the same game version as the server. A mismatched client is told so when it connects.
